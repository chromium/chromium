// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/suggestions/glic_cue_target.h"

#include <algorithm>

#include "base/notimplemented.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"
#include "chrome/browser/contextual_cueing/cueing_log.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
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
  // TODO(crbug.com/498987803): Prevent cueing if default tab context sharing is
  // turned off and no tab sharing UI exists.
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
      Target(browser_window_interface_->GetActiveTabInterface(),
             NewConversation()),
      glic::mojom::InvocationSource::kAutoOpenedByContextualCue);
  options.prompts.emplace_back(std::move(glic_data.prompt));

  // Also pin the active tab if it isn't already pinned.
  tabs::TabHandle active_handle = GetActiveTabHandle();
  if (std::find(glic_data.tabs_to_share.begin(), glic_data.tabs_to_share.end(),
                active_handle) == glic_data.tabs_to_share.end()) {
    glic_data.tabs_to_share.push_back(active_handle);
  }

  CUEING_LOG(
      base::StringPrintf("Sharing %d tabs", glic_data.tabs_to_share.size()));
  options.tab_sharing = TabSharingOptions(std::move(glic_data.tabs_to_share),
                                          GlicPinTrigger::kContextualCue);

  glic_keyed_service_->InvokeWithAutoSubmit(
      InvokeWithAutoSubmitPasskeyProvider::GetPassKey(), std::move(options));
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
    const optimization_guide::proto::ContextualCueingResponse& response) const {
  contextual_cueing::GlicCueActionData data;
  if (!response.has_gemini_in_chrome_surface()) {
    CUEING_LOG("Missing Gemini surface data.");
    return data;
  }
  data.prompt = response.gemini_in_chrome_surface().prompt();

  auto& tab_handle_factory = tabs::SessionMappedTabHandleFactory::GetInstance();
  for (auto& tab : response.gemini_in_chrome_surface().tabs_to_share()) {
    SessionID session_id = SessionID::FromSerializedValue(
        static_cast<SessionID::id_type>(tab.tab_id()));
    if (!session_id.is_valid()) {
      continue;
    }

    tabs::TabHandle handle(
        tab_handle_factory.GetHandleForSessionId(session_id.id()));
    // Ensure tab is valid
    if (handle.Get()) {
      data.tabs_to_share.push_back(handle);
    }
  }

  CUEING_LOG(
      base::StringPrintf("%d tabs in response.", data.tabs_to_share.size()));
  return data;
}

optimization_guide::proto::ContextualCueingSurface GlicCueTarget::GetSurface()
    const {
  return optimization_guide::proto::CONTEXTUAL_CUEING_SURFACE_GEMINI_IN_CHROME;
}

tabs::TabHandle GlicCueTarget::GetActiveTabHandle() {
  if (auto* tab_list_interface =
          TabListInterface::From(&*browser_window_interface_)) {
    if (tabs::TabInterface* active_tab = tab_list_interface->GetActiveTab()) {
      return active_tab->GetHandle();
    }
  }
  return tabs::TabHandle::Null();
}

}  // namespace glic
