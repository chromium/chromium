// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_cue_target.h"

#include <utility>

#include "base/command_line.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/indigo/indigo_page_action_controller.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/indigo/indigo_service_factory.h"
#include "chrome/browser/indigo/resources/grit/indigo_strings.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/gfx/image/image_skia.h"

namespace indigo {

// static
void IndigoCueTarget::Register(tabs::TabInterface& tab) {
  CHECK(base::FeatureList::IsEnabled(features::kIndigoContextualCueingV2));

  auto* indigo_service = IndigoServiceFactory::GetForProfile(tab.GetProfile());
  if (!indigo_service) {
    return;
  }

  auto* contextual_cueing_controller =
      tab.GetTabFeatures()->contextual_cueing_controller();
  CHECK(contextual_cueing_controller);
  contextual_cueing_controller->RegisterCueTarget(
      contextual_cueing::CueTargetType::kIndigo,
      std::make_unique<IndigoCueTarget>(*indigo_service, tab));
}

IndigoCueTarget::IndigoCueTarget(IndigoService& indigo_service,
                                 tabs::TabInterface& tab)
    : indigo_service_(indigo_service), tab_(tab) {}

IndigoCueTarget::~IndigoCueTarget() = default;

contextual_cueing::CueTargetType IndigoCueTarget::GetType() const {
  return contextual_cueing::CueTargetType::kIndigo;
}

bool IndigoCueTarget::RequiresModelExecution() const {
  return false;
}

bool IndigoCueTarget::IsEligible() const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kForceIndigoSwitch)) {
    return true;
  }
  return indigo_service_->IsLocallyEligible();
}

void IndigoCueTarget::CheckEligibility(
    base::WeakPtr<content::WebContents> web_contents,
    contextual_cueing::CueIntrusiveness intrusiveness,
    EligibilityCallback callback) {
  if (!web_contents || !IsEligible()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false, ContentGenerator()));
    return;
  }

  auto* controller = IndigoPageActionController::From(&tab_.get());
  if (!controller) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false, ContentGenerator()));
    return;
  }

  controller->CheckEligibilityForCueing(
      base::BindOnce(&IndigoCueTarget::OnEligibilityChecked,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IndigoCueTarget::OnEligibilityChecked(EligibilityCallback callback,
                                           bool eligible) {
  std::move(callback).Run(eligible,
                          base::BindOnce(&IndigoCueTarget::GenerateContent,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void IndigoCueTarget::GenerateContent(
    base::OnceCallback<void(
        std::optional<optimization_guide::proto::ContextualCue>)> callback) {
  if (auto* controller = IndigoPageActionController::From(&tab_.get())) {
    controller->RefreshDiscoverySkills();
    controller->RecordTriggerSource();
  }
  optimization_guide::proto::ContextualCue cue;
  cue.set_suggested_cuj(
      contextual_cueing::GetName(contextual_cueing::CueTargetType::kIndigo));
  auto* anchored_cue = cue.mutable_anchored_message_cue();
  anchored_cue->set_action_text(base::UTF16ToUTF8(
      l10n_util::GetStringUTF16(IDS_INDIGO_ENTRYPOINT_CHIP_TEXT)));
  anchored_cue->set_anchored_message_text(base::UTF16ToUTF8(
      l10n_util::GetStringUTF16(IDS_INDIGO_ENTRYPOINT_ANCHORED_MESSAGE_TEXT)));
  std::move(callback).Run(std::move(cue));
}

bool IndigoCueTarget::IsPageEligible(
    const page_content_annotations::PageContentAnnotationsResult& result,
    content::WebContents* active_web_contents) const {
  return false;
}

void IndigoCueTarget::OnClick(contextual_cueing::CueActionData data) {
  auto* controller = IndigoPageActionController::From(&tab_.get());
  if (controller) {
    controller->InvokeAction(EntryPoint::kAnchoredMessage);
  }
}

void IndigoCueTarget::OnEditPrompt(contextual_cueing::CueActionData data) {
  NOTREACHED();
}

ui::ImageModel IndigoCueTarget::GetAnchoredMessageIcon() const {
  gfx::ImageSkia* icon =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_GLIC_BUTTON_ALT_ICON);
  return icon ? ui::ImageModel::FromImageSkia(*icon) : ui::ImageModel();
}

ui::ImageModel IndigoCueTarget::GetOmniboxChipIcon() const {
  return ui::ImageModel::FromVectorIcon(
      glic::GlicVectorIconManager::GetVectorIcon(IDR_GLIC_BUTTON_VECTOR_ICON),
      ui::kColorSysOnSurface, 16);
}

contextual_cueing::CueActionData IndigoCueTarget::CueActionDataFromResponse(
    const optimization_guide::proto::ContextualCue& cue,
    std::vector<tabs::TabHandle> tabs_to_show) const {
  return std::monostate{};
}

optimization_guide::proto::ContextualCueingSurface IndigoCueTarget::GetSurface()
    const {
  return optimization_guide::proto::CONTEXTUAL_CUEING_SURFACE_UNSPECIFIED;
}

}  // namespace indigo
