// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ANNOTATOR_ANNOTATION_TRAY_H_
#define ASH_ANNOTATOR_ANNOTATION_TRAY_H_

#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/view_click_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"

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
// TODO(b/352729094): Consolidate the pen vs. marker terminology.
constexpr SkColor kAnnotatorMagentaPenColor = SkColorSetRGB(0xFF, 0x00, 0xE5);
constexpr SkColor kAnnotatorRedPenColor = SkColorSetRGB(0xE9, 0x42, 0x35);
constexpr SkColor kAnnotatorYellowPenColor = SkColorSetRGB(0xFB, 0xF1, 0x04);
constexpr SkColor kAnnotatorBluePenColor = SkColorSetRGB(0x42, 0x85, 0xF4);
constexpr SkColor kAnnotatorDefaultPenColor = kAnnotatorMagentaPenColor;

// Status area tray which allows you to access the annotation tools.
class AnnotationTray : public TrayBackgroundView,
                                public SessionObserver {
  METADATA_HEADER(AnnotationTray, TrayBackgroundView)

 public:
  explicit AnnotationTray(Shelf* shelf);
  AnnotationTray(const AnnotationTray&) = delete;
  AnnotationTray& operator=(const AnnotationTray&) = delete;
  ~AnnotationTray() override;

  // TrayBackgroundView:
  void OnGestureEvent(ui::GestureEvent* event) override;
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override;
  void UpdateTrayItemColor(bool is_active) override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void CloseBubbleInternal() override;
  void ShowBubble() override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  void OnThemeChanged() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  void OnTrayButtonPressed(const ui::Event& event);
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

  // Sets the image with the color that corresponds to the active state.
  void SetIconImage(bool is_active);

  // Image view of the tray icon.
  const raw_ptr<views::ImageView> image_view_;

  raw_ptr<HoverHighlightView> pen_view_;

  // The bubble that appears after clicking the annotation tools tray button.
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  // The last selected pen color.
  SkColor current_pen_color_ = kAnnotatorDefaultPenColor;

  base::ScopedObservation<SessionControllerImpl, SessionObserver>
      session_observer_{this};
};

}  // namespace ash

#endif  // ASH_ANNOTATOR_ANNOTATION_TRAY_H_
