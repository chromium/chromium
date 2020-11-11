// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "base/scoped_observer.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class PrefChangeRegistrar;

namespace views {
class ImageView;
}  // namespace views

namespace ash {

class HoldingSpaceTrayIconPreview;
class Shelf;

// The icon used to represent holding space in its tray in the shelf.
class ASH_EXPORT HoldingSpaceTrayIcon : public views::View,
                                        public HoldingSpaceControllerObserver,
                                        public HoldingSpaceModelObserver,
                                        public ShellObserver,
                                        public SessionObserver {
 public:
  METADATA_HEADER(HoldingSpaceTrayIcon);

  explicit HoldingSpaceTrayIcon(Shelf* shelf);
  HoldingSpaceTrayIcon(const HoldingSpaceTrayIcon&) = delete;
  HoldingSpaceTrayIcon& operator=(const HoldingSpaceTrayIcon&) = delete;
  ~HoldingSpaceTrayIcon() override;

  // Invoked when the system locale has changed.
  void OnLocaleChanged();

  // Returns the shelf associated with this holding space tray icon.
  Shelf* shelf() { return shelf_; }

 private:
  // views::View:
  base::string16 GetTooltipText(const gfx::Point& point) const override;
  int GetHeightForWidth(int width) const override;

  // HoldingSpaceControllerObserver:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model) override;
  void OnHoldingSpaceModelDetached(HoldingSpaceModel* model) override;

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemAdded(const HoldingSpaceItem* item) override;
  void OnHoldingSpaceItemRemoved(const HoldingSpaceItem* item) override;
  void OnHoldingSpaceItemFinalized(const HoldingSpaceItem* item) override;

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  // SessionController:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  void InitLayout();
  void UpdatePreferredSize();
  void UpdatePreviewsEnabled();

  void ShowPreviews();
  void HidePreviews();

  // Invoked when the specified preview has completed animating out. At this
  // point it is owned by `removed_previews_` and should be destroyed.
  void OnHoldingSpaceTrayIconPreviewAnimatedOut(HoldingSpaceTrayIconPreview*);

  // The shelf associated with this holding space tray icon.
  Shelf* const shelf_;

  // The child of this holding space tray icon which should be visible only if
  // previews are disabled or there are no previews available.
  views::ImageView* no_previews_image_view_ = nullptr;

  // Whether previews are currently enabled. Note that if the previews feature
  // is disabled, this will always be false. Otherwise, previews can be enabled/
  // disabled by the user at runtime.
  bool previews_enabled_ = false;

  // A preview is added to the tray icon to visually represent each holding
  // space item. Upon creation, previews are added to `previews_` where they
  // are owned until being animated out. Upon being animated out, previews are
  // moved to `removed_previews_` where they are owned until the out animation
  // completes and they are subsequently destroyed.
  std::vector<std::unique_ptr<HoldingSpaceTrayIconPreview>> previews_;
  std::vector<std::unique_ptr<HoldingSpaceTrayIconPreview>> removed_previews_;

  // When the holding space previews feature is enabled, the user can enable/
  // disable previews at runtime. This registrar is associated with the active
  // user pref service and notifies the holding space tray icon of changes to
  // the user's preference.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  ScopedObserver<HoldingSpaceController, HoldingSpaceControllerObserver>
      controller_observer_{this};
  ScopedObserver<HoldingSpaceModel, HoldingSpaceModelObserver> model_observer_{
      this};
  ScopedObserver<Shell,
                 ShellObserver,
                 &Shell::AddShellObserver,
                 &Shell::RemoveShellObserver>
      shell_observer_{this};
  ScopedObserver<SessionController, SessionObserver> session_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_H_
