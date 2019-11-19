// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_PROMPT_CHANNEL_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_PROMPT_CHANNEL_WIN_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/win/scoped_handle.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_prompt_actions_win.h"
#include "components/chrome_cleaner/public/proto/chrome_prompt.pb.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace base {
class CommandLine;
}  // namespace base

namespace safe_browsing {

class ChromePromptActions;

// Handles IPC to the Chrome Cleaner process. The Chrome Cleaner process will
// send requests and ChromePromptChannel will handle the request, often by
// using ChromePromptActions, and write the response.
//
// This implementation serializes protobufs over a pipe, instead of using Mojo,
// because the Chrome Cleaner process might be built with a different version
// of Mojo that isn't wire-compatible.
//
// The interface specification is in
// "components/chrome_cleaner/public/proto/chrome_prompt.proto".
class ChromePromptChannel {
 public:
  // Gives access to the Chrome Cleaner process that the channel communicates
  // with.
  class CleanerProcessDelegate {
   public:
    virtual ~CleanerProcessDelegate() = default;

    virtual base::ProcessHandle Handle() const = 0;

    virtual void TerminateOnError() const = 0;
  };

  static const char kErrorHistogramName[];
  static constexpr uint32_t kMaxMessageLength = 1 * 1024 * 1024;  // 1M bytes

  // Values from this enum will serve as the high bits of the sparse histogram
  // values.  We will be able to use them to separate the errors by category if
  // we ever need to analyze them.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ErrorCategory : uint16_t {
    kCustomError = 1,
    kReadVersionWinError = 2,
    kReadRequestLengthWinError = 3,
    kReadRequestWinError = 4,
    kWriteResponseWinError = 5,
    kWriteResponseLengthWinError = 6,
  };

  // Code that describes the error precisely.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CustomErrors : uint16_t {
    kWrongHandshakeVersion = 1,
    kRequestLengthShortRead = 2,
    kRequestShortRead = 3,
    kRequestInvalidSize = 4,
    kRequestContentInvalid = 5,
    kRequestUnknown = 6,
    kRequestUnknownField = 7,
    kUndisplayableFilePath = 8,
    kUndisplayableRegistryKey = 9,
    kUndisplayableExtension = 10,
  };

  static int32_t GetErrorCodeInt(ErrorCategory category,
                                 CustomErrors error_code) {
    return GetErrorCodeInt(category, static_cast<uint16_t>(error_code));
  }

  static int32_t GetErrorCodeInt(ErrorCategory category, uint16_t error_code) {
    // We are storing the bits of the category and error_code in the return type
    // without caring about the numerical values. Make sure everything fits.
    using CategoryNumType = std::underlying_type<ErrorCategory>::type;
    using ErrorCodeType = decltype(error_code);
    static_assert(std::is_same<CategoryNumType, ErrorCodeType>::value,
                  "Category and error code types need to be the same.");
    static_assert(
        sizeof(int32_t) == sizeof(CategoryNumType) + sizeof(ErrorCodeType),
        "Values won't fit in return type.");

    int32_t category_and_code = static_cast<CategoryNumType>(category);
    category_and_code <<= sizeof(category) * CHAR_BIT;
    category_and_code |= error_code;
    return category_and_code;
  }

  // Returns a CleanerProcessDelegate that wraps |process|.
  static std::unique_ptr<CleanerProcessDelegate> CreateDelegateForProcess(
      const base::Process& process);

  // Creates a ChromePromptChannel that calls |on_connection_closed| when the
  // IPC channel closes (either normally or on error) and uses |actions| to
  // fulfill requests. |task_runner| can be used to run any tasks that must be
  // seqeuenced with destruction of the ChromePromptChannel.
  ChromePromptChannel(base::OnceClosure on_connection_closed,
                      std::unique_ptr<ChromePromptActions> actions,
                      scoped_refptr<base::SequencedTaskRunner> task_runner);

  ~ChromePromptChannel();

  // Prepares an IPC channel to be used by the cleaner process that is about to
  // be launched. Adds all handles used by the channel to |handles_to_inherit|
  // so that the cleaner process can access them, and adds switches to
  // |command_line| that the cleaner process can use to connect to the channel.
  bool PrepareForCleaner(base::CommandLine* command_line,
                         base::HandlesToInheritVector* handles_to_inherit);

  // Does any cleanup required if the cleaner process fails to launch after
  // PrepareForCleaner was called.
  void CleanupAfterCleanerLaunchFailed();

  // Kicks off communication between the IPC channel prepared by
  // PrepareForCleaner and the process in |cleaner_process|. If the connection
  // fails, |connection_closed_callback_| should be called.
  void ConnectToCleaner(
      std::unique_ptr<CleanerProcessDelegate> cleaner_process);

  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return task_runner_;
  }

  // Handles |request| and sends a QueryCapabilityResponse in reply.
  void HandleQueryCapabilityRequest(
      const chrome_cleaner::QueryCapabilityRequest& request);

  // Handles |request| and sends a PromptUserResponse in reply.
  void HandlePromptUserRequest(
      const chrome_cleaner::PromptUserRequest& request);

  // Handles |request| and sends a RemoveExtensionsResponse in reply.
  void HandleRemoveExtensionsRequest(
      const chrome_cleaner::RemoveExtensionsRequest& request);

  // Closes request_read_handle_ and request_write_handle_, which will trigger
  // an error handler in ServiceChromePromptRequests.
  void CloseHandles();

 private:
  ChromePromptChannel(const ChromePromptChannel& other) = delete;
  ChromePromptChannel& operator=(const ChromePromptChannel& other) = delete;

  // Serializes |message| to response_write_handle_. Calls CloseHandles on
  // error.
  void WriteResponseMessage(const google::protobuf::MessageLite& message);

  // Sends a PromptUserResponse with the given |acceptance| value.
  void SendPromptUserResponse(
      chrome_cleaner::PromptUserResponse::PromptAcceptance acceptance);

  SEQUENCE_CHECKER(sequence_checker_);

  base::OnceClosure on_connection_closed_;
  std::unique_ptr<ChromePromptActions> actions_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Requests always flow from the Chrome Cleanup tool to Chrome.
  // This class owns request_read_handle_ but request_write_handle_ will be
  // closed once it is passed to the child process.
  base::win::ScopedHandle request_read_handle_;
  base::win::ScopedHandle request_write_handle_;

  // Responses flow from Chrome to the Chrome Cleanup tool.
  // This class owns response_write_handle_ but response_read_handle_ will be
  // closed once it is passed to the child process.
  base::win::ScopedHandle response_read_handle_;
  base::win::ScopedHandle response_write_handle_;

  base::WeakPtrFactory<ChromePromptChannel> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_PROMPT_CHANNEL_WIN_H_
