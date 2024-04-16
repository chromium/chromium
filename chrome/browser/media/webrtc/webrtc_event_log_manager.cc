// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"

#include <limits>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

namespace webrtc_event_logging {

namespace {

using content::BrowserContext;
using content::BrowserThread;
using content::RenderFrameHost;
using content::RenderProcessHost;

using BrowserContextId = WebRtcEventLogManager::BrowserContextId;

class PeerConnectionTrackerProxyImpl
    : public WebRtcEventLogManager::PeerConnectionTrackerProxy {
 public:
  ~PeerConnectionTrackerProxyImpl() override = default;

  void EnableWebRtcEventLogging(const WebRtcEventLogPeerConnectionKey& key,
                                int output_period_ms) override {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PeerConnectionTrackerProxyImpl::EnableWebRtcEventLoggingInternal,
            key, output_period_ms));
  }

  void DisableWebRtcEventLogging(
      const WebRtcEventLogPeerConnectionKey& key) override {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PeerConnectionTrackerProxyImpl::DisableWebRtcEventLoggingInternal,
            key));
  }

 private:
  static void EnableWebRtcEventLoggingInternal(
      WebRtcEventLogPeerConnectionKey key,
      int output_period_ms) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto* host =
        RenderFrameHost::FromID(key.render_process_id, key.render_frame_id);
    if (!host) {
      return;  // The host has been asynchronously removed; not a problem.
    }
    host->EnableWebRtcEventLogOutput(key.lid, output_period_ms);
  }

  static void DisableWebRtcEventLoggingInternal(
      WebRtcEventLogPeerConnectionKey key) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto* host =
        RenderFrameHost::FromID(key.render_process_id, key.render_frame_id);
    if (!host) {
      return;  // The host has been asynchronously removed; not a problem.
    }
    host->DisableWebRtcEventLogOutput(key.lid);
  }
};

// Check whether remote-bound logging is generally allowed, although not
// necessarily for any given user profile.
// 1. Certain platforms (mobile) are blocked from remote-bound logging.
// 2. There is a Finch-controlled kill-switch for the feature.
bool IsRemoteLoggingFeatureEnabled() {
#if BUILDFLAG(IS_ANDROID)
  bool enabled = false;
#else
  bool enabled = base::FeatureList::IsEnabled(features::kWebRtcRemoteEventLog);
#endif

  VLOG(1) << "WebRTC remote-bound event logging "
          << (enabled ? "enabled" : "disabled") << ".";

  return enabled;
}

BrowserContext* GetBrowserContext(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* const host = RenderProcessHost::FromID(render_process_id);
  return host ? host->GetBrowserContext() : nullptr;
}

// Post reply back if non-empty.
template <typename... Args>
inline void MaybeReply(const base::Location& location,
                       base::OnceCallback<void(Args...)> reply,
                       Args... args) {
  if (reply) {
    content::GetUIThreadTaskRunner({})->PostTask(
        location, base::BindOnce(std::move(reply), args...));
  }
}

}  // namespace

WebRtcEventLogManager* WebRtcEventLogManager::g_webrtc_event_log_manager =
    nullptr;

std::unique_ptr<WebRtcEventLogManager>
WebRtcEventLogManager::CreateSingletonInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!g_webrtc_event_log_manager);
  g_webrtc_event_log_manager = new WebRtcEventLogManager;
  return base::WrapUnique<WebRtcEventLogManager>(g_webrtc_event_log_manager);
}

WebRtcEventLogManager* WebRtcEventLogManager::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_webrtc_event_log_manager;
}

base::FilePath WebRtcEventLogManager::GetRemoteBoundWebRtcEventLogsDir(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);
  // Incognito BrowserContext will return their parent profile's directory.
  return webrtc_event_logging::GetRemoteBoundWebRtcEventLogsDir(
      browser_context->GetPath());
}

WebRtcEventLogManager::WebRtcEventLogManager()
    : task_runner_(base::ThreadPool::CreateUpdateableSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::ThreadPolicy::PREFER_BACKGROUND,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      num_user_blocking_tasks_(0),
      remote_logging_feature_enabled_(IsRemoteLoggingFeatureEnabled()),
      local_logs_observer_(nullptr),
      remote_logs_observer_(nullptr),
      local_logs_manager_(this),
      remote_logs_manager_(this, task_runner_),
      pc_tracker_proxy_(new PeerConnectionTrackerProxyImpl),
      first_browser_context_initializations_done_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!g_webrtc_event_log_manager);
  g_webrtc_event_log_manager = this;
}

