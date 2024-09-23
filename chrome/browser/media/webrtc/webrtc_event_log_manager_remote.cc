// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager_remote.h"

#include <iterator>
#include <utility>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace webrtc_event_logging {

// TODO(crbug.com/40545136): Change max back to (1u << 29) after resolving the
// issue where we read the entire file into memory.
const size_t kMaxRemoteLogFileSizeBytes = 50000000u;

const int kDefaultOutputPeriodMs = 5000;
const int kMaxOutputPeriodMs = 60000;

namespace {
const base::TimeDelta kDefaultProactivePruningDelta = base::Minutes(5);

const base::TimeDelta kDefaultWebRtcRemoteEventLogUploadDelay =
    base::Seconds(30);

// Because history files are rarely used, their existence is not kept in memory.
// That means that pruning them involves inspecting data on disk. This is not
// terribly cheap (up to kMaxWebRtcEventLogHistoryFiles files per profile), and
// should therefore be done somewhat infrequently.
const base::TimeDelta kProactiveHistoryFilesPruneDelta = base::Minutes(30);

base::TimeDelta GetProactivePendingLogsPruneDelta() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kWebRtcRemoteEventLogProactivePruningDelta)) {
    const std::string delta_seconds_str =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            ::switches::kWebRtcRemoteEventLogProactivePruningDelta);
    int64_t seconds;
    if (base::StringToInt64(delta_seconds_str, &seconds) && seconds >= 0) {
      return base::Seconds(seconds);
    } else {
      LOG(WARNING) << "Proactive pruning delta could not be parsed.";
    }
  }

  return kDefaultProactivePruningDelta;
}

base::TimeDelta GetUploadDelay() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kWebRtcRemoteEventLogUploadDelayMs)) {
    const std::string delta_seconds_str =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            ::switches::kWebRtcRemoteEventLogUploadDelayMs);
    int64_t ms;
    if (base::StringToInt64(delta_seconds_str, &ms) && ms >= 0) {
      return base::Milliseconds(ms);
    } else {
      LOG(WARNING) << "Upload delay could not be parsed; using default delay.";
    }
  }

  return kDefaultWebRtcRemoteEventLogUploadDelay;
}

bool TimePointInRange(const base::Time& time_point,
                      const base::Time& range_begin,
                      const base::Time& range_end) {
  DCHECK(!time_point.is_null());
  DCHECK(range_begin.is_null() || range_end.is_null() ||
         range_begin <= range_end);
  return (range_begin.is_null() || range_begin <= time_point) &&
         (range_end.is_null() || time_point < range_end);
}

// Do not attempt to upload when there is no active connection.
// Do not attempt to upload if the connection is known to be a mobile one.
// Note #1: A device may have multiple connections, so this is not bullet-proof.
// Note #2: Does not attempt to recognize mobile hotspots.
bool UploadSupportedUsingConnectionType(
    network::mojom::ConnectionType connection) {
  return connection != network::mojom::ConnectionType::CONNECTION_NONE &&
         connection != network::mojom::ConnectionType::CONNECTION_2G &&
         connection != network::mojom::ConnectionType::CONNECTION_3G &&
         connection != network::mojom::ConnectionType::CONNECTION_4G;
}

// Produce a history file for a given file.
void CreateHistoryFile(const base::FilePath& log_file_path,
                       const base::Time& capture_time) {
  std::unique_ptr<WebRtcEventLogHistoryFileWriter> writer =
      WebRtcEventLogHistoryFileWriter::Create(
          GetWebRtcEventLogHistoryFilePath(log_file_path));
  if (!writer) {
    LOG(ERROR) << "Could not create history file.";
    return;
  }

  if (!writer->WriteCaptureTime(capture_time)) {
    LOG(ERROR) << "Could not write capture time to history file.";
    writer->Delete();
    return;
  }
}

// The following is a list of entry types used to transmit information
// from GetHistory() to the caller (normally - the UI).
// Each entry is of type UploadList::UploadInfo. Depending on the entry
// type, the fields in the UploadInfo have different values:
// 1+2. Currently-being-captured or pending -> State::Pending && !upload_time.
//   3. Currently-being-uploaded -> State::Pending && upload_time.
//   4. Pruned before being uploaded -> State::NotUploaded && !upload_time.
//   5. Unsuccessful upload attempt -> State::NotUploaded && upload_time.
//   6. Successfully uploaded -> State::Uploaded.
//
// As for the meaning of the local_id field, its semantics change according to
// the above entry type.
// * For cases 1-3 above, it is the filename, since the log is still on disk.
// * For cases 5-6 above, it is the local log ID that the now-deleted file used
// * to have.
namespace history {
UploadList::UploadInfo CreateActivelyCapturedLogEntry(
    const base::FilePath& path,
    const base::Time& capture_time) {
  using State = UploadList::UploadInfo::State;
  const std::string filename = path.BaseName().MaybeAsASCII();
  DCHECK(!filename.empty());
  return UploadList::UploadInfo(std::string(), base::Time(), filename,
                                capture_time, State::Pending);
}

UploadList::UploadInfo CreatePendingLogEntry(
    const WebRtcLogFileInfo& log_info) {
  using State = UploadList::UploadInfo::State;
  const std::string filename = log_info.path.BaseName().MaybeAsASCII();
  DCHECK(!filename.empty());
  return UploadList::UploadInfo(std::string(), base::Time(), filename,
                                log_info.last_modified, State::Pending);
}

UploadList::UploadInfo CreateActivelyUploadedLogEntry(
    const WebRtcLogFileInfo& log_info,
    const base::Time& upload_time) {
  using State = UploadList::UploadInfo::State;
  const std::string filename = log_info.path.BaseName().MaybeAsASCII();
  DCHECK(!filename.empty());
  return UploadList::UploadInfo(std::string(), upload_time, filename,
                                log_info.last_modified, State::Pending);
}

UploadList::UploadInfo CreateEntryFromHistoryFileReader(
    const WebRtcEventLogHistoryFileReader& reader) {
  using State = UploadList::UploadInfo::State;
  const auto state =
      reader.UploadId().empty() ? State::NotUploaded : State::Uploaded;
  return UploadList::UploadInfo(reader.UploadId(), reader.UploadTime(),
                                reader.LocalId(), reader.CaptureTime(), state);
}
}  // namespace history
}  // namespace

