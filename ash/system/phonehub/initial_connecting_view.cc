// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/initial_connecting_view.h"

#include <algorithm>
#include <memory>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/phonehub/phone_hub_interstitial_view.h"
#include "base/strings/string16.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

InitialConnectingView::InitialConnectingView() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  content_view_ = AddChildView(
      std::make_unique<PhoneHubInterstitialView>(/*show_progress=*/true));

  // TODO(crbug.com/1127996): Replace PNG file with vector icon.
  gfx::ImageSkia* image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_PHONE_HUB_CONNECTING_IMAGE);
  content_view_->SetImage(*image);
  content_view_->SetTitle(l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_INITIAL_CONNECTING_DIALOG_TITLE));
  content_view_->SetDescription(l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_INITIAL_CONNECTING_DIALOG_DESCRIPTION));
}

InitialConnectingView::~InitialConnectingView() = default;

BEGIN_METADATA(InitialConnectingView, views::View)
END_METADATA

}  // namespace ash
