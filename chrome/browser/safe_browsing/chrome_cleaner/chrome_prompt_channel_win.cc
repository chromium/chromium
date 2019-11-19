// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_prompt_channel_win.h"

#include <windows.h>

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/sparse_histogram.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/unguessable_token.h"
#include "base/win/win_util.h"
#include "base/win/windows_types.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "extensions/browser/extension_system.h"

namespace safe_browsing {

using base::win::ScopedHandle;
using chrome_cleaner::ChromePromptRequest;
using CleanerProcessDelegate = ChromePromptChannel::CleanerProcessDelegate;
using ErrorCategory = ChromePromptChannel::ErrorCategory;
using CustomErrors = ChromePromptChannel::CustomErrors;

constexpr char ChromePromptChannel::kErrorHistogramName[] =
    "SoftwareReporter.Cleaner.ChromePromptChannelError";

namespace {

template <typename ErrorType>
void WriteStatusErrorCodeToHistogram(ErrorCategory category,
                                     ErrorType error_type) {
  base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
      ChromePromptChannel::kErrorHistogramName,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(ChromePromptChannel::GetErrorCodeInt(category, error_type));
}

class CleanerProcessWrapper : public CleanerProcessDelegate {
 public:
  explicit CleanerProcessWrapper(const base::Process& process)
      : process_(process.Duplicate()) {}

  ~CleanerProcessWrapper() override = default;

  base::ProcessHandle Handle() const override { return process_.Handle(); }

  void TerminateOnError() const override {
    process_.Terminate(
        chrome_cleaner::RESULT_CODE_CHROME_PROMPT_IPC_DISCONNECTED_TOO_SOON,
        /*wait=*/false);
  }

 private:
  CleanerProcessWrapper(const CleanerProcessWrapper& other) = delete;
  CleanerProcessWrapper& operator=(const CleanerProcessWrapper& other) = delete;