const size_t kMaxActiveRemoteBoundWebRtcEventLogs = 3;
const size_t kMaxPendingRemoteBoundWebRtcEventLogs = 5;
static_assert(kMaxActiveRemoteBoundWebRtcEventLogs <=
                  kMaxPendingRemoteBoundWebRtcEventLogs,
              "This assumption affects unit test coverage.");
const size_t kMaxWebRtcEventLogHistoryFiles = 50;

// Maximum time to keep remote-bound logs on disk.
const base::TimeDelta kRemoteBoundWebRtcEventLogsMaxRetention = base::Days(7);

// Maximum time to keep history files on disk. These serve to display an upload
// on chrome://webrtc-logs/. It is persisted for longer than the log itself.
const base::TimeDelta kHistoryFileRetention = base::Days(30);

WebRtcRemoteEventLogManager::WebRtcRemoteEventLogManager(
    WebRtcRemoteEventLogsObserver* observer,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : upload_suppression_disabled_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              ::switches::kWebRtcRemoteEventLogUploadNoSuppression)),
      upload_delay_(GetUploadDelay()),
      proactive_pending_logs_prune_delta_(GetProactivePendingLogsPruneDelta()),
      proactive_prune_scheduling_started_(false),
      observer_(observer),
      network_connection_tracker_(nullptr),
      uploading_supported_for_connection_type_(false),
      scheduled_upload_tasks_(0),
      uploader_factory_(
          std::make_unique<WebRtcEventLogUploaderImpl::Factory>(task_runner)),
      task_runner_(task_runner),
      weak_ptr_factory_(
          std::make_unique<base::WeakPtrFactory<WebRtcRemoteEventLogManager>>(
              this)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Proactive pruning would not do anything at the moment; it will be started
  // with the first enabled browser context. This will all have the benefit
  // of doing so on |task_runner_| rather than the UI thread.
}

WebRtcRemoteEventLogManager::~WebRtcRemoteEventLogManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug.com/40545136): Purge from disk files which were being uploaded
  // while destruction took place, thereby avoiding endless attempts to upload
  // the same file.

  if (weak_ptr_factory_) {
    // Not a unit test; that would have gone through ShutDownForTesting().
    const bool will_delete =
        task_runner_->DeleteSoon(FROM_HERE, weak_ptr_factory_.release());
    DCHECK(!will_delete)
        << "Task runners must have been stopped by this stage of shutdown.";
  }

  if (network_connection_tracker_) {
    // * |network_connection_tracker_| might already have posted a task back
    //   to us, but it will not run, because |task_runner_| has already been
    //   stopped.
    // * RemoveNetworkConnectionObserver() should generally be called on the
    //   same thread as AddNetworkConnectionObserver(), but in this case it's
    //   okay to remove on a separate thread, because this only happens during
    //   Chrome shutdown, when no others tasks are running; there can be no
    //   concurrently executing notification from the tracker.
    network_connection_tracker_->RemoveNetworkConnectionObserver(this);
  }
}

void WebRtcRemoteEventLogManager::SetNetworkConnectionTracker(
    network::NetworkConnectionTracker* network_connection_tracker) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(network_connection_tracker);
  DCHECK(!network_connection_tracker_);

  // |this| is only destroyed (on the UI thread) after |task_runner_| stops,
  // so AddNetworkConnectionObserver() is safe.

  network_connection_tracker_ = network_connection_tracker;
  network_connection_tracker_->AddNetworkConnectionObserver(this);

  auto callback =
      base::BindOnce(&WebRtcRemoteEventLogManager::OnConnectionChanged,
                     weak_ptr_factory_->GetWeakPtr());
  network::mojom::ConnectionType connection_type;
  const bool sync_answer = network_connection_tracker_->GetConnectionType(
      &connection_type, std::move(callback));

  if (sync_answer) {
    OnConnectionChanged(connection_type);
  }

  // Because this happens while enabling the first browser context, there is no
  // necessity to consider uploading yet.
  DCHECK_EQ(enabled_browser_contexts_.size(), 0u);
}

void WebRtcRemoteEventLogManager::SetLogFileWriterFactory(
    std::unique_ptr<LogFileWriter::Factory> log_file_writer_factory) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(log_file_writer_factory);
  DCHECK(!log_file_writer_factory_);
  log_file_writer_factory_ = std::move(log_file_writer_factory);
}

void WebRtcRemoteEventLogManager::EnableForBrowserContext(
    BrowserContextId browser_context_id,
    const base::FilePath& browser_context_dir) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(network_connection_tracker_)
      << "SetNetworkConnectionTracker not called.";
  DCHECK(log_file_writer_factory_) << "SetLogFileWriterFactory() not called.";

  if (BrowserContextEnabled(browser_context_id)) {
    return;
  }

  const base::FilePath remote_bound_logs_dir =
      GetRemoteBoundWebRtcEventLogsDir(browser_context_dir);
  if (!MaybeCreateLogsDirectory(remote_bound_logs_dir)) {
    LOG(WARNING)
        << "WebRtcRemoteEventLogManager couldn't create logs directory.";
    return;
  }

  enabled_browser_contexts_.emplace(browser_context_id, remote_bound_logs_dir);

  LoadLogsDirectory(browser_context_id, remote_bound_logs_dir);

  if (!proactive_prune_scheduling_started_) {
    proactive_prune_scheduling_started_ = true;

    if (!proactive_pending_logs_prune_delta_.is_zero()) {
      RecurringlyPrunePendingLogs();
    }

    RecurringlyPruneHistoryFiles();
  }
}

void WebRtcRemoteEventLogManager::DisableForBrowserContext(
    BrowserContextId browser_context_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!BrowserContextEnabled(browser_context_id)) {
    return;  // Enabling may have failed due to lacking permissions.
  }

  enabled_browser_contexts_.erase(browser_context_id);

