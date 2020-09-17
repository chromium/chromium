// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_INTERSTITIAL_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_INTERSTITIAL_VIEW_H_

#include <vector>

#include "ash/ash_export.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Button;
class ImageView;
class ImageSkia;
class Label;
}  // namespace views

namespace ash {

// A interstitial view for Phone Hub with a customized layout that can be shared
// by the initial onboarding, connecting/disconnecting and error state UI.
class ASH_EXPORT PhoneHubInterstitialView : public views::View {
 public:
  METADATA_HEADER(PhoneHubInterstitialView);

  PhoneHubInterstitialView();
  PhoneHubInterstitialView(const PhoneHubInterstitialView&) = delete;
  PhoneHubInterstitialView& operator=(const PhoneHubInterstitialView&) = delete;
  ~PhoneHubInterstitialView() override;

  void SetImage(const gfx::ImageSkia& image);
  void SetTitle(const base::string16& title);
  void SetDescription(const base::string16& desc);
  void SetButtons(const std::vector<views::Button*>& buttons);

 private:
  void InitLayout();

  views::ImageView* image_ = nullptr;
  views::Label* title_ = nullptr;
  views::Label* description_ = nullptr;
  views::View* button_container_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_INTERSTITIAL_VIEW_H_
