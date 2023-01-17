// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_MORE_APPS_BUTTON_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_MORE_APPS_BUTTON_H_

#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"

namespace ash {

// A view in phone hub that displays the first three apps in a user's app
// drawer, as well as a count of how many apps they have on their phone.
class VIEWS_EXPORT PhoneHubMoreAppsButton
    : public views::View,
      phonehub::AppStreamLauncherDataModel::Observer {
 public:
  METADATA_HEADER(PhoneHubMoreAppsButton);

  explicit PhoneHubMoreAppsButton(
      phonehub::AppStreamLauncherDataModel* app_stream_launcher_data_model);
  PhoneHubMoreAppsButton(const PhoneHubMoreAppsButton&) = delete;
  PhoneHubMoreAppsButton& operator=(const PhoneHubMoreAppsButton&) = delete;
  ~PhoneHubMoreAppsButton() override;

  // phonehub::AppStreamLauncherDataModel
  void OnShouldShowMiniLauncherChanged() override;
  void OnAppListChanged() override;

 private:
  void InitLayout();
  void LoadAppList();

  views::TableLayout* table_layout_ = nullptr;
  phonehub::AppStreamLauncherDataModel* app_stream_launcher_data_model_;
};
}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_MORE_APPS_BUTTON_H_