#if DCHECK_IS_ON()
  // DisableForBrowserContext() is called in one of two cases:
  // 1. If Chrome is shutting down. In that case, all the RPHs associated with
  //    this BrowserContext must already have exited, which should have
  //    implicitly stopped all active logs.
  // 2. Remote-bound logging is no longer allowed for this BrowserContext.
  //    In that case, some peer connections associated with this BrowserContext
  //    might still be active, or become active at a later time, but all
  //    logs must have already been stopped.
  DCHECK(!base::Contains(active_logs_, browser_context_id,
                         [](const decltype(active_logs_)::value_type& log) {
                           return log.first.browser_context_id;
                         }));
#endif

  // Pending logs for this BrowserContext are no longer eligible for upload.
  for (auto it = pending_logs_.begin(); it != pending_logs_.end();) {
    if (it->browser_context_id == browser_context_id) {
      it = pending_logs_.erase(it);
    } else {
      ++it;
    }
  }

  // Active uploads of logs associated with this BrowserContext must be stopped.
  MaybeCancelUpload(base::Time::Min(), base::Time::Max(), browser_context_id);

  // Active logs may have been removed, which could remove upload suppression,
  // or pending logs which were about to be uploaded may have been removed,
  // so uploading may no longer be possible.
  ManageUploadSchedule();
}

bool WebRtcRemoteEventLogManager::OnPeerConnectionAdded(
    const PeerConnectionKey& key) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  PrunePendingLogs();  // Infrequent event - good opportunity to prune.

  const auto result = active_peer_connections_.emplace(key, std::string());

  // An upload about to start might need to be suppressed.
  ManageUploadSchedule();

  return result.second;
}

bool WebRtcRemoteEventLogManager::OnPeerConnectionRemoved(
    const PeerConnectionKey& key) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  PrunePendingLogs();  // Infrequent event - good opportunity to prune.

  const auto peer_connection = active_peer_connections_.find(key);
  if (peer_connection == active_peer_connections_.end()) {
    return false;
  }

  MaybeStopRemoteLogging(key);

  active_peer_connections_.erase(peer_connection);

  ManageUploadSchedule();  // Suppression might have been removed.

  return true;
}

bool WebRtcRemoteEventLogManager::OnPeerConnectionSessionIdSet(
    const PeerConnectionKey& key,
    const std::string& session_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  PrunePendingLogs();  // Infrequent event - good opportunity to prune.

  if (session_id.empty()) {
    LOG(ERROR) << "Empty session ID.";
    return false;
  }

  auto peer_connection = active_peer_connections_.find(key);
  if (peer_connection == active_peer_connections_.end()) {
    return false;  // Unknown peer connection; already closed?
  }

  if (peer_connection->second.empty()) {
    peer_connection->second = session_id;
  } else if (session_id != peer_connection->second) {
    LOG(ERROR) << "Session ID already set to " << peer_connection->second
               << ". Cannot change to " << session_id << ".";
    return false;
  }

  return true;
}

bool WebRtcRemoteEventLogManager::StartRemoteLogging(
    int render_process_id,
    BrowserContextId browser_context_id,
    const std::string& session_id,
    const base::FilePath& browser_context_dir,
    size_t max_file_size_bytes,
    int output_period_ms,
    size_t web_app_id,
    std::string* log_id,
    std::string* error_message) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(log_id);
  DCHECK(log_id->empty());
  DCHECK(error_message);
  DCHECK(error_message->empty());

  if (output_period_ms < 0) {
    output_period_ms = kDefaultOutputPeriodMs;
  }

  if (!AreLogParametersValid(max_file_size_bytes, output_period_ms, web_app_id,
                             error_message)) {
    // |error_message| will have been set by AreLogParametersValid().
    DCHECK(!error_message->empty()) << "AreLogParametersValid() reported an "
                                       "error without an error message.";
    UmaRecordWebRtcEventLoggingApi(WebRtcEventLoggingApiUma::kInvalidArguments);
    return false;
  }

  if (session_id.empty()) {
    *error_message = kStartRemoteLoggingFailureUnknownOrInactivePeerConnection;
    UmaRecordWebRtcEventLoggingApi(WebRtcEventLoggingApiUma::kIllegalSessionId);
    return false;
  }

  if (!BrowserContextEnabled(browser_context_id)) {
    // Remote-bound event logging has either not yet been enabled for this
    // BrowserContext, or has been recently disabled. This error should not
    // really be reached, barring a timing issue.
    *error_message = kStartRemoteLoggingFailureLoggingDisabledBrowserContext;
    UmaRecordWebRtcEventLoggingApi(
        WebRtcEventLoggingApiUma::kDisabledBrowserContext);
    return false;
  }

  PeerConnectionKey key;
  if (!FindPeerConnection(render_process_id, session_id, &key)) {
    *error_message = kStartRemoteLoggingFailureUnknownOrInactivePeerConnection;
    UmaRecordWebRtcEventLoggingApi(
        WebRtcEventLoggingApiUma::kUnknownOrInvalidPeerConnection);
    return false;
  }

  // May not restart active remote logs.
  auto it = active_logs_.find(key);
  if (it != active_logs_.end()) {
    LOG(ERROR) << "Remote logging already underway for " << session_id << ".";
    *error_message = kStartRemoteLoggingFailureAlreadyLogging;
    UmaRecordWebRtcEventLoggingApi(WebRtcEventLoggingApiUma::kAlreadyLogging);
    return false;
  }

  // This is a good opportunity to prune the list of pending logs, potentially
  // making room for this file.
  PrunePendingLogs();

  if (!AdditionalActiveLogAllowed(key.browser_context_id)) {
    *error_message = kStartRemoteLoggingFailureNoAdditionalActiveLogsAllowed;
    UmaRecordWebRtcEventLoggingApi(
        WebRtcEventLoggingApiUma::kNoAdditionalLogsAllowed);
    return false;
  }

  return StartWritingLog(key, browser_context_dir, max_file_size_bytes,
                         output_period_ms, web_app_id, log_id, error_message);
}

