// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_remoting_connector.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "components/media_router/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using media::mojom::RemotingSinkMetadata;
using media::mojom::RemotingStartFailReason;
using media::mojom::RemotingStopReason;

// static
const void* const CastRemotingConnector::kUserDataKey = &kUserDataKey;

// static
CastRemotingConnector* CastRemotingConnector::Get(
    content::WebContents* contents) {
  CHECK(contents, base::NotFatalUntil::M161);
  CastRemotingConnector* connector =
      static_cast<CastRemotingConnector*>(contents->GetUserData(kUserDataKey));
  if (!connector) {
    if (!media_router::MediaRouterEnabled(contents->GetBrowserContext()))
      return nullptr;
    connector = new CastRemotingConnector(
        user_prefs::UserPrefs::Get(contents->GetBrowserContext()),
        sessions::SessionTabHelper::IdForTab(contents));
    contents->SetUserData(kUserDataKey, base::WrapUnique(connector));
  }
  return connector;
}

CastRemotingConnector::CastRemotingConnector(PrefService* pref_service,
                                             SessionID tab_id)
    : pref_service_(pref_service), tab_id_(tab_id) {
  StartObservingPref();
}

CastRemotingConnector::~CastRemotingConnector() {
  // Assume nothing about destruction/shutdown sequence of a tab. For example,
  // it's possible the owning WebContents will be destroyed before the Mojo
  // message pipes to the RemotingBridges have been closed.
  if (active_bridge_)
    StopRemoting(active_bridge_, RemotingStopReason::ROUTE_TERMINATED, false);
  for (RemotingBridge* notifyee : bridges_) {
    notifyee->OnSinkGone(this);
    notifyee->OnClientDestroyed(this);
  }
}

void CastRemotingConnector::ResetRemotingPermission() {
  remoting_allowed_ = GetRemotingAllowedUserPref();
}

void CastRemotingConnector::ConnectWithMediaRemoter(
    mojo::PendingRemote<media::mojom::Remoter> remoter,
    mojo::PendingReceiver<media::mojom::RemotingSource> receiver) {
  CHECK(!remoter_, base::NotFatalUntil::M161);
  CHECK(remoter, base::NotFatalUntil::M161);
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  DVLOG(2) << __func__;

  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &CastRemotingConnector::OnMirrorServiceStopped, base::Unretained(this)));
  remoter_.Bind(std::move(remoter));
  remoter_.set_disconnect_handler(base::BindOnce(
      &CastRemotingConnector::OnMirrorServiceStopped, base::Unretained(this)));

  for (RemotingBridge* notifyee : bridges_) {
    notifyee->SetClientAvailable(this, true);
  }
}

void CastRemotingConnector::OnMirrorServiceStopped() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  DVLOG(2) << __func__;

  receiver_.reset();
  remoter_.reset();

  sink_metadata_ = RemotingSinkMetadata();
  if (active_bridge_)
    StopRemoting(active_bridge_, RemotingStopReason::SERVICE_GONE, false);
  for (RemotingBridge* notifyee : bridges_)
    notifyee->OnSinkGone(this);
  for (RemotingBridge* notifyee : bridges_) {
    notifyee->SetClientAvailable(this, false);
  }
}

void CastRemotingConnector::RegisterBridge(RemotingBridge* bridge) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(bridges_.find(bridge) == bridges_.end());

  bridges_.insert(bridge);
  bridge->SetClientAvailable(this, remoter_.is_bound());
}

void CastRemotingConnector::DeregisterBridge(RemotingBridge* bridge,
                                             RemotingStopReason reason) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(bridges_.find(bridge) != bridges_.end());

  bridges_.erase(bridge);
  if (bridge == active_bridge_)
    StopRemoting(bridge, reason, true);
}

void CastRemotingConnector::StartRemoting(RemotingBridge* bridge) {
  if (!StartRemotingCommon(bridge)) {
    return;
  }

  StartRemotingIfPermitted();
}

void CastRemotingConnector::StartWithPermissionAlreadyGranted(
    RemotingBridge* bridge) {
  if (!StartRemotingCommon(bridge)) {
    return;
  }

  CHECK(remoter_, base::NotFatalUntil::M161);
  remoter_->Start();
}

