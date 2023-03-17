// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_MORE_APPS_BUTTON_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_MORE_APPS_BUTTON_H_

#include <vector>

#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/table_layout.h"

namespace ash {

class AppLoadingIcon;

// A view in phone hub that displays the first three apps in a user's app
// drawer, as well as a count of how many apps they have on their phone.
class VIEWS_EXPORT PhoneHubMoreAppsButton
    : public views::Button,
      phonehub::AppStreamLauncherDataModel::Observer {
 public:
  METADATA_HEADER(PhoneHubMoreAppsButton);

  // Only use this constructor to create a skeleton view for the LoadingView.
  // Does not process click events or load app icons.
  PhoneHubMoreAppsButton();

  PhoneHubMoreAppsButton(
      phonehub::AppStreamLauncherDataModel* app_stream_launcher_data_model,
      views::Button::PressedCallback callback);

  PhoneHubMoreAppsButton(const PhoneHubMoreAppsButton&) = delete;
  PhoneHubMoreAppsButton& operator=(const PhoneHubMoreAppsButton&) = delete;
  ~PhoneHubMoreAppsButton() override;

  void StartLoadingAnimation(absl::optional<base::TimeDelta> initial_delay);
  void StopLoadingAnimation();

  // phonehub::AppStreamLauncherDataModel::Observer:
  void OnShouldShowMiniLauncherChanged() override;
  void OnAppListChanged() override;

 private:
  void InitLayout();
  void LoadAppList();

  base::TimeTicks load_app_list_latency_ = base::TimeTicks();
  views::TableLayout* table_layout_ = nullptr;
  phonehub::AppStreamLauncherDataModel* app_stream_launcher_data_model_ =
      nullptr;
  std::vector<AppLoadingIcon*> app_loading_icons_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_MORE_APPS_BUTTON_H_
