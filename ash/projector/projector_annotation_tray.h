// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_ANNOTATION_TRAY_H_
#define ASH_PROJECTOR_PROJECTOR_ANNOTATION_TRAY_H_

#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/view_click_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"

namespace ui {
class GestureEvent;
}  // namespace ui

namespace views {
class ImageView;
class Widget;
}  // namespace views

namespace ash {

class HoverHighlightView;
class TrayBubbleWrapper;

// Pen colors.
constexpr SkColor kProjectorMagentaPenColor = SkColorSetRGB(0xFF, 0x00, 0xE5);
constexpr SkColor kProjectorRedPenColor = SkColorSetRGB(0xE9, 0x42, 0x35);
constexpr SkColor kProjectorYellowPenColor = SkColorSetRGB(0xFB, 0xF1, 0x04);
constexpr SkColor kProjectorBluePenColor = SkColorSetRGB(0x42, 0x85, 0xF4);
constexpr SkColor kProjectorDefaultPenColor = kProjectorMagentaPenColor;

// Status area tray which allows you to access the annotation tools for
// Projector.
class ProjectorAnnotationTray : public TrayBackgroundView,
                                public SessionObserver {
 public:
  explicit ProjectorAnnotationTray(Shelf* shelf);
  ProjectorAnnotationTray(const ProjectorAnnotationTray&) = delete;
  ProjectorAnnotationTray& operator=(const ProjectorAnnotationTray&) = delete;
  ~ProjectorAnnotationTray() override;

  // TrayBackgroundView:
  void OnGestureEvent(ui::GestureEvent* event) override;
  void ClickedOutsideBubble() override;
  void UpdateTrayItemColor(bool is_active) override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void CloseBubble() override;
  void ShowBubble() override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  void OnThemeChanged() override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  void HideAnnotationTray();
  void SetTrayEnabled(bool enabled);
  void ToggleAnnotator();

 private:
  void EnableAnnotatorWithPenColor();
  // Deactivates any annotation tool that is currently enabled and updates the
  // UI.
  void DeactivateActiveTool();

  // Updates the icon and tooltip of `image_view_` in the status area.
  void UpdateIcon();

  void OnPenColorPressed(SkColor color);

  // Returns the message ID of the accessible name for the color.
  int GetAccessibleNameForColor(SkColor color);

  // Resets the tray to its default state.
  void ResetTray();

  std::u16string GetTooltip();

  // Image view of the tray icon.
  const raw_ptr<views::ImageView, ExperimentalAsh> image_view_;

  raw_ptr<HoverHighlightView, ExperimentalAsh> pen_view_;

  // The bubble that appears after clicking the annotation tools tray button.
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  // The last selected pen color.
  SkColor current_pen_color_ = kProjectorDefaultPenColor;

  base::ScopedObservation<SessionControllerImpl, SessionObserver>
      session_observer_{this};
};

}  // namespace ash

#endif  // ASH_PROJECTOR_PROJECTOR_ANNOTATION_TRAY_H_