  base::Process process_;
};

enum class ServerPipeDirection {
  kInbound,
  kOutbound,
};

// Opens a pipe of type PIPE_TYPE_MESSAGE and returns a pair (server handle,
// client handle). The client handle is safe to pass to a subprocess. Both
// handles will be invalid on failure.
std::pair<ScopedHandle, ScopedHandle> CreateMessagePipe(
    ServerPipeDirection server_direction) {
  SECURITY_ATTRIBUTES security_attributes = {};
  security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  // Use this process's default access token.
  security_attributes.lpSecurityDescriptor = nullptr;
  // Handles to inherit will be added to the LaunchOptions explicitly.
  security_attributes.bInheritHandle = false;

  base::string16 pipe_name = base::UTF8ToUTF16(
      base::StrCat({"\\\\.\\pipe\\chrome-cleaner-",
                    base::UnguessableToken::Create().ToString()}));

  // Create the server end of the pipe.
  DWORD direction_flag = server_direction == ServerPipeDirection::kInbound
                             ? PIPE_ACCESS_INBOUND
                             : PIPE_ACCESS_OUTBOUND;
  ScopedHandle server_handle(::CreateNamedPipe(
      pipe_name.c_str(), direction_flag | FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT |
          PIPE_REJECT_REMOTE_CLIENTS,
      /*nMaxInstances=*/1, /*nOutBufferSize=*/0, /*nInBufferSize=*/0,
      /*nDefaultTimeOut=*/0, &security_attributes));
  if (!server_handle.IsValid()) {
    PLOG(ERROR) << "Error creating server pipe";
    return std::make_pair(ScopedHandle(), ScopedHandle());
  }

  // The client pipe's read/write permissions are the opposite of the server's.
  DWORD client_mode = server_direction == ServerPipeDirection::kInbound
                          ? GENERIC_WRITE
                          : GENERIC_READ;

  // Create the client end of the pipe.
  ScopedHandle client_handle(::CreateFile(
      pipe_name.c_str(), client_mode, /*dwShareMode=*/0,
      /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS,
      /*hTemplateFile=*/nullptr));
  if (!client_handle.IsValid()) {
    PLOG(ERROR) << "Error creating client pipe";
    return std::make_pair(ScopedHandle(), ScopedHandle());
  }

  // Wait for the client end to connect (this should return
  // ERROR_PIPE_CONNECTED immediately since it's already connected).
  if (::ConnectNamedPipe(server_handle.Get(), /*lpOverlapped=*/nullptr)) {
    LOG(ERROR) << "ConnectNamedPipe got an unexpected connection";
    return std::make_pair(ScopedHandle(), ScopedHandle());
  }
  const auto error = ::GetLastError();
  if (error != ERROR_PIPE_CONNECTED) {
    LOG(ERROR) << "ConnectNamedPipe returned unexpected error: "
               << logging::SystemErrorCodeToString(error);
    return std::make_pair(ScopedHandle(), ScopedHandle());
  }

  return std::make_pair(std::move(server_handle), std::move(client_handle));
}

void AppendHandleToCommandLine(base::CommandLine* command_line,
                               const std::string& switch_string,
                               HANDLE handle) {
  DCHECK(command_line);
  command_line->AppendSwitchASCII(
      switch_string, base::NumberToString(base::win::HandleToUint32(handle)));
}

// Reads |buffer_size| bytes from a pipe of PIPE_TYPE_MESSAGE.
// Uses |error_category| to report WinAPI errors if needed.
// Uses |custom_error| to report short reads if needed.
bool ReadMessageFromPipe(HANDLE handle,
                         LPVOID buffer,
                         DWORD buffer_size,
                         ErrorCategory error_category,
                         CustomErrors short_read_error) {
  // If the process at the other end of the pipe is behaving correctly it will
  // write exactly |buffer_size| bytes to the pipe and PIPE_TYPE_MESSAGE
  // ensures that we read all of them at once. If the process writes more bytes
  // than expected, ::ReadFile will return ERROR_MORE_DATA. If it writes less
  // we treat that as an error.
  DWORD bytes_read = 0;
  if (!::ReadFile(handle, buffer, buffer_size, &bytes_read, nullptr)) {
    WriteStatusErrorCodeToHistogram(error_category,
                                    logging::GetLastSystemErrorCode());
    PLOG(ERROR) << "ReadFile failed";
    return false;
  }

  CHECK_LE(bytes_read, buffer_size);
  if (bytes_read != buffer_size) {
    LOG(ERROR) << "Short read (read " << bytes_read << " of " << buffer_size
               << ")";

    WriteStatusErrorCodeToHistogram(ErrorCategory::kCustomError,
                                    short_read_error);
    return false;
  }
  return true;
}

// Loop doing blocking reads from the IPC pipe with |request_read_handle| until
// a CloseConnection message is received or an error occurs. For each message
// deserialized from the pipe a handler method on |channel| will be called.
//
// |request_read_handle| is owned by |channel|, which is never deleted until
// after the cleaner process at the other end of the pipe exits.
//
// This should be invoked from a CONTINUE_ON_SHUTDOWN task so that it does not
// block Chrome shutdown.
//
// Exit conditions:
// * If the cleaner process sends a CloseConnection message the loop will exit
//   cleanly.
// * If the cleaner process exits without sending a CloseConnection message,
//   the next ::ReadFile call will return ERROR_BROKEN_PIPE.
// * If |channel| is deleted or channel->CloseHandles is called, the next
//   ::ReadFile call will return ERROR_INVALID_HANDLE.
//
// Important: it is not safe to dereference |channel| on this sequence. Always
// post to |task_runner| instead. Since |channel| is deleted on |task_runner|,
// dereferencing the weak pointer on any other sequence is racy.
//
// When done, calls |on_connection_closed|. On error also slays
// |cleaner_process|, which is the other end of the pipe.
void ServiceChromePromptRequests(
    base::WeakPtr<ChromePromptChannel> channel,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    HANDLE request_read_handle,
    std::unique_ptr<CleanerProcessDelegate> cleaner_process,
    base::OnceClosure on_connection_closed) {
  // Always call OnConnectionClosed when finished whether it's with an error or
  // because a CloseConnectionRequest was received.
  base::ScopedClosureRunner call_connection_closed(
      std::move(on_connection_closed));

  // On any error condition, kill the cleaner process (if it's already dead this
  // is a no-op).
  base::ScopedClosureRunner kill_cleaner_on_error(base::BindOnce(
      &CleanerProcessDelegate::TerminateOnError, std::move(cleaner_process)));

  // Wait for the cleaner to write a single version byte and make sure it's
  // using a recognized version.
  DWORD bytes_read = 0;
  uint8_t version = 0;
  // Since the field is only 1 byte it's not possible to have a short read.
  static_assert(sizeof(version) == 1,
                "version field must be readable in 1 byte");

  if (!::ReadFile(request_read_handle, &version, sizeof(version), &bytes_read,
                  nullptr)) {
    WriteStatusErrorCodeToHistogram(ErrorCategory::kReadVersionWinError,
                                    logging::GetLastSystemErrorCode());
    PLOG(ERROR) << "Failed to read protocol version";
    return;
  }

  CHECK_EQ(bytes_read, sizeof(version));
  if (version != 1) {
    WriteStatusErrorCodeToHistogram(ErrorCategory::kCustomError,
                                    CustomErrors::kWrongHandshakeVersion);

    LOG(ERROR) << "Cleaner requested unsupported version " << version;
    return;
  }

  // Repeatedly wait for requests from the cleaner.
  while (true) {
    // Read the request length followed by a request.
    uint32_t request_length = 0;
    if (!ReadMessageFromPipe(request_read_handle, &request_length,
                             sizeof(request_length),
                             ErrorCategory::kReadRequestLengthWinError,
                             CustomErrors::kRequestLengthShortRead)) {
      return;
    }

    if (request_length < 1 ||
        request_length > ChromePromptChannel::kMaxMessageLength) {
      WriteStatusErrorCodeToHistogram(ErrorCategory::kCustomError,
                                      CustomErrors::kRequestInvalidSize);
      LOG(ERROR) << "Bad request length: " << request_length;
      return;
    }
    std::string request;
    if (!ReadMessageFromPipe(
            request_read_handle, base::WriteInto(&request, request_length + 1),
            request_length, ErrorCategory::kReadRequestWinError,
            CustomErrors::kRequestShortRead)) {
      return;
    }

    ChromePromptRequest chrome_prompt_request;
    if (!chrome_prompt_request.ParseFromString(request)) {
      LOG(ERROR) << "Read invalid message";
      WriteStatusErrorCodeToHistogram(ErrorCategory::kCustomError,
                                      CustomErrors::kRequestContentInvalid);
      return;
    }

    switch (chrome_prompt_request.request_case()) {
      case ChromePromptRequest::kQueryCapability:
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&ChromePromptChannel::HandleQueryCapabilityRequest,
                           channel, chrome_prompt_request.query_capability()));
        break;
      case ChromePromptRequest::kPromptUser:
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&ChromePromptChannel::HandlePromptUserRequest,
                           channel, chrome_prompt_request.prompt_user()));
        break;
      case ChromePromptRequest::kRemoveExtensions:
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&ChromePromptChannel::HandleRemoveExtensionsRequest,
                           channel, chrome_prompt_request.remove_extensions()));
        break;
      case ChromePromptRequest::kCloseConnection: {
        // Normal exit: do not kill the cleaner. OnConnectionClosed will still
        // be called.
        kill_cleaner_on_error.ReplaceClosure(base::DoNothing());

        // The cleaner process will continue running but the pipe handles are
        // no longer needed.
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&ChromePromptChannel::CloseHandles, channel));
        return;
      }
      default:
        LOG(ERROR) << "Read unknown request";

        WriteStatusErrorCodeToHistogram(ErrorCategory::kCustomError,
                                        CustomErrors::kRequestUnknown);
        return;
    }
  }
}

}  // namespace