WebRtcEventLogManager::~WebRtcEventLogManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (RenderProcessHost* host : observed_render_process_hosts_) {
    host->RemoveObserver(this);
  }

  DCHECK(g_webrtc_event_log_manager);
  g_webrtc_event_log_manager = nullptr;
}

void WebRtcEventLogManager::EnableForBrowserContext(
    BrowserContext* browser_context,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);
  CHECK(!browser_context->IsOffTheRecord());

  if (!first_browser_context_initializations_done_) {
    OnFirstBrowserContextLoaded();
    first_browser_context_initializations_done_ = true;
  }

  StartListeningForPrefChangeForBrowserContext(browser_context);

  if (!IsRemoteLoggingAllowedForBrowserContext(browser_context)) {
    // If remote-bound logging was enabled during a previous Chrome session,
    // it might have produced some pending log files, which we will now
    // wish to remove.
    // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
    // will not be dereferenced after destruction.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &WebRtcEventLogManager::
                RemovePendingRemoteBoundLogsForNotEnabledBrowserContext,
            base::Unretained(this), GetBrowserContextId(browser_context),
            browser_context->GetPath(), std::move(reply)));
    return;
  }

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::EnableRemoteBoundLoggingForBrowserContext,
          base::Unretained(this), GetBrowserContextId(browser_context),
          browser_context->GetPath(), std::move(reply)));
}

void WebRtcEventLogManager::DisableForBrowserContext(
    content::BrowserContext* browser_context,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);

  StopListeningForPrefChangeForBrowserContext(browser_context);

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::DisableRemoteBoundLoggingForBrowserContext,
          base::Unretained(this), GetBrowserContextId(browser_context),
          std::move(reply)));
}

void WebRtcEventLogManager::OnPeerConnectionAdded(
    content::GlobalRenderFrameHostId frame_id,
    int lid,
    base::ProcessId pid,
    const std::string& url,
    const std::string& rtc_configuration) {
  OnPeerConnectionAdded(frame_id, lid, base::NullCallback());
}

void WebRtcEventLogManager::OnPeerConnectionRemoved(
    content::GlobalRenderFrameHostId frame_id,
    int lid) {
  OnPeerConnectionRemoved(frame_id, lid, base::NullCallback());
}

void WebRtcEventLogManager::OnPeerConnectionUpdated(
    content::GlobalRenderFrameHostId frame_id,
    int lid,
    const std::string& type,
    const std::string& value) {
  // TODO(crbug.com/40562061): Get rid of magic value.
  if (type == "stop") {
    OnPeerConnectionStopped(frame_id, lid, base::NullCallback());
  }
}

void WebRtcEventLogManager::OnPeerConnectionSessionIdSet(
    content::GlobalRenderFrameHostId frame_id,
    int lid,
    const std::string& session_id) {
  OnPeerConnectionSessionIdSet(frame_id, lid, session_id, base::NullCallback());
}

void WebRtcEventLogManager::OnWebRtcEventLogWrite(
    content::GlobalRenderFrameHostId frame_id,
    int lid,
    const std::string& message) {
  OnWebRtcEventLogWrite(frame_id, lid, message, base::NullCallback());
}

void WebRtcEventLogManager::EnableLocalLogging(
    const base::FilePath& base_path) {
  EnableLocalLogging(base_path, base::NullCallback());
}

void WebRtcEventLogManager::DisableLocalLogging() {
  DisableLocalLogging(base::NullCallback());
}

