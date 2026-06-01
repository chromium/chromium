// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/suggestions/glic_cue_target.h"

#include <algorithm>

#include "base/notimplemented.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_metrics.h"
#include "chrome/browser/contextual_cueing/cueing_log.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_handle_factory.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace glic {

// static
void GlicCueTarget::Register(BrowserWindowInterface& browser_window_interface) {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED() << "Glic contextual cue not yet implemented for Android.";
#else
  auto* glic_keyed_service =
      GlicKeyedService::Get(browser_window_interface.GetProfile());
  if (!glic_keyed_service) {
    return;
  }

  auto* contextual_cueing_controller =
      browser_window_interface.GetFeatures().contextual_cueing_controller();
  CHECK(contextual_cueing_controller);
  contextual_cueing_controller->RegisterCueTarget(
      contextual_cueing::CueTargetType::kGlic,
      std::make_unique<GlicCueTarget>(
          *glic_keyed_service,
          OptimizationGuideKeyedServiceFactory::GetForProfile(
              browser_window_interface.GetProfile()),
          browser_window_interface));
#endif
}

GlicCueTarget::GlicCueTarget(
    GlicKeyedService& glic_keyed_service,
    OptimizationGuideKeyedService* optimization_guide_keyed_service,
    BrowserWindowInterface& browser_window_interface)
    : glic_keyed_service_(glic_keyed_service),
      optimization_guide_keyed_service_(optimization_guide_keyed_service),
      browser_window_interface_(browser_window_interface) {}
GlicCueTarget::~GlicCueTarget() = default;

bool GlicCueTarget::IsEligible() const {
  return GlicEnabling::IsEnabledForProfile(
             browser_window_interface_->GetProfile()) &&
         browser_window_interface_->GetProfile()->GetPrefs()->GetBoolean(
             prefs::kGlicPinnedToTabstrip) &&
         !glic_keyed_service_->IsPanelShowingForBrowser(
             *browser_window_interface_) &&
         // TODO(crbug.com/507551989): Default tab context sharing check won't
         // be needed once tab sharing UI is implemented.
         browser_window_interface_->GetProfile()->GetPrefs()->GetBoolean(
             glic::prefs::kGlicDefaultTabContextEnabled);
}

void GlicCueTarget::OnClick(contextual_cueing::CueActionData data) {
  InvokeGlic(std::move(data), base::FeatureList::IsEnabled(
                                  features::kGlicContextualCueingV2AutoSubmit));
}

void GlicCueTarget::OnEditPrompt(contextual_cueing::CueActionData data) {
  InvokeGlic(std::move(data), /*should_autosubmit=*/false);
}

void GlicCueTarget::InvokeGlic(contextual_cueing::CueActionData data,
                               bool should_autosubmit) {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED() << "Glic contextual cue not yet implemented for Android.";
#else
  if (!std::holds_alternative<contextual_cueing::GlicCueActionData>(data)) {
    return;
  }
  auto& glic_data = std::get<contextual_cueing::GlicCueActionData>(data);
  GlicInvokeOptions options(
      Target(browser_window_interface_->GetActiveTabInterface(),
             NewConversation()),
      glic::mojom::InvocationSource::kAutoOpenedByContextualCue);
  options.prompts.emplace_back(std::move(glic_data.prompt));

  CUEING_LOG(
      base::StringPrintf("Sharing %d tabs", glic_data.tabs_to_share.size()));
  options.tab_sharing = TabSharingOptions(std::move(glic_data.tabs_to_share),
                                          GlicPinTrigger::kContextualCue);

  if (should_autosubmit) {
    glic_keyed_service_->InvokeWithAutoSubmit(
        InvokeWithAutoSubmitPasskeyProvider::GetPassKey(), std::move(options));
  } else {
    // If autosubmit is disabled, invoke with a prefilled prompt but don't
    // submit.
    glic_keyed_service_->Invoke(std::move(options));
  }
#endif
}

ui::ImageModel GlicCueTarget::GetAnchoredMessageIcon() const {
  gfx::ImageSkia* icon =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_GLIC_BUTTON_ALT_ICON);
  return icon ? ui::ImageModel::FromImageSkia(*icon) : ui::ImageModel();
}

ui::ImageModel GlicCueTarget::GetOmniboxChipIcon() const {
  return ui::ImageModel::FromVectorIcon(
      glic::GlicVectorIconManager::GetVectorIcon(IDR_GLIC_BUTTON_VECTOR_ICON),
      ui::kColorSysOnSurface, 18);
}

contextual_cueing::CueActionData GlicCueTarget::CueActionDataFromResponse(
    const optimization_guide::proto::ContextualCue& cue,
    std::vector<tabs::TabHandle> tabs_to_show) const {
  contextual_cueing::GlicCueActionData data;
  if (!cue.has_gemini_in_chrome_surface()) {
    CUEING_LOG("Missing Gemini surface data.");
    return data;
  }
  data.prompt = cue.gemini_in_chrome_surface().prompt();
  data.tabs_to_share = std::move(tabs_to_show);
  return data;
}

optimization_guide::proto::ContextualCueingSurface GlicCueTarget::GetSurface()
    const {
  return optimization_guide::proto::CONTEXTUAL_CUEING_SURFACE_GEMINI_IN_CHROME;
}

}  // namespace glic
