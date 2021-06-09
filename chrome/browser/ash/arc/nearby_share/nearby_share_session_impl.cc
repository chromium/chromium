// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/nearby_share_session_impl.h"

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_types_util.h"
#include "base/bind.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "components/arc/arc_util.h"
#include "components/arc/intent_helper/custom_tab.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/mojom/types.mojom-forward.h"

namespace arc {

namespace {
// Maximum time to wait for the ARC window to be initialized.
// ARC Wayland messages and Mojo messages are sent across the same pipe. The
// order in which the messages are sent is not deterministic. If the ARC
// activity starts Nearby Share before the wayland message for the new has been
// processed, the corresponding aura::Window for a given ARC activity task ID
// will not be found. To get around this, NearbyShareSessionImpl will wait a
// little while for the Wayland message to be processed and the window to be
// initialized.
constexpr base::TimeDelta kWindowInitializationTimeout =
    base::TimeDelta::FromSeconds(1);

constexpr char kIntentExtraText[] = "android.intent.extra.TEXT";
}  // namespace

// static
mojo::PendingRemote<mojom::NearbyShareSessionHost>
NearbyShareSessionImpl::Create(
    std::unique_ptr<content::WebContents> web_contents,
    int32_t task_id,
    mojom::ShareIntentInfoPtr share_info,
    mojo::PendingRemote<mojom::NearbyShareSessionInstance> instance) {
  if (!instance) {
    LOG(ERROR) << "instance is null. Unable to create NearbyShareSessionImpl";
    return mojo::NullRemote();
  }

  auto* web_contents_ptr = web_contents.get();
  mojo::PendingRemote<mojom::NearbyShareSessionHost> remote;
  // The NearbyShareSessionImpl instance will be deleted when the mojo
  // connection is closed.
  NearbyShareSessionImpl::CreateForWebContents(
      web_contents_ptr, std::move(web_contents), task_id, std::move(share_info),
      std::move(instance), remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

NearbyShareSessionImpl::~NearbyShareSessionImpl() {
  env_observation_.Reset();
  arc_window_observation_.Reset();
  web_contents_->RemoveUserData(UserDataKey());
}

void NearbyShareSessionImpl::OnNearbyShareClosed() {
  session_instance_->OnNearbyShareViewClosed();
}

// Overridden from aura::EnvObserver:
void NearbyShareSessionImpl::OnWindowInitialized(aura::Window* window) {
  if (ash::IsArcWindow(window) && (arc::GetWindowTaskId(window) == task_id_)) {
    env_observation_.Reset();
    arc_window_observation_.Observe(window);
  }
}

// Overridden from aura::WindowObserver
void NearbyShareSessionImpl::OnWindowVisibilityChanged(aura::Window* window,
                                                       bool visible) {
  if (visible && (arc::GetWindowTaskId(window) == task_id_)) {
    VLOG(1) << "ARC Window is visible";
    window_initialization_timer_.Stop();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&NearbyShareSessionImpl::ShowNearbyBubble,
                       weak_ptr_factory_.GetWeakPtr(), std::move(window)));
  }
}

// Overridden from aura::WindowObserver
void NearbyShareSessionImpl::OnWindowDestroying(aura::Window* removed_window) {
  Close();
}

NearbyShareSessionImpl::NearbyShareSessionImpl(
    content::WebContents* web_contents_ptr,
    std::unique_ptr<content::WebContents> web_contents,
    int32_t task_id,
    mojom::ShareIntentInfoPtr share_info,
    mojo::PendingRemote<mojom::NearbyShareSessionInstance> session_instance,
    mojo::PendingReceiver<mojom::NearbyShareSessionHost> session_receiver)
    : ArcCustomTabModalDialogHost(nullptr, web_contents.get()),
      task_id_(task_id),
      session_instance_(std::move(session_instance)),
      session_receiver_(this, std::move(session_receiver)),
      share_info_(std::move(share_info)),
      web_contents_(std::move(web_contents)) {
  session_receiver_.set_disconnect_handler(base::BindOnce(
      &NearbyShareSessionImpl::Close, weak_ptr_factory_.GetWeakPtr()));

  aura::Window* arc_window = GetArcWindow(task_id_);
  if (arc_window) {
    VLOG(1) << "ARC window found. Creating NearbySession.";
    ShowNearbyBubble(std::move(arc_window));
  } else {
    VLOG(1) << "No ARC window found for task ID " << task_id_;
    env_observation_.Observe(aura::Env::GetInstance());
    window_initialization_timer_.Start(
        FROM_HERE, kWindowInitializationTimeout,
        base::BindOnce(&NearbyShareSessionImpl::OnTimerFired,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void NearbyShareSessionImpl::ShowNearbyBubble(aura::Window* arc_window) {
  // Create ARC Custom Tab for given ARC window
  custom_tab_ = std::make_unique<CustomTab>(arc_window);

  // Attach |web_contents_| to the Custom Tab and show it.
  aura::Window* window = web_contents_->GetNativeView();
  custom_tab_->Attach(window);
  window->Show();

  VLOG(1) << "Getting Sharesheet service";
  content::BrowserContext* browser_context = web_contents_->GetBrowserContext();
  sharesheet::SharesheetService* sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  if (!sharesheet_service) {
    LOG(ERROR) << "Cannot find sharesheet service";
    return;
  }

  VLOG(1) << "Calling ShowNearbyShareBubble";
  sharesheet_service->ShowNearbyShareBubble(
      web_contents_.get(), ConvertShareIntentInfoToIntentFilter(),
      sharesheet::SharesheetMetrics::LaunchSource::kArcNearbyShare,
      base::BindOnce(&NearbyShareSessionImpl::OnNearbyShareBubbleShown,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&NearbyShareSessionImpl::OnNearbyShareClosed,
                     weak_ptr_factory_.GetWeakPtr()));
}

apps::mojom::IntentPtr
NearbyShareSessionImpl::ConvertShareIntentInfoToIntentFilter() const {
  // Sharing files & text
  if (share_info_->files.has_value()) {
    std::vector<GURL> content_urls;
    std::vector<std::string> file_mime_types;
    for (const mojom::FileInfoPtr& file_info : share_info_->files.value()) {
      content_urls.emplace_back(GURL(file_info->content_uri));
      file_mime_types.emplace_back(file_info->mime_type);
    }
    std::string text;
    if (share_info_->extras.has_value() &&
        share_info_->extras->contains(kIntentExtraText)) {
      text = share_info_->extras->at(kIntentExtraText);
    }
    return apps_util::CreateShareIntentFromFiles(content_urls, file_mime_types,
                                                 text, share_info_->title);
  }

  // Sharing only text
  if (share_info_->extras.has_value() &&
      share_info_->extras->contains(kIntentExtraText)) {
    apps::mojom::IntentPtr share_intent = apps_util::CreateShareIntentFromText(
        share_info_->extras->at(kIntentExtraText), share_info_->title);
    share_intent->mime_type = share_info_->mime_type;
    return share_intent;
  }
  VLOG(1) << "No Sharing info found";
  return nullptr;
}

void NearbyShareSessionImpl::OnNearbyShareBubbleShown(
    sharesheet::SharesheetResult result) {
  if (VLOG_IS_ON(1)) {
    switch (result) {
      case sharesheet::SharesheetResult::kSuccess:
        VLOG(1) << "OnNearbyShareBubbleShown: SUCCESS";
        break;
      case sharesheet::SharesheetResult::kCancel:
        VLOG(1) << "OnNearbyShareBubbleShown: CANCEL";
        break;
      case sharesheet::SharesheetResult::kErrorAlreadyOpen:
        VLOG(1) << "OnNearbyShareBubbleShown: ALREADY OPEN";
        break;
      default:
        VLOG(1) << "OnNearbyShareBubbleShown: UNKNOWN";
    }
  }
  if (result != sharesheet::SharesheetResult::kSuccess) {
    session_instance_->OnNearbyShareViewClosed();
  }
}

void NearbyShareSessionImpl::OnTimerFired() {
  // TODO(phshah): Handle error case and add UMA metric.
  LOG(ERROR) << " ARC window didn't get initialized in time.";
  env_observation_.Reset();
}

void NearbyShareSessionImpl::Close() {
  web_contents_->RemoveUserData(UserDataKey());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(NearbyShareSessionImpl)

}  // namespace arc
