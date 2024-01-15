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
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/media_router/media_remoting_dialog_view.h"
#endif

using content::BrowserThread;
using media::mojom::RemotingSinkMetadata;
using media::mojom::RemotingStartFailReason;
using media::mojom::RemotingStopReason;

bool MediaRemotingDialogCoordinator::Show(
    PermissionCallback permission_callback) {
  return false;
}

void MediaRemotingDialogCoordinator::Hide() {}

bool MediaRemotingDialogCoordinator::IsShowing() const {
  return false;
}

class CastRemotingConnector::RemotingBridge final
    : public media::mojom::Remoter {
 public:
  // Constructs a "bridge" to delegate calls between the given |source| and
  // |connector|. |connector| must be valid at the time of construction, but is
  // otherwise a weak pointer that can become invalid during the lifetime of a
  // RemotingBridge.
  RemotingBridge(mojo::PendingRemote<media::mojom::RemotingSource> source,
                 CastRemotingConnector* connector)
      : source_(std::move(source)), connector_(connector) {
    DCHECK(connector_);
    source_.set_disconnect_handler(
        base::BindOnce(&RemotingBridge::Stop, base::Unretained(this),
                       RemotingStopReason::SOURCE_GONE));
    connector_->RegisterBridge(this);
  }

  RemotingBridge(const RemotingBridge&) = delete;
  RemotingBridge& operator=(const RemotingBridge&) = delete;

  ~RemotingBridge() final {
    if (connector_)
      connector_->DeregisterBridge(this, RemotingStopReason::SOURCE_GONE);
  }

  // The CastRemotingConnector calls these to call back to the RemotingSource.
  void OnSinkAvailable(const RemotingSinkMetadata& metadata) {
    source_->OnSinkAvailable(metadata.Clone());
  }
  void OnSinkGone() { source_->OnSinkGone(); }
  void OnStarted() { source_->OnStarted(); }
  void OnStartFailed(RemotingStartFailReason reason) {
    source_->OnStartFailed(reason);
  }
  void OnMessageFromSink(const std::vector<uint8_t>& message) {
    source_->OnMessageFromSink(message);
  }
  void OnStopped(RemotingStopReason reason) { source_->OnStopped(reason); }

  // The CastRemotingConnector calls this when it is no longer valid.
  void OnCastRemotingConnectorDestroyed() { connector_ = nullptr; }

  // media::mojom::Remoter implementation. The source calls these to start/stop
  // media remoting and send messages to the sink. These simply delegate to the
  // CastRemotingConnector, which mediates to establish only one remoting
  // session among possibly multiple requests. The connector will respond to
  // this request by calling one of: OnStarted() or OnStartFailed().
  void Start() final {
    if (connector_)
      connector_->StartRemoting(this);
  }
  void StartWithPermissionAlreadyGranted() final {
    if (connector_) {
      connector_->StartWithPermissionAlreadyGranted(this);
    }
  }
  void StartDataStreams(
      mojo::ScopedDataPipeConsumerHandle audio_pipe,
      mojo::ScopedDataPipeConsumerHandle video_pipe,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
          audio_sender,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
          video_sender) final {
    if (connector_) {
      connector_->StartRemotingDataStreams(
          this, std::move(audio_pipe), std::move(video_pipe),
          std::move(audio_sender), std::move(video_sender));
    }
  }
  void Stop(RemotingStopReason reason) final {
    if (connector_)
      connector_->StopRemoting(this, reason, true);
  }
  void SendMessageToSink(const std::vector<uint8_t>& message) final {
    if (connector_)
      connector_->SendMessageToSink(this, message);
  }
  void EstimateTransmissionCapacity(
      media::mojom::Remoter::EstimateTransmissionCapacityCallback callback)
      final {
    if (connector_)
      connector_->EstimateTransmissionCapacity(std::move(callback));
    else
      std::move(callback).Run(0);
  }

 private:
  mojo::Remote<media::mojom::RemotingSource> source_;

  // Weak pointer. Will be set to nullptr if the CastRemotingConnector is
  // destroyed before this RemotingBridge.
  raw_ptr<CastRemotingConnector> connector_;
};

