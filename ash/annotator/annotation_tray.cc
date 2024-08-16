// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/annotator/annotation_tray.h"

#include "ash/annotator/annotator_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/ui/projector_color_button.h"
#include "ash/public/cpp/annotator/annotator_tool.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Margins between the title view and the edges around it (dp).
constexpr int kPaddingBetweenBottomAndLastTrayItem = 4;

// Width of the bubble itself (dp).
constexpr int kBubbleWidth = 196;

// Insets for the views (dp).
constexpr auto kPenViewPadding = gfx::Insets::TLBR(4, 16, 0, 16);

// Spacing between buttons (dp).
constexpr int kButtonsPadding = 12;

// Size of menu rows.
constexpr int kMenuRowHeight = 48;

// Color selection buttons.
constexpr int kColorButtonColorViewSize = 16;
constexpr int kColorButtonViewRadius = 28;

constexpr SkColor kPenColors[] = {
    kAnnotatorMagentaPenColor, kAnnotatorBluePenColor, kAnnotatorYellowPenColor,
    kAnnotatorRedPenColor};

bool IsAnnotatorEnabled() {
  auto* controller = Shell::Get()->annotator_controller();
  // `controller` may not be available yet as the `AnnotationTray`
  // is created before it.
  return controller && controller->is_annotator_enabled();
}

AnnotatorToolType GetCurrentTool() {
  return IsAnnotatorEnabled() ?
    AnnotatorToolType::kMarker : AnnotatorToolType::kToolNone;
}

const gfx::VectorIcon& GetIconForTool(AnnotatorToolType tool, SkColor color) {
  if(tool == AnnotatorToolType::kToolNone){
     return kPaletteTrayIconProjectorIcon;
  }
  if(tool == AnnotatorToolType::kMarker){
    switch (color) {
        case kAnnotatorMagentaPenColor:
          return kPaletteTrayIconProjectorMagentaIcon;
        case kAnnotatorBluePenColor:
          return kPaletteTrayIconProjectorBlueIcon;
        case kAnnotatorRedPenColor:
          return kPaletteTrayIconProjectorRedIcon;
        case kAnnotatorYellowPenColor:
          return kPaletteTrayIconProjectorYellowIcon;
      }
  }

  NOTREACHED();
}

}  // namespace

AnnotationTray::AnnotationTray(Shelf* shelf)
    : TrayBackgroundView(shelf,
                         TrayBackgroundViewCatalogName::kProjectorAnnotation),
      image_view_(
          tray_container()->AddChildView(std::make_unique<views::ImageView>())),
      pen_view_(nullptr) {
  SetCallback(base::BindRepeating(&AnnotationTray::OnTrayButtonPressed,
                                  base::Unretained(this)));

  // Right click should show the bubble. In tablet mode, long press is
  // synonymous with right click, gesture long press must be intercepted via
  // `OnGestureEvent()` override, as `views::Button` forces long press to show a
  // contextual menu.
  SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                           ui::EF_RIGHT_MOUSE_BUTTON);

  image_view_->SetTooltipText(GetTooltip());
  image_view_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize));
  ResetTray();

  session_observer_.Observe(Shell::Get()->session_controller());
}

AnnotationTray::~AnnotationTray() = default;

void AnnotationTray::OnGestureEvent(ui::GestureEvent* event) {
  // Long Press typically is used to show a contextual menu, but because in
  // tablet mode tapping the pod is used to toggle a feature, long press is the
  // only available way to show the bubble.
  // TODO(crbug.com/40242435): Put this where we handle other button
  // activations, once the `views::Button` code allows it.
  if (event->details().type() != ui::EventType::kGestureLongPress) {
    TrayBackgroundView::OnGestureEvent(event);
    return;
  }

  ShowBubble();
}

void AnnotationTray::ClickedOutsideBubble(
    const ui::LocatedEvent& event) {
  CloseBubble();
}

void AnnotationTray::UpdateTrayItemColor(bool is_active) {
  SetIconImage(is_active);
}

