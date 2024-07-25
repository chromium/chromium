// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_feature_tour.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "base/functional/callback.h"
#include "build/branding_buildflags.h"
#include "chromeos/ash/grit/ash_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {
namespace {

// Pref storing whether the feature tour was completed.
constexpr char kFeatureTourCompletedPref[] =
    "ash.picker.feature_tour.completed";

bool g_feature_tour_enabled = true;

std::u16string GetHeadingText() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return l10n_util::GetStringUTF16(IDS_PICKER_FEATURE_TOUR_HEADING_TEXT);
#else
  return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::u16string GetBodyText() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return l10n_util::GetStringUTF16(IDS_PICKER_FEATURE_TOUR_BODY_TEXT);
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
    base::RepeatingClosure completion_callback) {
  auto feature_tour_dialog =
      views::Builder<SystemDialogDelegateView>()
          .SetTitleText(GetHeadingText())
          .SetDescription(GetBodyText())
          .SetAcceptButtonText(l10n_util::GetStringUTF16(
              IDS_PICKER_FEATURE_TOUR_START_BUTTON_LABEL))
          .SetAcceptCallback(completion_callback)
          .SetCancelButtonText(l10n_util::GetStringUTF16(
              IDS_PICKER_FEATURE_TOUR_LEARN_MORE_BUTTON_LABEL))
          .SetTopContentView(
              views::Builder<views::ImageView>().SetImage(GetIllustration()))
          .Build();

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.delegate = feature_tour_dialog.release();
  params.name = "PickerFeatureTourWidget";

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

void PickerFeatureTour::DisableFeatureTourForTesting() {
  g_feature_tour_enabled = false;
}

bool PickerFeatureTour::MaybeShowForFirstUse(
    PrefService* prefs,
    base::RepeatingClosure completion_callback) {
  if (!g_feature_tour_enabled) {
    return false;
  }

  auto* pref = prefs->FindPreference(kFeatureTourCompletedPref);
  // Don't show if `pref` is null (this happens in unit tests that don't call
  // `RegisterProfilePrefs`).
  if (pref == nullptr || pref->GetValue()->GetBool()) {
    return false;
  }

  widget_ = CreateWidget(std::move(completion_callback));
  widget_->Show();

  prefs->SetBoolean(kFeatureTourCompletedPref, true);
  return true;
}

const views::Button* PickerFeatureTour::complete_button_for_testing() const {
  if (!widget_) {
    return nullptr;
  }

  auto* feature_tour_dialog =
      static_cast<SystemDialogDelegateView*>(widget_->GetContentsView());
  return feature_tour_dialog
             ? feature_tour_dialog->GetAcceptButtonForTesting()  // IN-TEST
             : nullptr;
}

views::Widget* PickerFeatureTour::widget_for_testing() {
  return widget_.get();
}

}  // namespace ash
