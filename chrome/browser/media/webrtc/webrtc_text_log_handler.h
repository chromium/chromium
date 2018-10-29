// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_TEXT_LOG_HANDLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_TEXT_LOG_HANDLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "chrome/common/media/webrtc_logging_message_data.h"
#include "components/webrtc_logging/common/partial_circular_buffer.h"
#include "net/base/network_interfaces.h"

#if defined(OS_ANDROID)
const size_t kWebRtcLogSize = 1 * 1024 * 1024;  // 1 MB
#else
const size_t kWebRtcLogSize = 6 * 1024 * 1024;  // 6 MB
#endif

typedef std::map<std::string, std::string> MetaDataMap;

class WebRtcLogUploader;

class WebRtcLogBuffer {
 public:
  WebRtcLogBuffer();
  ~WebRtcLogBuffer();

  void Log(const std::string& message);

  // Returns a circular buffer instance for reading the internal log buffer.
  // Must only be called after the log has been marked as complete
  // (see SetComplete) and the caller must ensure that the WebRtcLogBuffer
  // instance remains in scope for the lifetime of the returned circular buffer.
  webrtc_logging::PartialCircularBuffer Read();

  // Switches the buffer to read-only mode, where access to the internal
  // buffer is allowed from different threads than were used to contribute
  // to the log.  Calls to Log() won't be allowed after calling
  // SetComplete() and the call to SetComplete() must be done on the same
  // thread as constructed the buffer and calls Log().
  void SetComplete();

 private:
  base::ThreadChecker thread_checker_;
  uint8_t buffer_[kWebRtcLogSize];
  webrtc_logging::PartialCircularBuffer circular_;
  bool read_only_;
};

class WebRtcTextLogHandler
    : public base::RefCountedThreadSafe<WebRtcTextLogHandler> {
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

  // Returns the current state of the log. Must be called on the IO thread.
  LoggingState GetState() const;

  // Returns true if channel is closing. Must be called on the IO thread.
  bool GetChannelIsClosing() const;

  // Sets meta data for log uploading. Merged with any already set meta data.
  // Values for existing keys are overwritten. The meta data already set at log
  // start is written to the beginning of the log. Meta data set after log start
  // is written to the log at that time.
  void SetMetaData(std::unique_ptr<MetaDataMap> meta_data,
                   const GenericDoneCallback& callback);

  // Opens a log and starts logging if allowed by the LogUploader.
  // Returns false if logging could not be started.
  // Must be called on the IO thread.
  bool StartLogging(WebRtcLogUploader* log_uploader,
                    const GenericDoneCallback& callback);

  // Stops logging. Log will remain open until UploadLog or DiscardLog is
  // called. Must be called on the IO thread.
  bool StopLogging(const GenericDoneCallback& callback);

  // Called by the WebRtcLoggingHandlerHost when logging has stopped in the
  // renderer. Should only be called in response to a
  // WebRtcLoggingMsg_LoggingStopped IPC message.
  // Must be called on the IO thread.
  void StopDone();

  // Signals that the renderer is closing, which de facto stops logging but
  // keeps the log in memory.
  // Can be called in any state except CLOSED. Must be called on the IO thread.
  void ChannelClosing();

  // Discards a stopped log. Must be called on the IO thread.
  void DiscardLog();

  // Releases a stopped log to the caller. Must be called on the IO thread.
  void ReleaseLog(std::unique_ptr<WebRtcLogBuffer>* log_buffer,
                  std::unique_ptr<MetaDataMap>* meta_data);

  // Adds a message to the log. Must be called on the IO thread.
  void LogMessage(const std::string& message);

  // Adds a message to the log. Must be called on the IO thread.
  void LogWebRtcLoggingMessageData(const WebRtcLoggingMessageData& message);

  // Returns true if the logging state is CLOSED and fires an the callback
  // with an error message otherwise. Must be called on the IO thread.
  bool ExpectLoggingStateStopped(const GenericDoneCallback& callback);

  void FireGenericDoneCallback(const GenericDoneCallback& callback,
                               bool success,
                               const std::string& error_message);

 private:
  friend class base::RefCountedThreadSafe<WebRtcTextLogHandler>;
  ~WebRtcTextLogHandler();

  void StartDone(const GenericDoneCallback& callback);

  void LogToCircularBuffer(const std::string& message);

  void LogInitialInfoOnIOThread(const GenericDoneCallback& callback,
                                const net::NetworkInterfaceList& network_list);

  // The render process ID this object belongs to.
  const int render_process_id_;

  // Should be created by StartLogging().
  std::unique_ptr<WebRtcLogBuffer> log_buffer_;

  // These are only accessed on the IO thread, except when in STARTING state. In
  // this state we are protected since entering any function that alters the
  // state is not allowed.
  // Should be created by StartLogging().
  std::unique_ptr<MetaDataMap> meta_data_;

  // Only accessed on the IO thread.
  GenericDoneCallback stop_callback_;

  // Only accessed on the IO thread.
  LoggingState logging_state_;

  // True if renderer is closing. The log (if there is one) can still be
  // released or discarded (i.e. closed). No new logs can be created. The only
  // state change possible when channel is closing is from any state to CLOSED.
  // Can only accessed on the IO thread.
  bool channel_is_closing_ = false;

  // The system time in ms when logging is started. Reset when logging_state_
  // changes to STOPPED.
  base::Time logging_started_time_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcTextLogHandler);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_TEXT_LOG_HANDLER_H_
