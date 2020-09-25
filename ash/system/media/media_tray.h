// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_MEDIA_TRAY_H_
#define ASH_SYSTEM_MEDIA_MEDIA_TRAY_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/media_notification_provider_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/unified/top_shortcut_button.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace views {
class ImageView;
}  // namespace views

namespace ash {

class Shelf;
class TrayBubbleWrapper;

class ASH_EXPORT MediaTray : public MediaNotificationProviderObserver,
                             public TrayBackgroundView,
                             public SessionObserver {
 public:
  // Register prefs::kGlobalMediaControlsPinned.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns if global media controls is pinned to shelf.
  static bool IsPinnedToShelf();

  // Set kGlobalMediaControlsPinned.
  static void SetPinnedToShelf(bool pinned);

  // Pin button showed in media tray bubble's title view and media controls
  // detailed view's title view.
  class PinButton : public TopShortcutButton, public ButtonListener {
   public:
    PinButton();
    ~PinButton() override = default;

    // views::ButtonListener implementation.
    void ButtonPressed(views::Button* sender, const ui::Event& event) override;
  };

  explicit MediaTray(Shelf* shelf);
  ~MediaTray() override;

  // MediaNotificationProviderObserver implementations.
  void OnNotificationListChanged() override;
  void OnNotificationListViewSizeChanged() override;

  // TrayBackgroundview implementations.
  base::string16 GetAccessibleNameForTray() override;
  void UpdateAfterLoginStatusChange() override;
  void HandleLocaleChange() override;
  bool PerformAction(const ui::Event& event) override;
  void ShowBubble(bool show_by_click) override;
  void CloseBubble() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;

  // SessionObserver implementation.
  void OnLockStateChanged(bool locked) override;
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // Show/hide media tray.
  void UpdateDisplayState();

  TrayBubbleWrapper* tray_bubble_wrapper_for_testing() { return bubble_.get(); }

  views::View* pin_button_for_testing() { return pin_button_; }

 private:
  friend class MediaTrayTest;

  // Called when global media controls pin pref is changed.
  void OnGlobalMediaControlsPinPrefChanged();

  // Ptr to pin button in the dialog, owned by the view hierarchy.
  views::Button* pin_button_ = nullptr;

  std::unique_ptr<TrayBubbleWrapper> bubble_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  views::ImageView* icon_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_MEDIA_TRAY_H_
