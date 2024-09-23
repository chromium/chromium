// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_APP_STREAM_LAUNCHER_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_APP_STREAM_LAUNCHER_VIEW_H_

#include <cstdint>
#include <memory>

#include "ash/ash_export.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/recent_app_click_observer.h"
#include "chromeos/ash/components/phonehub/recent_apps_interaction_handler.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class Button;
}
namespace ash {

namespace phonehub {
class PhoneHubManager;
}

// A view of the Phone Hub panel, displaying the apps that user can launch for
// app streaming.
class ASH_EXPORT AppStreamLauncherView
    : public PhoneHubContentView,
      public phonehub::AppStreamLauncherDataModel::Observer {
  METADATA_HEADER(AppStreamLauncherView, PhoneHubContentView)
 public:
  explicit AppStreamLauncherView(phonehub::PhoneHubManager* phone_hub_manager);
  ~AppStreamLauncherView() override;

  // views::View:
  void ChildPreferredSizeChanged(View* child) override;
  void ChildVisibilityChanged(View* child) override;

  // PhoneHubContentView:
  phone_hub_metrics::Screen GetScreenForMetrics() const override;
  void OnBubbleClose() override;

  // phonehub::AppStreamLauncherDataModel::Observer:
  void OnAppListChanged() override;

  views::View* items_container_for_test() { return items_container_; }

 private:
  friend class AppStreamLauncherViewTest;
  FRIEND_TEST_ALL_PREFIXES(AppStreamLauncherViewTest, OpenView);
  FRIEND_TEST_ALL_PREFIXES(AppStreamLauncherViewTest, AddItems);
  FRIEND_TEST_ALL_PREFIXES(AppStreamLauncherViewTest, ClickOnItem);

  std::unique_ptr<views::View> CreateAppListView();
  std::unique_ptr<views::View> CreateItemView(
      const phonehub::Notification::AppMetadata& app);
  std::unique_ptr<views::View> CreateListItemView(
      const phonehub::Notification::AppMetadata& app);
  std::unique_ptr<views::View> CreateHeaderView();
  std::unique_ptr<views::Button> CreateButton(
      views::Button::PressedCallback callback,
      const gfx::VectorIcon& icon,
      int message_id);
  void AppIconActivated(phonehub::Notification::AppMetadata app);

  // Update the UI based on the information in the data model.
  void UpdateFromDataModel();

  // Handles the click on the "back" arrow in the header.
  void OnArrowBackActivated();

  void CreateListView(
      const std::vector<phonehub::Notification::AppMetadata>* apps_list);
  void CreateGridView(
      const std::vector<phonehub::Notification::AppMetadata>* apps_list);

  raw_ptr<views::Button, DanglingUntriaged> arrow_back_button_ = nullptr;
  raw_ptr<phonehub::PhoneHubManager> phone_hub_manager_;

  // Contains all the apps
  raw_ptr<views::View, DanglingUntriaged> items_container_;

  base::WeakPtrFactory<AppStreamLauncherView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_APP_STREAM_LAUNCHER_VIEW_H_
