// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_menu_model.h"

#include "base/logging.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_metrics.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"

namespace contextual_cueing {

namespace {

// Command IDs for the contextual cueing anchored message menu.
static constexpr int kContextualCueingDismissCommand = 1;
static constexpr int kContextualCueingEditPromptCommand = 2;
static constexpr int kContextualCueingOpenSettingsCommand = 3;

}  // namespace

ContextualCueingMenuModel::ContextualCueingMenuModel(
    Profile* profile,
    base::WeakPtr<ContextualCueingController> controller,
    CueTargetType cue_type,
    CueActionData data)
    : ui::SimpleMenuModel(this),
      profile_(profile),
      controller_(controller),
      cue_type_(cue_type),
      data_(data) {
  contextual_cueing_service_ =
      ContextualCueingServiceFactory::GetForProfile(profile_);

  // Add menu items.
  AddItemWithStringIdAndIcon(
      kContextualCueingDismissCommand, IDS_CONTEXTUAL_CUEING_MENU_DISMISS,
      ui::ImageModel::FromVectorIcon(vector_icons::kCloseIcon,
                                     ui::kColorMenuIcon, 16));
  AddItemWithStringIdAndIcon(
      kContextualCueingEditPromptCommand,
      IDS_CONTEXTUAL_CUEING_MENU_EDIT_PROMPT,
      ui::ImageModel::FromVectorIcon(vector_icons::kEditSquareIcon,
                                     ui::kColorMenuIcon, 16));
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringIdAndIcon(
      kContextualCueingOpenSettingsCommand,
      IDS_CONTEXTUAL_CUEING_MENU_SUGGESTION_SETTINGS,
      ui::ImageModel::FromVectorIcon(vector_icons::kSettingsIcon,
                                     ui::kColorMenuIcon, 16));
}

ContextualCueingMenuModel::~ContextualCueingMenuModel() = default;

void ContextualCueingMenuModel::ExecuteCommand(int command_id,
                                               int event_flags) {
  switch (command_id) {
    case kContextualCueingDismissCommand:
      RecordContextualCueingInteraction(
          ContextualCueingInteraction::kCueDismissed,
          controller_->current_cuj());
      contextual_cueing_service_->OnCueDismissed(cue_type_);
      break;
    case kContextualCueingEditPromptCommand:
      RecordContextualCueingInteraction(
          ContextualCueingInteraction::kCueEditPrompt,
          controller_->current_cuj());
      if (CueTarget* target = controller_->GetTarget(cue_type_)) {
        target->OnEditPrompt(std::move(data_));
      }
      break;
    case kContextualCueingOpenSettingsCommand: {
      RecordContextualCueingInteraction(
          ContextualCueingInteraction::kCueSuggestionsSettings,
          controller_->current_cuj());
      chrome::ShowSettingsSubPageForProfile(profile_,
                                            chrome::kSuggestionsSubPage);
      break;
    }
    default:
      break;
  }

  // User interacted with the cue, so hide it.
  controller_->HideCue();
}

}  // namespace contextual_cueing
