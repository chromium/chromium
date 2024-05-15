// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_logging_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/supports_user_data.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"
#include "chrome/browser/media/webrtc/webrtc_log_uploader.h"
#include "chrome/browser/media/webrtc/webrtc_rtp_dump_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/webrtc_logging/browser/text_log_list.h"
#include "content/public/browser/render_process_host.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "content/public/browser/child_process_security_policy.h"
#include "storage/browser/file_system/isolated_context.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

using webrtc_event_logging::WebRtcEventLogManager;

namespace {

// Key used to attach the handler to the RenderProcessHost.
constexpr char kRenderProcessHostKey[] = "kWebRtcLoggingControllerKey";

}  // namespace

// static
void WebRtcLoggingController::AttachToRenderProcessHost(
    content::RenderProcessHost* host) {
  host->SetUserData(
      kRenderProcessHostKey,
      std::make_unique<base::UserDataAdapter<WebRtcLoggingController>>(
          new WebRtcLoggingController(host->GetID(),
                                      host->GetBrowserContext())));
}

// static
WebRtcLoggingController* WebRtcLoggingController::FromRenderProcessHost(
    content::RenderProcessHost* host) {
  return base::UserDataAdapter<WebRtcLoggingController>::Get(
      host, kRenderProcessHostKey);
}

void WebRtcLoggingController::SetMetaData(
    std::unique_ptr<WebRtcLogMetaDataMap> meta_data,
    GenericDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  // Set the web app ID if there's a "client" key, otherwise leave it unchanged.
  for (const auto& it : *meta_data) {
    if (it.first == "client") {
      web_app_id_ = static_cast<int>(base::PersistentHash(it.second));
      text_log_handler_->SetWebAppId(web_app_id_);
      break;
    }
  }

  text_log_handler_->SetMetaData(std::move(meta_data), std::move(callback));
}

void WebRtcLoggingController::StartLogging(GenericDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  // Request a log_slot from the LogUploader and start logging.
  if (text_log_handler_->StartLogging(std::move(callback))) {
    // Start logging in the renderer. The callback has already been fired since
    // there is no acknowledgement when the renderer actually starts.
    content::RenderProcessHost* host =
        content::RenderProcessHost::FromID(render_process_id_);

    // OK to rebind existing |logging_agent_| and |receiver_| connections.
    logging_agent_.reset();
    receiver_.reset();

    host->BindReceiver(logging_agent_.BindNewPipeAndPassReceiver());
    logging_agent_.set_disconnect_handler(
        base::BindOnce(&WebRtcLoggingController::OnAgentDisconnected, this));
    logging_agent_->Start(receiver_.BindNewPipeAndPassRemote());
  }
}

void WebRtcLoggingController::StopLogging(GenericDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  // Change the state to STOPPING and disable logging in the browser.
  if (text_log_handler_->StopLogging(std::move(callback))) {
    // Stop logging in the renderer. OnStopped will be called when this is done
    // to change the state from STOPPING to STOPPED and fire the callback.
    logging_agent_->Stop();
  }
}

void WebRtcLoggingController::UploadLog(UploadDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  // This functions uploads both text logs (mandatory) and RTP dumps (optional).
  // TODO(terelius): If there's no text log available (either because it hasn't
  // been started or because it hasn't been stopped), the current implementation
  // will fire an error callback and leave any RTP dumps in a local directory.
  // Would it be better to upload whatever logs we have, or would the lack of
  // an error callback make it harder to debug potential errors?

  base::UmaHistogramSparse("WebRtcTextLogging.UploadStarted", web_app_id_);

  WebRtcLogUploader* log_uploader = WebRtcLogUploader::GetInstance();
  log_uploader->background_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(log_directory_getter_),
      base::BindOnce(&WebRtcLoggingController::TriggerUpload, this,
                     std::move(callback)));
}

