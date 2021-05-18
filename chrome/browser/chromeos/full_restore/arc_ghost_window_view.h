// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_GHOST_WINDOW_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_GHOST_WINDOW_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
}

namespace chromeos {
namespace full_restore {

// The view of ARC ghost window content. It shows the icon of app and a
// throbber. It is used on ARC ghost window shell surface overlay, so it will
// be destroyed after actual ARC task window launched.
class ArcGhostWindowView : public views::View {
 public:
  explicit ArcGhostWindowView(int throbber_diameter,
                              const std::string& app_id,
                              uint32_t theme_color);
  ArcGhostWindowView(const ArcGhostWindowView&) = delete;
  ArcGhostWindowView operator=(const ArcGhostWindowView&) = delete;
  ~ArcGhostWindowView() override;

 private:
  void InitLayout(uint32_t theme_color, int diameter);

  void LoadIcon(const std::string& app_id);
  void OnIconLoaded(apps::mojom::IconType icon_type,
                    apps::mojom::IconValuePtr icon_value);

  views::ImageView* icon_view_;

  base::WeakPtrFactory<ArcGhostWindowView> weak_ptr_factory_{this};
};

}  // namespace full_restore
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_GHOST_WINDOW_VIEW_H_
