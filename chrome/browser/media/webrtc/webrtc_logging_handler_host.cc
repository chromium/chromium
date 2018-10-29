// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_logging_handler_host.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"
#include "chrome/browser/media/webrtc/webrtc_log_uploader.h"
#include "chrome/browser/media/webrtc/webrtc_rtp_dump_handler.h"
#include "chrome/common/media/webrtc_logging_messages.h"
#include "components/webrtc_logging/browser/text_log_list.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "content/public/browser/child_process_security_policy.h"
#include "storage/browser/fileapi/isolated_context.h"
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

using content::BrowserThread;
using webrtc_event_logging::WebRtcEventLogManager;

// Key used to attach the handler to the RenderProcessHost.
const char WebRtcLoggingHandlerHost::kWebRtcLoggingHandlerHostKey[] =
    "kWebRtcLoggingHandlerHostKey";

WebRtcLoggingHandlerHost::WebRtcLoggingHandlerHost(
    int render_process_id,
    content::BrowserContext* browser_context,
    WebRtcLogUploader* log_uploader)
    : BrowserMessageFilter(WebRtcLoggingMsgStart),
      render_process_id_(render_process_id),
      browser_context_directory_path_(browser_context->GetPath()),
      upload_log_on_render_close_(false),
      text_log_handler_(new WebRtcTextLogHandler(render_process_id)),
      rtp_dump_handler_(),
      stop_rtp_dump_callback_(),
      log_uploader_(log_uploader) {
  DCHECK(!browser_context_directory_path_.empty());
  DCHECK(log_uploader_);
}

WebRtcLoggingHandlerHost::~WebRtcLoggingHandlerHost() {
  // If we hit this, then we might be leaking a log reference count (see
  // ApplyForStartLogging).
  DCHECK_EQ(WebRtcTextLogHandler::CLOSED, text_log_handler_->GetState());
}

void WebRtcLoggingHandlerHost::SetMetaData(
    std::unique_ptr<MetaDataMap> meta_data,
    const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  text_log_handler_->SetMetaData(std::move(meta_data), callback);
}

void WebRtcLoggingHandlerHost::StartLogging(
    const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  // Request a log_slot from the LogUploader and start logging.
  if (text_log_handler_->StartLogging(log_uploader_, callback)) {
    // Start logging in the renderer. The callback has already been fired since
    // there is no acknowledgement when the renderer actually starts.
    Send(new WebRtcLoggingMsg_StartLogging());
  }
}

void WebRtcLoggingHandlerHost::StopLogging(
    const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  // Change the state to STOPPING and disable logging in the browser.
  if (text_log_handler_->StopLogging(callback)) {
    // Stop logging in the renderer. OnLoggingStoppedInRenderer will be called
    // when this is done to change the state from STOPPING to STOPPED and fire
    // the callback.
    Send(new WebRtcLoggingMsg_StopLogging());
  }
}

void WebRtcLoggingHandlerHost::UploadLog(const UploadDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  // This functions uploads both text logs (mandatory) and RTP dumps (optional).
  // TODO(terelius): If there's no text log available (either because it hasn't
  // been started or because it hasn't been stopped), the current implementation
  // will fire an error callback and leave any RTP dumps in a local directory.
  // Would it be better to upload whatever logs we have, or would the lack of
  // an error callback make it harder to debug potential errors?

  base::PostTaskAndReplyWithResult(
      log_uploader_->background_task_runner().get(), FROM_HERE,
      base::Bind(&WebRtcLoggingHandlerHost::GetLogDirectoryAndEnsureExists,
                 this),
      base::Bind(&WebRtcLoggingHandlerHost::TriggerUpload, this, callback));
}

void WebRtcLoggingHandlerHost::UploadStoredLog(
    const std::string& log_id,
    const UploadDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  log_uploader_->background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcLoggingHandlerHost::UploadStoredLogOnFileThread,
                     this, log_id, callback));
}

