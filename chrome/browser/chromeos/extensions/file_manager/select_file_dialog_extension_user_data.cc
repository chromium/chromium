// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/select_file_dialog_extension_user_data.h"

#include "content/public/browser/web_contents.h"

const char kSelectFileDialogExtensionUserDataKey[] =
    "SelectFileDialogExtensionUserDataKey";

// static
void SelectFileDialogExtensionUserData::SetRoutingIdForWebContents(
    content::WebContents* web_contents,
    const std::string& routing_id) {
  DCHECK(web_contents);
  web_contents->SetUserData(
      kSelectFileDialogExtensionUserDataKey,
      base::WrapUnique(new SelectFileDialogExtensionUserData(routing_id)));
}

// static
std::string SelectFileDialogExtensionUserData::GetRoutingIdForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  SelectFileDialogExtensionUserData* data =
      static_cast<SelectFileDialogExtensionUserData*>(
          web_contents->GetUserData(kSelectFileDialogExtensionUserDataKey));
  return data ? data->routing_id() : "";
}

SelectFileDialogExtensionUserData::SelectFileDialogExtensionUserData(
    const std::string& routing_id)
    : routing_id_(routing_id) {}
