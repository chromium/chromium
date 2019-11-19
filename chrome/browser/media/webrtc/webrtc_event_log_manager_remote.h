// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_REMOTE_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_REMOTE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_history.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_uploader.h"
#include "components/upload_list/upload_list.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace webrtc_event_logging {

class WebRtcRemoteEventLogManager final
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
  using BrowserContextId = WebRtcEventLogPeerConnectionKey::BrowserContextId;
  using LogFilesMap =
      std::map<WebRtcEventLogPeerConnectionKey, std::unique_ptr<LogFileWriter>>;
  using PeerConnectionKey = WebRtcEventLogPeerConnectionKey;

 public:
  WebRtcRemoteEventLogManager(
      WebRtcRemoteEventLogsObserver* observer,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~WebRtcRemoteEventLogManager() override;

  // Sets a network::NetworkConnectionTracker which will be used to track
  // network connectivity.
  // Must not be called more than once.
  // Must be called before any call to EnableForBrowserContext().
  void SetNetworkConnectionTracker(
      network::NetworkConnectionTracker* network_connection_tracker);

  // Sets a LogFileWriter factory.
  // Must not be called more than once.
  // Must be called before any call to EnableForBrowserContext().
  void SetLogFileWriterFactory(
      std::unique_ptr<LogFileWriter::Factory> log_file_writer_factory);

  // Enables remote-bound logging for a given BrowserContext. Logs stored during
  // previous sessions become eligible for upload, and recording of new logs for
  // peer connections associated with this BrowserContext, in the
  // BrowserContext's user-data directory, becomes possible.
  // This method would typically be called when a BrowserContext is initialized.
  // Enabling for the same BrowserContext twice in a row, without disabling
  // in between, is an error.
  void EnableForBrowserContext(BrowserContextId browser_context_id,
                               const base::FilePath& browser_context_dir);

  // Disables remote-bound logging for a given BrowserContext. Pending logs from
  // earlier (while it was enabled) may no longer be uploaded, additional
  // logs will not be created, and any active uploads associated with the
  // BrowserContext will be cancelled.
  // Disabling for a BrowserContext which was not enabled is not an error,
  // because the caller is not required to know whether a previous call
  // to EnableForBrowserContext() was successful.
  void DisableForBrowserContext(BrowserContextId browser_context_id);

  // Called to inform |this| of peer connections being added/removed.
  // This information is used to:
  // 1. Make decisions about when to upload previously finished logs.
  // 2. When a peer connection is removed, if it was being logged, its log
  //    changes from ACTIVE to PENDING.
  // The return value of both methods indicates only the consistency of the
  // information with previously received information (e.g. can't remove a
  // peer connection that was never added, etc.).
  bool PeerConnectionAdded(const PeerConnectionKey& key);
  bool PeerConnectionRemoved(const PeerConnectionKey& key);

  // Called to inform |this| that a peer connection has been associated
  // with |session_id|. After this, it is possible to refer to  that peer
  // connection using StartRemoteLogging() by providing |session_id|.
  bool PeerConnectionSessionIdSet(const PeerConnectionKey& key,
                                  const std::string& session_id);

  // Attempt to start logging the WebRTC events of an active peer connection.
  // Logging is subject to several restrictions:
  // 1. May not log more than kMaxNumberActiveRemoteWebRtcEventLogFiles logs
  //    at the same time.
  // 2. Each browser context may have only kMaxPendingLogFilesPerBrowserContext
  //    pending logs. Since active logs later become pending logs, it is also
  //    forbidden to start a remote-bound log that would, once completed, become
  //    a pending log that would exceed that limit.
  // 3. The maximum file size must be sensible.
  //
  // If all of the restrictions were observed, and if a file was successfully
  // created, true will be returned.
  //
  // If the call succeeds, the log's identifier will be written to |log_id|.
  // The log identifier is exactly 32 uppercase ASCII characters from the
  // ranges 0-9 and A-F.
  //
  // The log's filename will also incorporate |web_app_id|.
  // |web_app_id| must be between 1 and 99 (inclusive); error otherwise.
  //
  // If the call fails, an error message is written to |error_message|.
  // The error message will be specific to the failure (as opposed to a generic
  // one) is produced only if that error message is useful for the caller:
  // * Bad parameters.
  // * Function called at a time when the caller could know it would fail,
  //   such as for a peer connection that was already logged.
  // We intentionally avoid giving specific errors in some cases, so as
  // to avoid leaking information such as having too many active and/or
  // pending logs.
  bool StartRemoteLogging(int render_process_id,
                          BrowserContextId browser_context_id,
                          const std::string& session_id,
                          const base::FilePath& browser_context_dir,
                          size_t max_file_size_bytes,
                          int output_period_ms,
                          size_t web_app_id,
                          std::string* log_id,
                          std::string* error_message);

  // If an active remote-bound log exists for the given peer connection, this
  // will append |message| to that log.
  // If writing |message| to the log would exceed the log's maximum allowed
  // size, the write is disallowed and the file is closed instead (and changes
  // from ACTIVE to PENDING).
  // If the log file's capacity is exhausted as a result of this function call,
  // or if a write error occurs, the file is closed, and the remote-bound log
  // changes from ACTIVE to PENDING.
  // True is returned if and only if |message| was written in its entirety to
  // an active log.
  bool EventLogWrite(const PeerConnectionKey& key, const std::string& message);

  // Clear PENDING WebRTC event logs associated with a given browser context,
  // in a given time range, then post |reply| back to the thread from which
  // the method was originally invoked (which can be any thread).
  // Log files currently being written are *not* interrupted.
  // Active uploads *are* interrupted.
  void ClearCacheForBrowserContext(BrowserContextId browser_context_id,
                                   const base::Time& delete_begin,
                                   const base::Time& delete_end);

  // See documentation of same method in WebRtcEventLogManager for details.
  void GetHistory(
      BrowserContextId browser_context_id,
      base::OnceCallback<void(const std::vector<UploadList::UploadInfo>&)>
          reply);

  // Works on not-enabled BrowserContext-s, which means the logs are never made
  // eligible for upload. Useful when a BrowserContext is loaded which in
  // the past had remote-logging enabled, but no longer does.
  void RemovePendingLogsForNotEnabledBrowserContext(
      BrowserContextId browser_context_id,
      const base::FilePath& browser_context_dir);

  // An implicit PeerConnectionRemoved() on all of the peer connections that
  // were associated with the renderer process.
  void RenderProcessHostExitedDestroyed(int render_process_id);

  // network::NetworkConnectionTracker::NetworkConnectionObserver implementation
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // Unit tests may use this to inject null uploaders, or ones which are
  // directly controlled by the unit test (succeed or fail according to the
  // test's needs).
  // Note that for simplicity's sake, this may be called from outside the
  // task queue on which this object lives (WebRtcEventLogManager::task_queue_).
  // Therefore, if a test calls this, it should call it before it initializes
  // any BrowserContext with pending log files in its directory.
  void SetWebRtcEventLogUploaderFactoryForTesting(
      std::unique_ptr<WebRtcEventLogUploader::Factory> uploader_factory);

  // Exposes UploadConditionsHold() to unit tests. See WebRtcEventLogManager's
  // documentation for the rationale.
  void UploadConditionsHoldForTesting(base::OnceCallback<void(bool)> callback);

  // In production code, |task_runner_| stops running tasks as part of Chrome's
  // shut-down process, before |this| is torn down. In unit tests, this is
  // not the case.
  void ShutDownForTesting(base::OnceClosure reply);

 private:
  using PeerConnectionMap = std::map<PeerConnectionKey, std::string>;

  // Validates log parameters.
  // If valid, returns true. Otherwise, false, and |error_message| gets
  // a relevant error.
  bool AreLogParametersValid(size_t max_file_size_bytes,
                             int output_period_ms,
                             size_t web_app_id,
                             std::string* error_message) const;

  // Checks whether a browser context has already been enabled via a call to
  // EnableForBrowserContext(), and not yet disabled using a call to
  // DisableForBrowserContext().
  bool BrowserContextEnabled(BrowserContextId browser_context_id) const;

  // Closes an active log file.
  // If |make_pending| is true, closing the file changes its state from ACTIVE
  // to PENDING. If |make_pending| is false, or if the file couldn't be closed
  // correctly, the file will be deleted.
  // Returns an iterator to the next ACTIVE file.
  LogFilesMap::iterator CloseLogFile(LogFilesMap::iterator it,
                                     bool make_pending);

  // Attempts to create the directory where we'll write the logs, if it does
  // not already exist. Returns true if the directory exists (either it already
  // existed, or it was successfully created).
  bool MaybeCreateLogsDirectory(const base::FilePath& remote_bound_logs_dir);

  // Scans the user data directory associated with the BrowserContext
  // associated with the given BrowserContextId remote-bound logs that were
  // created during previous Chrome sessions and for history files,
  // then process them (discard expired files, etc.)
  void LoadLogsDirectory(BrowserContextId browser_context_id,
                         const base::FilePath& remote_bound_logs_dir);

  // Loads the pending log file whose path is |path|, into the BrowserContext
  // indicated by |browser_context_id|. Note that the contents of the file are
  // note read by this method.
  // Returns true if the file was loaded correctly, and should be kept on disk;
  // false if the file was not loaded (e.g. incomplete or expired), and needs
  // to be deleted.
  bool LoadPendingLogInfo(BrowserContextId browser_context_id,
                          const base::FilePath& path,
                          base::Time last_modified);

  // Loads a history file. Returns a WebRtcEventLogHistoryFileReader if the
  // file was loaded correctly, and should be kept on disk; nullptr otherwise,
  // signaling that the file should be deleted.
  // |prune_begin| and |prune_end| define a time range where, if the log falls
  // within the range, it will not be loaded.
  std::unique_ptr<WebRtcEventLogHistoryFileReader> LoadHistoryFile(
      BrowserContextId browser_context_id,
      const base::FilePath& path,
      const base::Time& prune_begin,
      const base::Time& prune_end);

  // Deletes any history logs associated with |browser_context_id| captured or
  // uploaded between |prune_begin| and |prune_end|, inclusive, then returns a
  // set of readers for the remaining (meaning not-pruned) history files.
  std::set<WebRtcEventLogHistoryFileReader>
  PruneAndLoadHistoryFilesForBrowserContext(
      const base::Time& prune_begin,
      const base::Time& prune_end,
      BrowserContextId browser_context_id);

  // Attempts the creation of a locally stored file into which a remote-bound
  // log may be written. The log-identifier is returned if successful, the empty
  // string otherwise.
  bool StartWritingLog(const PeerConnectionKey& key,
                       const base::FilePath& browser_context_dir,
                       size_t max_file_size_bytes,
                       int output_period_ms,
                       size_t web_app_id,
                       std::string* log_id_out,
                       std::string* error_message_out);

  // Checks if the referenced peer connection has an associated active
  // remote-bound log. If it does, the log is changed from ACTIVE to PENDING.
  void MaybeStopRemoteLogging(const PeerConnectionKey& key);

  // Get rid of pending logs whose age exceeds our retention policy.
  // On the one hand, we want to remove expired files as soon as possible, but
  // on the other hand, we don't want to waste CPU by checking this too often.
  // Therefore, we prune pending files:
  // 1. When a new BrowserContext is initalized, thereby also pruning the
  //    pending logs contributed by that BrowserContext.
  // 2. Before initiating a new upload, thereby avoiding uploading a file that
  //    has just now expired.
  // 3. On infrequent events - peer connection addition/removal, but NOT
  //    on something that could potentially be frequent, such as EventLogWrite.
  // Note that the last modification date of a file, which is the value measured
  // against for retention, is only read from disk once per file, meaning
  // this check is not too expensive.
  // If a |browser_context_id| is provided, logs are only pruned for it.
  void PrunePendingLogs(
      base::Optional<BrowserContextId> browser_context_id = base::nullopt);

  // PrunePendingLogs() and schedule the next proactive pending logs prune.
  void RecurringlyPrunePendingLogs();

  // Removes expired history files.
  // Since these are small, and since looking for them is not as cheap as
  // looking for pending logs, we do not make an effort to remove them as
  // soon as possible.
  void PruneHistoryFiles();

  // PruneHistoryFiles() and schedule the next proactive history files prune.
  void RecurringlyPruneHistoryFiles();

  // Cancels and deletes active logs which match the given filter criteria, as
  // described by MatchesFilter's documentation.
  // This method not trigger any pending logs to be uploaded, allowing it to
  // be safely used in a context that clears browsing data.
  void MaybeCancelActiveLogs(const base::Time& delete_begin,
                             const base::Time& delete_end,
                             BrowserContextId browser_context_id);

  // Removes pending logs files which match the given filter criteria, as
  // described by MatchesFilter's documentation.
  // This method not trigger any pending logs to be uploaded, allowing it to
  // be safely used in a context that clears browsing data.
  void MaybeRemovePendingLogs(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      base::Optional<BrowserContextId> browser_context_id,
      bool is_cache_clear);

  // Remove all history files associated with |browser_context_id| which were
  // either captured or uploaded between |delete_begin| and |delete_end|.
  // This method not trigger any pending logs to be uploaded, allowing it to
  // be safely used in a context that clears browsing data.
  void MaybeRemoveHistoryFiles(const base::Time& delete_begin,
                               const base::Time& delete_end,
                               BrowserContextId browser_context_id);

  // If the currently uploaded file matches the given filter criteria, as
  // described by MatchesFilter's documentation, the upload will be
  // cancelled, and the log file deleted. If this happens, the next pending log
  // file will be considered for upload.
  // This method is used to ensure that clearing of browsing data by the user
  // does not leave the currently-uploaded file on disk, even for the duration
  // of the upload.
  // This method not trigger any pending logs to be uploaded, allowing it to
  // be safely used in a context that clears browsing data.
  void MaybeCancelUpload(const base::Time& delete_begin,
                         const base::Time& delete_end,
                         BrowserContextId browser_context_id);

  // Checks whether a log file matches a range and (potentially) BrowserContext:
  // * A file matches if its last modification date was at or later than
  //   |filter_range_begin|, and earlier than |filter_range_end|.
  // * If a null time-point is given as either |filter_range_begin| or
  //   |filter_range_end|, it is treated as "beginning-of-time" or
  //   "end-of-time", respectively.
  // * If |filter_browser_context_id| is set, only log files associated with it
  //   can match the filter.
  bool MatchesFilter(BrowserContextId log_browser_context_id,
                     const base::Time& log_last_modification,
                     base::Optional<BrowserContextId> filter_browser_context_id,
                     const base::Time& filter_range_begin,
                     const base::Time& filter_range_end) const;

  // Return |true| if and only if we can start another active log (with respect
  // to limitations on the numbers active and pending logs).
  bool AdditionalActiveLogAllowed(BrowserContextId browser_context_id) const;

  // Uploading suppressed while active peer connections exist (unless
  // suppression) is turned off from the command line.
  bool UploadSuppressed() const;

  // Check whether all the conditions necessary for uploading log files are
  // currently satisfied.
  // 1. There may be no active peer connections which might be adversely
  //    affected by the bandwidth consumption of the upload.
  // 2. Chrome has a network connection, and that conneciton is either a wired
  //    one, or WiFi. (That is, not 3G, etc.)
  // 3. Naturally, a file pending upload must exist.
  bool UploadConditionsHold() const;

  // When the conditions necessary for uploading first hold, schedule a delayed
  // task to upload (MaybeStartUploading). If they ever stop holding, void it.
  void ManageUploadSchedule();

  // Posted as a delayed task by ManageUploadSchedule. If not voided until
  // executed, will initiate an upload of the next log file.
  void MaybeStartUploading();

  // Callback for the success/failure of an upload.
  // When an upload is complete, it might be time to upload the next file.
  // Note: |log_file| and |upload_successful| are ignored in production; they
  // are used in unit tests, so we keep them here to make things simpler, so
  // that this method would match WebRtcEventLogUploader::UploadResultCallback
  // without adaptation.
  void OnWebRtcEventLogUploadComplete(const base::FilePath& log_file,
                                      bool upload_successful);

  // Given a renderer process ID and peer connection's session ID, find the
  // peer connection to which they refer.
  bool FindPeerConnection(int render_process_id,
                          const std::string& session_id,
                          PeerConnectionKey* key) const;

  // Find the next peer connection in a map to which the renderer process ID
  // and session ID refer.
  // This helper allows FindPeerConnection() to DCHECK on uniqueness of the ID
  // without descending down a recursive rabbit hole.
  PeerConnectionMap::const_iterator FindNextPeerConnection(
      PeerConnectionMap::const_iterator begin,
      int render_process_id,
      const std::string& session_id) const;

  // Normally, uploading is suppressed while there are active peer connections.
  // This may be disabled from the command line.
  const bool upload_suppression_disabled_;

  // The conditions for upload must hold for this much time, uninterrupted,
  // before an upload may be initiated.
  const base::TimeDelta upload_delay_;

  // If non-zero, every |proactive_pending_logs_prune_delta_|, pending logs
  // will be pruned. This avoids them staying around on disk for longer than
  // their expiration if no event occurs which triggers reactive pruning.
  const base::TimeDelta proactive_pending_logs_prune_delta_;

  // Proactive pruning, if enabled, starts with the first enabled browser
  // context. To avoid unnecessary complexity, if that browser context is
  // disabled, proactive pruning is not disabled.
  bool proactive_prune_scheduling_started_;

  // This is used to inform WebRtcEventLogManager when remote-bound logging
  // of a peer connection starts/stops, which allows WebRtcEventLogManager to
  // decide when to ask WebRTC to start/stop sending event logs.
  WebRtcRemoteEventLogsObserver* const observer_;

  // The IDs of the BrowserContexts for which logging is enabled, mapped to
  // the directory where each BrowserContext's remote-bound logs are stored.
  std::map<BrowserContextId, base::FilePath> enabled_browser_contexts_;

  // Currently active peer connections, mapped to their session IDs (once the
  // session ID is set).
  // PeerConnections which have been closed are not considered active,
  // regardless of whether they have been torn down.
  PeerConnectionMap active_peer_connections_;

  // Creates LogFileWriter instances (compressed/uncompressed, etc.).
  std::unique_ptr<LogFileWriter::Factory> log_file_writer_factory_;

  // Remote-bound logs which we're currently in the process of writing to disk.
  LogFilesMap active_logs_;

  // Remote-bound logs which have been written to disk before (either during
  // this Chrome session or during an earlier one), and which are no waiting to
  // be uploaded.
  std::set<WebRtcLogFileInfo> pending_logs_;

  // Null if no ongoing upload, or an uploader which owns a file, and is
  // currently busy uploading it to a remote server.
  std::unique_ptr<WebRtcEventLogUploader> uploader_;

  // Provides notifications of network changes.
  network::NetworkConnectionTracker* network_connection_tracker_;

  // Whether the network we are currently connected to, if any, is one over
  // which we may upload.
  bool uploading_supported_for_connection_type_;

  // If the conditions for initiating an upload do not hold, this will be
  // set to an empty base::TimeTicks.
  // If the conditions were found to hold, this will record the time when they
  // started holding. (It will be set back to 0 if they ever cease holding.)
  base::TimeTicks time_when_upload_conditions_met_;

  // This is a vehicle for DCHECKs to ensure code sanity. It counts the number
  // of scheduled tasks of MaybeStartUploading(), and proves that we never
  // end up with a scheduled upload that never occurs.
  size_t scheduled_upload_tasks_;

  // Producer of uploader objects. (In unit tests, this would create
  // null-implementation uploaders, or uploaders whose behavior is controlled
  // by the unit test.)
  std::unique_ptr<WebRtcEventLogUploader::Factory> uploader_factory_;

  // |this| is created and destroyed on the UI thread, but operates on the
  // following IO-capable sequenced task runner.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Weak pointer factory. Only expected to be useful for unit tests, because
  // in production, |task_runner_| is stopped during shut-down, so tasks will
  // either find the pointer to be valid, or not run because the runner has
  // already been stopped.
  // Note that the unique_ptr is used just to make it clearer that ownership is
  // here. In reality, this is never auto-destroyed; see destructor for details.
  std::unique_ptr<base::WeakPtrFactory<WebRtcRemoteEventLogManager>>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcRemoteEventLogManager);
};

}  // namespace webrtc_event_logging

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_REMOTE_H_