bool WebRtcRemoteEventLogManager::EventLogWrite(const PeerConnectionKey& key,
                                                const std::string& message) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  auto it = active_logs_.find(key);
  if (it == active_logs_.end()) {
    return false;
  }

  const bool write_successful = it->second->Write(message);
  if (!write_successful || it->second->MaxSizeReached()) {
    // Note: If the file is invalid, CloseLogFile() will discard it.
    CloseLogFile(it, /*make_pending=*/true);
    ManageUploadSchedule();
  }

  return write_successful;
}

void WebRtcRemoteEventLogManager::ClearCacheForBrowserContext(
    BrowserContextId browser_context_id,
    const base::Time& delete_begin,
    const base::Time& delete_end) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // Rationale for the order:
  // 1. Active logs cancelled. This has no side effects, and can be safely
  //    done before anything else.
  // 2. Pending logs removed, before they can be considered as the
  //    next log to be uploaded. This may cause history files to be created.
  // 3. Remove history files, including those that #2 might have created.
  // 4. Cancel any active upload precisely at a time when nothing being cleared
  //    by ClearCacheForBrowserContext() could accidentally replace it.
  // 5. Explicitly consider uploading, now that things have changed.
  MaybeCancelActiveLogs(delete_begin, delete_end, browser_context_id);
  MaybeRemovePendingLogs(delete_begin, delete_end, browser_context_id,
                         /*is_cache_clear=*/true);
  MaybeRemoveHistoryFiles(delete_begin, delete_end, browser_context_id);
  MaybeCancelUpload(delete_begin, delete_end, browser_context_id);
  ManageUploadSchedule();
}

void WebRtcRemoteEventLogManager::GetHistory(
    BrowserContextId browser_context_id,
    base::OnceCallback<void(const std::vector<UploadList::UploadInfo>&)>
        reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  std::vector<UploadList::UploadInfo> history;

  if (!BrowserContextEnabled(browser_context_id)) {
    // Either the browser context is unknown, or more likely, it's not
    // enabled for remote logging.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(reply), history));
    return;
  }

  PrunePendingLogs(browser_context_id);

  const base::Time now = base::Time::Now();

  std::set<WebRtcEventLogHistoryFileReader> history_files =
      PruneAndLoadHistoryFilesForBrowserContext(
          base::Time::Min(), now - kHistoryFileRetention, browser_context_id);
  for (const auto& history_file : history_files) {
    history.push_back(history::CreateEntryFromHistoryFileReader(history_file));
  }

  for (const WebRtcLogFileInfo& log_info : pending_logs_) {
    if (browser_context_id == log_info.browser_context_id) {
      history.push_back(history::CreatePendingLogEntry(log_info));
    }
  }

  for (const auto& it : active_logs_) {
    if (browser_context_id == it.first.browser_context_id) {
      history.push_back(
          history::CreateActivelyCapturedLogEntry(it.second->path(), now));
    }
  }

  if (uploader_) {
    const WebRtcLogFileInfo log_info = uploader_->GetWebRtcLogFileInfo();
    if (browser_context_id == log_info.browser_context_id) {
      history.push_back(history::CreateActivelyUploadedLogEntry(log_info, now));
    }
  }

  // Sort according to capture time, for consistent orders regardless of
  // future operations on the log files.
  auto cmp = [](const UploadList::UploadInfo& lhs,
                const UploadList::UploadInfo& rhs) {
    if (lhs.capture_time == rhs.capture_time) {
      // Resolve ties arbitrarily, but consistently. (Local ID expected to be
      // distinct for distinct items; if not, anything goes.)
      return lhs.local_id < rhs.local_id;
    }
    return (lhs.capture_time < rhs.capture_time);
  };
  std::sort(history.begin(), history.end(), cmp);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(reply), history));
}

void WebRtcRemoteEventLogManager::RemovePendingLogsForNotEnabledBrowserContext(
    BrowserContextId browser_context_id,
    const base::FilePath& browser_context_dir) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!BrowserContextEnabled(browser_context_id));
  const base::FilePath remote_bound_logs_dir =
      GetRemoteBoundWebRtcEventLogsDir(browser_context_dir);
  if (!base::DeletePathRecursively(remote_bound_logs_dir)) {
    LOG(ERROR) << "Failed to delete  `" << remote_bound_logs_dir << ".";
  }
}

void WebRtcRemoteEventLogManager::RenderProcessHostExitedDestroyed(
    int render_process_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Remove all of the peer connections associated with this render process.
  // It's important to do this before closing the actual files, because closing
  // files can trigger a new upload if no active peer connections are present.
  auto pc_it = active_peer_connections_.begin();
  while (pc_it != active_peer_connections_.end()) {
    if (pc_it->first.render_process_id == render_process_id) {
      pc_it = active_peer_connections_.erase(pc_it);
    } else {
      ++pc_it;
    }
  }

  // Close all of the files that were associated with peer connections which
  // belonged to this render process.
  auto log_it = active_logs_.begin();
  while (log_it != active_logs_.end()) {
    if (log_it->first.render_process_id == render_process_id) {
      log_it = CloseLogFile(log_it, /*make_pending=*/true);
    } else {
      ++log_it;
    }
  }

  ManageUploadSchedule();
}

void WebRtcRemoteEventLogManager::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // Even if switching from WiFi to Ethernet, or between to WiFi connections,
  // reset the timer (if running) until an upload is permissible due to stable
  // upload-supporting conditions.
  time_when_upload_conditions_met_ = base::TimeTicks();

  uploading_supported_for_connection_type_ =
      UploadSupportedUsingConnectionType(type);

  ManageUploadSchedule();

  // TODO(crbug.com/40545136): Support pausing uploads when connection goes
  // down, or switches to an unsupported connection type.
}

void WebRtcRemoteEventLogManager::SetWebRtcEventLogUploaderFactoryForTesting(
    std::unique_ptr<WebRtcEventLogUploader::Factory> uploader_factory) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(uploader_factory);
  uploader_factory_ = std::move(uploader_factory);
}

void WebRtcRemoteEventLogManager::UploadConditionsHoldForTesting(
    base::OnceCallback<void(bool)> callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), UploadConditionsHold()));
}

void WebRtcRemoteEventLogManager::ShutDownForTesting(base::OnceClosure reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  weak_ptr_factory_->InvalidateWeakPtrs();
  weak_ptr_factory_.reset();
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(reply));
}