// static
const void* const CastRemotingConnector::kUserDataKey = &kUserDataKey;

// static
CastRemotingConnector* CastRemotingConnector::Get(
    content::WebContents* contents) {
  DCHECK(contents);
  CastRemotingConnector* connector =
      static_cast<CastRemotingConnector*>(contents->GetUserData(kUserDataKey));
  if (!connector) {
    if (!media_router::MediaRouterEnabled(contents->GetBrowserContext()))
      return nullptr;
    connector = new CastRemotingConnector(
        user_prefs::UserPrefs::Get(contents->GetBrowserContext()),
        sessions::SessionTabHelper::IdForTab(contents),
#if defined(TOOLKIT_VIEWS)
        std::make_unique<media_router::MediaRemotingDialogCoordinatorViews>(
            contents)
#else
        std::make_unique<MediaRemotingDialogCoordinator>()
#endif
    );
    contents->SetUserData(kUserDataKey, base::WrapUnique(connector));
  }
  return connector;
}

// static
void CastRemotingConnector::CreateMediaRemoter(
    content::RenderFrameHost* host,
    mojo::PendingRemote<media::mojom::RemotingSource> source,
    mojo::PendingReceiver<media::mojom::Remoter> receiver) {
  DCHECK(host);
  auto* const contents = content::WebContents::FromRenderFrameHost(host);
  if (!contents)
    return;
  CastRemotingConnector* const connector = CastRemotingConnector::Get(contents);
  if (!connector)
    return;
  connector->CreateBridge(std::move(source), std::move(receiver));
}

CastRemotingConnector::CastRemotingConnector(
    PrefService* pref_service,
    SessionID tab_id,
    std::unique_ptr<MediaRemotingDialogCoordinator> dialog_coordinator)
    : pref_service_(pref_service),
      tab_id_(tab_id),
      dialog_coordinator_(std::move(dialog_coordinator)) {
  StartObservingPref();
}

CastRemotingConnector::~CastRemotingConnector() {
  // Assume nothing about destruction/shutdown sequence of a tab. For example,
  // it's possible the owning WebContents will be destroyed before the Mojo
  // message pipes to the RemotingBridges have been closed.
  if (active_bridge_)
    StopRemoting(active_bridge_, RemotingStopReason::ROUTE_TERMINATED, false);
  for (RemotingBridge* notifyee : bridges_) {
    notifyee->OnSinkGone();
    notifyee->OnCastRemotingConnectorDestroyed();
  }
}

void CastRemotingConnector::ResetRemotingPermission() {
  remoting_allowed_ = GetRemotingAllowedUserPref();
}

void CastRemotingConnector::ConnectWithMediaRemoter(
    mojo::PendingRemote<media::mojom::Remoter> remoter,
    mojo::PendingReceiver<media::mojom::RemotingSource> receiver) {
  DCHECK(!remoter_);
  DCHECK(remoter);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(2) << __func__;

  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &CastRemotingConnector::OnMirrorServiceStopped, base::Unretained(this)));
  remoter_.Bind(std::move(remoter));
  remoter_.set_disconnect_handler(base::BindOnce(
      &CastRemotingConnector::OnMirrorServiceStopped, base::Unretained(this)));
}

void CastRemotingConnector::OnMirrorServiceStopped() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(2) << __func__;

  receiver_.reset();
  remoter_.reset();

  sink_metadata_ = RemotingSinkMetadata();
  if (active_bridge_)
    StopRemoting(active_bridge_, RemotingStopReason::SERVICE_GONE, false);
  for (RemotingBridge* notifyee : bridges_)
    notifyee->OnSinkGone();
}