bool CastRemotingConnector::StartRemotingCommon(RemotingBridge* bridge) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(bridges_.find(bridge) != bridges_.end());

  // Refuse to start if there is no remoting route available, or if remoting is
  // already active.
  if (!remoter_) {
    DVLOG(2) << "Remoting start failed: Invalid ANSWER message.";
    bridge->OnStartFailed(this,
                          RemotingStartFailReason::INVALID_ANSWER_MESSAGE);
    return false;
  }
  if (active_bridge_) {
    DVLOG(2) << "Remoting start failed: Cannot start multiple.";
    bridge->OnStartFailed(this, RemotingStartFailReason::CANNOT_START_MULTIPLE);
    return false;
  }

  // Notify all other sources that the sink is no longer available for remoting.
  // A race condition is possible, where one of the other sources will try to
  // start remoting before receiving this notification; but that attempt will
  // just fail later on.
  for (RemotingBridge* notifyee : bridges_) {
    if (notifyee == bridge)
      continue;
    notifyee->OnSinkGone(this);
  }

  active_bridge_ = bridge;
  return true;
}

void CastRemotingConnector::StartRemotingIfPermitted() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  if (!active_bridge_)
    return;

  if (remoting_allowed_) {
    CHECK(remoter_, base::NotFatalUntil::M161);
    remoter_->Start();
  } else {
    active_bridge_->OnStartFailed(
        this, RemotingStartFailReason::REMOTING_NOT_PERMITTED);
    active_bridge_->OnSinkGone(this);
    active_bridge_ = nullptr;
  }
}

void CastRemotingConnector::StartRemotingDataStreams(
    RemotingBridge* bridge,
    mojo::ScopedDataPipeConsumerHandle audio_pipe,
    mojo::ScopedDataPipeConsumerHandle video_pipe,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender> audio_sender,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
        video_sender) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  // Refuse to start if there is no remoting route available, or if remoting is
  // not active for this |bridge|.
  if (!remoter_ || active_bridge_ != bridge)
    return;
  // Also, if neither audio nor video pipe was provided, or if a receiver for a
  // RemotingDataStreamSender was not provided for a data pipe, error-out early.
  if ((!audio_pipe.is_valid() && !video_pipe.is_valid()) ||
      (audio_pipe.is_valid() && !audio_sender.is_valid()) ||
      (video_pipe.is_valid() && !video_sender.is_valid())) {
    StopRemoting(active_bridge_, RemotingStopReason::DATA_SEND_FAILED, false);
    return;
  }

  CHECK(remoter_, base::NotFatalUntil::M161);
  remoter_->StartDataStreams(std::move(audio_pipe), std::move(video_pipe),
                             std::move(audio_sender), std::move(video_sender));
}

void CastRemotingConnector::StopRemoting(RemotingBridge* bridge,
                                         RemotingStopReason reason,
                                         bool is_initiated_by_source) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  if (active_bridge_ != bridge)
    return;

  active_bridge_ = nullptr;

  // Reset |sink_metadata_|. Remoting can only be started after
  // OnSinkAvailable() is called again.
  sink_metadata_ = RemotingSinkMetadata();

  // Prevent the source from trying to start again until the Cast Provider has
  // indicated the stop operation has completed.
  bridge->OnSinkGone(this);
  // Note: At this point, all sources should think the sink is gone.

  if (remoter_) {
    remoter_->Stop(reason);
  }

  bridge->OnStopped(this, reason);
}

void CastRemotingConnector::OnStopped(RemotingStopReason reason) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  if (active_bridge_) {
    // This call will reset |sink_metadata_| and notify the source that sink is
    // gone.
    StopRemoting(active_bridge_, reason, false);
  } else if (reason == RemotingStopReason::USER_DISABLED) {
    // Notify all the sources that the sink is gone. Remoting can only be
    // started after OnSinkAvailable() is called again.
    sink_metadata_ = RemotingSinkMetadata();
    for (RemotingBridge* notifyee : bridges_)
      notifyee->OnSinkGone(this);
  }
}