void WebRtcEventLogManager::StartRemoteLogging(
    int render_process_id,
    const std::string& session_id,
    size_t max_file_size_bytes,
    int output_period_ms,
    size_t web_app_id,
    base::OnceCallback<void(bool, const std::string&, const std::string&)>
        reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(reply);

  BrowserContext* browser_context = GetBrowserContext(render_process_id);
  const char* error = nullptr;

  if (!browser_context) {
    // RPH died before processing of this notification.
    UmaRecordWebRtcEventLoggingApi(WebRtcEventLoggingApiUma::kDeadRph);
    error = kStartRemoteLoggingFailureDeadRenderProcessHost;
  } else if (!IsRemoteLoggingAllowedForBrowserContext(browser_context)) {
    UmaRecordWebRtcEventLoggingApi(WebRtcEventLoggingApiUma::kFeatureDisabled);
    error = kStartRemoteLoggingFailureFeatureDisabled;
  } else if (browser_context->IsOffTheRecord()) {
    // Feature disable in incognito. Since the feature can be disabled for
    // non-incognito sessions, this should not expose incognito mode.
    UmaRecordWebRtcEventLoggingApi(WebRtcEventLoggingApiUma::kIncognito);
    error = kStartRemoteLoggingFailureFeatureDisabled;
  }

  if (error) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(reply), false, std::string(),
                                  std::string(error)));
    return;
  }

  const auto browser_context_id = GetBrowserContextId(browser_context);
  DCHECK_NE(browser_context_id, kNullBrowserContextId);

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::StartRemoteLoggingInternal,
                     base::Unretained(this), render_process_id,
                     browser_context_id, session_id, browser_context->GetPath(),
                     max_file_size_bytes, output_period_ms, web_app_id,
                     std::move(reply)));
}

void WebRtcEventLogManager::ClearCacheForBrowserContext(
    const BrowserContext* browser_context,
    const base::Time& delete_begin,
    const base::Time& delete_end,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto browser_context_id = GetBrowserContextId(browser_context);
  DCHECK_NE(browser_context_id, kNullBrowserContextId);

  DCHECK_LT(num_user_blocking_tasks_, std::numeric_limits<size_t>::max());
  if (++num_user_blocking_tasks_ == 1) {
    task_runner_->UpdatePriority(base::TaskPriority::USER_BLOCKING);
  }

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::ClearCacheForBrowserContextInternal,
          base::Unretained(this), browser_context_id, delete_begin, delete_end),
      base::BindOnce(
          &WebRtcEventLogManager::OnClearCacheForBrowserContextDoneInternal,
          base::Unretained(this), std::move(reply)));
}

void WebRtcEventLogManager::GetHistory(
    BrowserContextId browser_context_id,
    base::OnceCallback<void(const std::vector<UploadList::UploadInfo>&)>
        reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(reply);

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcEventLogManager::GetHistoryInternal,
                                base::Unretained(this), browser_context_id,
                                std::move(reply)));
}

void WebRtcEventLogManager::SetLocalLogsObserver(
    WebRtcLocalEventLogsObserver* observer,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::SetLocalLogsObserverInternal,
                     base::Unretained(this), observer, std::move(reply)));
}

void WebRtcEventLogManager::SetRemoteLogsObserver(
    WebRtcRemoteEventLogsObserver* observer,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::SetRemoteLogsObserverInternal,
                     base::Unretained(this), observer, std::move(reply)));
}

bool WebRtcEventLogManager::IsRemoteLoggingAllowedForBrowserContext(
    BrowserContext* browser_context) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);

  if (!remote_logging_feature_enabled_) {
    return false;
  }

  const Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  const PrefService::Preference* webrtc_event_log_collection_allowed_pref =
      profile->GetPrefs()->FindPreference(
          prefs::kWebRtcEventLogCollectionAllowed);
  DCHECK(webrtc_event_log_collection_allowed_pref);

  if (webrtc_event_log_collection_allowed_pref->IsDefaultValue()) {
    // The pref has not been set. GetBoolean would only return the default
    // value. However, there is no single default value,
    // because it depends on whether the profile receives cloud-based
    // enterprise policies.
    return DoesProfileDefaultToLoggingEnabled(profile);
  }

  // There is a non-default value set, so this value is authoritative.
  return profile->GetPrefs()->GetBoolean(
      prefs::kWebRtcEventLogCollectionAllowed);
}

std::unique_ptr<LogFileWriter::Factory>
WebRtcEventLogManager::CreateRemoteLogFileWriterFactory() {
  if (remote_log_file_writer_factory_for_testing_) {
    return std::move(remote_log_file_writer_factory_for_testing_);
#if !BUILDFLAG(IS_ANDROID)
  } else if (base::FeatureList::IsEnabled(
                 features::kWebRtcRemoteEventLogGzipped)) {
    return std::make_unique<GzippedLogFileWriterFactory>(
        std::make_unique<GzipLogCompressorFactory>(
            std::make_unique<DefaultGzippedSizeEstimator::Factory>()));
#endif
  } else {
    return std::make_unique<BaseLogFileWriterFactory>();
  }
}

