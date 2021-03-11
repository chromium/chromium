// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/example_action.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

ExampleAction::ExampleAction() {
  name_ = "example";
}

ExampleAction::~ExampleAction() = default;

const std::u16string ExampleAction::GetActionName() {
  return base::ASCIIToUTF16(name_);
}

const gfx::VectorIcon& ExampleAction::GetActionIcon() {
  return kAddIcon;
}

void ExampleAction::LaunchAction(sharesheet::SharesheetController* controller,
                                 views::View* root_view,
                                 apps::mojom::IntentPtr intent) {
  LOG(ERROR) << "ExampleAction launches.";
  controller_ = controller;
  controller_->CloseSharesheet();
}

void ExampleAction::OnClosing(sharesheet::SharesheetController* controller) {
  LOG(ERROR) << "ExampleAction knows it needs to spin down now.";
  controller_ = nullptr;
}