bool WebRtcRemoteEventLogManager::AreLogParametersValid(
    size_t max_file_size_bytes,
    int output_period_ms,
    size_t web_app_id,
    std::string* error_message) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (max_file_size_bytes == kWebRtcEventLogManagerUnlimitedFileSize) {
    LOG(WARNING) << "Unlimited file sizes not allowed for remote-bound logs.";
    *error_message = kStartRemoteLoggingFailureUnlimitedSizeDisallowed;
    return false;
  }

  if (max_file_size_bytes < log_file_writer_factory_->MinFileSizeBytes()) {
    LOG(WARNING) << "File size below minimum allowed.";
    *error_message = kStartRemoteLoggingFailureMaxSizeTooSmall;
    return false;
  }

  if (max_file_size_bytes > kMaxRemoteLogFileSizeBytes) {
    LOG(WARNING) << "File size exceeds maximum allowed.";
    *error_message = kStartRemoteLoggingFailureMaxSizeTooLarge;
    return false;
  }

  if (output_period_ms > kMaxOutputPeriodMs) {
    LOG(WARNING) << "Output period (ms) exceeds maximum allowed.";
    *error_message = kStartRemoteLoggingFailureOutputPeriodMsTooLarge;
    return false;
  }

  if (web_app_id < kMinWebRtcEventLogWebAppId ||
      web_app_id > kMaxWebRtcEventLogWebAppId) {
    LOG(WARNING) << "Illegal web-app identifier.";
    *error_message = kStartRemoteLoggingFailureIllegalWebAppId;
    return false;
  }

  return true;
}

bool WebRtcRemoteEventLogManager::BrowserContextEnabled(
    BrowserContextId browser_context_id) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  const auto it = enabled_browser_contexts_.find(browser_context_id);
  return it != enabled_browser_contexts_.cend();
}

WebRtcRemoteEventLogManager::LogFilesMap::iterator
WebRtcRemoteEventLogManager::CloseLogFile(LogFilesMap::iterator it,
                                          bool make_pending) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const PeerConnectionKey peer_connection = it->first;  // Copy, not reference.

  const bool valid_file = it->second->Close();
  if (valid_file) {
    if (make_pending) {
      // The current time is a good enough approximation of the file's last
      // modification time.
      const base::Time last_modified = base::Time::Now();

      // The stopped log becomes a pending log.
      const auto emplace_result =
          pending_logs_.emplace(peer_connection.browser_context_id,
                                it->second->path(), last_modified);
      DCHECK(emplace_result.second);  // No pre-existing entry.
    } else {
      const base::FilePath log_file_path = it->second->path();
      if (!base::DeleteFile(log_file_path)) {
        LOG(ERROR) << "Failed to delete " << log_file_path << ".";
      }
    }
  } else {  // !valid_file
    // Close() deleted the file.
    UmaRecordWebRtcEventLoggingUpload(
        WebRtcEventLoggingUploadUma::kLogFileWriteError);
  }

  it = active_logs_.erase(it);

  if (observer_) {
    observer_->OnRemoteLogStopped(peer_connection);
  }

  return it;
}

bool WebRtcRemoteEventLogManager::MaybeCreateLogsDirectory(
    const base::FilePath& remote_bound_logs_dir) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (base::PathExists(remote_bound_logs_dir)) {
    if (!base::DirectoryExists(remote_bound_logs_dir)) {
      LOG(ERROR) << "Path for remote-bound logs is taken by a non-directory.";
      return false;
    }
  } else if (!base::CreateDirectory(remote_bound_logs_dir)) {
    LOG(ERROR) << "Failed to create the local directory for remote-bound logs.";
    return false;
  }

  // TODO(crbug.com/40545136): Test for appropriate permissions.

  return true;
}

void WebRtcRemoteEventLogManager::LoadLogsDirectory(
    BrowserContextId browser_context_id,
    const base::FilePath& remote_bound_logs_dir) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const auto separator =
      base::FilePath::StringType(1, base::FilePath::kExtensionSeparator);
  const base::Time now = base::Time::Now();

  std::set<std::pair<base::FilePath, base::Time>> log_files_to_delete;
  std::set<base::FilePath> history_files_to_delete;

  // Iterate over all of the files in the directory; find the ones that need
  // to be deleted. Skip unknown files; they may belong to the OS.
  base::FileEnumerator enumerator(remote_bound_logs_dir,
                                  /*recursive=*/false,
                                  base::FileEnumerator::FILES);
  for (auto path = enumerator.Next(); !path.empty(); path = enumerator.Next()) {
    const base::FileEnumerator::FileInfo info = enumerator.GetInfo();
    const base::FilePath::StringType extension = info.GetName().Extension();
    if (extension == separator + kWebRtcEventLogUncompressedExtension ||
        extension == separator + kWebRtcEventLogGzippedExtension) {
      const bool loaded = LoadPendingLogInfo(
          browser_context_id, path, enumerator.GetInfo().GetLastModifiedTime());
      if (!loaded) {
        log_files_to_delete.insert(
            std::make_pair(path, info.GetLastModifiedTime()));
      }
    } else if (extension == separator + kWebRtcEventLogHistoryExtension) {
      auto reader = LoadHistoryFile(browser_context_id, path, base::Time::Min(),
                                    now - kHistoryFileRetention);
      if (!reader) {
        history_files_to_delete.insert(path);
      }
    }
  }

  // Remove expired logs.
  for (const auto& file_to_delete : log_files_to_delete) {
    // Produce history file, unless we're discarding this log file precisely
    // because we see it has a history file associated.
    const base::FilePath& log_file_path = file_to_delete.first;
    if (!base::PathExists(GetWebRtcEventLogHistoryFilePath(log_file_path))) {
      const base::Time capture_time = file_to_delete.second;
      CreateHistoryFile(log_file_path, capture_time);
    }

    // Remove the log file itself.
    if (!base::DeleteFile(log_file_path)) {
      LOG(ERROR) << "Failed to delete " << file_to_delete.first << ".";
    }
  }

  // Remove expired history files.
  for (const base::FilePath& history_file_path : history_files_to_delete) {
    if (!base::DeleteFile(history_file_path)) {
      LOG(ERROR) << "Failed to delete " << history_file_path << ".";
    }
  }

  ManageUploadSchedule();
}

