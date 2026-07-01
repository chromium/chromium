// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_menu_model.h"

#include <optional>

#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"

namespace contextual_cueing {

namespace {

// LINT.IfChange
// Command IDs for the contextual cueing anchored message menu.
static constexpr int kContextualCueingDismissCommand = 1;
static constexpr int kContextualCueingEditPromptCommand = 2;
static constexpr int kContextualCueingOpenSettingsCommand = 3;
// LINT.ThenChange(:command_id_switch)

std::optional<ContextualCueingInteraction> CommandIdToInteraction(
    int command_id) {
  // LINT.IfChange(command_id_switch)
  switch (command_id) {
    case kContextualCueingDismissCommand:
      return ContextualCueingInteraction::kCueDismissed;
    case kContextualCueingEditPromptCommand:
      return ContextualCueingInteraction::kCueEditPrompt;
    case kContextualCueingOpenSettingsCommand:
      return ContextualCueingInteraction::kCueSuggestionsSettings;
    default:
      return std::nullopt;
  }
  // LINT.ThenChange()
}

}  // namespace

ContextualCueingMenuModel::ContextualCueingMenuModel(
    Profile* profile,
    base::WeakPtr<ContextualCueingController> controller,
    CueTargetType cue_type,
    std::string cuj,
    CueActionData data,
    std::string cue_id)
    : ui::SimpleMenuModel(this),
      profile_(profile),
      controller_(controller),
      cue_type_(cue_type),
      cuj_(cuj),
      data_(std::move(data)),
      cue_id_(cue_id) {
  contextual_cueing_service_ =
      ContextualCueingServiceFactory::GetForProfile(profile_);

  // Add menu items.
  AddItemWithStringIdAndIcon(
      kContextualCueingDismissCommand, IDS_CONTEXTUAL_CUEING_MENU_DISMISS,
      ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                         ? vector_icons::kCloseIcon
                                         : vector_icons::kCloseOldIcon,
                                     ui::kColorMenuIcon, 16));
  AddItemWithStringIdAndIcon(
      kContextualCueingEditPromptCommand,
      IDS_CONTEXTUAL_CUEING_MENU_EDIT_PROMPT,
      ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                         ? vector_icons::kEditSquareIcon
                                         : vector_icons::kEditSquareOldIcon,
                                     ui::kColorMenuIcon, 16));
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringIdAndIcon(
      kContextualCueingOpenSettingsCommand, IDS_CONTEXTUAL_CUEING_MENU_SETTINGS,
      ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                         ? vector_icons::kSettingsFilledIcon
                                         : vector_icons::kSettingsOldIcon,
                                     ui::kColorMenuIcon, 16));
}

ContextualCueingMenuModel::~ContextualCueingMenuModel() = default;

void ContextualCueingMenuModel::ExecuteCommand(int command_id,
                                               int event_flags) {
  if (!controller_) {
    return;
  }

  auto interaction = CommandIdToInteraction(command_id);
  if (!interaction) {
    return;
  }

  controller_->OnCueInteraction(*interaction, cue_type_, cuj_, std::move(data_),
                                cue_id_);
}

}  // namespace contextual_cueing
