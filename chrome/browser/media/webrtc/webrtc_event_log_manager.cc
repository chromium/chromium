// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"

#include <limits>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/rtc_diagnostic_logging_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/webrtc_logging/browser/text_log_list.h"
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

  void EnableWebRtcDataChannelLogging(
      const WebRtcEventLogPeerConnectionKey& key) override {
    auto enable_logging = [](const WebRtcEventLogPeerConnectionKey& key) {
      if (auto* host = RenderFrameHost::FromID(key.render_process_id,
                                               key.render_frame_id)) {
        host->EnableWebRtcDataChannelLogOutput(key.lid);
      }
    };

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(enable_logging, key));
  }

  void DisableWebRtcDataChannelLogging(
      const WebRtcEventLogPeerConnectionKey& key) override {
    auto disable_logging = [](const WebRtcEventLogPeerConnectionKey& key) {
      if (auto* host = RenderFrameHost::FromID(key.render_process_id,
                                               key.render_frame_id)) {
        host->DisableWebRtcDataChannelLogOutput(key.lid);
      }
    };
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(disable_logging, key));
  }

 private:
  static void EnableWebRtcEventLoggingInternal(
      WebRtcEventLogPeerConnectionKey key,
      int output_period_ms) {
    CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
    auto* host =
        RenderFrameHost::FromID(key.render_process_id, key.render_frame_id);
    if (!host) {
      return;  // The host has been asynchronously removed; not a problem.
    }
    host->EnableWebRtcEventLogOutput(key.lid, output_period_ms);
  }

  static void DisableWebRtcEventLoggingInternal(
      WebRtcEventLogPeerConnectionKey key) {
    CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
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
// Certain platforms (mobile) are blocked from remote-bound logging.
bool IsRemoteLoggingFeatureEnabled() {
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
  bool enabled = false;
#else
  bool enabled = true;
#endif

  VLOG(1) << "WebRTC remote-bound event logging "
          << (enabled ? "enabled" : "disabled") << ".";

  return enabled;
}

BrowserContext* GetBrowserContext(content::ChildProcessId render_process_id) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(!g_webrtc_event_log_manager, base::NotFatalUntil::M161);
  g_webrtc_event_log_manager = new WebRtcEventLogManager;
  return base::WrapUnique<WebRtcEventLogManager>(g_webrtc_event_log_manager);
}

WebRtcEventLogManager* WebRtcEventLogManager::GetInstance() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  return g_webrtc_event_log_manager;
}

base::FilePath WebRtcEventLogManager::GetRemoteBoundWebRtcEventLogsDir(
    content::BrowserContext* browser_context) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(browser_context, base::NotFatalUntil::M161);
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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(!g_webrtc_event_log_manager, base::NotFatalUntil::M161);
  g_webrtc_event_log_manager = this;
}

WebRtcEventLogManager::~WebRtcEventLogManager() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  for (RenderProcessHost* host : observed_render_process_hosts_) {
    host->RemoveObserver(this);
  }

  CHECK(g_webrtc_event_log_manager, base::NotFatalUntil::M161);
  g_webrtc_event_log_manager = nullptr;
}

void WebRtcEventLogManager::EnableForBrowserContext(
    BrowserContext* browser_context,
    base::OnceClosure reply) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(browser_context, base::NotFatalUntil::M161);
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
        base::BindOnce(&WebRtcEventLogManager::
                           RemoveRemoteBoundLogsForNotEnabledBrowserContext,
                       base::Unretained(this),
                       GetBrowserContextId(browser_context),
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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(browser_context, base::NotFatalUntil::M161);

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
  if (type == "stop") {
    OnPeerConnectionStopped(frame_id, lid, base::NullCallback());
  }
}