void WebRtcLoggingController::UploadStoredLog(const std::string& log_id,
                                              UploadDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  base::UmaHistogramSparse("WebRtcTextLogging.UploadStoredStarted",
                           web_app_id_);

  // Make this a method call on log_uploader

  WebRtcLogUploader::UploadDoneData upload_data;
  upload_data.callback = std::move(callback);
  upload_data.local_log_id = log_id;
  upload_data.web_app_id = web_app_id_;

  WebRtcLogUploader* log_uploader = WebRtcLogUploader::GetInstance();
  if (!IsWebRtcTextLogAllowed(GetBrowserContext())) {
    log_uploader->NotifyUploadDisabled(std::move(upload_data));
    return;
  }

  log_uploader->background_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](WebRtcLogUploader::UploadDoneData upload_data,
                        base::RepeatingCallback<base::FilePath(void)>
                            log_directory_getter) {
                       upload_data.paths.directory = log_directory_getter.Run();
                       WebRtcLogUploader* uploader =
                           WebRtcLogUploader::GetInstance();
                       uploader->UploadStoredLog(std::move(upload_data));
                     },
                     std::move(upload_data), log_directory_getter_));
}

void WebRtcLoggingController::DiscardLog(GenericDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  if (!text_log_handler_->ExpectLoggingStateStopped(&callback)) {
    // The callback is fired with an error message by ExpectLoggingStateStopped.
    return;
  }
  WebRtcLogUploader* log_uploader = WebRtcLogUploader::GetInstance();
  log_uploader->LoggingStoppedDontUpload();
  text_log_handler_->DiscardLog();
  rtp_dump_handler_.reset();
  stop_rtp_dump_callback_.Reset();
  FireGenericDoneCallback(std::move(callback), true, "");
}

// Stores the log locally using a hash of log_id + security origin.
void WebRtcLoggingController::StoreLog(const std::string& log_id,
                                       GenericDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  if (!text_log_handler_->ExpectLoggingStateStopped(&callback)) {
    // The callback is fired with an error message by ExpectLoggingStateStopped.
    return;
  }

  if (rtp_dump_handler_) {
    if (stop_rtp_dump_callback_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(stop_rtp_dump_callback_), true, true));
      stop_rtp_dump_callback_.Reset();
    }

    rtp_dump_handler_->StopOngoingDumps(
        base::BindOnce(&WebRtcLoggingController::StoreLogContinue, this, log_id,
                       std::move(callback)));
    return;
  }

  StoreLogContinue(log_id, std::move(callback));
}

void WebRtcLoggingController::StoreLogContinue(const std::string& log_id,
                                               GenericDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  std::unique_ptr<WebRtcLogPaths> log_paths(new WebRtcLogPaths());
  ReleaseRtpDumps(log_paths.get());

  WebRtcLogUploader* log_uploader = WebRtcLogUploader::GetInstance();
  log_uploader->background_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(log_directory_getter_),
      base::BindOnce(&WebRtcLoggingController::StoreLogInDirectory, this,
                     log_id, std::move(log_paths), std::move(callback)));
}

void WebRtcLoggingController::StartRtpDump(RtpDumpType type,
                                           GenericDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (stop_rtp_dump_callback_) {
    DCHECK(rtp_dump_handler_);
    FireGenericDoneCallback(std::move(callback), false,
                            "RTP dump already started.");
    return;
  }

  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id_);

  // This call cannot fail.
  stop_rtp_dump_callback_ = host->StartRtpDump(
      type == RTP_DUMP_INCOMING || type == RTP_DUMP_BOTH,
      type == RTP_DUMP_OUTGOING || type == RTP_DUMP_BOTH,
      base::BindRepeating(&WebRtcLoggingController::OnRtpPacket, this));

  if (!rtp_dump_handler_) {
    WebRtcLogUploader* log_uploader = WebRtcLogUploader::GetInstance();
    log_uploader->background_task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(log_directory_getter_),
        base::BindOnce(&WebRtcLoggingController::CreateRtpDumpHandlerAndStart,
                       this, type, std::move(callback)));
    return;
  }

  DoStartRtpDump(type, std::move(callback));
}

void WebRtcLoggingController::StopRtpDump(RtpDumpType type,
                                          GenericDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  if (!rtp_dump_handler_) {
    FireGenericDoneCallback(std::move(callback), false,
                            "RTP dump has not been started.");
    return;
  }

  if (stop_rtp_dump_callback_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(stop_rtp_dump_callback_),
                       type == RTP_DUMP_INCOMING || type == RTP_DUMP_BOTH,
                       type == RTP_DUMP_OUTGOING || type == RTP_DUMP_BOTH));
    stop_rtp_dump_callback_.Reset();
  }

  rtp_dump_handler_->StopDump(type, std::move(callback));
}

