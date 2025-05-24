// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_LOCAL_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_LOCAL_H_

#include <map>
#include <optional>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"

namespace webrtc_event_logging {

class WebRtcLocalEventLogManager final {
  struct LogFiles;
  using PeerConnectionKey = WebRtcEventLogPeerConnectionKey;
  using LogFilesMap = std::map<PeerConnectionKey, LogFiles>;

 public:
  explicit WebRtcLocalEventLogManager(WebRtcLocalEventLogsObserver* observer);

  WebRtcLocalEventLogManager(const WebRtcLocalEventLogManager&) = delete;
  WebRtcLocalEventLogManager& operator=(const WebRtcLocalEventLogManager&) =
      delete;

  ~WebRtcLocalEventLogManager();

  bool OnPeerConnectionAdded(const PeerConnectionKey& key);
  bool OnPeerConnectionRemoved(const PeerConnectionKey& key);

  bool EnableEventLogging(const base::FilePath& base_path,
                          size_t max_file_size_bytes);
  bool DisableEventLogging();

  bool EventLogWrite(const PeerConnectionKey& key, const std::string& message);

  bool EnableDataChannelLogging(const base::FilePath& base_path,
                                size_t max_file_size_bytes);
  bool DisableDataChannelLogging();

  bool DataChannelLogWrite(const PeerConnectionKey& key,
                           const std::string& message);

  void RenderProcessHostExitedDestroyed(int render_process_id);

  // This function is public, but this entire class is a protected
  // implementation detail of WebRtcEventLogManager, which hides this
  // function from everybody except its own unit tests.
  void SetClockForTesting(base::Clock* clock);

 private:
  struct LogTypeState {
    LogTypeState(const base::FilePath& base_path,
                 std::optional<size_t> max_size_bytes,
                 std::optional<size_t> max_logs_active,
                 std::optional<size_t> max_logs_created)
        : base_path(base_path),
          max_size_bytes(max_size_bytes),
          max_logs_active(max_logs_active),
          max_logs_created(max_logs_created) {}
    const base::FilePath base_path;
    const std::optional<size_t> max_size_bytes;
    const std::optional<size_t> max_logs_active;
    const std::optional<size_t> max_logs_created;
    size_t logs_active = 0;
    size_t logs_created = 0;
  };

  void MaybeStartEventLogFile(LogFilesMap::iterator log_it);
  void StopEventLogFileIfStarted(LogFilesMap::iterator);
  void MaybeStartDataChannelLogFile(LogFilesMap::iterator log_it);
  void StopDataChannelLogFileIfStarted(LogFilesMap::iterator);

  std::unique_ptr<LogFileWriter> CreateLogFile(
      const PeerConnectionKey& key,
      const base::FilePath& base_path,
      std::optional<size_t> max_log_size_bytes);

  // Derives the name of a local log file. The format is:
  // [user_defined]_[date]_[time]_[render_process_id]_[lid].[extension]
  base::FilePath GetFilePath(const base::FilePath& base_path,
                             const PeerConnectionKey& key) const;

  // This object is expected to be created and destroyed on the UI thread,
  // but live on its owner's internal, IO-capable task queue.
  SEQUENCE_CHECKER(io_task_sequence_checker_);

  // Produces LogFileWriter instances, for writing the logs to files.
  BaseLogFileWriterFactory log_file_writer_factory_;

  // Observer which will be informed whenever a local log file is started or
  // stopped. Through this, the owning WebRtcEventLogManager can be informed,
  // and decide whether it wants to turn notifications from WebRTC on/off.
  const raw_ptr<WebRtcLocalEventLogsObserver> observer_;

  // For unit tests only, and specifically for unit tests that verify the
  // filename format (derived from the current time as well as the renderer PID
  // and PeerConnection local ID), we want to make sure that the time and date
  // cannot change between the time the clock is read by the unit under test
  // (namely WebRtcEventLogManager) and the time it's read by the test.
  raw_ptr<base::Clock> clock_for_testing_;

  // Local log files, stored at the behest of the user (via WebRTCInternals).
  LogFilesMap log_files_;

  std::optional<LogTypeState> event_log_state_;
  std::optional<LogTypeState> data_channel_log_state_;
};

}  // namespace webrtc_event_logging

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_LOCAL_H_