void WebRtcEventLogManager::OnPeerConnectionSessionIdSet(
    content::GlobalRenderFrameHostId frame_id,
    int lid,
    const std::string& session_id,
    base::OnceClosure reply) {
  auto custom_callback = base::BindOnce(
      [](content::GlobalRenderFrameHostId frame_id, std::string session_id,
         base::OnceClosure original_reply, bool success) {
        if (auto* rfh = content::RenderFrameHost::FromID(frame_id);
            success && rfh) {
          rtc_diagnostic_logging::StartRtcPeerConnectionEventDiagnosticLogging(
              *rfh, session_id, std::move(original_reply));
        } else {
          std::move(original_reply).Run();
        }
      },
      frame_id, session_id, std::move(reply));

  OnSessionIdSetForPeerConnection(frame_id, lid, session_id,
                                  std::move(custom_callback));
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
    std::optional<std::string> diagnostic_uuid,
    bool local_only,
    base::OnceCallback<void(bool, const std::string&, const std::string&)>
        reply) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(reply, base::NotFatalUntil::M161);

  // TODO(crbug.com/379869738) Remove FromUnsafeValue.
  BrowserContext* browser_context = GetBrowserContext(
      content::ChildProcessId::FromUnsafeValue(render_process_id));
  const char* error = nullptr;

  if (!browser_context) {
    UmaRecordWebRtcEventLoggingApi(
        WebRtcEventLoggingApiUma::kBrowserContextNotFound);
    error = kBrowserContextNotFound;
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
  CHECK_NE(browser_context_id, kNullBrowserContextId,
           base::NotFatalUntil::M161);

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::StartRemoteLoggingInternal,
                     base::Unretained(this), render_process_id,
                     browser_context_id, session_id, browser_context->GetPath(),
                     max_file_size_bytes, output_period_ms, web_app_id,
                     std::move(diagnostic_uuid), local_only, std::move(reply)));
}

void WebRtcEventLogManager::FinishLogging(int render_process_id,
                                          base::OnceClosure callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcEventLogManager::StopLoggingInternal,
                                base::Unretained(this), render_process_id,
                                StopLoggingAction::kStore, std::nullopt,
                                std::move(callback)));
}

void WebRtcEventLogManager::CancelLogging(int render_process_id,
                                          const std::string& diagnostic_uuid,
                                          base::OnceClosure callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::StopLoggingInternal,
                     base::Unretained(this), render_process_id,
                     StopLoggingAction::kDelete,
                     std::make_optional(diagnostic_uuid), std::move(callback)));
}

void WebRtcEventLogManager::EnableDataChannelLogging(
    const base::FilePath& base_path) {
  EnableDataChannelLogging(base_path, kDefaultMaxLocalDataChannelFileSizeBytes,
                           base::NullCallback());
}

void WebRtcEventLogManager::DisableDataChannelLogging() {
  DisableDataChannelLogging(base::NullCallback());
}

void WebRtcEventLogManager::ClearCacheForBrowserContext(
    const BrowserContext* browser_context,
    const base::Time& delete_begin,
    const base::Time& delete_end,
    base::OnceClosure reply) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  const auto browser_context_id = GetBrowserContextId(browser_context);
  CHECK_NE(browser_context_id, kNullBrowserContextId,
           base::NotFatalUntil::M161);

  CHECK_LT(num_user_blocking_tasks_, std::numeric_limits<size_t>::max(),
           base::NotFatalUntil::M161);
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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(reply, base::NotFatalUntil::M161);

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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::SetRemoteLogsObserverInternal,
                     base::Unretained(this), observer, std::move(reply)));
}

bool WebRtcEventLogManager::IsRemoteLoggingAllowedForBrowserContext(
    BrowserContext* browser_context) const {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(browser_context, base::NotFatalUntil::M161);

  if (!remote_logging_feature_enabled_) {
    return false;
  }
  const Profile* profile = Profile::FromBrowserContext(browser_context);
  CHECK(profile, base::NotFatalUntil::M161);

  const PrefService::Preference* webrtc_event_log_collection_allowed_pref =
      profile->GetPrefs()->FindPreference(
          prefs::kWebRtcEventLogCollectionAllowed);
  CHECK(webrtc_event_log_collection_allowed_pref, base::NotFatalUntil::M161);

  if (webrtc_event_log_collection_allowed_pref->IsDefaultValue()) {
    // The pref has not been set. GetBoolean would only return the default
    // value. However, there is no single default value,
    // because it depends on whether the profile receives cloud-based
    // enterprise policies.
    // Return true if either Extension or Web API logging defaults to true
    // to signal that remote logging is enabled for this browser context.
    // Actual logging requests will always be checked for authorization
    // separately, based on the specific API and origin.
    return DoesProfileDefaultToLoggingEnabled(
               profile, webrtc_logging::ApiType::kExtension) ||
           DoesProfileDefaultToLoggingEnabled(profile,
                                              webrtc_logging::ApiType::kWeb);
  }

  // There is a non-default value set, so this value is authoritative.
  return profile->GetPrefs()->GetBoolean(
      prefs::kWebRtcEventLogCollectionAllowed);
}

