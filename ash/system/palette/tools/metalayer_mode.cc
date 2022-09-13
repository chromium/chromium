// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/tools/metalayer_mode.h"

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "ash/system/palette/palette_ids.h"
#include "ash/system/palette/palette_utils.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

const char kToastId[] = "palette_metalayer_mode";

// Toast ID for toast that shows for long press stylus actions when metalayer
// mode is deprecated.
const char kDeprecateAssistantStylusToastId[] = "deprecate_assistant_stylus";

// Histogram for Assistant stylus features deprecation toast events.
const char kDeprecateStylusFeaturesToastEvent[] =
    "Ash.Shelf.Palette.Assistant.DeprecateStylusFeaturesToastEvent";

// If the last stroke happened within this amount of time,
// assume writing/sketching usage.
const int kMaxStrokeGapWhenWritingMs = 1000;

// Returns the last active user pref service.
PrefService* GetPrefs() {
  return Shell::Get()->session_controller()->GetLastActiveUserPrefService();
}

}  // namespace

MetalayerMode::MetalayerMode(Delegate* delegate) : CommonPaletteTool(delegate) {
  Shell::Get()->AddPreTargetHandler(this);
  AssistantState::Get()->AddObserver(this);
  Shell::Get()->highlighter_controller()->AddObserver(this);
}

MetalayerMode::~MetalayerMode() {
  Shell::Get()->highlighter_controller()->RemoveObserver(this);
  if (AssistantState::Get())
    AssistantState::Get()->RemoveObserver(this);
  Shell::Get()->RemovePreTargetHandler(this);
}

PaletteGroup MetalayerMode::GetGroup() const {
  return PaletteGroup::MODE;
}

PaletteToolId MetalayerMode::GetToolId() const {
  return PaletteToolId::METALAYER;
}

void MetalayerMode::OnEnable() {
  CommonPaletteTool::OnEnable();

  HighlighterController* controller = Shell::Get()->highlighter_controller();
  controller->SetExitCallback(
      base::BindOnce(&MetalayerMode::OnMetalayerSessionComplete,
                     weak_factory_.GetWeakPtr()),
      !activated_via_button_ /* no retries if activated via button */);
  controller->UpdateEnabledState(HighlighterEnabledState::kEnabled);
  delegate()->HidePalette();
}

void MetalayerMode::OnDisable() {
  CommonPaletteTool::OnDisable();
  activated_via_button_ = false;

  HighlighterController* controller = Shell::Get()->highlighter_controller();
  if (controller->enabled_state() != HighlighterEnabledState::kEnabled)
    return;
  // Call UpdateEnabledState() only when it hasn't been disabled to ensure this
  // emits the disabling signal by deselecting on palette tool.
  Shell::Get()->highlighter_controller()->UpdateEnabledState(
      HighlighterEnabledState::kDisabledByUser);
}

const gfx::VectorIcon& MetalayerMode::GetActiveTrayIcon() const {
  return kPaletteModeMetalayerIcon;
}

const gfx::VectorIcon& MetalayerMode::GetPaletteIcon() const {
  return kPaletteModeMetalayerIcon;
}

views::View* MetalayerMode::CreateView() {
  if (ash::features::IsDeprecateAssistantStylusFeaturesEnabled())
    return nullptr;

  views::View* view = CreateDefaultView(std::u16string());
  UpdateView();
  return view;
}

void MetalayerMode::OnTouchEvent(ui::TouchEvent* event) {
  if (!feature_enabled())
    return;

  if (!palette_utils::IsInUserSession())
    return;

  // The metalayer tool is already selected, no need to do anything.
  if (enabled())
    return;

  if (event->pointer_details().pointer_type != ui::EventPointerType::kPen)
    return;

  if (event->type() == ui::ET_TOUCH_RELEASED) {
    previous_stroke_end_ = event->time_stamp();
    return;
  }

  if (event->type() != ui::ET_TOUCH_PRESSED)
    return;

  if (event->time_stamp() - previous_stroke_end_ <
      base::Milliseconds(kMaxStrokeGapWhenWritingMs)) {
    // The press is happening too soon after the release, the user is most
    // likely writing/sketching and does not want the metalayer to activate.
    return;
  }

  // The stylus "barrel" button press is encoded as ui::EF_LEFT_MOUSE_BUTTON
  if (!(event->flags() & ui::EF_LEFT_MOUSE_BUTTON))
    return;

  if (palette_utils::PaletteContainsPointInScreen(event->root_location()))
    return;

  DeprecateStylusFeaturesToastEvent toast_event = kNotDeprecatedToastNotShown;

  // Assistant stylus features are in the process of being deprecated.
  // After deprecation, which is currently gated by a feature flag, long
  // press stylus events will not trigger the metalayer mode.
  if (ash::features::IsDeprecateAssistantStylusFeaturesEnabled()) {
    // Only show the toast once when the metalayer is triggered for the first
    // time.
    toast_event = kDeprecatedToastNotShown;
    if (!GetPrefs()->GetBoolean(
            assistant::prefs::kAssistantDeprecateStylusToast)) {
      // Set the deprecate stylus toast assistant pref so that the toast doesn't
      // repeatedly show.
      GetPrefs()->SetBoolean(assistant::prefs::kAssistantDeprecateStylusToast,
                             true);
      Shell::Get()->toast_manager()->Show(
          ToastData(kDeprecateAssistantStylusToastId,
                    ToastCatalogName::kDeprecateAssistantStylus,
                    l10n_util::GetStringUTF16(
                        IDS_ASH_STYLUS_TOOLS_METALAYER_TOAST_DEPRECATE),
                    ToastData::kDefaultToastDuration,
                    /*visible_on_lock_screen=*/false,
                    /*has_dismiss_button=*/true));
      toast_event = kDeprecatedToastShown;
    }
    // Record toast event (feature is deprecated).
    base::UmaHistogramEnumeration(kDeprecateStylusFeaturesToastEvent,
                                  toast_event);
    return;
  }

  // Record toast event (feature is not deprecated).
  base::UmaHistogramEnumeration(kDeprecateStylusFeaturesToastEvent,
                                toast_event);

  if (loading()) {
    // Repetitive presses will create toasts with the same id which will be
    // ignored.
    Shell::Get()->toast_manager()->Show(ToastData(
        kToastId, ToastCatalogName::kAssistantLoading,
        l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_METALAYER_TOAST_LOADING),
        ToastData::kDefaultToastDuration,
        /*visible_on_lock_screen=*/false,
        /*has_dismiss_button=*/true));
  } else {
    delegate()->RecordPaletteOptionsUsage(
        PaletteToolIdToPaletteTrayOptions(GetToolId()),
        PaletteInvocationMethod::SHORTCUT);
    activated_via_button_ = true;
    delegate()->EnableTool(GetToolId());
  }
  event->StopPropagation();
}

