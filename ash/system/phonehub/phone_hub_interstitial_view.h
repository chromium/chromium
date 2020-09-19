// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_INTERSTITIAL_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_INTERSTITIAL_VIEW_H_

#include <vector>

#include "ash/ash_export.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Button;
class ImageView;
class ImageSkia;
class Label;
class ProgressBar;
}  // namespace views

namespace ash {

// A generic view to display interstitial pages for the Phone Hub feature with
// image, text and buttons in a customized layout. It is reused by the
// onboarding, loading, disconnected/reconnecting and error state UI.
class ASH_EXPORT PhoneHubInterstitialView : public views::View {
 public:
  METADATA_HEADER(PhoneHubInterstitialView);

  explicit PhoneHubInterstitialView(bool show_progress);
  PhoneHubInterstitialView(const PhoneHubInterstitialView&) = delete;
  PhoneHubInterstitialView& operator=(const PhoneHubInterstitialView&) = delete;
  ~PhoneHubInterstitialView() override;

  void SetImage(const gfx::ImageSkia& image);
  void SetTitle(const base::string16& title);
  void SetDescription(const base::string16& desc);
  void AddButton(std::unique_ptr<views::Button> button);

 private:
  void InitLayout(bool show_progress);

  // A progress bar will be shown under the title row if |show_progress| is
  // true.
  views::ProgressBar* progress_bar_ = nullptr;
  views::ImageView* image_ = nullptr;
  views::Label* title_ = nullptr;
  views::Label* description_ = nullptr;
  views::View* button_container_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_INTERSTITIAL_VIEW_H_