// static
std::unique_ptr<CleanerProcessDelegate>
ChromePromptChannel::CreateDelegateForProcess(const base::Process& process) {
  return std::make_unique<CleanerProcessWrapper>(process);
}

ChromePromptChannel::ChromePromptChannel(
    base::OnceClosure on_connection_closed,
    std::unique_ptr<ChromePromptActions> actions,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : on_connection_closed_(std::move(on_connection_closed)),
      actions_(std::move(actions)),
      task_runner_(std::move(task_runner)) {
  // The sequence checker validates that all handler methods and the destructor
  // are called from the same sequence, which is not the same sequence as the
  // constructor.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ChromePromptChannel::~ChromePromptChannel() {
  // To avoid race conditions accessing WeakPtr's this must be deleted on the
  // same sequence as the request handler methods are called.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool ChromePromptChannel::PrepareForCleaner(
    base::CommandLine* command_line,
    base::HandlesToInheritVector* handles_to_inherit) {
  // Requests flow from client to server.
  std::tie(request_read_handle_, request_write_handle_) =
      CreateMessagePipe(ServerPipeDirection::kInbound);

  // Responses flow from server to client.
  std::tie(response_write_handle_, response_read_handle_) =
      CreateMessagePipe(ServerPipeDirection::kOutbound);

  if (!request_read_handle_.IsValid() || !request_write_handle_.IsValid() ||
      !response_read_handle_.IsValid() || !response_write_handle_.IsValid()) {
    return false;
  }

  // The Chrome Cleanup tool will write to the request pipe and read from the
  // response pipe.
  DCHECK(command_line);
  DCHECK(handles_to_inherit);
  AppendHandleToCommandLine(command_line,
                            chrome_cleaner::kChromeWriteHandleSwitch,
                            request_write_handle_.Get());
  handles_to_inherit->push_back(request_write_handle_.Get());
  AppendHandleToCommandLine(command_line,
                            chrome_cleaner::kChromeReadHandleSwitch,
                            response_read_handle_.Get());
  handles_to_inherit->push_back(response_read_handle_.Get());
  return true;
}

void ChromePromptChannel::CleanupAfterCleanerLaunchFailed() {
  request_read_handle_.Close();
  request_write_handle_.Close();
  response_read_handle_.Close();
  response_write_handle_.Close();
}

void ChromePromptChannel::ConnectToCleaner(
    std::unique_ptr<CleanerProcessDelegate> cleaner_process) {
  // The handles that were passed to the cleaner are no longer needed in this
  // process.
  request_write_handle_.Close();
  response_read_handle_.Close();

  // ServiceChromePromptRequests will loop until a CloseConnection message is
  // received over IPC or an error occurs, and will call request handler
  // methods on |task_runner_|.
  //
  // This object continues to own the pipe handles. ServiceChromePromptRequests
  // gets a raw handle, which can go invalid at any time either because the
  // other end of the pipe closes or CloseHandles is called. When that happens
  // the next call to ::ReadFile will return an error and
  // ServiceChromePromptRequests will return.
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ServiceChromePromptRequests, weak_factory_.GetWeakPtr(),
                     task_runner_, request_read_handle_.Get(),
                     std::move(cleaner_process),
                     std::move(on_connection_closed_)));
}