void WebRtcLoggingController::StartEventLogging(
    const std::string& session_id,
    size_t max_log_size_bytes,
    int output_period_ms,
    size_t web_app_id,
    const StartEventLoggingCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WebRtcEventLogManager::GetInstance()->StartRemoteLogging(
      render_process_id_, session_id, max_log_size_bytes, output_period_ms,
      web_app_id, callback);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void WebRtcLoggingController::GetLogsDirectory(
    LogsDirectoryCallback callback,
    LogsDirectoryErrorCallback error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  WebRtcLogUploader* log_uploader = WebRtcLogUploader::GetInstance();
  log_uploader->background_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(log_directory_getter_),
      base::BindOnce(&WebRtcLoggingController::GrantLogsDirectoryAccess, this,
                     std::move(callback), std::move(error_callback)));
}

void WebRtcLoggingController::GrantLogsDirectoryAccess(
    LogsDirectoryCallback callback,
    LogsDirectoryErrorCallback error_callback,
    const base::FilePath& logs_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (logs_path.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback),
                                  "Logs directory not available"));
    return;
  }

  storage::IsolatedContext* isolated_context =
      storage::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  std::string registered_name;
  storage::IsolatedContext::ScopedFSHandle file_system =
      isolated_context->RegisterFileSystemForPath(storage::kFileSystemTypeLocal,
                                                  std::string(), logs_path,
                                                  &registered_name);

  // Only granting read and delete access to reduce contention with
  // webrtcLogging APIs that modify files in that folder.
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  policy->GrantReadFileSystem(render_process_id_, file_system.id());
  // Delete is needed to prevent accumulation of files.
  policy->GrantDeleteFromFileSystem(render_process_id_, file_system.id());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), file_system.id(), registered_name));
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

void WebRtcLoggingController::OnRtpPacket(
    base::HeapArray<uint8_t> packet_header,
    size_t packet_length,
    bool incoming) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // |rtp_dump_handler_| could be null if we are waiting for the FILE thread to
  // create/ensure the log directory.
  if (rtp_dump_handler_) {
    rtp_dump_handler_->OnRtpPacket(packet_header.data(), packet_header.size(),
                                   packet_length, incoming);
  }
}

void WebRtcLoggingController::OnAddMessages(
    std::vector<chrome::mojom::WebRtcLoggingMessagePtr> messages) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (text_log_handler_->GetState() == WebRtcTextLogHandler::STARTED ||
      text_log_handler_->GetState() == WebRtcTextLogHandler::STOPPING) {
    for (auto& message : messages)
      text_log_handler_->LogWebRtcLoggingMessage(message.get());
  }
}

void WebRtcLoggingController::OnStopped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (text_log_handler_->GetState() != WebRtcTextLogHandler::STOPPING) {
    // If an out-of-order response is received, stop_callback_ may be invalid,
    // and must not be invoked.
    DLOG(ERROR) << "OnStopped invoked in state "
                << text_log_handler_->GetState();
    receiver_.ReportBadMessage("WRLHH: OnStopped invoked in unexpected state.");
    return;
  }
  text_log_handler_->StopDone();
}

WebRtcLoggingController::WebRtcLoggingController(
    int render_process_id,
    content::BrowserContext* browser_context)
    : receiver_(this),
      render_process_id_(render_process_id),
      log_directory_getter_(base::BindRepeating(
          &WebRtcLoggingController::GetLogDirectoryAndEnsureExists,
          browser_context->GetPath())),
      upload_log_on_render_close_(false),
      text_log_handler_(
          std::make_unique<WebRtcTextLogHandler>(render_process_id)) {}

WebRtcLoggingController::~WebRtcLoggingController() {
  // If we hit this, then we might be leaking a log reference count (see
  // ApplyForStartLogging).
  DCHECK_EQ(WebRtcTextLogHandler::CLOSED, text_log_handler_->GetState());
}