void CastRemotingConnector::CreateBridge(
    mojo::PendingRemote<media::mojom::RemotingSource> source,
    mojo::PendingReceiver<media::mojom::Remoter> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<RemotingBridge>(std::move(source), this),
      std::move(receiver));
}

void CastRemotingConnector::RegisterBridge(RemotingBridge* bridge) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(bridges_.find(bridge) == bridges_.end());

  bridges_.insert(bridge);
  if (remoter_ && !active_bridge_ && remoting_allowed_.value_or(true))
    bridge->OnSinkAvailable(sink_metadata_);
}

void CastRemotingConnector::DeregisterBridge(RemotingBridge* bridge,
                                             RemotingStopReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(bridges_.find(bridge) != bridges_.end());

  bridges_.erase(bridge);
  if (bridge == active_bridge_)
    StopRemoting(bridge, reason, true);
}

void CastRemotingConnector::StartRemoting(RemotingBridge* bridge) {
  if (!StartRemotingCommon(bridge)) {
    return;
  }

  if (remoting_allowed_.has_value()) {
    StartRemotingIfPermitted();
    return;
  }
  dialog_coordinator_->Show(base::BindOnce(
      &CastRemotingConnector::OnDialogClosed, weak_factory_.GetWeakPtr()));
}

void CastRemotingConnector::StartWithPermissionAlreadyGranted(
    RemotingBridge* bridge) {
  if (!StartRemotingCommon(bridge)) {
    return;
  }

  DCHECK(remoter_);
  remoter_->Start();
}

bool CastRemotingConnector::StartRemotingCommon(RemotingBridge* bridge) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(bridges_.find(bridge) != bridges_.end());

  // Refuse to start if there is no remoting route available, or if remoting is
  // already active.
  if (!remoter_) {
    DVLOG(2) << "Remoting start failed: Invalid ANSWER message.";
    bridge->OnStartFailed(RemotingStartFailReason::INVALID_ANSWER_MESSAGE);
    return false;
  }
  if (active_bridge_) {
    DVLOG(2) << "Remoting start failed: Cannot start multiple.";
    bridge->OnStartFailed(RemotingStartFailReason::CANNOT_START_MULTIPLE);
    return false;
  }

  // Notify all other sources that the sink is no longer available for remoting.
  // A race condition is possible, where one of the other sources will try to
  // start remoting before receiving this notification; but that attempt will
  // just fail later on.
  for (RemotingBridge* notifyee : bridges_) {
    if (notifyee == bridge)
      continue;
    notifyee->OnSinkGone();
  }

  active_bridge_ = bridge;
  return true;
}

void CastRemotingConnector::OnDialogClosed(bool remoting_allowed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  remoting_allowed_ = remoting_allowed;
  StartRemotingIfPermitted();
}

void CastRemotingConnector::StartRemotingIfPermitted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!active_bridge_)
    return;

  if (remoting_allowed_.value()) {
    DCHECK(remoter_);
    remoter_->Start();
  } else {
    active_bridge_->OnStartFailed(
        RemotingStartFailReason::REMOTING_NOT_PERMITTED);
    active_bridge_->OnSinkGone();
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

  DCHECK(remoter_);
  remoter_->StartDataStreams(std::move(audio_pipe), std::move(video_pipe),
                             std::move(audio_sender), std::move(video_sender));
}