void WebRtcLoggingHandlerHost::UploadStoredLogOnFileThread(
    const std::string& log_id,
    const UploadDoneCallback& callback) {
  DCHECK(log_uploader_->background_task_runner()->RunsTasksInCurrentSequence());

  WebRtcLogUploadDoneData upload_data;
  upload_data.log_path = GetLogDirectoryAndEnsureExists();
  upload_data.callback = callback;
  upload_data.host = this;
  upload_data.local_log_id = log_id;

  log_uploader_->UploadStoredLog(upload_data);
}

void WebRtcLoggingHandlerHost::UploadLogDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // The logging state changed to CLOSED when we released the logs prior to
  // uploading. We can't check the state because a new log might have started
  // already, so there is nothing for us to do here. In the future, we might
  // want to use this function to clean up files stored on disc.
}

void WebRtcLoggingHandlerHost::DiscardLog(const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  if (!text_log_handler_->ExpectLoggingStateStopped(callback)) {
    // The callback is fired with an error message by ExpectLoggingStateStopped.
    return;
  }
  log_uploader_->LoggingStoppedDontUpload();
  text_log_handler_->DiscardLog();
  rtp_dump_handler_.reset();
  stop_rtp_dump_callback_.Reset();
  FireGenericDoneCallback(callback, true, "");
}

// Stores the log locally using a hash of log_id + security origin.
void WebRtcLoggingHandlerHost::StoreLog(
    const std::string& log_id,
    const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  if (!text_log_handler_->ExpectLoggingStateStopped(callback)) {
    // The callback is fired with an error message by ExpectLoggingStateStopped.
    return;
  }

  if (rtp_dump_handler_) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(stop_rtp_dump_callback_, true, true));

    rtp_dump_handler_->StopOngoingDumps(
        base::Bind(&WebRtcLoggingHandlerHost::StoreLogContinue,
                   this, log_id, callback));
    return;
  }

  StoreLogContinue(log_id, callback);
}

void WebRtcLoggingHandlerHost::StoreLogContinue(
    const std::string& log_id,
    const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  std::unique_ptr<WebRtcLogPaths> log_paths(new WebRtcLogPaths());
  ReleaseRtpDumps(log_paths.get());

  base::PostTaskAndReplyWithResult(
      log_uploader_->background_task_runner().get(), FROM_HERE,
      base::Bind(&WebRtcLoggingHandlerHost::GetLogDirectoryAndEnsureExists,
                 this),
      base::Bind(&WebRtcLoggingHandlerHost::StoreLogInDirectory, this, log_id,
                 base::Passed(&log_paths), callback));
}


void WebRtcLoggingHandlerHost::StartRtpDump(
    RtpDumpType type,
    const GenericDoneCallback& callback,
    const content::RenderProcessHost::WebRtcStopRtpDumpCallback&
        stop_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(stop_rtp_dump_callback_.is_null() ||
         stop_rtp_dump_callback_.Equals(stop_callback));

  stop_rtp_dump_callback_ = stop_callback;

  if (!rtp_dump_handler_) {
    base::PostTaskAndReplyWithResult(
        log_uploader_->background_task_runner().get(), FROM_HERE,
        base::Bind(&WebRtcLoggingHandlerHost::GetLogDirectoryAndEnsureExists,
                   this),
        base::Bind(&WebRtcLoggingHandlerHost::CreateRtpDumpHandlerAndStart,
                   this, type, callback));
    return;
  }

  DoStartRtpDump(type, callback);
}

void WebRtcLoggingHandlerHost::StopRtpDump(
    RtpDumpType type,
    const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  if (!rtp_dump_handler_) {
    FireGenericDoneCallback(callback, false, "RTP dump has not been started.");
    return;
  }

  if (!stop_rtp_dump_callback_.is_null()) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(stop_rtp_dump_callback_,
                       type == RTP_DUMP_INCOMING || type == RTP_DUMP_BOTH,
                       type == RTP_DUMP_OUTGOING || type == RTP_DUMP_BOTH));
  }

  rtp_dump_handler_->StopDump(type, callback);
}