void WebRtcLoggingController::OnAgentDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (text_log_handler_->GetChannelIsClosing())
    return;

  WebRtcLogUploader* log_uploader = WebRtcLogUploader::GetInstance();
  switch (text_log_handler_->GetState()) {
    case WebRtcTextLogHandler::STARTING:
    case WebRtcTextLogHandler::STARTED:
    case WebRtcTextLogHandler::STOPPING:
    case WebRtcTextLogHandler::STOPPED:
      text_log_handler_->ChannelClosing();
      if (upload_log_on_render_close_) {
        log_uploader->background_task_runner()->PostTaskAndReplyWithResult(
            FROM_HERE, base::BindOnce(log_directory_getter_),
            base::BindOnce(&WebRtcLoggingController::TriggerUpload, this,
                           UploadDoneCallback()));
      } else {
        log_uploader->LoggingStoppedDontUpload();
        text_log_handler_->DiscardLog();
      }
      break;
    case WebRtcTextLogHandler::CLOSED:
      // Do nothing
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void WebRtcLoggingController::TriggerUpload(
    UploadDoneCallback callback,
    const base::FilePath& log_directory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (rtp_dump_handler_) {
    if (stop_rtp_dump_callback_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(stop_rtp_dump_callback_), true, true));
    }

    rtp_dump_handler_->StopOngoingDumps(
        base::BindOnce(&WebRtcLoggingController::DoUploadLogAndRtpDumps, this,
                       log_directory, std::move(callback)));
    return;
  }

  DoUploadLogAndRtpDumps(log_directory, std::move(callback));
}

void WebRtcLoggingController::StoreLogInDirectory(
    const std::string& log_id,
    std::unique_ptr<WebRtcLogPaths> log_paths,
    GenericDoneCallback done_callback,
    const base::FilePath& directory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If channel is not closing, storing is only allowed when in STOPPED state.
  // If channel is closing, storing is allowed for all states except CLOSED.
  const WebRtcTextLogHandler::LoggingState text_logging_state =
      text_log_handler_->GetState();
  const bool channel_is_closing = text_log_handler_->GetChannelIsClosing();
  if ((!channel_is_closing &&
       text_logging_state != WebRtcTextLogHandler::STOPPED) ||
      (channel_is_closing &&
       text_log_handler_->GetState() == WebRtcTextLogHandler::CLOSED)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(done_callback), false,
                                  "Logging not stopped or no log open."));
    return;
  }

  log_paths->directory = directory;

  std::unique_ptr<WebRtcLogBuffer> log_buffer;
  std::unique_ptr<WebRtcLogMetaDataMap> meta_data;
  text_log_handler_->ReleaseLog(&log_buffer, &meta_data);
  CHECK(log_buffer.get()) << "State=" << text_log_handler_->GetState()
                          << ", uorc=" << upload_log_on_render_close_;

  WebRtcLogUploader* log_uploader = WebRtcLogUploader::GetInstance();
  log_uploader->background_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](WebRtcLogPaths paths, const std::string& log_id,
                        std::unique_ptr<WebRtcLogBuffer> log_buffer,
                        std::unique_ptr<WebRtcLogMetaDataMap> meta_data,
                        GenericDoneCallback done_callback) {
                       WebRtcLogUploader* uploader =
                           WebRtcLogUploader::GetInstance();
                       uploader->LoggingStoppedDoStore(
                           paths, log_id, std::move(log_buffer),
                           std::move(meta_data), std::move(done_callback));
                     },
                     *log_paths, log_id, std::move(log_buffer),
                     std::move(meta_data), std::move(done_callback)));
}

void WebRtcLoggingController::DoUploadLogAndRtpDumps(
    const base::FilePath& log_directory,
    UploadDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If channel is not closing, upload is only allowed when in STOPPED state.
  // If channel is closing, uploading is allowed for all states except CLOSED.
  const WebRtcTextLogHandler::LoggingState text_logging_state =
      text_log_handler_->GetState();
  const bool channel_is_closing = text_log_handler_->GetChannelIsClosing();
  if ((!channel_is_closing &&
       text_logging_state != WebRtcTextLogHandler::STOPPED) ||
      (channel_is_closing &&
       text_log_handler_->GetState() == WebRtcTextLogHandler::CLOSED)) {
    // If the channel is not closing the log is expected to be uploaded, so
    // it's considered a failure if it isn't.
    // If the channel is closing we don't log failure to UMA for consistency,
    // since there are other cases during shutdown were we don't get a chance
    // to log.
    if (!channel_is_closing) {
      base::UmaHistogramSparse("WebRtcTextLogging.UploadFailed", web_app_id_);
      base::UmaHistogramSparse("WebRtcTextLogging.UploadFailureReason",
                               WebRtcLogUploadFailureReason::kInvalidState);
    }

    // Do not fire callback if it is null. Nesting null callbacks is not
    // allowed, as it can lead to crashes. See https://crbug.com/1071475
    if (!callback.is_null()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), false, "",
                                    "Logging not stopped or no log open."));
    }
    return;
  }

  WebRtcLogUploader::UploadDoneData upload_done_data;
  upload_done_data.paths.directory = log_directory;
  upload_done_data.callback = std::move(callback);
  upload_done_data.web_app_id = web_app_id_;
  ReleaseRtpDumps(&upload_done_data.paths);

  std::unique_ptr<WebRtcLogBuffer> log_buffer;
  std::unique_ptr<WebRtcLogMetaDataMap> meta_data;
  text_log_handler_->ReleaseLog(&log_buffer, &meta_data);
  CHECK(log_buffer.get()) << "State=" << text_log_handler_->GetState()
                          << ", uorc=" << upload_log_on_render_close_;

  content::BrowserContext* browser_context = GetBrowserContext();
  bool is_text_log_upload_allowed = IsWebRtcTextLogAllowed(browser_context);

  WebRtcLogUploader* log_uploader = WebRtcLogUploader::GetInstance();
  log_uploader->background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<WebRtcLogBuffer> log_buffer,
             std::unique_ptr<WebRtcLogMetaDataMap> meta_data,
             WebRtcLogUploader::UploadDoneData upload_done_data,
             bool is_text_log_upload_allowed) {
            WebRtcLogUploader* uploader = WebRtcLogUploader::GetInstance();
            uploader->OnLoggingStopped(
                std::move(log_buffer), std::move(meta_data),
                std::move(upload_done_data), is_text_log_upload_allowed);
          },
          std::move(log_buffer), std::move(meta_data),
          std::move(upload_done_data), is_text_log_upload_allowed));
}

