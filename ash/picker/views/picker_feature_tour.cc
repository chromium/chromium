// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_feature_tour.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "build/branding_buildflags.h"
#include "chromeos/ash/grit/ash_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/public/activation_client.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {
namespace {

constexpr auto kFeatureTourDialogBorderInsets =
    gfx::Insets::TLBR(0, 32, 28, 32);

constexpr int kFeatureTourDialogCornerRadius = 20;
constexpr auto kFeatureTourDialogIllustrationCornerRadii =
    gfx::RoundedCornersF(/*upper_left=*/kFeatureTourDialogCornerRadius,
                         /*upper_right=*/kFeatureTourDialogCornerRadius,
                         /*lower_right=*/0,
                         /*lower_left=*/0);

// Pref storing whether the feature tour was completed.
constexpr char kFeatureTourCompletedPref[] =
    "ash.picker.feature_tour.completed";

std::u16string GetHeadingText(PickerFeatureTour::EditorStatus editor_status) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  switch (editor_status) {
    case PickerFeatureTour::EditorStatus::kEligible:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_FEATURE_TOUR_WITH_EDITOR_HEADING_TEXT);
    case PickerFeatureTour::EditorStatus::kNotEligible:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_FEATURE_TOUR_WITHOUT_EDITOR_HEADING_TEXT);
  }
#else
  return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::u16string GetBodyText(PickerFeatureTour::EditorStatus editor_status) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  switch (editor_status) {
    case PickerFeatureTour::EditorStatus::kEligible:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_FEATURE_TOUR_WITH_EDITOR_BODY_TEXT);
    case PickerFeatureTour::EditorStatus::kNotEligible:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_FEATURE_TOUR_WITHOUT_EDITOR_BODY_TEXT);
  }
#else
  return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

ui::ImageModel GetIllustration() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
      IDR_PICKER_FEATURE_TOUR_ILLUSTRATION);
#else
  return {};
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::unique_ptr<views::Widget> CreateWidget(
    PickerFeatureTour::EditorStatus editor_status,
    base::OnceClosure learn_more_callback,
    base::OnceClosure completion_callback) {
  auto feature_tour_dialog =
      views::Builder<SystemDialogDelegateView>()
          .SetBorder(views::CreatePaddedBorder(
              std::make_unique<views::HighlightBorder>(
                  kFeatureTourDialogCornerRadius,
                  views::HighlightBorder::Type::kHighlightBorderOnShadow),
              kFeatureTourDialogBorderInsets))
          .SetTitleText(GetHeadingText(editor_status))
          .SetDescription(GetBodyText(editor_status))
          .SetAcceptButtonText(l10n_util::GetStringUTF16(
              IDS_PICKER_FEATURE_TOUR_START_BUTTON_LABEL))
          .SetAcceptCallback(std::move(completion_callback))
          .SetCancelButtonText(l10n_util::GetStringUTF16(
              IDS_PICKER_FEATURE_TOUR_LEARN_MORE_BUTTON_LABEL))
          .SetCancelCallback(std::move(learn_more_callback))
          .SetTopContentView(
              views::Builder<views::ImageView>()
                  .SetBackground(views::CreateThemedRoundedRectBackground(
                      cros_tokens::kCrosSysIlloColor12,
                      kFeatureTourDialogIllustrationCornerRadii))
                  .SetImage(GetIllustration()))
          .SetModalType(ui::mojom::ModalType::kSystem)
          .Build();

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.delegate = feature_tour_dialog.release();
  params.name = "PickerFeatureTourWidget";
  params.activatable = views::Widget::InitParams::Activatable::kYes;

  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->GetLayer()->SetFillsBoundsOpaquely(false);
  return widget;
}

}  // namespace

PickerFeatureTour::PickerFeatureTour() = default;

PickerFeatureTour::~PickerFeatureTour() {
  if (widget_) {
    widget_->CloseNow();
  }
}

void PickerFeatureTour::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kFeatureTourCompletedPref, false);
}