void CastRemotingConnector::StopRemoting(RemotingBridge* bridge,
                                         RemotingStopReason reason,
                                         bool is_initiated_by_source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (active_bridge_ != bridge)
    return;

  active_bridge_ = nullptr;

  // Cancel all outstanding callbacks related to the remoting session.
  weak_factory_.InvalidateWeakPtrs();

  if (dialog_coordinator_->IsShowing()) {
    dialog_coordinator_->Hide();
    if (is_initiated_by_source && remoter_) {
      // The source requested remoting be stopped before the permission request
      // was resolved. This means the |remoter_| was never started, and remains
      // in the available state, still all ready to go. Thus, notify the sources
      // that the sink is available once again.
      for (RemotingBridge* notifyee : bridges_)
        notifyee->OnSinkAvailable(sink_metadata_);
    }
    return;  // Early returns since the |remoter_| was never started.
  }

  // Reset |sink_metadata_|. Remoting can only be started after
  // OnSinkAvailable() is called again.
  sink_metadata_ = RemotingSinkMetadata();

  // Prevent the source from trying to start again until the Cast Provider has
  // indicated the stop operation has completed.
  bridge->OnSinkGone();
  // Note: At this point, all sources should think the sink is gone.

  if (remoter_) {
    remoter_->Stop(reason);
  }

  bridge->OnStopped(reason);
}

void CastRemotingConnector::OnStopped(RemotingStopReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (active_bridge_) {
    // This call will reset |sink_metadata_| and notify the source that sink is
    // gone.
    StopRemoting(active_bridge_, reason, false);
  } else if (reason == RemotingStopReason::USER_DISABLED) {
    // Notify all the sources that the sink is gone. Remoting can only be
    // started after OnSinkAvailable() is called again.
    sink_metadata_ = RemotingSinkMetadata();
    for (RemotingBridge* notifyee : bridges_)
      notifyee->OnSinkGone();
  }
}

void CastRemotingConnector::SendMessageToSink(
    RemotingBridge* bridge,
    const std::vector<uint8_t>& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // During an active remoting session, simply pass all binary messages through
  // to the sink.
  if (!remoter_ || active_bridge_ != bridge)
    return;
  remoter_->SendMessageToSink(message);
}

void CastRemotingConnector::OnMessageFromSink(
    const std::vector<uint8_t>& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // During an active remoting session, simply pass all binary messages through
  // to the source.
  if (!active_bridge_)
    return;
  active_bridge_->OnMessageFromSink(message);
}

void CastRemotingConnector::EstimateTransmissionCapacity(
    media::mojom::Remoter::EstimateTransmissionCapacityCallback callback) {
  std::move(callback).Run(0);
}

void CastRemotingConnector::OnSinkAvailable(
    media::mojom::RemotingSinkMetadataPtr metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
    notifyee->OnSinkAvailable(sink_metadata_);
  }
}

void CastRemotingConnector::OnSinkGone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(2) << __func__;
  sink_metadata_ = RemotingSinkMetadata();
  if (active_bridge_)
    StopRemoting(active_bridge_, RemotingStopReason::SERVICE_GONE, false);
  for (RemotingBridge* notifyee : bridges_)
    notifyee->OnSinkGone();
}

void CastRemotingConnector::OnStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(remoter_);
  if (active_bridge_) {
    active_bridge_->OnStarted();
  } else {
    remoter_->Stop(RemotingStopReason::SOURCE_GONE);
  }
}

void CastRemotingConnector::OnStartFailed(RemotingStartFailReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (active_bridge_)
    active_bridge_->OnStartFailed(reason);
}

void CastRemotingConnector::OnDataSendFailed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
  const PrefService::Preference* pref = pref_service_->FindPreference(
      media_router::prefs::kMediaRouterMediaRemotingEnabled);
  bool enabled = pref->GetValue()->GetIfBool().value_or(false);
  remoting_allowed_ = enabled;
  if (!enabled)
    OnStopped(media::mojom::RemotingStopReason::USER_DISABLED);
#endif
}

std::optional<bool> CastRemotingConnector::GetRemotingAllowedUserPref() const {
#if BUILDFLAG(IS_ANDROID)
  return std::nullopt;
#else
  const PrefService::Preference* pref = pref_service_->FindPreference(
      media_router::prefs::kMediaRouterMediaRemotingEnabled);
  if (!pref || pref->IsDefaultValue()) {
    return std::nullopt;
  }
  return pref->GetValue()->GetBool();
#endif
}