bool WebRtcRemoteEventLogManager::LoadPendingLogInfo(
    BrowserContextId browser_context_id,
    const base::FilePath& path,
    base::Time last_modified) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!IsValidRemoteBoundLogFilePath(path)) {
    return false;
  }

  const base::FilePath history_path = GetWebRtcEventLogHistoryFilePath(path);
  if (base::PathExists(history_path)) {
    // Log file has associated history file, indicating an upload was started
    // for it. We should delete the original log from disk.
    UmaRecordWebRtcEventLoggingUpload(
        WebRtcEventLoggingUploadUma::kIncompletePastUpload);
    return false;
  }

  const base::Time now = base::Time::Now();
  if (last_modified + kRemoteBoundWebRtcEventLogsMaxRetention < now) {
    UmaRecordWebRtcEventLoggingUpload(
        WebRtcEventLoggingUploadUma::kExpiredLogFileAtChromeStart);
    return false;
  }

  auto it = pending_logs_.emplace(browser_context_id, path, last_modified);
  DCHECK(it.second);  // No pre-existing entry.

  return true;
}

std::unique_ptr<WebRtcEventLogHistoryFileReader>
WebRtcRemoteEventLogManager::LoadHistoryFile(
    BrowserContextId browser_context_id,
    const base::FilePath& path,
    const base::Time& prune_begin,
    const base::Time& prune_end) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!IsValidRemoteBoundLogFilePath(path)) {
    return nullptr;
  }

  std::unique_ptr<WebRtcEventLogHistoryFileReader> reader =
      WebRtcEventLogHistoryFileReader::Create(path);
  if (!reader) {
    return nullptr;
  }

  const base::Time capture_time = reader->CaptureTime();
  if (prune_begin <= capture_time && capture_time <= prune_end) {
    return nullptr;
  }

  const base::Time upload_time = reader->UploadTime();
  if (!upload_time.is_null()) {
    if (prune_begin <= upload_time && upload_time <= prune_end) {
      return nullptr;
    }
  }

  return reader;
}

std::set<WebRtcEventLogHistoryFileReader>
WebRtcRemoteEventLogManager::PruneAndLoadHistoryFilesForBrowserContext(
    const base::Time& prune_begin,
    const base::Time& prune_end,
    BrowserContextId browser_context_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  std::set<WebRtcEventLogHistoryFileReader> history_files;

  auto browser_contexts_it = enabled_browser_contexts_.find(browser_context_id);
  if (browser_contexts_it == enabled_browser_contexts_.end()) {
    return history_files;
  }

  std::set<base::FilePath> files_to_delete;

  base::FileEnumerator enumerator(browser_contexts_it->second,
                                  /*recursive=*/false,
                                  base::FileEnumerator::FILES);

  for (auto path = enumerator.Next(); !path.empty(); path = enumerator.Next()) {
    const base::FileEnumerator::FileInfo info = enumerator.GetInfo();
    const base::FilePath::StringType extension = info.GetName().Extension();
    const auto separator =
        base::FilePath::StringType(1, base::FilePath::kExtensionSeparator);
    if (extension != separator + kWebRtcEventLogHistoryExtension) {
      continue;
    }

    if (uploader_) {
      const base::FilePath log_path = uploader_->GetWebRtcLogFileInfo().path;
      const base::FilePath history_path =
          GetWebRtcEventLogHistoryFilePath(log_path);
      if (path == history_path) {
        continue;
      }
    }

    auto reader =
        LoadHistoryFile(browser_context_id, path, prune_begin, prune_end);
    if (reader) {
      history_files.insert(std::move(*reader));
      reader.reset();  // |reader| in undetermined state after move().
    } else {           // Defective or expired.
      files_to_delete.insert(path);
    }
  }

  // |history_files| is sorted by log capture time in ascending order;
  // remove the oldest entries until kMaxWebRtcEventLogHistoryFiles is obeyed.
  size_t num_history_files = history_files.size();
  for (auto it = history_files.begin();
       num_history_files > kMaxWebRtcEventLogHistoryFiles;
       --num_history_files) {
    CHECK(it != history_files.end(), base::NotFatalUntil::M130);
    files_to_delete.insert(it->path());
    it = history_files.erase(it);
  }

  for (const base::FilePath& path : files_to_delete) {
    if (!base::DeleteFile(path)) {
      LOG(ERROR) << "Failed to delete " << path << ".";
    }
  }

  return history_files;
}

bool WebRtcRemoteEventLogManager::StartWritingLog(
    const PeerConnectionKey& key,
    const base::FilePath& browser_context_dir,
    size_t max_file_size_bytes,
    int output_period_ms,
    size_t web_app_id,
    std::string* log_id_out,
    std::string* error_message_out) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // The log is assigned a universally unique ID (with high probability).
  const std::string log_id = CreateWebRtcEventLogId();

  // Use the log ID as part of the filename. In the highly unlikely event that
  // this filename is already taken, or that an earlier log with the same name
  // existed and left a history file behind, it will be treated the same way as
  // any other failure to start the log file.
  // TODO(crbug.com/40545136): Add a unit test for above comment.
  const base::FilePath remote_logs_dir =
      GetRemoteBoundWebRtcEventLogsDir(browser_context_dir);
  const base::FilePath log_path =
      WebRtcEventLogPath(remote_logs_dir, log_id, web_app_id,
                         log_file_writer_factory_->Extension());

  if (base::PathExists(log_path)) {
    LOG(ERROR) << "Previously used ID selected.";
    *error_message_out = kStartRemoteLoggingFailureFilePathUsedLog;
    UmaRecordWebRtcEventLoggingApi(
        WebRtcEventLoggingApiUma::kLogPathNotAvailable);
    return false;
  }

  const base::FilePath history_file_path =
      GetWebRtcEventLogHistoryFilePath(log_path);
  if (base::PathExists(history_file_path)) {
    LOG(ERROR) << "Previously used ID selected.";
    *error_message_out = kStartRemoteLoggingFailureFilePathUsedHistory;
    UmaRecordWebRtcEventLoggingApi(
        WebRtcEventLoggingApiUma::kHistoryPathNotAvailable);
    return false;
  }

  // The log is now ACTIVE.
  DCHECK_NE(max_file_size_bytes, kWebRtcEventLogManagerUnlimitedFileSize);
  auto log_file =
      log_file_writer_factory_->Create(log_path, max_file_size_bytes);
  if (!log_file) {
    LOG(ERROR) << "Failed to initialize remote-bound WebRTC event log file.";
    *error_message_out = kStartRemoteLoggingFailureFileCreationError;
    UmaRecordWebRtcEventLoggingApi(
        WebRtcEventLoggingApiUma::kFileCreationError);
    return false;
  }
  const auto it = active_logs_.emplace(key, std::move(log_file));
  DCHECK(it.second);

  observer_->OnRemoteLogStarted(key, it.first->second->path(),
                                output_period_ms);

  UmaRecordWebRtcEventLoggingApi(WebRtcEventLoggingApiUma::kSuccess);

  *log_id_out = log_id;
  return true;
}