std::u16string AnnotationTray::GetAccessibleNameForTray() {
  std::u16string enabled_state = l10n_util::GetStringUTF16(
      GetCurrentTool() == AnnotatorToolType::kToolNone
          ? IDS_ASH_STATUS_AREA_PROJECTOR_ANNOTATION_TRAY_OFF_STATE
          : IDS_ASH_STATUS_AREA_PROJECTOR_ANNOTATION_TRAY_ON_STATE);
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_AREA_PROJECTOR_ANNOTATION_TRAY_ACCESSIBLE_TITLE,
      enabled_state);
}

void AnnotationTray::HandleLocaleChange() {}

void AnnotationTray::HideBubbleWithView(
    const TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view)
    CloseBubble();
}

void AnnotationTray::CloseBubbleInternal() {
  pen_view_ = nullptr;
  bubble_.reset();
  // Annotator can be enabled after closing the bubble so set the activity state
  // according to it.
  SetIsActive(IsAnnotatorEnabled());
  shelf()->UpdateAutoHideState();
}

void AnnotationTray::ShowBubble() {
  if (bubble_)
    return;

  DCHECK(tray_container());

  TrayBubbleView::InitParams init_params = CreateInitParamsForTrayBubble(this);
  init_params.preferred_width = kBubbleWidth;

  // Create and customize bubble view.
  auto bubble_view = std::make_unique<TrayBubbleView>(init_params);
  bubble_view->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, 0, kPaddingBetweenBottomAndLastTrayItem, 0)));

  // Add drawing tools
  {
    auto* marker_view_container =
        bubble_view->AddChildView(std::make_unique<views::View>());

    auto box_layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, kPenViewPadding,
        kButtonsPadding);
    box_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    box_layout->set_minimum_cross_axis_size(kMenuRowHeight);
    marker_view_container->SetLayoutManager(std::move(box_layout));

    for (SkColor color : kPenColors) {
      auto* color_button = marker_view_container->AddChildView(
          std::make_unique<ProjectorColorButton>(
              base::BindRepeating(&AnnotationTray::OnPenColorPressed,
                                  base::Unretained(this), color),
              color, kColorButtonColorViewSize, kColorButtonViewRadius,
              l10n_util::GetStringUTF16(GetAccessibleNameForColor(color))));
      color_button->SetToggled(current_pen_color_ == color);
    }
  }

  // Show the bubble.
  bubble_ = std::make_unique<TrayBubbleWrapper>(this);
  bubble_->ShowBubble(std::move(bubble_view));
  SetIsActive(true);
}

TrayBubbleView* AnnotationTray::GetBubbleView() {
  return bubble_ ? bubble_->bubble_view() : nullptr;
}

views::Widget* AnnotationTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->GetBubbleWidget() : nullptr;
}

void AnnotationTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  UpdateIcon();
}

void AnnotationTray::HideBubble(const TrayBubbleView* bubble_view) {
  CloseBubble();
}

void AnnotationTray::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  const uint64_t color =
      pref_service->GetUint64(prefs::kProjectorAnnotatorLastUsedMarkerColor);
  current_pen_color_ = !color ? kAnnotatorDefaultPenColor : color;
}

void AnnotationTray::HideAnnotationTray() {
  SetVisiblePreferred(false);
  UpdateIcon();
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  pref_service->SetUint64(prefs::kProjectorAnnotatorLastUsedMarkerColor,
                          current_pen_color_);
  ResetTray();
}

void AnnotationTray::OnTrayButtonPressed(const ui::Event& event) {
  // NOTE: Long press not supported via the `views::Button` callback, it
  // is handled via OnGestureEvent override.
  if (event.IsMouseEvent() && event.AsMouseEvent()->IsRightMouseButton()) {
    ShowBubble();
    return;
  }

  ToggleAnnotator();
}

