// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/share_action/example_action.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharesheet/sharesheet_controller.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chromeos/components/sharesheet/constants.h"

namespace sharesheet {

ExampleAction::ExampleAction() {
  name_ = "example";
}

ExampleAction::~ExampleAction() = default;

ShareActionType ExampleAction::GetActionType() const {
  return ShareActionType::kExample;
}

const std::u16string ExampleAction::GetActionName() {
  return base::ASCIIToUTF16(name_);
}

const gfx::VectorIcon& ExampleAction::GetActionIcon() {
  return kAddIcon;
}

void ExampleAction::LaunchAction(SharesheetController* controller,
                                 views::View* root_view,
                                 apps::IntentPtr intent) {
  LOG(ERROR) << "ExampleAction launches.";
  controller_ = controller;
}

void ExampleAction::OnClosing(SharesheetController* controller) {
  LOG(ERROR) << "ExampleAction knows it needs to spin down now.";
  controller_ = nullptr;
}

bool ExampleAction::HasActionView() {
  // Return true so that the UI is shown after it has been selected.
  return true;
}

}  // namespace sharesheet