void ChromePromptChannel::WriteResponseMessage(
    const google::protobuf::MessageLite& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedClosureRunner error_handler(base::BindOnce(
      &ChromePromptChannel::CloseHandles, base::Unretained(this)));

  std::string response_string;
  if (!message.SerializeToString(&response_string)) {
    LOG(ERROR) << "Failed to serialize response message";
    return;
  }

  DWORD bytes_written = 0;
  uint32_t message_size = response_string.size();
  if (!::WriteFile(response_write_handle_.Get(), &message_size,
                   sizeof(uint32_t), &bytes_written, nullptr)) {
    WriteStatusErrorCodeToHistogram(ErrorCategory::kWriteResponseLengthWinError,
                                    logging::GetLastSystemErrorCode());
    PLOG(ERROR) << "Failed to write message size";
    return;
  }
  CHECK_EQ(bytes_written, sizeof(uint32_t));
  if (!::WriteFile(response_write_handle_.Get(), response_string.data(),
                   message_size, &bytes_written, nullptr)) {
    WriteStatusErrorCodeToHistogram(ErrorCategory::kWriteResponseWinError,
                                    logging::GetLastSystemErrorCode());
    PLOG(ERROR) << "Failed to write message of length " << message_size;
    return;
  }
  CHECK_EQ(bytes_written, message_size);

  // No error occurred.
  error_handler.ReplaceClosure(base::DoNothing());
}

void ChromePromptChannel::CloseHandles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This will cause the next ::ReadFile call in ServiceChromePromptRequests to
  // fail, triggering the error handler that kills the cleaner process.
  ::CancelIoEx(request_read_handle_.Get(), nullptr);
  ::CancelIoEx(response_write_handle_.Get(), nullptr);
  request_read_handle_.Close();
  response_write_handle_.Close();
}

void ChromePromptChannel::HandleQueryCapabilityRequest(
    const chrome_cleaner::QueryCapabilityRequest&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // No optional capabilities are supported. Send back an empty response.
  WriteResponseMessage(chrome_cleaner::QueryCapabilityResponse());
}