bool PickerFeatureTour::MaybeShowForFirstUse(
    PrefService* prefs,
    EditorStatus editor_status,
    base::RepeatingClosure learn_more_callback,
    base::RepeatingClosure completion_callback) {
  auto* pref = prefs->FindPreference(kFeatureTourCompletedPref);
  // Don't show if `pref` is null (this happens in unit tests that don't call
  // `RegisterProfilePrefs`).
  if (!base::FeatureList::IsEnabled(
          ash::features::kPickerAlwaysShowFeatureTour) &&
      (pref == nullptr || pref->GetValue()->GetBool())) {
    return false;
  }

  widget_ = CreateWidget(
      editor_status,
      base::BindOnce(&PickerFeatureTour::SetOnWindowDeactivatedCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(learn_more_callback)),
      base::BindOnce(&PickerFeatureTour::SetOnWindowDeactivatedCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(completion_callback)));

  aura::Window* window = widget_->GetNativeWindow();
  CHECK_NE(window, nullptr);
  wm::ActivationClient* activation_client =
      wm::GetActivationClient(window->GetRootWindow());
  CHECK_NE(activation_client, nullptr);
  obs_.Reset();
  obs_.Observe(activation_client);

  widget_->Show();

  prefs->SetBoolean(kFeatureTourCompletedPref, true);
  return true;
}

const views::Button* PickerFeatureTour::learn_more_button_for_testing() const {
  if (!widget_) {
    return nullptr;
  }

  auto* feature_tour_dialog =
      static_cast<SystemDialogDelegateView*>(widget_->GetContentsView());
  return feature_tour_dialog != nullptr
             ? feature_tour_dialog->GetCancelButtonForTesting()  // IN-TEST
             : nullptr;
}

const views::Button* PickerFeatureTour::complete_button_for_testing() const {
  if (!widget_) {
    return nullptr;
  }

  auto* feature_tour_dialog =
      static_cast<SystemDialogDelegateView*>(widget_->GetContentsView());
  return feature_tour_dialog != nullptr
             ? feature_tour_dialog->GetAcceptButtonForTesting()  // IN-TEST
             : nullptr;
}

views::Widget* PickerFeatureTour::widget_for_testing() {
  return widget_.get();
}

void PickerFeatureTour::OnWindowActivated(ActivationReason reason,
                                          aura::Window* gained_active,
                                          aura::Window* lost_active) {
  RunOnWindowDeactivatedIfNeeded();
}

void PickerFeatureTour::SetOnWindowDeactivatedCallback(
    base::OnceClosure callback) {
  on_window_deactivated_callback_ = std::move(callback);

  RunOnWindowDeactivatedIfNeeded();
}

void PickerFeatureTour::RunOnWindowDeactivatedIfNeeded() {
  if (on_window_deactivated_callback_.is_null()) {
    return;
  }
  if (widget_ && obs_.IsObserving() &&
      widget_->GetNativeWindow() == obs_.GetSource()->GetActiveWindow()) {
    return;
  }

  // As of writing, this method is called from two code paths:
  //
  // 1. `OnWindowActivated`, which is called from
  // `wm::FocusController::SetActiveWindow`.
  // When `OnWindowActivated` is called, the active window should be set... but
  // we cannot activate any other windows (such as Picker) synchronously due to
  // being in the middle of `wm::FocusController::SetActiveWindow`'s
  // "active window stack".
  // Doing so will cause a `DCHECK` crash in
  // `wm::FocusController::FocusAndActivateWindow` due to the active window
  // changing reentrantly. Turning off `DCHECK`s will result in no window
  // being shown / activated.
  //
  // We should only run callbacks after the `SetActiveWindow` "stack" is fully
  // resolved to avoid this. The only feasible way of doing this is to post a
  // task.
  //
  // 2. `SetOnWindowDeactivatedCallback`, which is passed in as callbacks to
  // `SystemDialogDelegateView`. Those callbacks are called from
  // `SystemDialogDelegateView::RunCallbackAndCloseDialog` before the widget is
  // closed.
  // Therefore, the active window should still be `widget_`'s native window, so
  // we should not have gotten to this point.
  //
  // However, `SystemDialogDelegateView` behaviour might change in the future.
  // The worst case would be `SystemDialogDelegateView` changing its behaviour
  // to call callbacks during `OnWindowDeactivated`, which would be equivalent
  // to the above code path. Therefore, we should also post a task in this
  // scenario.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(on_window_deactivated_callback_));
}

}  // namespace ash
