// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_LOCAL_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_LOCAL_H_

#include <map>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"

namespace webrtc_event_logging {

class WebRtcLocalEventLogManager final {
  using LogFilesMap =
      std::map<WebRtcEventLogPeerConnectionKey, std::unique_ptr<LogFileWriter>>;
  using PeerConnectionKey = WebRtcEventLogPeerConnectionKey;

 public:
  explicit WebRtcLocalEventLogManager(WebRtcLocalEventLogsObserver* observer);
  ~WebRtcLocalEventLogManager();

  bool PeerConnectionAdded(const PeerConnectionKey& key);
  bool PeerConnectionRemoved(const PeerConnectionKey& key);

  bool EnableLogging(const base::FilePath& base_path,
                     size_t max_file_size_bytes);
  bool DisableLogging();

  bool EventLogWrite(const PeerConnectionKey& key, const std::string& message);

  void RenderProcessHostExitedDestroyed(int render_process_id);

  // This function is public, but this entire class is a protected
  // implementation detail of WebRtcEventLogManager, which hides this
  // function from everybody except its own unit tests.
  void SetClockForTesting(base::Clock* clock);

 private:
  // Create a local log file.
  void StartLogFile(const PeerConnectionKey& key);

  // Closes an active log file.
  // Returns an iterator to the next active log file.
  LogFilesMap::iterator CloseLogFile(LogFilesMap::iterator it);

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
  WebRtcLocalEventLogsObserver* const observer_;

  // For unit tests only, and specifically for unit tests that verify the
  // filename format (derived from the current time as well as the renderer PID
  // and PeerConnection local ID), we want to make sure that the time and date
  // cannot change between the time the clock is read by the unit under test
  // (namely WebRtcEventLogManager) and the time it's read by the test.
  base::Clock* clock_for_testing_;

  // Currently active peer connections. PeerConnections which have been closed
  // are not considered active, regardless of whether they have been torn down.
  std::set<PeerConnectionKey> active_peer_connections_;

  // Local log files, stored at the behest of the user (via WebRTCInternals).
  LogFilesMap log_files_;

  // If |base_path_| is empty, local logging is disabled.
  // If nonempty, local logging is enabled, and all local logs will be saved
  // to this directory.
  base::FilePath base_path_;

  // The maximum size for local logs, in bytes.
  // If !has_value(), the value is unlimited.
  base::Optional<size_t> max_log_file_size_bytes_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLocalEventLogManager);
};

}  // namespace webrtc_event_logging

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_LOCAL_H_