std::unique_ptr<LogFileWriter::Factory>
WebRtcEventLogManager::CreateRemoteLogFileWriterFactory() {
  if (remote_log_file_writer_factory_for_testing_) {
    return std::move(remote_log_file_writer_factory_for_testing_);
  } else {
#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)
    return std::make_unique<GzippedLogFileWriterFactory>(
        std::make_unique<GzipLogCompressorFactory>(
            std::make_unique<DefaultGzippedSizeEstimator::Factory>()));
#else
    return std::make_unique<BaseLogFileWriterFactory>();
#endif
  }
}

void WebRtcEventLogManager::RenderProcessExited(
    RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  RenderProcessHostExitedDestroyed(host);
}

void WebRtcEventLogManager::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  RenderProcessHostExitedDestroyed(host);
}

void WebRtcEventLogManager::RenderProcessHostExitedDestroyed(
    RenderProcessHost* host) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(host, base::NotFatalUntil::M161);

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
                     base::Unretained(this), host->GetDeprecatedID()));
}

void WebRtcEventLogManager::OnPeerConnectionAdded(
    content::GlobalRenderFrameHostId frame_id,
    int lid,
    base::OnceCallback<void(bool)> reply) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

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
  CHECK_NE(browser_context_id, kNullBrowserContextId,
           base::NotFatalUntil::M161);

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  // TODO(crbug.com/379869738) Remove GetUnsafeValue.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::OnPeerConnectionAddedInternal,
          base::Unretained(this),
          PeerConnectionKey(frame_id.child_id.GetUnsafeValue(), lid,
                            browser_context_id, frame_id.frame_routing_id),
          std::move(reply)));
}

void WebRtcEventLogManager::OnPeerConnectionRemoved(
    content::GlobalRenderFrameHostId frame_id,
    int lid,
    base::OnceCallback<void(bool)> reply) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  const auto browser_context_id = GetBrowserContextId(frame_id.child_id);
  if (browser_context_id == kNullBrowserContextId) {
    // RPH died before processing of this notification. This is handled by
    // RenderProcessExited() / RenderProcessHostDestroyed.
    MaybeReply(FROM_HERE, std::move(reply), false);
    return;
  }

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  // TODO(crbug.com/379869738) Remove GetUnsafeValue.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::OnPeerConnectionRemovedInternal,
          base::Unretained(this),
          PeerConnectionKey(frame_id.child_id.GetUnsafeValue(), lid,
                            browser_context_id, frame_id.frame_routing_id),
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

void WebRtcEventLogManager::OnSessionIdSetForPeerConnection(
    content::GlobalRenderFrameHostId frame_id,
    int lid,
    const std::string& session_id,
    base::OnceCallback<void(bool)> reply) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  const auto browser_context_id = GetBrowserContextId(frame_id.child_id);
  if (browser_context_id == kNullBrowserContextId) {
    // RPH died before processing of this notification. This is handled by
    // RenderProcessExited() / RenderProcessHostDestroyed.
    MaybeReply(FROM_HERE, std::move(reply), false);
    return;
  }

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  // TODO(crbug.com/379869738) Remove GetUnsafeValue.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::OnSessionIdSetForPeerConnectionInternal,
          base::Unretained(this),
          PeerConnectionKey(frame_id.child_id.GetUnsafeValue(), lid,
                            browser_context_id, frame_id.frame_routing_id),
          session_id, std::move(reply)));
}

void WebRtcEventLogManager::OnWebRtcEventLogWrite(
    content::GlobalRenderFrameHostId frame_id,
    int lid,
    const std::string& message,
    base::OnceCallback<void(std::pair<bool, bool>)> reply) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  const BrowserContext* browser_context = GetBrowserContext(frame_id.child_id);
  if (!browser_context) {
    // RPH died before processing of this notification.
    MaybeReply(FROM_HERE, std::move(reply), std::make_pair(false, false));
    return;
  }

  const auto browser_context_id = GetBrowserContextId(browser_context);
  CHECK_NE(browser_context_id, kNullBrowserContextId,
           base::NotFatalUntil::M161);

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  // TODO(crbug.com/379869738) Remove GetUnsafeValue.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::OnWebRtcEventLogWriteInternal,
          base::Unretained(this),
          PeerConnectionKey(frame_id.child_id.GetUnsafeValue(), lid,
                            browser_context_id, frame_id.frame_routing_id),
          message, std::move(reply)));
}