void WebRtcEventLogManager::RenderProcessExited(
    RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHostExitedDestroyed(host);
}

void WebRtcEventLogManager::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHostExitedDestroyed(host);
}

void WebRtcEventLogManager::RenderProcessHostExitedDestroyed(
    RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(host);

  auto it = observed_render_process_hosts_.find(host);
  if (it == observed_render_process_hosts_.end()) {
    return;  // We've never seen PeerConnections associated with this RPH.
  }
  host->RemoveObserver(this);
  observed_render_process_hosts_.erase(host);

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::RenderProcessExitedInternal,
                     base::Unretained(this), host->GetID()));
}

void WebRtcEventLogManager::OnPeerConnectionAdded(
    content::GlobalRenderFrameHostId frame_id,
    int lid,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/40169214): Should this look at RFH shutdown instead of RPH?
  RenderProcessHost* rph = RenderProcessHost::FromID(frame_id.child_id);
  if (!rph) {
    // RPH died before processing of this notification.
    MaybeReply(FROM_HERE, std::move(reply), false);
    return;
  }

  auto it = observed_render_process_hosts_.find(rph);
  if (it == observed_render_process_hosts_.end()) {
    // This is the first PeerConnection which we see that's associated
    // with this RPH.
    rph->AddObserver(this);
    observed_render_process_hosts_.insert(rph);
  }

  const auto browser_context_id = GetBrowserContextId(rph->GetBrowserContext());
  DCHECK_NE(browser_context_id, kNullBrowserContextId);

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::OnPeerConnectionAddedInternal,
          base::Unretained(this),
          PeerConnectionKey(frame_id.child_id, lid, browser_context_id,
                            frame_id.frame_routing_id),
          std::move(reply)));
}

void WebRtcEventLogManager::OnPeerConnectionRemoved(
    content::GlobalRenderFrameHostId frame_id,
    int lid,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto browser_context_id = GetBrowserContextId(frame_id.child_id);
  if (browser_context_id == kNullBrowserContextId) {
    // RPH died before processing of this notification. This is handled by
    // RenderProcessExited() / RenderProcessHostDestroyed.
    MaybeReply(FROM_HERE, std::move(reply), false);
    return;
  }

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::OnPeerConnectionRemovedInternal,
          base::Unretained(this),
          PeerConnectionKey(frame_id.child_id, lid, browser_context_id,
                            frame_id.frame_routing_id),
          std::move(reply)));
}

void WebRtcEventLogManager::OnPeerConnectionStopped(
    content::GlobalRenderFrameHostId frame_id,
    int lid,
    base::OnceCallback<void(bool)> reply) {
  // From the logger's perspective, we treat stopping a peer connection the
  // same as we do its removal. Should a stopped peer connection be later
  // removed, the removal callback will assume the value |false|.
  OnPeerConnectionRemoved(frame_id, lid, std::move(reply));
}

void WebRtcEventLogManager::OnPeerConnectionSessionIdSet(
    content::GlobalRenderFrameHostId frame_id,
    int lid,
    const std::string& session_id,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto browser_context_id = GetBrowserContextId(frame_id.child_id);
  if (browser_context_id == kNullBrowserContextId) {
    // RPH died before processing of this notification. This is handled by
    // RenderProcessExited() / RenderProcessHostDestroyed.
    MaybeReply(FROM_HERE, std::move(reply), false);
    return;
  }

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::OnPeerConnectionSessionIdSetInternal,
          base::Unretained(this),
          PeerConnectionKey(frame_id.child_id, lid, browser_context_id,
                            frame_id.frame_routing_id),
          session_id, std::move(reply)));
}

