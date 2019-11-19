// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager_local.h"

#include "base/files/file_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#define NumberToStringType base::NumberToString16
#else
#define NumberToStringType base::NumberToString
#endif

namespace webrtc_event_logging {

#if defined(OS_ANDROID)
const size_t kDefaultMaxLocalLogFileSizeBytes = 10000000;
const size_t kMaxNumberLocalWebRtcEventLogFiles = 3;
#else
const size_t kDefaultMaxLocalLogFileSizeBytes = 60000000;
const size_t kMaxNumberLocalWebRtcEventLogFiles = 5;
#endif

WebRtcLocalEventLogManager::WebRtcLocalEventLogManager(
    WebRtcLocalEventLogsObserver* observer)
    : observer_(observer),
      clock_for_testing_(nullptr),
      max_log_file_size_bytes_(kDefaultMaxLocalLogFileSizeBytes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DETACH_FROM_SEQUENCE(io_task_sequence_checker_);
}

WebRtcLocalEventLogManager::~WebRtcLocalEventLogManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

bool WebRtcLocalEventLogManager::PeerConnectionAdded(
    const PeerConnectionKey& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  const auto insertion_result = active_peer_connections_.insert(key);
  if (!insertion_result.second) {
    return false;  // Attempt to re-add the PeerConnection.
  }

  if (!base_path_.empty() &&
      log_files_.size() < kMaxNumberLocalWebRtcEventLogFiles) {
    // Note that success/failure of starting the local log file is unrelated
    // to the success/failure of PeerConnectionAdded().
    StartLogFile(key);
  }

  return true;
}

bool WebRtcLocalEventLogManager::PeerConnectionRemoved(
    const PeerConnectionKey& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  auto peer_connection = active_peer_connections_.find(key);

  if (peer_connection == active_peer_connections_.end()) {
    DCHECK(log_files_.find(key) == log_files_.end());
    return false;
  }

  auto local_log = log_files_.find(key);
  if (local_log != log_files_.end()) {
    // Note that success/failure of stopping the local log file is unrelated
    // to the success/failure of PeerConnectionRemoved().
    CloseLogFile(local_log);
  }

  active_peer_connections_.erase(peer_connection);

  return true;
}

bool WebRtcLocalEventLogManager::EnableLogging(const base::FilePath& base_path,
                                               size_t max_file_size_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  if (!base_path_.empty()) {
    return false;
  }

  DCHECK_EQ(log_files_.size(), 0u);

  base_path_ = base_path;

  max_log_file_size_bytes_ =
      (max_file_size_bytes == kWebRtcEventLogManagerUnlimitedFileSize)
          ? base::Optional<size_t>()
          : base::Optional<size_t>(max_file_size_bytes);

  for (const PeerConnectionKey& peer_connection : active_peer_connections_) {
    if (log_files_.size() >= kMaxNumberLocalWebRtcEventLogFiles) {
      break;
    }
    StartLogFile(peer_connection);
  }

  return true;
}

bool WebRtcLocalEventLogManager::DisableLogging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  if (base_path_.empty()) {
    return false;
  }

  for (auto local_log = log_files_.begin(); local_log != log_files_.end();) {
    local_log = CloseLogFile(local_log);
  }

  base_path_.clear();  // Marks local-logging as disabled.
  max_log_file_size_bytes_ = kDefaultMaxLocalLogFileSizeBytes;

  return true;
}

bool WebRtcLocalEventLogManager::EventLogWrite(const PeerConnectionKey& key,
                                               const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);
  auto it = log_files_.find(key);
  if (it == log_files_.end()) {
    return false;
  }

  const bool write_successful = it->second->Write(message);

  if (!write_successful || it->second->MaxSizeReached()) {
    CloseLogFile(it);
  }

  return write_successful;
}

void WebRtcLocalEventLogManager::RenderProcessHostExitedDestroyed(
    int render_process_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  // Remove all of the peer connections associated with this render process.
  auto pc_it = active_peer_connections_.begin();
  while (pc_it != active_peer_connections_.end()) {
    if (pc_it->render_process_id == render_process_id) {
      pc_it = active_peer_connections_.erase(pc_it);
    } else {
      ++pc_it;
    }
  }

  // Close all of the files that were associated with peer connections which
  // belonged to this render process.
  auto log_it = log_files_.begin();
  while (log_it != log_files_.end()) {
    if (log_it->first.render_process_id == render_process_id) {
      log_it = CloseLogFile(log_it);
    } else {
      ++log_it;
    }
  }
}

void WebRtcLocalEventLogManager::SetClockForTesting(base::Clock* clock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);
  clock_for_testing_ = clock;
}

void WebRtcLocalEventLogManager::StartLogFile(const PeerConnectionKey& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);
  DCHECK(log_files_.find(key) == log_files_.end());

  // Add some information to the name given by the caller.
  base::FilePath file_path = GetFilePath(base_path_, key);
  CHECK(!file_path.empty()) << "Couldn't set path for local WebRTC log file.";

  // In the unlikely case that this filename is already taken, find a unique
  // number to append to the filename, if possible.
  file_path = base::GetUniquePath(file_path);
  if (file_path.empty()) {
    return;  // No available file path was found.
  }

  auto log_file =
      log_file_writer_factory_.Create(file_path, max_log_file_size_bytes_);
  if (!log_file) {
    LOG(WARNING) << "Couldn't create and/or open local WebRTC event log file.";
    return;
  }

  const auto it = log_files_.emplace(key, std::move(log_file));
  DCHECK(it.second);

  // The observer needs to be able to run on any TaskQueue.
  if (observer_) {
    LogFilesMap::iterator map_iter = it.first;
    // map_iter->second is a std::unique_ptr<LogFileWriter>.
    observer_->OnLocalLogStarted(key, map_iter->second->path());
  }
}

WebRtcLocalEventLogManager::LogFilesMap::iterator
WebRtcLocalEventLogManager::CloseLogFile(LogFilesMap::iterator it) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  const PeerConnectionKey peer_connection = it->first;

  it->second->Close();
  it = log_files_.erase(it);

  if (observer_) {
    observer_->OnLocalLogStopped(peer_connection);
  }

  return it;
}

base::FilePath WebRtcLocalEventLogManager::GetFilePath(
    const base::FilePath& base_path,
    const PeerConnectionKey& key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  base::Time::Exploded now;
  if (clock_for_testing_) {
    clock_for_testing_->Now().LocalExplode(&now);
  } else {
    base::Time::Now().LocalExplode(&now);
  }

  // [user_defined]_[date]_[time]_[render_process_id]_[lid].[extension]
  char stamp[100];
  int written =
      base::snprintf(stamp, base::size(stamp), "%04d%02d%02d_%02d%02d_%d_%d",
                     now.year, now.month, now.day_of_month, now.hour,
                     now.minute, key.render_process_id, key.lid);
  CHECK_GT(written, 0);
  CHECK_LT(static_cast<size_t>(written), base::size(stamp));

  return base_path.InsertBeforeExtension(FILE_PATH_LITERAL("_"))
      .AddExtension(log_file_writer_factory_.Extension())
      .InsertBeforeExtensionASCII(base::StringPiece(stamp));
}

}  // namespace webrtc_event_logging