void AnnotationTray::SetTrayEnabled(bool enabled) {
  SetEnabled(enabled);
  if (enabled)
    return;

  // For disabled state, set icon color to kIconColorPrimary with 30% opacity.
  SkColor disabled_icon_color =
      SkColorSetA(AshColorProvider::Get()->GetContentLayerColor(
                      AshColorProvider::ContentLayerType::kIconColorPrimary),
                  0x4D);
  image_view_->SetImage(gfx::CreateVectorIcon(kPaletteTrayIconProjectorIcon,
                                              disabled_icon_color));
  image_view_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_AREA_PROJECTOR_ANNOTATION_TRAY_UNAVAILABLE));
}

void AnnotationTray::ToggleAnnotator() {
  if (GetCurrentTool() == AnnotatorToolType::kToolNone) {
    EnableAnnotatorWithPenColor();
  } else {
    DeactivateActiveTool();
  }
  if (bubble_) {
    CloseBubble();
  }
  UpdateIcon();
}

void AnnotationTray::EnableAnnotatorWithPenColor() {
  auto* controller = Shell::Get()->annotator_controller();
  DCHECK(controller);
  AnnotatorTool tool;
  tool.color = current_pen_color_;
  controller->SetAnnotatorTool(tool);
  controller->EnableAnnotatorTool();
}

void AnnotationTray::DeactivateActiveTool() {
  auto* controller = Shell::Get()->annotator_controller();
  DCHECK(controller);
  controller->ResetTools();
}

void AnnotationTray::UpdateIcon() {
  bool annotator_toggled = false;
  if (is_active() != IsAnnotatorEnabled()) {
    SetIsActive(IsAnnotatorEnabled());
    annotator_toggled = true;
  }
  // Only sets the image if Jelly is not enabled or if the annotator was not
  // toggled, since `UpdateTrayItemColor()` will be called in `SetIsActive()` to
  // set the image for Jelly only when active state changes.
  if (!chromeos::features::IsJellyEnabled()) {
    image_view_->SetImage(gfx::CreateVectorIcon(
        GetIconForTool(GetCurrentTool(), current_pen_color_),
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPrimary)));
  } else if (!annotator_toggled) {
    SetIconImage(is_active());
  }
  image_view_->SetTooltipText(GetTooltip());
}

void AnnotationTray::OnPenColorPressed(SkColor color) {
  current_pen_color_ = color;
  EnableAnnotatorWithPenColor();
  CloseBubble();
  UpdateIcon();
}

int AnnotationTray::GetAccessibleNameForColor(SkColor color) {
  switch (color) {
    case kAnnotatorRedPenColor:
      return IDS_RED_COLOR_BUTTON;
    case kAnnotatorBluePenColor:
      return IDS_BLUE_COLOR_BUTTON;
    case kAnnotatorYellowPenColor:
      return IDS_YELLOW_COLOR_BUTTON;
    case kAnnotatorMagentaPenColor:
      return IDS_MAGENTA_COLOR_BUTTON;
  }
  NOTREACHED();
}

void AnnotationTray::ResetTray() {
  // Disable the tray icon. It is enabled once the ink canvas is initialized.
  SetEnabled(false);
}

std::u16string AnnotationTray::GetTooltip() {
  std::u16string enabled_state = l10n_util::GetStringUTF16(
      GetCurrentTool() == AnnotatorToolType::kToolNone
          ? IDS_ASH_STATUS_AREA_PROJECTOR_ANNOTATION_TRAY_OFF_STATE
          : IDS_ASH_STATUS_AREA_PROJECTOR_ANNOTATION_TRAY_ON_STATE);
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_AREA_PROJECTOR_ANNOTATION_TRAY_TOOLTIP, enabled_state);
}

void AnnotationTray::SetIconImage(bool is_active) {
  DCHECK(chromeos::features::IsJellyEnabled());
  image_view_->SetImage(ui::ImageModel::FromVectorIcon(
      GetIconForTool(GetCurrentTool(), current_pen_color_),
      is_active ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                : cros_tokens::kCrosSysOnSurface));
}

BEGIN_METADATA(AnnotationTray)
END_METADATA

}  // namespace ash