void WebRtcEventLogManager::OnWebRtcEventLogWrite(
    content::GlobalRenderFrameHostId frame_id,
    int lid,
    const std::string& message,
    base::OnceCallback<void(std::pair<bool, bool>)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const BrowserContext* browser_context = GetBrowserContext(frame_id.child_id);
  if (!browser_context) {
    // RPH died before processing of this notification.
    MaybeReply(FROM_HERE, std::move(reply), std::make_pair(false, false));
    return;
  }

  const auto browser_context_id = GetBrowserContextId(browser_context);
  DCHECK_NE(browser_context_id, kNullBrowserContextId);

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::OnWebRtcEventLogWriteInternal,
          base::Unretained(this),
          PeerConnectionKey(frame_id.child_id, lid, browser_context_id,
                            frame_id.frame_routing_id),
          message, std::move(reply)));
}

void WebRtcEventLogManager::EnableLocalLogging(
    const base::FilePath& base_path,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  EnableLocalLogging(base_path, kDefaultMaxLocalLogFileSizeBytes,
                     std::move(reply));
}

void WebRtcEventLogManager::EnableLocalLogging(
    const base::FilePath& base_path,
    size_t max_file_size_bytes,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!base_path.empty());
  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::EnableLocalLoggingInternal,
                     base::Unretained(this), base_path, max_file_size_bytes,
                     std::move(reply)));
}

void WebRtcEventLogManager::DisableLocalLogging(
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::DisableLocalLoggingInternal,
                     base::Unretained(this), std::move(reply)));
}

void WebRtcEventLogManager::OnLocalLogStarted(PeerConnectionKey peer_connection,
                                              const base::FilePath& file_path) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  OnLoggingTargetStarted(LoggingTarget::kLocalLogging, peer_connection,
                         /*output_period_ms=*/5000);

  if (local_logs_observer_) {
    local_logs_observer_->OnLocalLogStarted(peer_connection, file_path);
  }
}

void WebRtcEventLogManager::OnLocalLogStopped(
    PeerConnectionKey peer_connection) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  OnLoggingTargetStopped(LoggingTarget::kLocalLogging, peer_connection);

  if (local_logs_observer_) {
    local_logs_observer_->OnLocalLogStopped(peer_connection);
  }
}

void WebRtcEventLogManager::OnRemoteLogStarted(PeerConnectionKey key,
                                               const base::FilePath& file_path,
                                               int output_period_ms) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  OnLoggingTargetStarted(LoggingTarget::kRemoteLogging, key, output_period_ms);
  if (remote_logs_observer_) {
    remote_logs_observer_->OnRemoteLogStarted(key, file_path, output_period_ms);
  }
}

void WebRtcEventLogManager::OnRemoteLogStopped(
    WebRtcEventLogPeerConnectionKey key) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  OnLoggingTargetStopped(LoggingTarget::kRemoteLogging, key);
  if (remote_logs_observer_) {
    remote_logs_observer_->OnRemoteLogStopped(key);
  }
}

void WebRtcEventLogManager::OnLoggingTargetStarted(LoggingTarget target,
                                                   PeerConnectionKey key,
                                                   int output_period_ms) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto it = peer_connections_with_event_logging_enabled_in_webrtc_.find(key);
  if (it != peer_connections_with_event_logging_enabled_in_webrtc_.end()) {
    DCHECK_EQ((it->second & target), 0u);
    it->second |= target;
  } else {
    // This is the first client for WebRTC event logging - let WebRTC know
    // that it should start informing us of events.
    peer_connections_with_event_logging_enabled_in_webrtc_.emplace(key, target);
    pc_tracker_proxy_->EnableWebRtcEventLogging(key, output_period_ms);
  }
}

void WebRtcEventLogManager::OnLoggingTargetStopped(LoggingTarget target,
                                                   PeerConnectionKey key) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Record that we're no longer performing this type of logging for this PC.
  auto it = peer_connections_with_event_logging_enabled_in_webrtc_.find(key);
  CHECK(it != peer_connections_with_event_logging_enabled_in_webrtc_.end());
  DCHECK_NE(it->second, 0u);
  it->second &= ~target;

  // If we're not doing any other type of logging for this peer connection,
  // it's time to stop receiving notifications for it from WebRTC.
  if (it->second == 0u) {
    peer_connections_with_event_logging_enabled_in_webrtc_.erase(it);
    pc_tracker_proxy_->DisableWebRtcEventLogging(key);
  }
}