void WebRtcLoggingHandlerHost::StartEventLogging(
    const std::string& peer_connection_id,
    size_t max_log_size_bytes,
    size_t web_app_id,
    const StartEventLoggingCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebRtcEventLogManager::GetInstance()->StartRemoteLogging(
      render_process_id_, peer_connection_id, max_log_size_bytes, web_app_id,
      callback);
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
void WebRtcLoggingHandlerHost::GetLogsDirectory(
    const LogsDirectoryCallback& callback,
    const LogsDirectoryErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  base::PostTaskAndReplyWithResult(
      log_uploader_->background_task_runner().get(), FROM_HERE,
      base::Bind(&WebRtcLoggingHandlerHost::GetLogDirectoryAndEnsureExists,
                 this),
      base::Bind(&WebRtcLoggingHandlerHost::GrantLogsDirectoryAccess, this,
                 callback, error_callback));
}

void WebRtcLoggingHandlerHost::GrantLogsDirectoryAccess(
    const LogsDirectoryCallback& callback,
    const LogsDirectoryErrorCallback& error_callback,
    const base::FilePath& logs_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (logs_path.empty()) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(error_callback, "Logs directory not available"));
    return;
  }

  storage::IsolatedContext* isolated_context =
      storage::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  std::string registered_name;
  std::string filesystem_id = isolated_context->RegisterFileSystemForPath(
      storage::kFileSystemTypeNativeLocal, std::string(), logs_path,
      &registered_name);

  // Only granting read and delete access to reduce contention with
  // webrtcLogging APIs that modify files in that folder.
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  policy->GrantReadFileSystem(render_process_id_, filesystem_id);
  // Delete is needed to prevent accumulation of files.
  policy->GrantDeleteFromFileSystem(render_process_id_, filesystem_id);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(callback, filesystem_id, registered_name));
}
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

void WebRtcLoggingHandlerHost::OnRtpPacket(
    std::unique_ptr<uint8_t[]> packet_header,
    size_t header_length,
    size_t packet_length,
    bool incoming) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&WebRtcLoggingHandlerHost::DumpRtpPacketOnIOThread, this,
                     std::move(packet_header), header_length, packet_length,
                     incoming));
}

void WebRtcLoggingHandlerHost::DumpRtpPacketOnIOThread(
    std::unique_ptr<uint8_t[]> packet_header,
    size_t header_length,
    size_t packet_length,
    bool incoming) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // |rtp_dump_handler_| could be NULL if we are waiting for the FILE thread to
  // create/ensure the log directory.
  if (rtp_dump_handler_) {
    rtp_dump_handler_->OnRtpPacket(
        packet_header.get(), header_length, packet_length, incoming);
  }
}

void WebRtcLoggingHandlerHost::OnChannelClosing() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!text_log_handler_->GetChannelIsClosing()) {
    switch (text_log_handler_->GetState()) {
      case WebRtcTextLogHandler::STARTING:
      case WebRtcTextLogHandler::STARTED:
      case WebRtcTextLogHandler::STOPPING:
      case WebRtcTextLogHandler::STOPPED:
        text_log_handler_->ChannelClosing();
        if (upload_log_on_render_close_) {
          base::PostTaskAndReplyWithResult(
              log_uploader_->background_task_runner().get(), FROM_HERE,
              base::BindOnce(
                  &WebRtcLoggingHandlerHost::GetLogDirectoryAndEnsureExists,
                  this),
              base::BindOnce(&WebRtcLoggingHandlerHost::TriggerUpload, this,
                             UploadDoneCallback()));
        } else {
          log_uploader_->LoggingStoppedDontUpload();
          text_log_handler_->DiscardLog();
        }
        break;
      case WebRtcTextLogHandler::CLOSED:
        // Do nothing
        break;
      default:
        NOTREACHED();
    }
  }

  content::BrowserMessageFilter::OnChannelClosing();
}

void WebRtcLoggingHandlerHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool WebRtcLoggingHandlerHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebRtcLoggingHandlerHost, message)
    IPC_MESSAGE_HANDLER(WebRtcLoggingMsg_AddLogMessages, OnAddLogMessages)
    IPC_MESSAGE_HANDLER(WebRtcLoggingMsg_LoggingStopped,
                        OnLoggingStoppedInRenderer)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void WebRtcLoggingHandlerHost::OnAddLogMessages(
    const std::vector<WebRtcLoggingMessageData>& messages) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (text_log_handler_->GetState() == WebRtcTextLogHandler::STARTED ||
      text_log_handler_->GetState() == WebRtcTextLogHandler::STOPPING) {
    for (auto& message : messages) {
      text_log_handler_->LogWebRtcLoggingMessageData(message);
    }
  }
}

void WebRtcLoggingHandlerHost::OnLoggingStoppedInRenderer() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (text_log_handler_->GetState() != WebRtcTextLogHandler::STOPPING) {
    // If an out-of-order response is received, stop_callback_ may be invalid,
    // and must not be invoked.
    DLOG(ERROR) << "OnLoggingStoppedInRenderer invoked in state "
                << text_log_handler_->GetState();
    bad_message::ReceivedBadMessage(
        this, bad_message::WRLHH_LOGGING_STOPPED_BAD_STATE);
    return;
  }
  text_log_handler_->StopDone();
}

base::FilePath WebRtcLoggingHandlerHost::GetLogDirectoryAndEnsureExists() {
  DCHECK(log_uploader_->background_task_runner()->RunsTasksInCurrentSequence());

  // Since we can be alive after the RenderProcessHost and the BrowserContext
  // (profile) have gone away, we could create the log directory here after a
  // profile has been deleted and removed from disk. If that happens it will be
  // cleaned up (at a higher level) the next browser restart.
  base::FilePath log_dir_path =
      webrtc_logging::TextLogList::GetWebRtcLogDirectoryForBrowserContextPath(
          browser_context_directory_path_);
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(log_dir_path, &error)) {
    DLOG(ERROR) << "Could not create WebRTC log directory, error: " << error;
    return base::FilePath();
  }
  return log_dir_path;
}

void WebRtcLoggingHandlerHost::TriggerUpload(
    const UploadDoneCallback& callback,
    const base::FilePath& log_directory) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (rtp_dump_handler_) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(stop_rtp_dump_callback_, true, true));

    rtp_dump_handler_->StopOngoingDumps(
        base::Bind(&WebRtcLoggingHandlerHost::DoUploadLogAndRtpDumps,
                   this,
                   log_directory,
                   callback));
    return;
  }

  DoUploadLogAndRtpDumps(log_directory, callback);
}

void WebRtcLoggingHandlerHost::StoreLogInDirectory(
    const std::string& log_id,
    std::unique_ptr<WebRtcLogPaths> log_paths,
    const GenericDoneCallback& done_callback,
    const base::FilePath& directory) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // If channel is not closing, storing is only allowed when in STOPPED state.
  // If channel is closing, storing is allowed for all states except CLOSED.
  const WebRtcTextLogHandler::LoggingState text_logging_state =
      text_log_handler_->GetState();
  const bool channel_is_closing = text_log_handler_->GetChannelIsClosing();
  if ((!channel_is_closing &&
       text_logging_state != WebRtcTextLogHandler::STOPPED) ||
      (channel_is_closing &&
       text_log_handler_->GetState() == WebRtcTextLogHandler::CLOSED)) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(done_callback, false,
                       "Logging not stopped or no log open."));
    return;
  }

  log_paths->log_path = directory;

  std::unique_ptr<WebRtcLogBuffer> log_buffer;
  std::unique_ptr<MetaDataMap> meta_data;
  text_log_handler_->ReleaseLog(&log_buffer, &meta_data);
  CHECK(log_buffer.get()) << "State=" << text_log_handler_->GetState()
                          << ", uorc=" << upload_log_on_render_close_;

  log_uploader_->background_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcLogUploader::LoggingStoppedDoStore,
                                base::Unretained(log_uploader_), *log_paths,
                                log_id, std::move(log_buffer),
                                std::move(meta_data), done_callback));
}

