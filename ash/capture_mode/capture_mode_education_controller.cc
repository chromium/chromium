// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_education_controller.h"

#include "ash/capture_mode/capture_mode_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/keyboard_shortcut_view.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// A nudge/tutorial will not be shown if it already been shown 3 times, or if 24
// hours have not yet passed since it was last shown.
constexpr int kNudgeMaxShownCount = 3;
constexpr base::TimeDelta kNudgeTimeBetweenShown = base::Hours(24);

constexpr char kCaptureModeNudgeId[] = "kCaptureModeNudge";

// Tutorial styling values.
constexpr int kRowSpacing = 30;
constexpr int kTitleShortcutSpacing = 8;
constexpr int kImageButtonSpacing = 20;
constexpr int kKeyboardImageWidth = 448;

// Clock that can be overridden for testing.
base::Clock* g_clock_override = nullptr;

PrefService* GetPrefService() {
  return Shell::Get()->session_controller()->GetActivePrefService();
}

base::Time GetTime() {
  return g_clock_override ? g_clock_override->Now() : base::Time::Now();
}

// Creates nudge data common to Arms 1 and 2.
AnchoredNudgeData CreateBaseNudgeData(NudgeCatalogName catalog_name) {
  AnchoredNudgeData nudge_data(
      kCaptureModeNudgeId, catalog_name,
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_EDUCATION_NUDGE_LABEL));

  nudge_data.image_model =
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_SCREEN_CAPTURE_EDUCATION_NUDGE_IMAGE);
  nudge_data.fill_image_size = true;
  nudge_data.keyboard_codes = {ui::VKEY_CONTROL, ui::VKEY_SHIFT,
                               ui::VKEY_MEDIA_LAUNCH_APP1};

  return nudge_data;
}

// Creates a view containing a keyboard illustration that indicates the
// location of the keys in the screenshot keyboard shortcut.
std::unique_ptr<views::ImageView> CreateImageView() {
  auto image_view = std::make_unique<views::ImageView>();
  image_view->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_SCREEN_CAPTURE_EDUCATION_KEYBOARD_IMAGE));
  // Rescale the image size to properly take up the width of the container.
  auto image_bounds = image_view->GetImageBounds();
  const float image_scale =
      static_cast<float>(kKeyboardImageWidth) / image_bounds.width();
  image_view->SetImageSize(
      gfx::Size(kKeyboardImageWidth, image_bounds.height() * image_scale));
  image_view->SetHorizontalAlignment(views::ImageViewBase::Alignment::kCenter);
  image_view->SetVerticalAlignment(views::ImageViewBase::Alignment::kCenter);
  image_view->SetID(VIEW_ID_SCREEN_CAPTURE_EDUCATION_KEYBOARD_IMAGE);
  return image_view;
}

// Creates a view containing a keyboard shortcut view and a keyboard
// illustration. To be used as the middle content in a
// `SystemDialogDelegateView`.
std::unique_ptr<views::TableLayoutView> CreateContentView() {
  // Use a vertical table with two rows, so we can choose which cells to
  // left-align.
  auto content_view = std::make_unique<views::TableLayoutView>();
  content_view->AddColumn(
      views::LayoutAlignment::kStretch, views::LayoutAlignment::kStretch,
      /*horizontal_resize=*/1.0f, views::TableLayout::ColumnSize::kUsePreferred,
      /*fixed_width=*/0, /*min_width=*/0);
  content_view->AddRows(1, views::TableLayout::kFixedSize);
  content_view->AddPaddingRow(views::TableLayout::kFixedSize, kRowSpacing);
  content_view->AddRows(1, views::TableLayout::kFixedSize);
  // If the middle content of `SystemDialogDelegateView` has no top margin, it
  // will automatically insert a default content padding. We want to avoid this,
  // so we will set the margin ourselves.
  content_view->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kTitleShortcutSpacing, 0, kImageButtonSpacing, 0));

  // The shortcut view should be left-aligned with the title text.
  const std::vector<ui::KeyboardCode> key_codes{
      ui::VKEY_CONTROL, ui::VKEY_SHIFT, ui::VKEY_MEDIA_LAUNCH_APP1};
  auto* shortcut_view = content_view->AddChildView(
      std::make_unique<KeyboardShortcutView>(key_codes));
  shortcut_view->SetProperty(views::kTableHorizAlignKey,
                             views::LayoutAlignment::kStart);

  // Add the keyboard illustration below the keyboard shortcut.
  content_view->AddChildView(CreateImageView());

  return content_view;
}

// Creates a `SystemDialogDelegateView` to be used as the `WidgetDelegate` for
// the Arm 2 tutorial widget.
std::unique_ptr<SystemDialogDelegateView> CreateDialogView() {
  auto dialog = std::make_unique<SystemDialogDelegateView>();
  dialog->SetTitleText(l10n_util::GetStringUTF16(
      IDS_ASH_SCREEN_CAPTURE_EDUCATION_TUTORIAL_TITLE));
  dialog->SetAccessibleTitle(l10n_util::GetStringUTF16(
      IDS_ASH_SCREEN_CAPTURE_EDUCATION_TUTORIAL_ACCESSIBLE_TITLE));
  dialog->SetMiddleContentView(CreateContentView());
  dialog->SetMiddleContentAlignment(views::LayoutAlignment::kStretch);
  // Override the title margins to be zero, as the space between the title and
  // the shortcut view has already been set by the `content_view` margins.
  dialog->SetTitleMargins(gfx::Insets());
  dialog->SetAcceptButtonVisible(false);
  dialog->SetModalType(ui::mojom::ModalType::kSystem);
  return dialog;
}

}  // namespace