void WebRtcEventLogManager::StartListeningForPrefChangeForBrowserContext(
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(first_browser_context_initializations_done_);
  CHECK(!browser_context->IsOffTheRecord());

  const auto browser_context_id = GetBrowserContextId(browser_context);
  auto it = pref_change_registrars_.emplace(std::piecewise_construct,
                                            std::make_tuple(browser_context_id),
                                            std::make_tuple());
  DCHECK(it.second) << "Already listening.";
  PrefChangeRegistrar& registrar = it.first->second;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);
  registrar.Init(profile->GetPrefs());

  // * |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  //   will not be dereferenced after destruction.
  // * base::Unretained(browser_context) is safe, because |browser_context|
  //   stays alive until Chrome shut-down, at which point we'll stop listening
  //   as part of its (BrowserContext's) tear-down process.
  registrar.Add(prefs::kWebRtcEventLogCollectionAllowed,
                base::BindRepeating(&WebRtcEventLogManager::OnPrefChange,
                                    base::Unretained(this),
                                    base::Unretained(browser_context)));
}

void WebRtcEventLogManager::StopListeningForPrefChangeForBrowserContext(
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto browser_context_id = GetBrowserContextId(browser_context);

  size_t erased_count = pref_change_registrars_.erase(browser_context_id);
  DCHECK_EQ(erased_count, 1u);
}

void WebRtcEventLogManager::OnPrefChange(BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(first_browser_context_initializations_done_);

  const Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  const bool enabled = IsRemoteLoggingAllowedForBrowserContext(browser_context);

  if (!enabled) {
    // Dynamic refresh of the policy to DISABLED; stop ongoing logs, remove
    // pending log files and stop any active uploads.
    ClearCacheForBrowserContext(browser_context, base::Time::Min(),
                                base::Time::Max(), base::DoNothing());
  }

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  base::OnceClosure task;
  if (enabled) {
    task = base::BindOnce(
        &WebRtcEventLogManager::EnableRemoteBoundLoggingForBrowserContext,
        base::Unretained(this), GetBrowserContextId(browser_context),
        browser_context->GetPath(), base::OnceClosure());
  } else {
    task = base::BindOnce(
        &WebRtcEventLogManager::DisableRemoteBoundLoggingForBrowserContext,
        base::Unretained(this), GetBrowserContextId(browser_context),
        base::OnceClosure());
  }

  task_runner_->PostTask(FROM_HERE, std::move(task));
}

void WebRtcEventLogManager::OnFirstBrowserContextLoaded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  network::NetworkConnectionTracker* network_connection_tracker =
      content::GetNetworkConnectionTracker();
  DCHECK(network_connection_tracker);

  auto log_file_writer_factory = CreateRemoteLogFileWriterFactory();
  DCHECK(log_file_writer_factory);

  // |network_connection_tracker| is owned by BrowserProcessImpl, which owns
  // the IOThread. The internal task runner on which |this| uses
  // |network_connection_tracker|, stops before IOThread dies, so we can trust
  // that |network_connection_tracker| will not be used after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::OnFirstBrowserContextLoadedInternal,
          base::Unretained(this), base::Unretained(network_connection_tracker),
          std::move(log_file_writer_factory)));
}

void WebRtcEventLogManager::OnFirstBrowserContextLoadedInternal(
    network::NetworkConnectionTracker* network_connection_tracker,
    std::unique_ptr<LogFileWriter::Factory> log_file_writer_factory) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(network_connection_tracker);
  DCHECK(log_file_writer_factory);
  remote_logs_manager_.SetNetworkConnectionTracker(network_connection_tracker);
  remote_logs_manager_.SetLogFileWriterFactory(
      std::move(log_file_writer_factory));
}

void WebRtcEventLogManager::EnableRemoteBoundLoggingForBrowserContext(
    BrowserContextId browser_context_id,
    const base::FilePath& browser_context_dir,
    base::OnceClosure reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(browser_context_id, kNullBrowserContextId);

  remote_logs_manager_.EnableForBrowserContext(browser_context_id,
                                               browser_context_dir);

  MaybeReply(FROM_HERE, std::move(reply));
}

void WebRtcEventLogManager::DisableRemoteBoundLoggingForBrowserContext(
    BrowserContextId browser_context_id,
    base::OnceClosure reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Note that the BrowserContext might never have been enabled in the
  // remote-bound manager; that's not a problem.
  remote_logs_manager_.DisableForBrowserContext(browser_context_id);

  MaybeReply(FROM_HERE, std::move(reply));
}

