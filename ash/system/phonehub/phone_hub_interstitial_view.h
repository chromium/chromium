// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_INTERSTITIAL_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_INTERSTITIAL_VIEW_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/view.h"

namespace ui {
class ImageModel;
}  // namespace ui

namespace views {
class BoxLayoutView;
class Button;
class ImageView;
class Label;
class ProgressBar;
}  // namespace views

namespace ash {

// A generic view to display interstitial pages for the Phone Hub feature with
// image, text and buttons in a customized layout. It is reused by the
// onboarding, loading, disconnected/reconnecting and error state UI.
class ASH_EXPORT PhoneHubInterstitialView : public PhoneHubContentView {
  METADATA_HEADER(PhoneHubInterstitialView, PhoneHubContentView)

 public:
  explicit PhoneHubInterstitialView(bool show_progress, bool show_image = true);
  PhoneHubInterstitialView(const PhoneHubInterstitialView&) = delete;
  PhoneHubInterstitialView& operator=(const PhoneHubInterstitialView&) = delete;
  ~PhoneHubInterstitialView() override;

  void SetImage(const ui::ImageModel& image_model);
  void SetTitle(const std::u16string& title);
  void SetDescription(const std::u16string& desc);
  void AddButton(std::unique_ptr<views::Button> button);

 private:
  // A progress bar will be shown under the title row if |show_progress| is
  // true.
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;
  raw_ptr<views::ImageView> image_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> description_ = nullptr;
  raw_ptr<views::BoxLayoutView> button_container_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_INTERSTITIAL_VIEW_H_
