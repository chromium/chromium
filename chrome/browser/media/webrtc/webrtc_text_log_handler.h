// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_TEXT_LOG_HANDLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_TEXT_LOG_HANDLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/media/webrtc/webrtc_log_uploader.h"
#include "net/base/network_interfaces.h"

namespace chrome {
namespace mojom {
class WebRtcLoggingMessage;
}  // namespace mojom
}  // namespace chrome

class WebRtcLogBuffer;

class WebRtcTextLogHandler {
 public:
  // States used for protecting from function calls made at non-allowed points
  // in time. For example, StartLogging() is only allowed in CLOSED state.
  // See also comment on |channel_is_closing_| below.
  // Transitions: SetMetaData():    CLOSED -> CLOSED, or
  //                                STARTED -> STARTED
  //              StartLogging():   CLOSED -> STARTING.
  //              StartDone():      STARTING -> STARTED.
  //              StopLogging():    STARTED -> STOPPING.
  //              StopDone():       STOPPING -> STOPPED.
  //              DiscardLog():     STOPPED -> CLOSED.
  //              ReleaseLog():     STOPPED -> CLOSED.
  enum LoggingState {
    CLOSED,           // Logging not started, no log in memory.
    STARTING,         // Start logging is in progress.
    STARTED,          // Logging started.
    STOPPING,         // Stop logging is in progress.
    STOPPED,          // Logging has been stopped, log still open in memory.
  };

  typedef base::Callback<void(bool, const std::string&)> GenericDoneCallback;

  explicit WebRtcTextLogHandler(int render_process_id);
  ~WebRtcTextLogHandler();

  // Returns the current state of the log.
  LoggingState GetState() const;

  // Returns true if channel is closing.
  bool GetChannelIsClosing() const;

  // Sets meta data for log uploading. Merged with any already set meta data.
  // Values for existing keys are overwritten. The meta data already set at log
  // start is written to the beginning of the log. Meta data set after log start
  // is written to the log at that time.
  void SetMetaData(std::unique_ptr<WebRtcLogMetaDataMap> meta_data,
                   const GenericDoneCallback& callback);

  // Opens a log and starts logging if allowed by the LogUploader.
  // Returns false if logging could not be started.
  bool StartLogging(WebRtcLogUploader* log_uploader,
                    const GenericDoneCallback& callback);

  // Stops logging. Log will remain open until UploadLog or DiscardLog is
  // called.
  bool StopLogging(const GenericDoneCallback& callback);

  // Called by the WebRtcLoggingHandlerHost when logging has stopped in the
  // renderer. Should only be called in response to a
  // WebRtcLoggingMsg_LoggingStopped IPC message.
  void StopDone();

  // Signals that the renderer is closing, which de facto stops logging but
  // keeps the log in memory.
  // Can be called in any state except CLOSED.
  void ChannelClosing();

  // Discards a stopped log.
  void DiscardLog();

  // Releases a stopped log to the caller.
  void ReleaseLog(std::unique_ptr<WebRtcLogBuffer>* log_buffer,
                  std::unique_ptr<WebRtcLogMetaDataMap>* meta_data);

  // Adds a message to the log.
  void LogMessage(const std::string& message);

  // Adds a message to the log.
  void LogWebRtcLoggingMessage(
      const chrome::mojom::WebRtcLoggingMessage* message);

  // Returns true if the logging state is CLOSED and fires an the callback
  // with an error message otherwise.
  bool ExpectLoggingStateStopped(const GenericDoneCallback& callback);

  void FireGenericDoneCallback(const GenericDoneCallback& callback,
                               bool success,
                               const std::string& error_message);

  void SetWebAppId(int web_app_id);

 private:
  void StartDone(const GenericDoneCallback& callback);

  void LogToCircularBuffer(const std::string& message);

  void OnGetNetworkInterfaceList(
      const GenericDoneCallback& callback,
      const base::Optional<net::NetworkInterfaceList>& networks);

  SEQUENCE_CHECKER(sequence_checker_);

  // The render process ID this object belongs to.
  const int render_process_id_;

  // Should be created by StartLogging().
  std::unique_ptr<WebRtcLogBuffer> log_buffer_;

  // Should be created by StartLogging().
  std::unique_ptr<WebRtcLogMetaDataMap> meta_data_;

  GenericDoneCallback stop_callback_;
  LoggingState logging_state_;

  // True if renderer is closing. The log (if there is one) can still be
  // released or discarded (i.e. closed). No new logs can be created. The only
  // state change possible when channel is closing is from any state to CLOSED.
  bool channel_is_closing_ = false;

  // The system time in ms when logging is started. Reset when logging_state_
  // changes to STOPPED.
  base::Time logging_started_time_;

  // Web app id used for statistics. See
  // |WebRtcLoggingHandlerHost::web_app_id_|.
  int web_app_id_ = 0;

  base::WeakPtrFactory<WebRtcTextLogHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebRtcTextLogHandler);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_TEXT_LOG_HANDLER_H_