void WebRtcEventLogManager::
    RemovePendingRemoteBoundLogsForNotEnabledBrowserContext(
        BrowserContextId browser_context_id,
        const base::FilePath& browser_context_dir,
        base::OnceClosure reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  remote_logs_manager_.RemovePendingLogsForNotEnabledBrowserContext(
      browser_context_id, browser_context_dir);

  MaybeReply(FROM_HERE, std::move(reply));
}

void WebRtcEventLogManager::OnPeerConnectionAddedInternal(
    PeerConnectionKey key,
    base::OnceCallback<void(bool)> reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const bool local_result = local_logs_manager_.OnPeerConnectionAdded(key);
  const bool remote_result = remote_logs_manager_.OnPeerConnectionAdded(key);
  DCHECK_EQ(local_result, remote_result);

  MaybeReply(FROM_HERE, std::move(reply), local_result);
}

void WebRtcEventLogManager::OnPeerConnectionRemovedInternal(
    PeerConnectionKey key,
    base::OnceCallback<void(bool)> reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const bool local_result = local_logs_manager_.OnPeerConnectionRemoved(key);
  const bool remote_result = remote_logs_manager_.OnPeerConnectionRemoved(key);
  DCHECK_EQ(local_result, remote_result);

  MaybeReply(FROM_HERE, std::move(reply), local_result);
}

void WebRtcEventLogManager::OnPeerConnectionSessionIdSetInternal(
    PeerConnectionKey key,
    const std::string& session_id,
    base::OnceCallback<void(bool)> reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  const bool result =
      remote_logs_manager_.OnPeerConnectionSessionIdSet(key, session_id);
  MaybeReply(FROM_HERE, std::move(reply), result);
}

void WebRtcEventLogManager::OnWebRtcEventLogWriteInternal(
    PeerConnectionKey key,
    const std::string& message,
    base::OnceCallback<void(std::pair<bool, bool>)> reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const bool local_result = local_logs_manager_.EventLogWrite(key, message);
  const bool remote_result = remote_logs_manager_.EventLogWrite(key, message);

  MaybeReply(FROM_HERE, std::move(reply),
             std::make_pair(local_result, remote_result));
}

void WebRtcEventLogManager::EnableLocalLoggingInternal(
    const base::FilePath& base_path,
    size_t max_file_size_bytes,
    base::OnceCallback<void(bool)> reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const bool result =
      local_logs_manager_.EnableLogging(base_path, max_file_size_bytes);

  MaybeReply(FROM_HERE, std::move(reply), result);
}

void WebRtcEventLogManager::DisableLocalLoggingInternal(
    base::OnceCallback<void(bool)> reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const bool result = local_logs_manager_.DisableLogging();

  MaybeReply(FROM_HERE, std::move(reply), result);
}

void WebRtcEventLogManager::StartRemoteLoggingInternal(
    int render_process_id,
    BrowserContextId browser_context_id,
    const std::string& session_id,
    const base::FilePath& browser_context_dir,
    size_t max_file_size_bytes,
    int output_period_ms,
    size_t web_app_id,
    base::OnceCallback<void(bool, const std::string&, const std::string&)>
        reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  std::string log_id;
  std::string error_message;
  const bool result = remote_logs_manager_.StartRemoteLogging(
      render_process_id, browser_context_id, session_id, browser_context_dir,
      max_file_size_bytes, output_period_ms, web_app_id, &log_id,
      &error_message);

  // |log_id| set only if successful; |error_message| set only if unsuccessful.
  DCHECK_EQ(result, !log_id.empty());
  DCHECK_EQ(!result, !error_message.empty());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(reply), result, log_id, error_message));
}

void WebRtcEventLogManager::ClearCacheForBrowserContextInternal(
    BrowserContextId browser_context_id,
    const base::Time& delete_begin,
    const base::Time& delete_end) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  remote_logs_manager_.ClearCacheForBrowserContext(browser_context_id,
                                                   delete_begin, delete_end);
}

void WebRtcEventLogManager::OnClearCacheForBrowserContextDoneInternal(
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_GT(num_user_blocking_tasks_, 0u);
  if (--num_user_blocking_tasks_ == 0) {
    task_runner_->UpdatePriority(base::TaskPriority::BEST_EFFORT);
  }
  std::move(reply).Run();
}

