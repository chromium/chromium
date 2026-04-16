// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/suggestions/glic_cue_target.h"

#include "base/notimplemented.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
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
      std::make_unique<GlicCueTarget>(*glic_keyed_service,
                                      browser_window_interface));
#endif
}

GlicCueTarget::GlicCueTarget(GlicKeyedService& glic_keyed_service,
                             BrowserWindowInterface& browser_window_interface)
    : glic_keyed_service_(glic_keyed_service),
      browser_window_interface_(browser_window_interface) {}
GlicCueTarget::~GlicCueTarget() = default;

bool GlicCueTarget::IsEligible() const {
  return GlicEnabling::IsEnabledForProfile(
             browser_window_interface_->GetProfile()) &&
         !glic_keyed_service_->IsPanelShowingForBrowser(
             *browser_window_interface_);
}

void GlicCueTarget::OnClick(contextual_cueing::CueActionData data) {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED() << "Glic contextual cue not yet implemented for Android.";
#else
  if (!std::holds_alternative<contextual_cueing::GlicCueActionData>(data)) {
    return;
  }
  auto& glic_data = std::get<contextual_cueing::GlicCueActionData>(data);
  GlicInvokeOptions options(
      Target(browser_window_interface_->GetActiveTabInterface()),
      glic::mojom::InvocationSource::kAutoOpenedByContextualCue);
  options.prompts.emplace_back(std::move(glic_data.prompt));
  // TODO(crbug.com/500407600): Add tabs to pin.
  glic_keyed_service_->InvokeWithAutoSubmit(
      InvokeWithAutoSubmitPasskeyProvider::GetPassKey(), std::move(options));
#endif
}

ui::ImageModel GlicCueTarget::GetIcon() const {
  gfx::ImageSkia* icon =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_GLIC_BUTTON_ALT_ICON);
  return icon ? ui::ImageModel::FromImageSkia(*icon) : ui::ImageModel();
}

contextual_cueing::CueActionData GlicCueTarget::CueActionDataFromResponse(
    const optimization_guide::proto::ContextualCueingResponse& response) const {
  contextual_cueing::GlicCueActionData data;
  if (!response.has_gemini_in_chrome_surface()) {
    return data;
  }
  data.prompt = response.gemini_in_chrome_surface().prompt();
  for (auto& tab : response.gemini_in_chrome_surface().tabs_to_share()) {
    // TODO(crbug.com/500407600): Verify that this is a valid tab.
    data.tabs_to_share.emplace_back(tab.tab_id());
  }

  return data;
}

}  // namespace glic
