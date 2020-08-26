// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/sharesheet/nearby_share_action.h"

#include "base/logging.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/webview/webview.h"

namespace {

constexpr int kCornerRadius = 12;

gfx::Size ComputeSize() {
  // TODO(vecore): compute expected size based on screen size
  return {500, 500};
}

}  // namespace

NearbyShareAction::NearbyShareAction() = default;

NearbyShareAction::~NearbyShareAction() = default;

const base::string16 NearbyShareAction::GetActionName() {
  return l10n_util::GetStringUTF16(IDS_NEARBY_SHARE_FEATURE_NAME);
}

const gfx::ImageSkia NearbyShareAction::GetActionIcon() {
  return gfx::CreateVectorIcon(kNearbyShareIcon);
}

void NearbyShareAction::LaunchAction(
    sharesheet::SharesheetController* controller,
    views::View* root_view,
    apps::mojom::IntentPtr intent) {
  // Store so we can trigger the share sheet close later.
  controller_ = controller;

  gfx::Size size = ComputeSize();
  controller->SetSharesheetSize(size.width(), size.height());

  // TODO(vecore): SharesheetController will eventually provide the profile.
  auto* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  auto view = std::make_unique<views::WebView>(profile);
  // If this is not done, we don't see anything in our view.
  view->SetPreferredSize(size);
  views::WebView* web_view = root_view->AddChildView(std::move(view));
  // TODO(vecore): Query this from the container view
  web_view->holder()->SetCornerRadii(gfx::RoundedCornersF(kCornerRadius));

  // load chrome://nearby into the webview
  web_view->LoadInitialURL(GURL(chrome::kChromeUINearbyShareURL));

  auto* webui = web_view->GetWebContents()->GetWebUI();
  DCHECK(webui != nullptr);

  nearby_ui_ =
      webui->GetController()->GetAs<nearby_share::NearbyShareDialogUI>();
  DCHECK(nearby_ui_ != nullptr);

  nearby_ui_->AddObserver(this);
  nearby_ui_->SetShareIntent(std::move(intent));
}

void NearbyShareAction::OnClose() {
  // The nearby WebUI requested to close through user action
  if (controller_) {
    controller_->CloseSharesheet();
  }
}

void NearbyShareAction::OnClosing(
    sharesheet::SharesheetController* controller) {
  if (nearby_ui_) {
    nearby_ui_->RemoveObserver(this);
    nearby_ui_ = nullptr;
  }
  controller_ = nullptr;
}