void WebRtcEventLogManager::EnableLocalLogging(
    const base::FilePath& base_path,
    base::OnceCallback<void(bool)> reply) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  EnableLocalLogging(base_path, kDefaultMaxLocalEventLogFileSizeBytes,
                     std::move(reply));
}

void WebRtcEventLogManager::EnableLocalLogging(
    const base::FilePath& base_path,
    size_t max_file_size_bytes,
    base::OnceCallback<void(bool)> reply) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(!base_path.empty(), base::NotFatalUntil::M161);
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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::DisableLocalLoggingInternal,
                     base::Unretained(this), std::move(reply)));
}

void WebRtcEventLogManager::OnWebRtcDataChannelLogWrite(
    content::GlobalRenderFrameHostId frame_id,
    int lid,
    const std::string& message) {
  OnWebRtcDataChannelLogWrite(frame_id, lid, message, base::NullCallback());
}

void WebRtcEventLogManager::OnWebRtcDataChannelLogWrite(
    content::GlobalRenderFrameHostId frame_id,
    int lid,
    const std::string& message,
    base::OnceCallback<void(bool)> reply) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  const BrowserContext* browser_context = GetBrowserContext(frame_id.child_id);
  if (!browser_context) {
    // RFH died before processing of this notification.
    MaybeReply(FROM_HERE, std::move(reply), false);
    return;
  }

  const auto browser_context_id = GetBrowserContextId(browser_context);
  CHECK_NE(browser_context_id, kNullBrowserContextId);

  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  // TODO(crbug.com/379869738) Remove GetUnsafeValue.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcEventLogManager::OnWebRtcDataChannelLogWriteInternal,
          base::Unretained(this),
          PeerConnectionKey(frame_id.child_id.GetUnsafeValue(), lid,
                            browser_context_id, frame_id.frame_routing_id),
          message, std::move(reply)));
}

void WebRtcEventLogManager::EnableDataChannelLogging(
    const base::FilePath& base_path,
    size_t max_file_size_bytes,
    base::OnceCallback<void(bool)> reply) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(!base_path.empty(), base::NotFatalUntil::M161);
  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::EnableDataChannelLoggingInternal,
                     base::Unretained(this), base_path, max_file_size_bytes,
                     std::move(reply)));
}

void WebRtcEventLogManager::DisableDataChannelLogging(
    base::OnceCallback<void(bool)> reply) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  // |this| is destroyed by ~BrowserProcessImpl(), so base::Unretained(this)
  // will not be dereferenced after destruction.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::DisableDataChannelLoggingInternal,
                     base::Unretained(this), std::move(reply)));
}

void WebRtcEventLogManager::OnLocalEventLogStarted(
    PeerConnectionKey peer_connection,
    const base::FilePath& file_path) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  OnLoggingTargetStarted(LoggingTarget::kLocalLogging, peer_connection,
                         /*output_period_ms=*/5000);

  if (local_logs_observer_) {
    local_logs_observer_->OnLocalEventLogStarted(peer_connection, file_path);
  }
}

void WebRtcEventLogManager::OnLocalEventLogStopped(
    PeerConnectionKey peer_connection) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  OnLoggingTargetStopped(LoggingTarget::kLocalLogging, peer_connection);

  if (local_logs_observer_) {
    local_logs_observer_->OnLocalEventLogStopped(peer_connection);
  }
}

void WebRtcEventLogManager::OnLocalDataChannelLogStarted(
    PeerConnectionKey peer_connection,
    const base::FilePath& file_path) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  pc_tracker_proxy_->EnableWebRtcDataChannelLogging(peer_connection);

  if (local_logs_observer_) {
    local_logs_observer_->OnLocalDataChannelLogStarted(peer_connection,
                                                       file_path);
  }
}

