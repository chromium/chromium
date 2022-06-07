// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_context_menu_manager.h"

#include <string>

#include "chrome/app/chrome_command_ids.h"

namespace autofill {

namespace {

// The range of command IDs reserved for autofill's custom menus.
static constexpr int kAutofillContextCustomFirst =
    IDC_CONTENT_CONTEXT_AUTOFILL_CUSTOM_FIRST;
static constexpr int kAutofillContextCustomLast =
    IDC_CONTENT_CONTEXT_AUTOFILL_CUSTOM_LAST;

}  // namespace

// static
bool AutofillContextMenuManager::IsAutofillCustomCommandId(
    CommandId command_id) {
  return command_id.value() >= kAutofillContextCustomFirst &&
         command_id.value() <= kAutofillContextCustomLast;
}

void AutofillContextMenuManager::AppendTopLevelItems() {
  // TODO(crbug.com/1325811): Implement
}

bool AutofillContextMenuManager::IsCommandIdChecked(
    CommandId command_id) const {
  return false;
}

bool AutofillContextMenuManager::IsCommandIdVisible(
    CommandId command_id) const {
  return true;
}

bool AutofillContextMenuManager::IsCommandIdEnabled(
    CommandId command_id) const {
  return true;
}

void AutofillContextMenuManager::ExecuteCommand(
    CommandId command_id,
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host) {
  // TODO(crbug.com/1325811): Implement.
}

}  // namespace autofill