void MetalayerMode::OnGestureEvent(ui::GestureEvent* event) {
  if (!feature_enabled())
    return;

  // When the stylus button is pressed, a ET_GESTURE_LONG_PRESS event with
  // EF_LEFT_MOUSE_BUTTON will be generated by the GestureDetector. If the
  // metalayer feature is enabled, these should be consumed.
  if (event->type() == ui::ET_GESTURE_LONG_PRESS &&
      (event->flags() & ui::EF_LEFT_MOUSE_BUTTON)) {
    event->StopPropagation();
  }
}

void MetalayerMode::OnAssistantStatusChanged(
    assistant::AssistantStatus status) {
  assistant_status_ = status;
  UpdateState();
}

void MetalayerMode::OnAssistantSettingsEnabled(bool enabled) {
  assistant_enabled_ = enabled;
  UpdateState();
}

void MetalayerMode::OnAssistantContextEnabled(bool enabled) {
  assistant_context_enabled_ = enabled;
  UpdateState();
}

void MetalayerMode::OnAssistantFeatureAllowedChanged(
    assistant::AssistantAllowedState state) {
  assistant_allowed_state_ = state;
  UpdateState();
}

void MetalayerMode::OnHighlighterEnabledChanged(HighlighterEnabledState state) {
  // OnHighlighterEnabledChanged is used by the caller that disables highlighter
  // enabled state to disable the palette tool only.
  if (state == HighlighterEnabledState::kEnabled)
    return;

  // TODO(warx): We disable palette tool of metalayer mode when receiving the
  // disabled state of highlighter controller. And this class also calls
  // HighlighterController's UpdateEnabledState method to send the state signal.
  // We shall think of removing this circular dependency.
  delegate()->DisableTool(GetToolId());
}

void MetalayerMode::UpdateState() {
  if (enabled() && !selectable())
    delegate()->DisableTool(GetToolId());

  if (!loading())
    Shell::Get()->toast_manager()->Cancel(kToastId);

  UpdateView();
}

void MetalayerMode::UpdateView() {
  if (!highlight_view_)
    return;

  const std::u16string text = l10n_util::GetStringUTF16(
      loading() ? IDS_ASH_STYLUS_TOOLS_METALAYER_MODE_LOADING
                : IDS_ASH_STYLUS_TOOLS_METALAYER_MODE);
  highlight_view_->text_label()->SetText(text);
  highlight_view_->SetAccessibleName(text);

  highlight_view_->SetEnabled(selectable());
  const bool enabled = highlight_view_->GetEnabled();
  auto* color_provider = AshColorProvider::Get();
  auto label_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  if (!enabled)
    label_color = ColorUtil::GetDisabledColor(label_color);
  highlight_view_->text_label()->SetEnabledColor(label_color);
  TrayPopupUtils::SetLabelFontList(
      highlight_view_->text_label(),
      TrayPopupUtils::FontStyle::kDetailedViewLabel);

  auto icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
  if (!enabled)
    icon_color = ColorUtil::GetDisabledColor(icon_color);

  DCHECK(views::IsViewClass<views::ImageView>(highlight_view_->left_view()));
  views::ImageView* left_icon =
      static_cast<views::ImageView*>(highlight_view_->left_view());
  left_icon->SetImage(
      CreateVectorIcon(GetPaletteIcon(), kMenuIconSize, icon_color));
}

void MetalayerMode::OnMetalayerSessionComplete() {
  Shell::Get()->highlighter_controller()->UpdateEnabledState(
      HighlighterEnabledState::kDisabledBySessionComplete);
}

}  // namespace ash