CaptureModeEducationController::CaptureModeEducationController() = default;

CaptureModeEducationController::~CaptureModeEducationController() = default;

// static
void CaptureModeEducationController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kCaptureModeEducationShownCount, 0);
  registry->RegisterTimePref(prefs::kCaptureModeEducationLastShown,
                             base::Time());
}

// static
bool CaptureModeEducationController::IsArm1ShortcutNudgeEnabled() {
  return features::IsCaptureModeEducationEnabled() &&
         features::kCaptureModeEducationParam.Get() ==
             features::CaptureModeEducationParam::kShortcutNudge;
}

// static
bool CaptureModeEducationController::IsArm2ShortcutTutorialEnabled() {
  return features::IsCaptureModeEducationEnabled() &&
         features::kCaptureModeEducationParam.Get() ==
             features::CaptureModeEducationParam::kShortcutTutorial;
}

// static
bool CaptureModeEducationController::IsArm3QuickSettingsNudgeEnabled() {
  return features::IsCaptureModeEducationEnabled() &&
         features::kCaptureModeEducationParam.Get() ==
             features::CaptureModeEducationParam::kQuickSettingsNudge;
}

void CaptureModeEducationController::MaybeShowEducation() {
  // We don't want to show the nudge if the user is not signed in yet or is on
  // the lock screen.
  if (Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    return;
  }

  // Check the feature here so we only record data for users that could have hit
  // the education nudge.
  if (!features::IsCaptureModeEducationEnabled()) {
    return;
  }

  auto* pref_service = GetPrefService();
  CHECK(pref_service);

  if (!(features::IsCaptureModeEducationBypassLimitsEnabled() ||
        skip_prefs_for_test_)) {
    const int shown_count =
        pref_service->GetInteger(prefs::kCaptureModeEducationShownCount);
    const base::Time last_shown_time =
        pref_service->GetTime(prefs::kCaptureModeEducationLastShown);

    // Do not show the nudge more than three times, or if it has already been
    // shown in the past 24 hours.
    const base::Time now = GetTime();
    if ((shown_count >= kNudgeMaxShownCount) ||
        ((now - last_shown_time) < kNudgeTimeBetweenShown)) {
      return;
    }

    // Update the preferences since a form of education must have been shown, as
    // `this` is only created if the feature flag is enabled.
    pref_service->SetInteger(prefs::kCaptureModeEducationShownCount,
                             shown_count + 1);
    pref_service->SetTime(prefs::kCaptureModeEducationLastShown, now);
  }

  CloseAllEducationNudgesAndTutorials();

  if (IsArm1ShortcutNudgeEnabled()) {
    ShowShortcutNudge();
  }

  if (IsArm2ShortcutTutorialEnabled()) {
    ShowTutorialNudge();
  }

  if (IsArm3QuickSettingsNudgeEnabled()) {
    ShowQuickSettingsNudge();
  }
}

void CaptureModeEducationController::CloseAllEducationNudgesAndTutorials() {
  AnchoredNudgeManager::Get()->Cancel(kCaptureModeNudgeId);
  tutorial_widget_.reset();
}

// static
void CaptureModeEducationController::SetOverrideClockForTesting(
    base::Clock* test_clock) {
  g_clock_override = test_clock;
}

void CaptureModeEducationController::ShowShortcutNudge() {
  AnchoredNudgeData nudge_data =
      CreateBaseNudgeData(NudgeCatalogName::kCaptureModeEducationShortcutNudge);

  AnchoredNudgeManager::Get()->Show(nudge_data);
}

void CaptureModeEducationController::ShowTutorialNudge() {
  AnchoredNudgeData nudge_data = CreateBaseNudgeData(
      NudgeCatalogName::kCaptureModeEducationShortcutTutorial);

  nudge_data.primary_button_text =
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_EDUCATION_NUDGE_BUTTON);
  nudge_data.primary_button_callback = base::BindRepeating(
      &CaptureModeEducationController::OnShowMeHowButtonPressed,
      weak_ptr_factory_.GetWeakPtr());

  AnchoredNudgeManager::Get()->Show(nudge_data);
}

void CaptureModeEducationController::ShowQuickSettingsNudge() {
  AnchoredNudgeData nudge_data(
      kCaptureModeNudgeId,
      NudgeCatalogName::kCaptureModeEducationQuickSettingsNudge,
      l10n_util::GetStringUTF16(
          IDS_ASH_SCREEN_CAPTURE_EDUCATION_SETTINGS_NUDGE_LABEL));

  nudge_data.image_model =
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_SCREEN_CAPTURE_EDUCATION_NUDGE_IMAGE);
  nudge_data.fill_image_size = true;
  nudge_data.SetAnchorView(
      RootWindowController::ForWindow(Shell::GetRootWindowForNewWindows())
          ->shelf()
          ->status_area_widget()
          ->unified_system_tray());

  AnchoredNudgeManager::Get()->Show(nudge_data);
}

void CaptureModeEducationController::CreateAndShowTutorialDialog() {
  // As we are creating a system modal dialog, it will automatically be parented
  // to `kShellWindowId_SystemModalContainer`.
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.delegate = CreateDialogView().release();
  params.name = "CaptureModeEducationTutorialWidget";
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  tutorial_widget_ = std::make_unique<views::Widget>();
  tutorial_widget_->Init(std::move(params));
  tutorial_widget_->Show();
}

void CaptureModeEducationController::OnShowMeHowButtonPressed() {
  CHECK(!tutorial_widget_);
  CreateAndShowTutorialDialog();
}

}  // namespace ash
