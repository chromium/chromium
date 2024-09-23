// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_local.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_remote.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/upload_list/upload_list.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/peer_connection_tracker_host_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/webrtc_event_logger.h"

class WebRTCInternalsIntegrationBrowserTest;

namespace content {
class BrowserContext;
}  // namespace content

namespace network {
class NetworkConnectionTracker;
}  // namespace network

FORWARD_DECLARE_TEST(WebRtcEventLogCollectionAllowedPolicyTest, RunTest);

namespace webrtc_event_logging {

// This is a singleton class running in the browser UI thread (ownership of
// the only instance lies in BrowserContext). It is in charge of writing WebRTC
// event logs to temporary files, then uploading those files to remote servers,
// as well as of writing the logs to files which were manually indicated by the
// user from the WebRTCIntenals. (A log may simulatenously be written to both,
// either, or none.)
// The only instance of this class is owned by BrowserProcessImpl. It is
// destroyed from ~BrowserProcessImpl(), at which point any tasks posted to the
// internal SequencedTaskRunner, or coming from another thread, would no longer
// execute.
class WebRtcEventLogManager final
    : public content::RenderProcessHostObserver,
      public content::PeerConnectionTrackerHostObserver,
      public content::WebRtcEventLogger,
      public WebRtcLocalEventLogsObserver,
      public WebRtcRemoteEventLogsObserver {
 public:
  using BrowserContextId = WebRtcEventLogPeerConnectionKey::BrowserContextId;

  // To turn WebRTC on and off, we go through PeerConnectionTrackerProxy. In
  // order to make this toggling easily testable, PeerConnectionTrackerProxyImpl
  // will send real messages to PeerConnectionTracker, whereas
  // PeerConnectionTrackerProxyForTesting will be a mock that just makes sure
  // the correct messages were attempted to be sent.
  class PeerConnectionTrackerProxy {
   public:
    virtual ~PeerConnectionTrackerProxy() = default;

    virtual void EnableWebRtcEventLogging(
        const WebRtcEventLogPeerConnectionKey& key,
        int output_period_ms) = 0;

    virtual void DisableWebRtcEventLogging(
        const WebRtcEventLogPeerConnectionKey& key) = 0;
  };

  // Ensures that no previous instantiation of the class was performed, then
  // instantiates the class and returns the object (ownership is transfered to
  // the caller). Subsequent calls to GetInstance() will return this object,
  // until it is destructed, at which pointer nullptr will be returned by
  // subsequent calls.
  static std::unique_ptr<WebRtcEventLogManager> CreateSingletonInstance();

  // Returns the object previously constructed using CreateSingletonInstance(),
  // if it was constructed and was not yet destroyed; nullptr otherwise.
  static WebRtcEventLogManager* GetInstance();

  // Given a BrowserContext, return the path to the directory where its
  // remote-bound event logs are kept.
  // Since incognito sessions don't have such a directory, an empty
  // base::FilePath will be returned for them.
  static base::FilePath GetRemoteBoundWebRtcEventLogsDir(
      content::BrowserContext* browser_context);

  WebRtcEventLogManager(const WebRtcEventLogManager&) = delete;
  WebRtcEventLogManager& operator=(const WebRtcEventLogManager&) = delete;

  ~WebRtcEventLogManager() override;

  void EnableForBrowserContext(content::BrowserContext* browser_context,
                               base::OnceClosure reply);

  void DisableForBrowserContext(content::BrowserContext* browser_context,
                                base::OnceClosure reply);

  // content::PeerConnectionTrackerHostObserver implementation.
  void OnPeerConnectionAdded(content::GlobalRenderFrameHostId frame_id,
                             int lid,
                             base::ProcessId pid,
                             const std::string& url,
                             const std::string& rtc_configuration) override;
  void OnPeerConnectionRemoved(content::GlobalRenderFrameHostId frame_id,
                               int lid) override;
  void OnPeerConnectionUpdated(content::GlobalRenderFrameHostId frame_id,
                               int lid,
                               const std::string& type,
                               const std::string& value) override;
  void OnPeerConnectionSessionIdSet(content::GlobalRenderFrameHostId frame_id,
                                    int lid,
                                    const std::string& session_id) override;
  void OnWebRtcEventLogWrite(content::GlobalRenderFrameHostId frame_id,
                             int lid,
                             const std::string& message) override;

  // content::WebRtcEventLogger implementation.
  void EnableLocalLogging(const base::FilePath& base_path) override;
  void DisableLocalLogging() override;

  // Start logging a peer connection's WebRTC events to a file, which will
  // later be uploaded to a remote server. If a reply is provided, it will be
  // posted back to BrowserThread::UI with the log-identifier (if successful)
  // of the created log or (if unsuccessful) the error message.
  // See the comment in  WebRtcRemoteEventLogManager::StartRemoteLogging for
  // more details.
  void StartRemoteLogging(
      int render_process_id,
      const std::string& session_id,
      size_t max_file_size_bytes,
      int output_period_ms,
      size_t web_app_id,
      base::OnceCallback<void(bool, const std::string&, const std::string&)>
          reply);

  // Clear WebRTC event logs associated with a given browser context, in a given
  // time range (|delete_begin| inclusive, |delete_end| exclusive), then
  // post |reply| back to the thread from which the method was originally
  // invoked (which can be any thread).
  void ClearCacheForBrowserContext(
      const content::BrowserContext* browser_context,
      const base::Time& delete_begin,
      const base::Time& delete_end,
      base::OnceClosure reply);

  // Get the logging history (relevant only to remote-bound logs). This includes
  // information such as when logs were captured, when they were uploaded,
  // and what their ID in the remote server was.
  // Must be called on the UI thread.
  // The results to the query are posted using |reply| back to the UI thread.
  // If |browser_context_id| is not the ID a profile for which remote-bound
  // logging is enabled, an empty list is returned.
  // The returned vector is sorted by capture time in ascending order.
  void GetHistory(
      BrowserContextId browser_context_id,
      base::OnceCallback<void(const std::vector<UploadList::UploadInfo>&)>
          reply);

  // Set (or unset) an observer that will be informed whenever a local log file
  // is started/stopped. The observer needs to be able to either run from
  // anywhere. If you need the code to run on specific runners or queues, have
  // the observer post them there.
  // If a reply callback is given, it will be posted back to BrowserThread::UI
  // after the observer has been set.
  void SetLocalLogsObserver(WebRtcLocalEventLogsObserver* observer,
                            base::OnceClosure reply);

  // Set (or unset) an observer that will be informed whenever a remote log file
  // is started/stopped. Note that this refers to writing these files to disk,
  // not for uploading them to the server.
  // The observer needs to be able to either run from anywhere. If you need the
  // code to run on specific runners or queues, have the observer post
  // them there.
  // If a reply callback is given, it will be posted back to BrowserThread::UI
  // after the observer has been set.
  void SetRemoteLogsObserver(WebRtcRemoteEventLogsObserver* observer,
                             base::OnceClosure reply);

 private:
  FRIEND_TEST_ALL_PREFIXES(::WebRtcEventLogCollectionAllowedPolicyTest,
                           RunTest);
  friend class WebRtcEventLogManagerTestBase;
  friend class ::WebRTCInternalsIntegrationBrowserTest;

  using PeerConnectionKey = WebRtcEventLogPeerConnectionKey;

  // This bitmap allows us to track for which clients (local/remote logging)
  // we have turned WebRTC event logging on for a given peer connection, so that
  // we may turn it off only when the last client no longer needs it.
  enum LoggingTarget : unsigned int {
    kLocalLogging = 1 << 0,
    kRemoteLogging = 1 << 1
  };
  using LoggingTargetBitmap = std::underlying_type<LoggingTarget>::type;

  WebRtcEventLogManager();

  bool IsRemoteLoggingAllowedForBrowserContext(
      content::BrowserContext* browser_context) const;

  // Determines the exact subclass of LogFileWriter::Factory to be used for
  // producing remote-bound logs.
  std::unique_ptr<LogFileWriter::Factory> CreateRemoteLogFileWriterFactory();

  // RenderProcessHostObserver implementation.
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // RenderProcessExited() and RenderProcessHostDestroyed() treated similarly
  // by this function.
  void RenderProcessHostExitedDestroyed(content::RenderProcessHost* host);

  // Each method overridden from content::PeerConnectionTrackerHostObserver and
  // content::WebRtcEventLogger has an overload that posts back a reply on the
  // UI thread with the result of the operation. Used for testing only.

  // An overload of OnPeerConnectionAdded() that replies true if and only if the
  // operation was successful. A failure can happen if a peer connection with
  // this exact key was previously added, but not removed. Another failure mode
  // is if the RPH of the frame was destroyed.
  void OnPeerConnectionAdded(content::GlobalRenderFrameHostId frame_id,
                             int lid,
                             base::OnceCallback<void(bool)> reply);

  // An overload of OnPeerConnectionRemoved() that replies true if and only if
  // the operation was successful. A failure can happen is a peer connection
  // with this key was not previously added or if it has since already been
  // removed. Another failure mode is if the RPH of the frame was destroyed.
  void OnPeerConnectionRemoved(content::GlobalRenderFrameHostId frame_id,
                               int lid,
                               base::OnceCallback<void(bool)> reply);

  // Handles a "stop" peer connection update. Replies true if and only if the
  // operation was successful. Same failure mode as OnPeerConnectionRemoved().
  void OnPeerConnectionStopped(content::GlobalRenderFrameHostId frame_id,
                               int lid,
                               base::OnceCallback<void(bool)> reply);

  // An overload of OnPeerConnectionSessionIdSet() that replies true if and only
  // if the operation was successful.
  void OnPeerConnectionSessionIdSet(content::GlobalRenderFrameHostId frame_id,
                                    int lid,
                                    const std::string& session_id,
                                    base::OnceCallback<void(bool)> reply);

  // An overload of OnWebRtcEventLogWrite() that replies with a pair of bool.
  // The first bool is associated with local logging and the second bool is
  // associated with remote-bound logging. Each bool assumes the value true if
  // and only if the message was written in its entirety into a
  // local/remote-bound log file.
  void OnWebRtcEventLogWrite(
      content::GlobalRenderFrameHostId frame_id,
      int lid,
      const std::string& message,
      base::OnceCallback<void(std::pair<bool, bool>)> reply);

  // An overload of EnableLocalLogging() replies true if the logging was
  // actually enabled. i.e. The logging was not already enabled before the call.
  void EnableLocalLogging(const base::FilePath& base_path,
                          base::OnceCallback<void(bool)> reply);

  // Same as the above, but allows the caller to customize the maximum size of
  // the log file.
  void EnableLocalLogging(const base::FilePath& base_path,
                          size_t max_file_size_bytes,
                          base::OnceCallback<void(bool)> reply);

  // An overload of DisableLocalLogging() that replies true if the logging was
  // actually disabled. i.e. The logging was enabled before the call.
  void DisableLocalLogging(base::OnceCallback<void(bool)> reply);

  // WebRtcLocalEventLogsObserver implementation:
  void OnLocalLogStarted(PeerConnectionKey peer_connection,
                         const base::FilePath& file_path) override;
  void OnLocalLogStopped(PeerConnectionKey peer_connection) override;

  // WebRtcRemoteEventLogsObserver implementation:
  void OnRemoteLogStarted(PeerConnectionKey key,
                          const base::FilePath& file_path,
                          int output_period_ms) override;
  void OnRemoteLogStopped(PeerConnectionKey key) override;

  void OnLoggingTargetStarted(LoggingTarget target,
                              PeerConnectionKey key,
                              int output_period_ms);
  void OnLoggingTargetStopped(LoggingTarget target, PeerConnectionKey key);

  void StartListeningForPrefChangeForBrowserContext(
      content::BrowserContext* browser_context);
  void StopListeningForPrefChangeForBrowserContext(
      content::BrowserContext* browser_context);

  void OnPrefChange(content::BrowserContext* browser_context);

  // network_connection_tracker() is not available during instantiation;
  // we get it when the first profile is loaded, which is also the earliest
  // time when it could be needed.
  // The LogFileWriter::Factory is similarly deferred, but for a different
  // reason - it makes it easier to allow unit tests to inject their own.
  // OnFirstBrowserContextLoaded() is on the UI thread.
  // OnFirstBrowserContextLoadedInternal() is the task sent to |task_runner_|.
  void OnFirstBrowserContextLoaded();
  void OnFirstBrowserContextLoadedInternal(
      network::NetworkConnectionTracker* network_connection_tracker,
      std::unique_ptr<LogFileWriter::Factory> log_file_writer_factory);

  void EnableRemoteBoundLoggingForBrowserContext(
      BrowserContextId browser_context_id,
      const base::FilePath& browser_context_dir,
      base::OnceClosure reply);

  void DisableRemoteBoundLoggingForBrowserContext(
      BrowserContextId browser_context_id,
      base::OnceClosure reply);

  void RemovePendingRemoteBoundLogsForNotEnabledBrowserContext(
      BrowserContextId browser_context_id,
      const base::FilePath& browser_context_dir,
      base::OnceClosure reply);

  void OnPeerConnectionAddedInternal(PeerConnectionKey key,
                                     base::OnceCallback<void(bool)> reply);
  void OnPeerConnectionRemovedInternal(PeerConnectionKey key,
                                       base::OnceCallback<void(bool)> reply);

  void OnPeerConnectionSessionIdSetInternal(
      PeerConnectionKey key,
      const std::string& session_id,
      base::OnceCallback<void(bool)> reply);

  void EnableLocalLoggingInternal(const base::FilePath& base_path,
                                  size_t max_file_size_bytes,
                                  base::OnceCallback<void(bool)> reply);
  void DisableLocalLoggingInternal(base::OnceCallback<void(bool)> reply);

  void OnWebRtcEventLogWriteInternal(
      PeerConnectionKey key,
      const std::string& message,
      base::OnceCallback<void(std::pair<bool, bool>)> reply);

  void StartRemoteLoggingInternal(
      int render_process_id,
      BrowserContextId browser_context_id,
      const std::string& session_id,
      const base::FilePath& browser_context_dir,
      size_t max_file_size_bytes,
      int output_period_ms,
      size_t web_app_id,
      base::OnceCallback<void(bool, const std::string&, const std::string&)>
          reply);

  void ClearCacheForBrowserContextInternal(BrowserContextId browser_context_id,
                                           const base::Time& delete_begin,
                                           const base::Time& delete_end);

  void OnClearCacheForBrowserContextDoneInternal(base::OnceClosure reply);

  void GetHistoryInternal(
      BrowserContextId browser_context_id,
      base::OnceCallback<void(const std::vector<UploadList::UploadInfo>&)>
          reply);

  void RenderProcessExitedInternal(int render_process_id);

  void SetLocalLogsObserverInternal(WebRtcLocalEventLogsObserver* observer,
                                    base::OnceClosure reply);

  void SetRemoteLogsObserverInternal(WebRtcRemoteEventLogsObserver* observer,
                                     base::OnceClosure reply);

  // Injects a fake clock, to be used by tests. For example, this could be
  // used to inject a frozen clock, thereby allowing unit tests to know what a
  // local log's filename would end up being.
  void SetClockForTesting(base::Clock* clock, base::OnceClosure reply);

  // Injects a PeerConnectionTrackerProxy for testing. The normal tracker proxy
  // is used to communicate back to WebRTC whether event logging is desired for
  // a given peer connection. Using this function, those indications can be
  // intercepted by a unit test.
  void SetPeerConnectionTrackerProxyForTesting(
      std::unique_ptr<PeerConnectionTrackerProxy> pc_tracker_proxy,
      base::OnceClosure reply);

  // Injects a fake uploader, to be used by unit tests.
  void SetWebRtcEventLogUploaderFactoryForTesting(
      std::unique_ptr<WebRtcEventLogUploader::Factory> uploader_factory,
      base::OnceClosure reply);

  // Sets a LogFileWriter factory for remote-bound files.
  // Only usable in tests.
  // Must be called before the first browser context is enabled.
  // Effective immediately.
  void SetRemoteLogFileWriterFactoryForTesting(
      std::unique_ptr<LogFileWriter::Factory> factory);

  // It is not always feasible to check in unit tests that uploads do not occur
  // at a certain time, because that's (sometimes) racy with the event that
  // suppresses the upload. We therefore allow unit tests to glimpse into the
  // black box and verify that the box is aware that it should not upload.
  void UploadConditionsHoldForTesting(base::OnceCallback<void(bool)> callback);

  // This allows unit tests that do not wish to change the task runner to still
  // check when certain operations are finished.
  // TODO(crbug.com/40545136): Remove this and use PostNullTaskForTesting
  // instead.
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunnerForTesting();

  void PostNullTaskForTesting(base::OnceClosure reply);

  // Documented in WebRtcRemoteEventLogManager.
  void ShutDownForTesting(base::OnceClosure reply);

  static WebRtcEventLogManager* g_webrtc_event_log_manager;

  // The main logic will run sequentially on this runner, on which blocking
  // tasks are allowed.
  scoped_refptr<base::UpdateableSequencedTaskRunner> task_runner_;

  // The number of user-blocking tasks.
  // The priority of |task_runner_| is increased to USER_BLOCKING when this is
  // non-zero, and reduced to BEST_EFFORT when zero.
  // This object is only to be accessed on the UI thread.
  size_t num_user_blocking_tasks_;

  // Indicates whether remote-bound logging is generally allowed, although
  // possibly not for all profiles. This makes it possible for remote-bound to
  // be disabled through Finch.
  // TODO(crbug.com/40545136): Remove this kill-switch.
  const bool remote_logging_feature_enabled_;

  // Observer which will be informed whenever a local log file is started or
  // stopped. Its callbacks are called synchronously from |task_runner_|,
  // so the observer needs to be able to either run from any (sequenced) runner.
  raw_ptr<WebRtcLocalEventLogsObserver> local_logs_observer_;

  // Observer which will be informed whenever a remote log file is started or
  // stopped. Its callbacks are called synchronously from |task_runner_|,
  // so the observer needs to be able to either run from any (sequenced) runner.
  raw_ptr<WebRtcRemoteEventLogsObserver> remote_logs_observer_;

  // Manages local-bound logs - logs stored on the local filesystem when
  // logging has been explicitly enabled by the user.
  WebRtcLocalEventLogManager local_logs_manager_;

  // Manages remote-bound logs - logs which will be sent to a remote server.
  // This is only possible when the appropriate Chrome policy is configured.
  WebRtcRemoteEventLogManager remote_logs_manager_;

  // Each loaded BrowserContext is mapped to a PrefChangeRegistrar, which keeps
  // us informed about preference changes, thereby allowing as to support
  // dynamic refresh.
  std::map<BrowserContextId, PrefChangeRegistrar> pref_change_registrars_;

  // This keeps track of which peer connections have event logging turned on
  // in WebRTC, and for which client(s).
  std::map<PeerConnectionKey, LoggingTargetBitmap>
      peer_connections_with_event_logging_enabled_in_webrtc_;

  // The set of RenderProcessHosts with which the manager is registered for
  // observation. Allows us to register for each RPH only once, and get notified
  // when it exits (cleanly or due to a crash).
  // This object is only to be accessed on the UI thread.
  base::flat_set<raw_ptr<content::RenderProcessHost, CtnExperimental>>
      observed_render_process_hosts_;

  // In production, this holds a small object that just tells WebRTC (via
  // PeerConnectionTracker) to start/stop producing event logs for a specific
  // peer connection. In (relevant) unit tests, a mock will be injected.
  std::unique_ptr<PeerConnectionTrackerProxy> pc_tracker_proxy_;

  // The globals network_connection_tracker() and system_request_context() are
  // sent down to |remote_logs_manager_| with the first enabled browser context.
  // This member must only be accessed on the UI thread.
  bool first_browser_context_initializations_done_;

  // May only be set for tests, in which case, it will be passed to
  // |remote_logs_manager_| when (and if) produced.
  std::unique_ptr<LogFileWriter::Factory>
      remote_log_file_writer_factory_for_testing_;
};

}  // namespace webrtc_event_logging

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_H_