void WebRtcEventLogManager::OnLocalDataChannelLogStopped(
    PeerConnectionKey peer_connection) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  pc_tracker_proxy_->DisableWebRtcDataChannelLogging(peer_connection);

  if (local_logs_observer_) {
    local_logs_observer_->OnLocalDataChannelLogStopped(peer_connection);
  }
}

void WebRtcEventLogManager::OnRemoteLogStarted(PeerConnectionKey key,
                                               const base::FilePath& file_path,
                                               int output_period_ms) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);
  OnLoggingTargetStarted(LoggingTarget::kRemoteLogging, key, output_period_ms);
  if (remote_logs_observer_) {
    remote_logs_observer_->OnRemoteLogStarted(key, file_path, output_period_ms);
  }
}

void WebRtcEventLogManager::OnRemoteLogStopped(
    WebRtcEventLogPeerConnectionKey key) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);
  OnLoggingTargetStopped(LoggingTarget::kRemoteLogging, key);
  if (remote_logs_observer_) {
    remote_logs_observer_->OnRemoteLogStopped(key);
  }
}

void WebRtcEventLogManager::OnLoggingTargetStarted(LoggingTarget target,
                                                   PeerConnectionKey key,
                                                   int output_period_ms) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);
  auto it = peer_connections_with_event_logging_enabled_in_webrtc_.find(key);
  if (it != peer_connections_with_event_logging_enabled_in_webrtc_.end()) {
    CHECK_EQ((it->second & target), 0u, base::NotFatalUntil::M161);
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
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  // Record that we're no longer performing this type of logging for this PC.
  auto it = peer_connections_with_event_logging_enabled_in_webrtc_.find(key);
  CHECK(it != peer_connections_with_event_logging_enabled_in_webrtc_.end());
  CHECK_NE(it->second, 0u, base::NotFatalUntil::M161);
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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(first_browser_context_initializations_done_, base::NotFatalUntil::M161);
  CHECK(!browser_context->IsOffTheRecord());

  const auto browser_context_id = GetBrowserContextId(browser_context);
  auto it = pref_change_registrars_.emplace(std::piecewise_construct,
                                            std::make_tuple(browser_context_id),
                                            std::make_tuple());
  DCHECK(it.second) << "Already listening.";
  PrefChangeRegistrar& registrar = it.first->second;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  CHECK(profile, base::NotFatalUntil::M161);
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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  const auto browser_context_id = GetBrowserContextId(browser_context);

  size_t erased_count = pref_change_registrars_.erase(browser_context_id);
  CHECK_EQ(erased_count, 1u, base::NotFatalUntil::M161);
}

void WebRtcEventLogManager::OnPrefChange(BrowserContext* browser_context) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(first_browser_context_initializations_done_, base::NotFatalUntil::M161);

  const Profile* profile = Profile::FromBrowserContext(browser_context);
  CHECK(profile, base::NotFatalUntil::M161);

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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  network::NetworkConnectionTracker* network_connection_tracker =
      content::GetNetworkConnectionTracker();
  CHECK(network_connection_tracker, base::NotFatalUntil::M161);

  auto log_file_writer_factory = CreateRemoteLogFileWriterFactory();
  CHECK(log_file_writer_factory, base::NotFatalUntil::M161);

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
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);
  CHECK(network_connection_tracker, base::NotFatalUntil::M161);
  CHECK(log_file_writer_factory, base::NotFatalUntil::M161);
  remote_logs_manager_.SetNetworkConnectionTracker(network_connection_tracker);
  remote_logs_manager_.SetLogFileWriterFactory(
      std::move(log_file_writer_factory));
}

void WebRtcEventLogManager::EnableRemoteBoundLoggingForBrowserContext(
    BrowserContextId browser_context_id,
    const base::FilePath& browser_context_dir,
    base::OnceClosure reply) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);
  CHECK_NE(browser_context_id, kNullBrowserContextId,
           base::NotFatalUntil::M161);

  remote_logs_manager_.EnableForBrowserContext(browser_context_id,
                                               browser_context_dir);

  MaybeReply(FROM_HERE, std::move(reply));
}

void WebRtcEventLogManager::DisableRemoteBoundLoggingForBrowserContext(
    BrowserContextId browser_context_id,
    base::OnceClosure reply) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  // Note that the BrowserContext might never have been enabled in the
  // remote-bound manager; that's not a problem.
  remote_logs_manager_.DisableForBrowserContext(browser_context_id);

  MaybeReply(FROM_HERE, std::move(reply));
}

