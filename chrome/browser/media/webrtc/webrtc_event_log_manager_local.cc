// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager_local.h"

#include "base/files/file_util.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_WIN)
#define NumberToStringType base::NumberToString16
#else
#define NumberToStringType base::NumberToString
#endif

namespace webrtc_event_logging {

#if BUILDFLAG(IS_ANDROID)
const size_t kDefaultMaxLocalEventLogFileSizeBytes = 10000000;
const size_t kMaxNumberLocalWebRtcEventLogFiles = 3;
#else
const size_t kDefaultMaxLocalEventLogFileSizeBytes = 60000000;
const size_t kMaxNumberLocalWebRtcEventLogFiles = 5;
#endif

struct WebRtcLocalEventLogManager::LogFiles {
  std::unique_ptr<LogFileWriter> event_log;
};

WebRtcLocalEventLogManager::WebRtcLocalEventLogManager(
    WebRtcLocalEventLogsObserver* observer)
    : observer_(observer),
      clock_for_testing_(nullptr),
      event_log_max_size_bytes_(kDefaultMaxLocalEventLogFileSizeBytes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DETACH_FROM_SEQUENCE(io_task_sequence_checker_);
}

WebRtcLocalEventLogManager::~WebRtcLocalEventLogManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

bool WebRtcLocalEventLogManager::OnPeerConnectionAdded(
    const PeerConnectionKey& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);
  const auto insertion_result = log_files_.try_emplace(key);
  if (!insertion_result.second) {
    return false;  // Attempt to re-add the PeerConnection.
  }

  MaybeStartEventLogFile(insertion_result.first);
  return true;
}

bool WebRtcLocalEventLogManager::OnPeerConnectionRemoved(
    const PeerConnectionKey& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  auto it = log_files_.find(key);
  if (it == log_files_.end()) {
    return false;
  }

  StopEventLogFileIfStarted(it);
  log_files_.erase(it);
  return true;
}

bool WebRtcLocalEventLogManager::EnableEventLogging(
    const base::FilePath& base_path,
    size_t max_file_size_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  if (!event_log_base_path_.empty()) {
    return false;
  }

  event_log_base_path_ = base_path;

  event_log_max_size_bytes_ =
      (max_file_size_bytes == kWebRtcEventLogManagerUnlimitedFileSize)
          ? std::optional<size_t>()
          : std::optional<size_t>(max_file_size_bytes);

  for (auto it = log_files_.begin(); it != log_files_.end(); ++it) {
    MaybeStartEventLogFile(it);
  }

  return true;
}

bool WebRtcLocalEventLogManager::DisableEventLogging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  if (event_log_base_path_.empty()) {
    return false;
  }

  for (auto it = log_files_.begin(); it != log_files_.end(); ++it) {
    StopEventLogFileIfStarted(it);
  }

  event_log_base_path_.clear();  // Marks local-logging as disabled.
  event_log_max_size_bytes_ = kDefaultMaxLocalEventLogFileSizeBytes;

  return true;
}

bool WebRtcLocalEventLogManager::EventLogWrite(const PeerConnectionKey& key,
                                               const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);
  auto it = log_files_.find(key);
  if (it == log_files_.end() || it->second.event_log == nullptr) {
    return false;
  }

  const bool write_successful = it->second.event_log->Write(message);

  if (!write_successful || it->second.event_log->MaxSizeReached()) {
    StopEventLogFileIfStarted(it);
  }

  return write_successful;
}

void WebRtcLocalEventLogManager::RenderProcessHostExitedDestroyed(
    int render_process_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  // Remove all of the peer connections associated with this render process.
  for (auto it = log_files_.begin(); it != log_files_.end();) {
    if (it->first.render_process_id == render_process_id) {
      StopEventLogFileIfStarted(it);
      it = log_files_.erase(it);
    } else {
      ++it;
    }
  }
}

void WebRtcLocalEventLogManager::SetClockForTesting(base::Clock* clock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);
  clock_for_testing_ = clock;
}

void WebRtcLocalEventLogManager::MaybeStartEventLogFile(
    LogFilesMap::iterator log_it) {
  if (event_log_base_path_.empty() ||
      num_event_logs_active_ >= kMaxNumberLocalWebRtcEventLogFiles ||
      log_it->second.event_log != nullptr) {
    return;
  }

  std::unique_ptr<LogFileWriter> log_file = CreateLogFile(
      log_it->first, event_log_base_path_, event_log_max_size_bytes_);
  if (!log_file) {
    return;
  }

  log_it->second.event_log = std::move(log_file);
  ++num_event_logs_active_;
  if (observer_) {
    observer_->OnLocalLogStarted(log_it->first,
                                 log_it->second.event_log->path());
  }
}

void WebRtcLocalEventLogManager::StopEventLogFileIfStarted(
    LogFilesMap::iterator log_it) {
  if (!log_it->second.event_log) {
    return;
  }

  log_it->second.event_log.reset();
  --num_event_logs_active_;
  if (observer_) {
    observer_->OnLocalLogStopped(log_it->first);
  }
}

std::unique_ptr<LogFileWriter> WebRtcLocalEventLogManager::CreateLogFile(
    const PeerConnectionKey& key,
    const base::FilePath& base_path,
    std::optional<size_t> max_log_size_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  // Add some information to the name given by the caller.
  base::FilePath file_path = GetFilePath(base_path, key);
  CHECK(!file_path.empty()) << "Couldn't set path for local log file.";

  // In the unlikely case that this filename is already taken, find a unique
  // number to append to the filename, if possible.
  file_path = base::GetUniquePath(file_path);
  if (file_path.empty()) {
    return nullptr;  // No available file path was found.
  }

  auto log_file =
      log_file_writer_factory_.Create(file_path, max_log_size_bytes);
  if (!log_file) {
    LOG(WARNING) << "Couldn't open " << file_path << " for logging.";
    return nullptr;
  }

  return log_file;
}

base::FilePath WebRtcLocalEventLogManager::GetFilePath(
    const base::FilePath& base_path,
    const PeerConnectionKey& key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  // [user_defined]_[date]_[time]_[render_process_id]_[lid].[extension]
  const base::Time now =
      clock_for_testing_ ? clock_for_testing_->Now() : base::Time::Now();
  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);
  const std::string timestamp =
      base::UnlocalizedTimeFormatWithPattern(now, "yyyyMMdd_HHmm");
  return base_path.InsertBeforeExtension(FILE_PATH_LITERAL("_"))
      .AddExtension(log_file_writer_factory_.Extension())
      .InsertBeforeExtensionASCII(base::StringPrintf(
          "%s_%d_%d", timestamp.c_str(), key.render_process_id, key.lid));
}

}  // namespace webrtc_event_logging