void WebRtcEventLogManager::GetHistoryInternal(
    BrowserContextId browser_context_id,
    base::OnceCallback<void(const std::vector<UploadList::UploadInfo>&)>
        reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(reply);
  remote_logs_manager_.GetHistory(browser_context_id, std::move(reply));
}

void WebRtcEventLogManager::RenderProcessExitedInternal(int render_process_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  local_logs_manager_.RenderProcessHostExitedDestroyed(render_process_id);
  remote_logs_manager_.RenderProcessHostExitedDestroyed(render_process_id);
}

void WebRtcEventLogManager::SetLocalLogsObserverInternal(
    WebRtcLocalEventLogsObserver* observer,
    base::OnceClosure reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  local_logs_observer_ = observer;

  if (reply) {
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(reply));
  }
}

void WebRtcEventLogManager::SetRemoteLogsObserverInternal(
    WebRtcRemoteEventLogsObserver* observer,
    base::OnceClosure reply) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  remote_logs_observer_ = observer;

  if (reply) {
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(reply));
  }
}

void WebRtcEventLogManager::SetClockForTesting(base::Clock* clock,
                                               base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(reply);

  auto task = [](WebRtcEventLogManager* manager, base::Clock* clock,
                 base::OnceClosure reply) {
    manager->local_logs_manager_.SetClockForTesting(clock);

    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(reply));
  };

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(FROM_HERE, base::BindOnce(task, base::Unretained(this),
                                                   clock, std::move(reply)));
}

void WebRtcEventLogManager::SetPeerConnectionTrackerProxyForTesting(
    std::unique_ptr<PeerConnectionTrackerProxy> pc_tracker_proxy,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(reply);

  auto task = [](WebRtcEventLogManager* manager,
                 std::unique_ptr<PeerConnectionTrackerProxy> pc_tracker_proxy,
                 base::OnceClosure reply) {
    manager->pc_tracker_proxy_ = std::move(pc_tracker_proxy);

    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(reply));
  };

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(task, base::Unretained(this),
                                std::move(pc_tracker_proxy), std::move(reply)));
}

void WebRtcEventLogManager::SetWebRtcEventLogUploaderFactoryForTesting(
    std::unique_ptr<WebRtcEventLogUploader::Factory> uploader_factory,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(reply);

  auto task =
      [](WebRtcEventLogManager* manager,
         std::unique_ptr<WebRtcEventLogUploader::Factory> uploader_factory,
         base::OnceClosure reply) {
        auto& remote_logs_manager = manager->remote_logs_manager_;
        remote_logs_manager.SetWebRtcEventLogUploaderFactoryForTesting(
            std::move(uploader_factory));

        content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                     std::move(reply));
      };

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(task, base::Unretained(this),
                                std::move(uploader_factory), std::move(reply)));
}

void WebRtcEventLogManager::SetRemoteLogFileWriterFactoryForTesting(
    std::unique_ptr<LogFileWriter::Factory> factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!first_browser_context_initializations_done_) << "Too late.";
  DCHECK(!remote_log_file_writer_factory_for_testing_) << "Already called.";
  remote_log_file_writer_factory_for_testing_ = std::move(factory);
}

void WebRtcEventLogManager::UploadConditionsHoldForTesting(
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Unit tests block until |callback| is sent back, so the use
  // of base::Unretained(&remote_logs_manager_) is safe.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcRemoteEventLogManager::UploadConditionsHoldForTesting,
          base::Unretained(&remote_logs_manager_), std::move(callback)));
}

scoped_refptr<base::SequencedTaskRunner>
WebRtcEventLogManager::GetTaskRunnerForTesting() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return task_runner_;
}

void WebRtcEventLogManager::PostNullTaskForTesting(base::OnceClosure reply) {
  task_runner_->PostTask(FROM_HERE, std::move(reply));
}

void WebRtcEventLogManager::ShutDownForTesting(base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Unit tests block until |callback| is sent back, so the use
  // of base::Unretained(&remote_logs_manager_) is safe.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcRemoteEventLogManager::ShutDownForTesting,
                     base::Unretained(&remote_logs_manager_),
                     std::move(reply)));
}

}  // namespace webrtc_event_logging