void WebRtcLoggingController::CreateRtpDumpHandlerAndStart(
    RtpDumpType type,
    GenericDoneCallback callback,
    const base::FilePath& dump_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // |rtp_dump_handler_| may be non-null if StartRtpDump is called again before
  // GetLogDirectoryAndEnsureExists returns on the FILE thread for a previous
  // StartRtpDump.
  if (!rtp_dump_handler_)
    rtp_dump_handler_ = std::make_unique<WebRtcRtpDumpHandler>(dump_dir);

  DoStartRtpDump(type, std::move(callback));
}

void WebRtcLoggingController::DoStartRtpDump(RtpDumpType type,
                                             GenericDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(rtp_dump_handler_);

  std::string error;
  bool result = rtp_dump_handler_->StartDump(type, &error);
  FireGenericDoneCallback(std::move(callback), result, error);
}

bool WebRtcLoggingController::ReleaseRtpDumps(WebRtcLogPaths* log_paths) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(log_paths);

  if (!rtp_dump_handler_)
    return false;

  WebRtcRtpDumpHandler::ReleasedDumps rtp_dumps(
      rtp_dump_handler_->ReleaseDumps());
  log_paths->incoming_rtp_dump = rtp_dumps.incoming_dump_path;
  log_paths->outgoing_rtp_dump = rtp_dumps.outgoing_dump_path;

  rtp_dump_handler_.reset();

  return true;
}

void WebRtcLoggingController::FireGenericDoneCallback(
    GenericDoneCallback callback,
    bool success,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  DCHECK_EQ(success, error_message.empty());

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success, error_message));
}

content::BrowserContext* WebRtcLoggingController::GetBrowserContext() const {
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id_);

  return host ? host->GetBrowserContext() : nullptr;
}

// static
bool WebRtcLoggingController::IsWebRtcTextLogAllowed(
    content::BrowserContext* browser_context) {
  // Historically by default webrtc text logs are always uploaded.
  if (!browser_context) {
    return true;
  }

  const Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  return profile->GetPrefs()->GetBoolean(
      prefs::kWebRtcTextLogCollectionAllowed);
}

base::FilePath WebRtcLoggingController::GetLogDirectoryAndEnsureExists(
    const base::FilePath& browser_context_directory_path) {
  DCHECK(!browser_context_directory_path.empty());
  // Since we can be alive after the RenderProcessHost and the BrowserContext
  // (profile) have gone away, we could create the log directory here after a
  // profile has been deleted and removed from disk. If that happens it will be
  // cleaned up (at a higher level) the next browser restart.
  base::FilePath log_dir_path =
      webrtc_logging::TextLogList::GetWebRtcLogDirectoryForBrowserContextPath(
          browser_context_directory_path);
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(log_dir_path, &error)) {
    DLOG(ERROR) << "Could not create WebRTC log directory, error: " << error;
    return base::FilePath();
  }
  return log_dir_path;
}