void WebRtcRemoteEventLogManager::MaybeStopRemoteLogging(
    const PeerConnectionKey& key) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const auto it = active_logs_.find(key);
  if (it == active_logs_.end()) {
    return;
  }

  CloseLogFile(it, /*make_pending=*/true);

  ManageUploadSchedule();
}

void WebRtcRemoteEventLogManager::PrunePendingLogs(
    std::optional<BrowserContextId> browser_context_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  MaybeRemovePendingLogs(
      base::Time::Min(),
      base::Time::Now() - kRemoteBoundWebRtcEventLogsMaxRetention,
      browser_context_id, /*is_cache_clear=*/false);
}

void WebRtcRemoteEventLogManager::RecurringlyPrunePendingLogs() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!proactive_pending_logs_prune_delta_.is_zero());
  DCHECK(proactive_prune_scheduling_started_);

  PrunePendingLogs();

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WebRtcRemoteEventLogManager::RecurringlyPrunePendingLogs,
                     weak_ptr_factory_->GetWeakPtr()),
      proactive_pending_logs_prune_delta_);
}

void WebRtcRemoteEventLogManager::PruneHistoryFiles() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  for (auto it = enabled_browser_contexts_.begin();
       it != enabled_browser_contexts_.end(); ++it) {
    const BrowserContextId browser_context_id = it->first;
    MaybeRemoveHistoryFiles(base::Time::Min(),
                            base::Time::Now() - kHistoryFileRetention,
                            browser_context_id);
  }
}

void WebRtcRemoteEventLogManager::RecurringlyPruneHistoryFiles() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(proactive_prune_scheduling_started_);

  PruneHistoryFiles();

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WebRtcRemoteEventLogManager::RecurringlyPruneHistoryFiles,
                     weak_ptr_factory_->GetWeakPtr()),
      kProactiveHistoryFilesPruneDelta);
}

void WebRtcRemoteEventLogManager::MaybeCancelActiveLogs(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    BrowserContextId browser_context_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  for (auto it = active_logs_.begin(); it != active_logs_.end();) {
    // Since the file is active, assume it's still being modified.
    if (MatchesFilter(it->first.browser_context_id, base::Time::Now(),
                      browser_context_id, delete_begin, delete_end)) {
      UmaRecordWebRtcEventLoggingUpload(
          WebRtcEventLoggingUploadUma::kActiveLogCancelledDueToCacheClear);
      it = CloseLogFile(it, /*make_pending=*/false);
    } else {
      ++it;
    }
  }
}

void WebRtcRemoteEventLogManager::MaybeRemovePendingLogs(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    std::optional<BrowserContextId> browser_context_id,
    bool is_cache_clear) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  for (auto it = pending_logs_.begin(); it != pending_logs_.end();) {
    if (MatchesFilter(it->browser_context_id, it->last_modified,
                      browser_context_id, delete_begin, delete_end)) {
      UmaRecordWebRtcEventLoggingUpload(
          is_cache_clear
              ? WebRtcEventLoggingUploadUma::kPendingLogDeletedDueToCacheClear
              : WebRtcEventLoggingUploadUma::kExpiredLogFileDuringSession);

      if (!base::DeleteFile(it->path)) {
        LOG(ERROR) << "Failed to delete " << it->path << ".";
      }

      // Produce a history file (they have longer retention) to replace the log.
      if (is_cache_clear) {  // Will be immediately deleted otherwise.
        CreateHistoryFile(it->path, it->last_modified);
      }

      it = pending_logs_.erase(it);
    } else {
      ++it;
    }
  }

  // The last pending log might have been removed.
  if (!UploadConditionsHold()) {
    time_when_upload_conditions_met_ = base::TimeTicks();
  }
}

void WebRtcRemoteEventLogManager::MaybeRemoveHistoryFiles(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    BrowserContextId browser_context_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  PruneAndLoadHistoryFilesForBrowserContext(delete_begin, delete_end,
                                            browser_context_id);
  return;
}

void WebRtcRemoteEventLogManager::MaybeCancelUpload(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    BrowserContextId browser_context_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!uploader_) {
    return;
  }

  const WebRtcLogFileInfo& info = uploader_->GetWebRtcLogFileInfo();
  if (!MatchesFilter(info.browser_context_id, info.last_modified,
                     browser_context_id, delete_begin, delete_end)) {
    return;
  }

  // Cancel the upload. `uploader_` will be released when the callback,
  // `OnWebRtcEventLogUploadComplete`, is posted back.
  uploader_->Cancel();
}

bool WebRtcRemoteEventLogManager::MatchesFilter(
    BrowserContextId log_browser_context_id,
    const base::Time& log_last_modification,
    std::optional<BrowserContextId> filter_browser_context_id,
    const base::Time& filter_range_begin,
    const base::Time& filter_range_end) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (filter_browser_context_id &&
      *filter_browser_context_id != log_browser_context_id) {
    return false;
  }
  return TimePointInRange(log_last_modification, filter_range_begin,
                          filter_range_end);
}

