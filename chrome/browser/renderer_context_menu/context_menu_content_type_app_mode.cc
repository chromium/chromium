// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/context_menu_content_type_app_mode.h"

ContextMenuContentTypeAppMode::ContextMenuContentTypeAppMode(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params)
    : ContextMenuContentType(web_contents, params, false) {
}

ContextMenuContentTypeAppMode::~ContextMenuContentTypeAppMode() {
}

bool ContextMenuContentTypeAppMode::SupportsGroup(int group) {
  switch (group) {
    case ITEM_GROUP_EDITABLE:
    case ITEM_GROUP_COPY:
      return ContextMenuContentType::SupportsGroup(group);
    default:
      return false;
  }
}