void WebRtcLoggingHandlerHost::DoUploadLogAndRtpDumps(
    const base::FilePath& log_directory,
    const UploadDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // If channel is not closing, upload is only allowed when in STOPPED state.
  // If channel is closing, uploading is allowed for all states except CLOSED.
  const WebRtcTextLogHandler::LoggingState text_logging_state =
      text_log_handler_->GetState();
  const bool channel_is_closing = text_log_handler_->GetChannelIsClosing();
  if ((!channel_is_closing &&
       text_logging_state != WebRtcTextLogHandler::STOPPED) ||
      (channel_is_closing &&
       text_log_handler_->GetState() == WebRtcTextLogHandler::CLOSED)) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(callback, false, "",
                       "Logging not stopped or no log open."));
    return;
  }

  WebRtcLogUploadDoneData upload_done_data;
  upload_done_data.log_path = log_directory;
  upload_done_data.callback = callback;
  upload_done_data.host = this;
  ReleaseRtpDumps(&upload_done_data);

  std::unique_ptr<WebRtcLogBuffer> log_buffer;
  std::unique_ptr<MetaDataMap> meta_data;
  text_log_handler_->ReleaseLog(&log_buffer, &meta_data);
  CHECK(log_buffer.get()) << "State=" << text_log_handler_->GetState()
                          << ", uorc=" << upload_log_on_render_close_;

  log_uploader_->background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcLogUploader::LoggingStoppedDoUpload,
                     base::Unretained(log_uploader_), std::move(log_buffer),
                     std::move(meta_data), upload_done_data));
}

void WebRtcLoggingHandlerHost::CreateRtpDumpHandlerAndStart(
    RtpDumpType type,
    const GenericDoneCallback& callback,
    const base::FilePath& dump_dir) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // |rtp_dump_handler_| may be non-NULL if StartRtpDump is called again before
  // GetLogDirectoryAndEnsureExists returns on the FILE thread for a previous
  // StartRtpDump.
  if (!rtp_dump_handler_)
    rtp_dump_handler_.reset(new WebRtcRtpDumpHandler(dump_dir));

  DoStartRtpDump(type, callback);
}

void WebRtcLoggingHandlerHost::DoStartRtpDump(
    RtpDumpType type, const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(rtp_dump_handler_);

  std::string error;
  bool result = rtp_dump_handler_->StartDump(type, &error);
  FireGenericDoneCallback(callback, result, error);
}

bool WebRtcLoggingHandlerHost::ReleaseRtpDumps(WebRtcLogPaths* log_paths) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(log_paths);

  if (!rtp_dump_handler_)
    return false;

  WebRtcRtpDumpHandler::ReleasedDumps rtp_dumps(
      rtp_dump_handler_->ReleaseDumps());
  log_paths->incoming_rtp_dump = rtp_dumps.incoming_dump_path;
  log_paths->outgoing_rtp_dump = rtp_dumps.outgoing_dump_path;

  rtp_dump_handler_.reset();
  stop_rtp_dump_callback_.Reset();

  return true;
}

void WebRtcLoggingHandlerHost::FireGenericDoneCallback(
    const GenericDoneCallback& callback,
    bool success,
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());
  DCHECK_EQ(success, error_message.empty());

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback, success, error_message));
}