void WebRtcEventLogManager::RemoveRemoteBoundLogsForNotEnabledBrowserContext(
    BrowserContextId browser_context_id,
    const base::FilePath& browser_context_dir,
    base::OnceClosure reply) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  remote_logs_manager_.RemoveLogsForNotEnabledBrowserContext(
      browser_context_id, browser_context_dir);

  MaybeReply(FROM_HERE, std::move(reply));
}

void WebRtcEventLogManager::OnPeerConnectionAddedInternal(
    PeerConnectionKey key,
    base::OnceCallback<void(bool)> reply) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  const bool local_result = local_logs_manager_.OnPeerConnectionAdded(key);
  const bool remote_result = remote_logs_manager_.OnPeerConnectionAdded(key);
  CHECK_EQ(local_result, remote_result, base::NotFatalUntil::M161);

  MaybeReply(FROM_HERE, std::move(reply), local_result);
}

void WebRtcEventLogManager::OnPeerConnectionRemovedInternal(
    PeerConnectionKey key,
    base::OnceCallback<void(bool)> reply) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  const bool local_result = local_logs_manager_.OnPeerConnectionRemoved(key);
  const bool remote_result = remote_logs_manager_.OnPeerConnectionRemoved(key);
  CHECK_EQ(local_result, remote_result, base::NotFatalUntil::M161);

  MaybeReply(FROM_HERE, std::move(reply), local_result);
}

void WebRtcEventLogManager::OnSessionIdSetForPeerConnectionInternal(
    PeerConnectionKey key,
    const std::string& session_id,
    base::OnceCallback<void(bool)> reply) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);
  const bool result =
      remote_logs_manager_.OnSessionIdSetForPeerConnection(key, session_id);
  MaybeReply(FROM_HERE, std::move(reply), result);
}

void WebRtcEventLogManager::OnWebRtcEventLogWriteInternal(
    PeerConnectionKey key,
    const std::string& message,
    base::OnceCallback<void(std::pair<bool, bool>)> reply) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  const bool local_result = local_logs_manager_.EventLogWrite(key, message);
  const bool remote_result = remote_logs_manager_.EventLogWrite(key, message);

  MaybeReply(FROM_HERE, std::move(reply),
             std::make_pair(local_result, remote_result));
}

void WebRtcEventLogManager::EnableLocalLoggingInternal(
    const base::FilePath& base_path,
    size_t max_file_size_bytes,
    base::OnceCallback<void(bool)> reply) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  const bool result =
      local_logs_manager_.EnableEventLogging(base_path, max_file_size_bytes);

  MaybeReply(FROM_HERE, std::move(reply), result);
}

void WebRtcEventLogManager::DisableLocalLoggingInternal(
    base::OnceCallback<void(bool)> reply) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  const bool result = local_logs_manager_.DisableEventLogging();

  MaybeReply(FROM_HERE, std::move(reply), result);
}

void WebRtcEventLogManager::OnWebRtcDataChannelLogWriteInternal(
    PeerConnectionKey key,
    const std::string& message,
    base::OnceCallback<void(bool)> reply) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  const bool result = local_logs_manager_.DataChannelLogWrite(key, message);

  MaybeReply(FROM_HERE, std::move(reply), result);
}

void WebRtcEventLogManager::EnableDataChannelLoggingInternal(
    const base::FilePath& base_path,
    size_t max_file_size_bytes,
    base::OnceCallback<void(bool)> reply) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  const bool result = local_logs_manager_.EnableDataChannelLogging(
      base_path, max_file_size_bytes);

  MaybeReply(FROM_HERE, std::move(reply), result);
}

void WebRtcEventLogManager::DisableDataChannelLoggingInternal(
    base::OnceCallback<void(bool)> reply) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  const bool result = local_logs_manager_.DisableDataChannelLogging();

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
    std::optional<std::string> diagnostic_uuid,
    bool local_only,
    base::OnceCallback<void(bool, const std::string&, const std::string&)>
        reply) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  std::string log_id;
  std::string error_message;
  const bool result = remote_logs_manager_.StartRemoteLogging(
      render_process_id, browser_context_id, session_id, browser_context_dir,
      max_file_size_bytes, output_period_ms, web_app_id,
      std::move(diagnostic_uuid), local_only, &log_id, &error_message);

  // |log_id| set only if successful; |error_message| set only if unsuccessful.
  CHECK_EQ(result, !log_id.empty(), base::NotFatalUntil::M161);
  CHECK_EQ(!result, !error_message.empty(), base::NotFatalUntil::M161);

  MaybeReply<bool, const std::string&, const std::string&>(
      FROM_HERE, std::move(reply), result, log_id, error_message);
}