bool WebRtcRemoteEventLogManager::AdditionalActiveLogAllowed(
    BrowserContextId browser_context_id) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Limit over concurrently active logs (across BrowserContext-s).
  if (active_logs_.size() >= kMaxActiveRemoteBoundWebRtcEventLogs) {
    return false;
  }

  // Limit over the number of pending logs (per BrowserContext). We count active
  // logs too, since they become pending logs once completed.
  const size_t active_count =
      base::ranges::count(active_logs_, browser_context_id,
                          [](const decltype(active_logs_)::value_type& log) {
                            return log.first.browser_context_id;
                          });
  const size_t pending_count =
      base::ranges::count(pending_logs_, browser_context_id,
                          [](const decltype(pending_logs_)::value_type& log) {
                            return log.browser_context_id;
                          });
  return active_count + pending_count < kMaxPendingRemoteBoundWebRtcEventLogs;
}

bool WebRtcRemoteEventLogManager::UploadSuppressed() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return !upload_suppression_disabled_ && !active_peer_connections_.empty();
}

bool WebRtcRemoteEventLogManager::UploadConditionsHold() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return !uploader_ && !pending_logs_.empty() && !UploadSuppressed() &&
         uploading_supported_for_connection_type_;
}

void WebRtcRemoteEventLogManager::ManageUploadSchedule() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  PrunePendingLogs();  // Avoid uploading freshly expired files.

  if (!UploadConditionsHold()) {
    time_when_upload_conditions_met_ = base::TimeTicks();
    return;
  }

  if (!time_when_upload_conditions_met_.is_null()) {
    // Conditions have been holding for a while; MaybeStartUploading() has
    // already been scheduled when |time_when_upload_conditions_met_| was set.
    return;
  }

  ++scheduled_upload_tasks_;

  time_when_upload_conditions_met_ = base::TimeTicks::Now();

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WebRtcRemoteEventLogManager::MaybeStartUploading,
                     weak_ptr_factory_->GetWeakPtr()),
      upload_delay_);
}

void WebRtcRemoteEventLogManager::MaybeStartUploading() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_GT(scheduled_upload_tasks_, 0u);

  // Since MaybeStartUploading() was scheduled, conditions might have stopped
  // holding at some point. They may have even stopped and started several times
  // while the currently running task was scheduled, meaning several tasks could
  // be pending now, only the last of which should really end up uploading.

  if (time_when_upload_conditions_met_.is_null()) {
    // Conditions no longer hold; no way to know how many (now irrelevant) other
    // similar tasks are pending, if any.
  } else if (base::TimeTicks::Now() - time_when_upload_conditions_met_ <
             upload_delay_) {
    // Conditions have stopped holding, then started holding again; there has
    // to be a more recent task scheduled, that will take over later.
    DCHECK_GT(scheduled_upload_tasks_, 1u);
  } else {
    // It's up to the rest of the code to turn |scheduled_upload_tasks_| off
    // if the conditions have at some point stopped holding, or it wouldn't
    // know to turn it on when they resume.
    DCHECK(UploadConditionsHold());

    // When the upload we're about to start finishes, there will be another
    // delay of length |upload_delay_| before the next one starts.
    time_when_upload_conditions_met_ = base::TimeTicks();

    auto callback = base::BindOnce(
        &WebRtcRemoteEventLogManager::OnWebRtcEventLogUploadComplete,
        weak_ptr_factory_->GetWeakPtr());

    // The uploader takes ownership of the file; it's no longer considered to be
    // pending. (If the upload fails, the log will be deleted.)
    // TODO(crbug.com/40545136): Add more refined retry behavior, so that we
    // would not delete the log permanently if the network is just down, on the
    // one hand, but also would not be uploading unlimited data on endless
    // retries on the other hand.
    // TODO(crbug.com/40545136): Rename the file before uploading, so that we
    // would not retry the upload after restarting Chrome, if the upload is
    // interrupted.
    currently_uploaded_file_ = pending_logs_.begin()->path;
    uploader_ =
        uploader_factory_->Create(*pending_logs_.begin(), std::move(callback));
    pending_logs_.erase(pending_logs_.begin());
  }

  --scheduled_upload_tasks_;
}

void WebRtcRemoteEventLogManager::OnWebRtcEventLogUploadComplete(
    const base::FilePath& log_file,
    bool upload_successful) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(uploader_);

  // Make sure this callback refers to the currently uploaded file. This might
  // not be the case if the upload was cancelled right after succeeding, in
  // which case we'll get two callbacks, one reporting success and one failure.
  // It can also be that the uploader was cancelled more than once, e.g. if
  // the user cleared cache while PrefService were changing.
  if (!uploader_ ||
      uploader_->GetWebRtcLogFileInfo().path != currently_uploaded_file_) {
    return;
  }

  uploader_.reset();
  currently_uploaded_file_.clear();

  ManageUploadSchedule();
}

bool WebRtcRemoteEventLogManager::FindPeerConnection(
    int render_process_id,
    const std::string& session_id,
    PeerConnectionKey* key) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!session_id.empty());

  const auto it = FindNextPeerConnection(active_peer_connections_.cbegin(),
                                         render_process_id, session_id);
  if (it == active_peer_connections_.cend()) {
    return false;
  }

  // Make sure that the session ID is unique for the renderer process,
  // though not necessarily between renderer processes.
  // (The helper exists solely to allow this DCHECK.)
  DCHECK(FindNextPeerConnection(std::next(it), render_process_id, session_id) ==
         active_peer_connections_.cend());

  *key = it->first;
  return true;
}

WebRtcRemoteEventLogManager::PeerConnectionMap::const_iterator
WebRtcRemoteEventLogManager::FindNextPeerConnection(
    PeerConnectionMap::const_iterator begin,
    int render_process_id,
    const std::string& session_id) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!session_id.empty());
  const auto end = active_peer_connections_.cend();
  for (auto it = begin; it != end; ++it) {
    if (it->first.render_process_id == render_process_id &&
        it->second == session_id) {
      return it;
    }
  }
  return end;
}

}  // namespace webrtc_event_logging
