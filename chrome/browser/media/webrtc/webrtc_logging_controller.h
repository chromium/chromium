// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOGGING_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOGGING_CONTROLLER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/rtp_dump_type.h"
#include "chrome/browser/media/webrtc/webrtc_log_uploader.h"
#include "chrome/browser/media/webrtc/webrtc_text_log_handler.h"
#include "chrome/common/media/webrtc_logging.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class WebRtcLogUploader;
class WebRtcRtpDumpHandler;

namespace content {
class BrowserContext;
}  // namespace content

// WebRtcLoggingController handles operations regarding the WebRTC logging:
// - Opens a connection to a WebRtcLoggingAgent that runs in the render process
//   and generates log messages.
// - Writes basic machine info to the log.
// - Informs the handler in the render process when to stop logging.
// - Closes the connection to the WebRtcLoggingAgent (and thereby discarding it)
//   or triggers uploading of the log.
// - Detects when the agent (e.g., because of a tab closure or crash) is going
//   away and possibly triggers uploading the log.
class WebRtcLoggingController
    : public base::RefCounted<WebRtcLoggingController>,
      public chrome::mojom::WebRtcLoggingClient {
 public:
  typedef WebRtcLogUploader::GenericDoneCallback GenericDoneCallback;
  typedef WebRtcLogUploader::UploadDoneCallback UploadDoneCallback;
  typedef base::OnceCallback<void(const std::string&, const std::string&)>
      LogsDirectoryCallback;
  typedef base::OnceCallback<void(const std::string&)>
      LogsDirectoryErrorCallback;

  // Argument #1: Indicate success/failure.
  // Argument #2: If success, the log's ID. Otherwise, empty.
  // Argument #3: If failure, the error message. Otherwise, empty.
  typedef base::RepeatingCallback<
      void(bool, const std::string&, const std::string&)>
      StartEventLoggingCallback;

  static void AttachToRenderProcessHost(content::RenderProcessHost* host);
  static WebRtcLoggingController* FromRenderProcessHost(
      content::RenderProcessHost* host);

  WebRtcLoggingController(const WebRtcLoggingController&) = delete;
  WebRtcLoggingController& operator=(const WebRtcLoggingController&) = delete;

  // Sets meta data that will be uploaded along with the log and also written
  // in the beginning of the log. Must be called on the IO thread before calling
  // StartLogging.
  void SetMetaData(std::unique_ptr<WebRtcLogMetaDataMap> meta_data,
                   GenericDoneCallback callback);

  // Opens a log and starts logging. Must be called on the IO thread.
  void StartLogging(GenericDoneCallback callback);

  // Stops logging. Log will remain open until UploadLog or DiscardLog is
  // called. Must be called on the IO thread.
  void StopLogging(GenericDoneCallback callback);

  // Uploads the text log and the RTP dumps. Discards the local copy. May only
  // be called after text logging has stopped. Must be called on the IO thread.
  void UploadLog(UploadDoneCallback callback);

  // Uploads a log that was previously saved via a call to StoreLog().
  // Otherwise operates in the same way as UploadLog.
  void UploadStoredLog(const std::string& log_id, UploadDoneCallback callback);

  // Discards the log and the RTP dumps. May only be called after logging has
  // stopped. Must be called on the IO thread.
  void DiscardLog(GenericDoneCallback callback);

  // Stores the log locally using a hash of log_id + security origin.
  void StoreLog(const std::string& log_id, GenericDoneCallback callback);

  // May be called on any thread. |upload_log_on_render_close_| is used
  // for decision making and it's OK if it changes before the execution based
  // on that decision has finished.
  void set_upload_log_on_render_close(bool should_upload) {
    upload_log_on_render_close_ = should_upload;
  }

  // Starts dumping the RTP headers for the specified direction. Must be called
  // on the UI thread. |type| specifies which direction(s) of RTP packets should
  // be dumped. |callback| will be called when starting the dump is done.
  void StartRtpDump(RtpDumpType type, GenericDoneCallback callback);

  // Stops dumping the RTP headers for the specified direction. Must be called
  // on the UI thread. |type| specifies which direction(s) of RTP packet dumping
  // should be stopped. |callback| will be called when stopping the dump is
  // done.
  void StopRtpDump(RtpDumpType type, GenericDoneCallback callback);

  // Called when an RTP packet is sent or received. Must be called on the UI
  // thread.
  void OnRtpPacket(base::HeapArray<uint8_t> packet_header,
                   size_t packet_length,
                   bool incoming);

  // Start remote-bound event logging for a specific peer connection
  // (indicated by its session description's ID).
  // The callback will be posted back, indicating |true| if and only if an
  // event log was successfully started, in which case the first of the string
  // arguments will be set to the log-ID. Otherwise, the second of the string
  // arguments will contain the error message.
  // This function must be called on the UI thread.
  void StartEventLogging(const std::string& session_id,
                         size_t max_log_size_bytes,
                         int output_period_ms,
                         size_t web_app_id,
                         const StartEventLoggingCallback& callback);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Ensures that the WebRTC Logs directory exists and then grants render
  // process access to the 'WebRTC Logs' directory, and invokes |callback| with
  // the ids necessary to create a DirectoryEntry object.
  void GetLogsDirectory(LogsDirectoryCallback callback,
                        LogsDirectoryErrorCallback error_callback);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  // chrome::mojom::WebRtcLoggingClient methods:
  void OnAddMessages(
      std::vector<chrome::mojom::WebRtcLoggingMessagePtr> messages) override;
  void OnStopped() override;

  // Checks whether WebRTC text-logs is permitted by
  // the relevant policy (prefs::kWebRtcTextLogCollectionAllowed).
  static bool IsWebRtcTextLogAllowed(content::BrowserContext* browser_context);

 private:
  friend class base::RefCounted<WebRtcLoggingController>;

  WebRtcLoggingController(int render_process_id,
                          content::BrowserContext* browser_context);
  ~WebRtcLoggingController() override;

  void OnAgentDisconnected();

  // Called after stopping RTP dumps.
  void StoreLogContinue(const std::string& log_id,
                        GenericDoneCallback callback);

  // Writes a formatted log |message| to the |circular_buffer_|.
  void LogToCircularBuffer(const std::string& message);

  void TriggerUpload(UploadDoneCallback callback,
                     const base::FilePath& log_directory);

  void StoreLogInDirectory(const std::string& log_id,
                           std::unique_ptr<WebRtcLogPaths> log_paths,
                           GenericDoneCallback done_callback,
                           const base::FilePath& directory);

  // A helper for TriggerUpload to do the real work.
  void DoUploadLogAndRtpDumps(const base::FilePath& log_directory,
                              UploadDoneCallback callback);

  // Create the RTP dump handler and start dumping. Must be called after making
  // sure the log directory exists.
  void CreateRtpDumpHandlerAndStart(RtpDumpType type,
                                    GenericDoneCallback callback,
                                    const base::FilePath& dump_dir);

  // A helper for starting RTP dump assuming the RTP dump handler has been
  // created.
  void DoStartRtpDump(RtpDumpType type, GenericDoneCallback callback);

  bool ReleaseRtpDumps(WebRtcLogPaths* log_paths);

  void FireGenericDoneCallback(
      WebRtcLoggingController::GenericDoneCallback callback,
      bool success,
      const std::string& error_message);

  content::BrowserContext* GetBrowserContext() const;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Grants the render process access to the 'WebRTC Logs' directory, and
  // invokes |callback| with the ids necessary to create a DirectoryEntry
  // object. If the |logs_path| couldn't be created or found, |error_callback|
  // is run.
  void GrantLogsDirectoryAccess(LogsDirectoryCallback callback,
                                LogsDirectoryErrorCallback error_callback,
                                const base::FilePath& logs_path);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  static base::FilePath GetLogDirectoryAndEnsureExists(
      const base::FilePath& browser_context_directory_path);

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Receiver<chrome::mojom::WebRtcLoggingClient> receiver_;
  mojo::Remote<chrome::mojom::WebRtcLoggingAgent> logging_agent_;

  // The render process ID this object belongs to.
  const int render_process_id_;

  // A callback that needs to be run from a blocking worker pool and returns
  // the browser context directory path associated with our renderer process.
  base::RepeatingCallback<base::FilePath(void)> log_directory_getter_;

  // True if we should upload whatever log we have when the renderer closes.
  bool upload_log_on_render_close_;

  // The text log handler owns the WebRtcLogBuffer object and keeps track of
  // the logging state.
  std::unique_ptr<WebRtcTextLogHandler> text_log_handler_;

  // The RTP dump handler responsible for creating the RTP header dump files.
  std::unique_ptr<WebRtcRtpDumpHandler> rtp_dump_handler_;

  // The callback to call when StopRtpDump is called.
  content::RenderProcessHost::WebRtcStopRtpDumpCallback stop_rtp_dump_callback_;

  // Web app id used for statistics. Created as the hash of the value of a
  // "client" meta data key, if exists. 0 means undefined, and is the hash of
  // the empty string.
  int web_app_id_ = 0;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOGGING_CONTROLLER_H_