void ChromePromptChannel::HandlePromptUserRequest(
    const chrome_cleaner::PromptUserRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedClosureRunner error_handler(base::BindOnce(
      &ChromePromptChannel::CloseHandles, base::Unretained(this)));

  // If there are any fields we don't know how to display, do not prompt. (Not
  // an error, could just be a more recent cleaner version.)
  if (!request.unknown_fields().empty()) {
    LOG(ERROR) << "Discarding PromptUserRequest with unknown fields.";
    WriteStatusErrorCodeToHistogram(ErrorCategory::kCustomError,
                                    CustomErrors::kRequestUnknownField);
    return;
  }

  std::vector<base::FilePath> files_to_delete;
  files_to_delete.reserve(request.files_to_delete_size());
  for (const std::string& file_path : request.files_to_delete()) {
    base::string16 file_path_utf16;
    if (!base::UTF8ToUTF16(file_path.c_str(), file_path.size(),
                           &file_path_utf16)) {
      LOG(ERROR) << "Undisplayable file path in PromptUserRequest.";
      WriteStatusErrorCodeToHistogram(ErrorCategory::kCustomError,
                                      CustomErrors::kUndisplayableFilePath);
      return;
    }
    files_to_delete.push_back(base::FilePath(file_path_utf16));
  }

  base::Optional<std::vector<base::string16>> optional_registry_keys;
  if (request.registry_keys_size()) {
    std::vector<base::string16> registry_keys;
    registry_keys.reserve(request.registry_keys_size());
    for (const std::string& registry_key : request.registry_keys()) {
      base::string16 registry_key_utf16;
      if (!base::UTF8ToUTF16(registry_key.c_str(), registry_key.size(),
                             &registry_key_utf16)) {
        LOG(ERROR) << "Undisplayable registry key in PromptUserRequest.";
        WriteStatusErrorCodeToHistogram(
            ErrorCategory::kCustomError,
            CustomErrors::kUndisplayableRegistryKey);
        return;
      }
      registry_keys.push_back(registry_key_utf16);
    }
    optional_registry_keys = registry_keys;
  }

  base::Optional<std::vector<base::string16>> optional_extension_ids;
  if (request.extension_ids_size()) {
    std::vector<base::string16> extension_ids;
    extension_ids.reserve(request.extension_ids_size());
    for (const std::string& extension_id : request.extension_ids()) {
      base::string16 extension_id_utf16;
      if (!base::UTF8ToUTF16(extension_id.c_str(), extension_id.size(),
                             &extension_id_utf16)) {
        LOG(ERROR) << "Undisplayable extension id in PromptUserRequest.";
        WriteStatusErrorCodeToHistogram(ErrorCategory::kCustomError,
                                        CustomErrors::kUndisplayableExtension);
        return;
      }
      extension_ids.push_back(extension_id_utf16);
    }
    optional_extension_ids = extension_ids;
  }

  // No error occurred.
  error_handler.ReplaceClosure(base::DoNothing());

  // Ensure SendPromptUserResponse runs on this sequence.
  auto response_callback = base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> task_runner,
         base::WeakPtr<ChromePromptChannel> channel,
         chrome_cleaner::PromptUserResponse::PromptAcceptance acceptance) {
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&ChromePromptChannel::SendPromptUserResponse,
                           channel, acceptance));
      },
      task_runner_, weak_factory_.GetWeakPtr());
  actions_->PromptUser(files_to_delete, optional_registry_keys,
                       optional_extension_ids, std::move(response_callback));
}

void ChromePromptChannel::HandleRemoveExtensionsRequest(
    const chrome_cleaner::RemoveExtensionsRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedClosureRunner error_handler(base::BindOnce(
      &ChromePromptChannel::CloseHandles, base::Unretained(this)));

  // extension_ids are mandatory.
  if (!request.extension_ids_size()) {
    LOG(ERROR) << "Bad RemoveExtensionsRequest";
    return;
  }

  std::vector<base::string16> extension_ids;
  extension_ids.reserve(request.extension_ids_size());
  for (const std::string& extension_id : request.extension_ids()) {
    base::string16 extension_id_utf16;
    if (!base::UTF8ToUTF16(extension_id.c_str(), extension_id.size(),
                           &extension_id_utf16)) {
      LOG(ERROR) << "Unusable extension id in RemoveExtensionsReqest.";
      return;
    }
    extension_ids.push_back(extension_id_utf16);
  }

  // No error occurred.
  error_handler.ReplaceClosure(base::DoNothing());

  chrome_cleaner::RemoveExtensionsResponse response;
  response.set_success(actions_->DisableExtensions(extension_ids));
  WriteResponseMessage(response);
}

void ChromePromptChannel::SendPromptUserResponse(
    chrome_cleaner::PromptUserResponse::PromptAcceptance acceptance) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  chrome_cleaner::PromptUserResponse response;
  response.set_prompt_acceptance(acceptance);
  WriteResponseMessage(response);
}

}  // namespace safe_browsing
