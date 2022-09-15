// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_ARC_GHOST_WINDOW_VIEW_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_ARC_GHOST_WINDOW_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
}  // namespace views

namespace ash::full_restore {

// The view of ARC ghost window content. It shows the icon of app and a
// throbber. It is used on ARC ghost window shell surface overlay, so it will
// be destroyed after actual ARC task window launched.
class ArcGhostWindowView : public views::View {
 public:
  METADATA_HEADER(ArcGhostWindowView);

  explicit ArcGhostWindowView(int throbber_diameter, uint32_t theme_color);
  ArcGhostWindowView(const ArcGhostWindowView&) = delete;
  ArcGhostWindowView operator=(const ArcGhostWindowView&) = delete;
  ~ArcGhostWindowView() override;

  void LoadIcon(const std::string& app_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(ArcGhostWindowViewTest, IconLoadTest);

  void InitLayout(uint32_t theme_color, int diameter);
  void OnIconLoaded(apps::IconValuePtr icon_value);

  views::ImageView* icon_view_;
  base::OnceCallback<void(apps::IconValuePtr icon_value)>
      icon_loaded_cb_for_testing_;

  base::WeakPtrFactory<ArcGhostWindowView> weak_ptr_factory_{this};
};

}  // namespace ash::full_restore

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_ARC_GHOST_WINDOW_VIEW_H_