void CastRemotingConnector::SendMessageToSink(
    RemotingBridge* bridge,
    const std::vector<uint8_t>& message) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  // During an active remoting session, simply pass all binary messages through
  // to the sink.
  if (!remoter_ || active_bridge_ != bridge)
    return;
  remoter_->SendMessageToSink(message);
}

void CastRemotingConnector::OnMessageFromSink(
    const std::vector<uint8_t>& message) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  // During an active remoting session, simply pass all binary messages through
  // to the source.
  if (!active_bridge_)
    return;
  active_bridge_->OnMessageFromSink(this, message);
}

bool CastRemotingConnector::HasSinkMetadata() const {
  return sink_metadata_ != RemotingSinkMetadata();
}

void CastRemotingConnector::OnClientActivated(RemotingBridge* bridge) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  if (HasSinkMetadata() && !active_bridge_ && remoting_allowed_) {
    bridge->OnSinkAvailable(this, sink_metadata_);
  }
}

void CastRemotingConnector::OnClientDeactivated(RemotingBridge* bridge) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  if (active_bridge_ == bridge) {
    StopRemoting(bridge, RemotingStopReason::ROUTE_TERMINATED, false);
    return;
  }

  if (HasSinkMetadata()) {
    bridge->OnSinkGone(this);
  }
}

void CastRemotingConnector::OnSinkAvailable(
    media::mojom::RemotingSinkMetadataPtr metadata) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  DVLOG(2) << __func__;

  // The receiver's metadata should be unchanged during an active remoting
  // session.
  if (active_bridge_) {
    LOG(WARNING) << "Unexpected OnSinkAvailable() call during an active"
                 << "remoting session.";
    return;
  }
  sink_metadata_ = *metadata;
#if !BUILDFLAG(IS_ANDROID)
  sink_metadata_.features.push_back(
      media::mojom::RemotingSinkFeature::RENDERING);
#endif

  for (RemotingBridge* notifyee : bridges_) {
    notifyee->OnSinkAvailable(this, sink_metadata_);
  }
}

void CastRemotingConnector::OnSinkGone() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  DVLOG(2) << __func__;
  sink_metadata_ = RemotingSinkMetadata();
  if (active_bridge_)
    StopRemoting(active_bridge_, RemotingStopReason::SERVICE_GONE, false);
  for (RemotingBridge* notifyee : bridges_)
    notifyee->OnSinkGone(this);
}

void CastRemotingConnector::OnStarted() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(remoter_, base::NotFatalUntil::M161);
  if (active_bridge_) {
    active_bridge_->OnStarted(this);
  } else {
    remoter_->Stop(RemotingStopReason::SOURCE_GONE);
  }
}

void CastRemotingConnector::OnStartFailed(RemotingStartFailReason reason) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  if (active_bridge_)
    active_bridge_->OnStartFailed(this, reason);
}

void CastRemotingConnector::OnDataSendFailed() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  // A single data send failure is treated as fatal to an active remoting
  // session.
  if (active_bridge_)
    StopRemoting(active_bridge_, RemotingStopReason::DATA_SEND_FAILED, false);
}

void CastRemotingConnector::StartObservingPref() {
  pref_change_registrar_.Init(pref_service_);
#if !BUILDFLAG(IS_ANDROID)
  pref_change_registrar_.Add(
      media_router::prefs::kMediaRouterMediaRemotingEnabled,
      base::BindRepeating(&CastRemotingConnector::OnPrefChanged,
                          base::Unretained(this)));
  remoting_allowed_ = GetRemotingAllowedUserPref();
#endif
}

void CastRemotingConnector::OnPrefChanged() {
#if !BUILDFLAG(IS_ANDROID)
  remoting_allowed_ = GetRemotingAllowedUserPref();
  if (!remoting_allowed_) {
    OnStopped(media::mojom::RemotingStopReason::USER_DISABLED);
  }
#endif
}

bool CastRemotingConnector::GetRemotingAllowedUserPref() const {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  const PrefService::Preference* pref = pref_service_->FindPreference(
      media_router::prefs::kMediaRouterMediaRemotingEnabled);
  return pref && pref->GetValue()->GetBool();
#endif
}