void WebRtcEventLogManager::StopLoggingInternal(
    int render_process_id,
    StopLoggingAction action,
    std::optional<std::string> diagnostic_uuid,
    base::OnceClosure callback) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);
  base::RepeatingClosure barrier = base::BarrierClosure(
      2, base::BindOnce(&MaybeReply<>, FROM_HERE, std::move(callback)));
  local_logs_manager_.StopLogging(render_process_id, action, barrier);
  remote_logs_manager_.StopLogging(render_process_id, action,
                                   std::move(diagnostic_uuid), barrier);
}

void WebRtcEventLogManager::ClearCacheForBrowserContextInternal(
    BrowserContextId browser_context_id,
    const base::Time& delete_begin,
    const base::Time& delete_end) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);
  remote_logs_manager_.ClearCacheForBrowserContext(browser_context_id,
                                                   delete_begin, delete_end);
}

void WebRtcEventLogManager::OnClearCacheForBrowserContextDoneInternal(
    base::OnceClosure reply) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK_GT(num_user_blocking_tasks_, 0u, base::NotFatalUntil::M161);
  if (--num_user_blocking_tasks_ == 0) {
    task_runner_->UpdatePriority(base::TaskPriority::BEST_EFFORT);
  }
  std::move(reply).Run();
}

void WebRtcEventLogManager::GetHistoryInternal(
    BrowserContextId browser_context_id,
    base::OnceCallback<void(const std::vector<UploadList::UploadInfo>&)>
        reply) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);
  CHECK(reply, base::NotFatalUntil::M161);
  remote_logs_manager_.GetHistory(browser_context_id, std::move(reply));
}

void WebRtcEventLogManager::RenderProcessExitedInternal(int render_process_id) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);
  local_logs_manager_.RenderProcessHostExitedDestroyed(render_process_id);
  remote_logs_manager_.RenderProcessHostExitedDestroyed(render_process_id);
}

void WebRtcEventLogManager::SetLocalLogsObserverInternal(
    WebRtcLocalEventLogsObserver* observer,
    base::OnceClosure reply) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  local_logs_observer_ = observer;

  if (reply) {
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(reply));
  }
}

void WebRtcEventLogManager::SetRemoteLogsObserverInternal(
    WebRtcRemoteEventLogsObserver* observer,
    base::OnceClosure reply) {
  CHECK(task_runner_->RunsTasksInCurrentSequence(), base::NotFatalUntil::M161);

  remote_logs_observer_ = observer;

  if (reply) {
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(reply));
  }
}

void WebRtcEventLogManager::SetClockForTesting(base::Clock* clock,
                                               base::OnceClosure reply) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(reply, base::NotFatalUntil::M161);

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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(reply, base::NotFatalUntil::M161);

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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(reply, base::NotFatalUntil::M161);

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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  DCHECK(!first_browser_context_initializations_done_) << "Too late.";
  DCHECK(!remote_log_file_writer_factory_for_testing_) << "Already called.";
  remote_log_file_writer_factory_for_testing_ = std::move(factory);
}

void WebRtcEventLogManager::UploadConditionsHoldForTesting(
    base::OnceCallback<void(bool)> callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  return task_runner_;
}

void WebRtcEventLogManager::PostNullTaskForTesting(base::OnceClosure reply) {
  task_runner_->PostTask(FROM_HERE, std::move(reply));
}

void WebRtcEventLogManager::ShutDownForTesting(base::OnceClosure reply) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  // Unit tests block until |callback| is sent back, so the use
  // of base::Unretained(&remote_logs_manager_) is safe.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcRemoteEventLogManager::ShutDownForTesting,
                     base::Unretained(&remote_logs_manager_),
                     std::move(reply)));
}

}  // namespace webrtc_event_logging
