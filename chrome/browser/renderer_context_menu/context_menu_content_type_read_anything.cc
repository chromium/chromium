// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/context_menu_content_type_read_anything.h"

ContextMenuContentTypeReadAnything::ContextMenuContentTypeReadAnything(
    const content::ContextMenuParams& params)
    : ContextMenuContentType(params, false) {}

ContextMenuContentTypeReadAnything::~ContextMenuContentTypeReadAnything() =
    default;

bool ContextMenuContentTypeReadAnything::SupportsGroup(int group) {
  switch (group) {
    case ITEM_GROUP_COPY:
    case ITEM_GROUP_SEARCH_PROVIDER:
    case ITEM_GROUP_LINK:
    case ITEM_GROUP_DEVELOPER:
      return ContextMenuContentType::SupportsGroup(group);
    default:
      return false;
  }
}
