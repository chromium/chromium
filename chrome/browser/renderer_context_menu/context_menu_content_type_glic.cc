// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/context_menu_content_type_glic.h"

ContextMenuContentTypeGlic::ContextMenuContentTypeGlic(
    const content::ContextMenuParams& params)
    : ContextMenuContentType(params, /*supports_custom_items=*/true) {}

ContextMenuContentTypeGlic::~ContextMenuContentTypeGlic() = default;

bool ContextMenuContentTypeGlic::SupportsGroup(int group) {
  switch (group) {
    case ITEM_GROUP_PAGE:
    case ITEM_GROUP_FRAME:
    case ITEM_GROUP_SEARCHWEBFORIMAGE:
    case ITEM_GROUP_SEARCH_PROVIDER:
    case ITEM_GROUP_PRINT:
    case ITEM_GROUP_ALL_EXTENSION:
    case ITEM_GROUP_PRINT_PREVIEW:
    case ITEM_GROUP_LINK:
    case ITEM_GROUP_GLIC:
    case ITEM_GROUP_GLICSHAREIMAGE:
      return false;
    case ITEM_GROUP_DEVELOPER:
    default:
      return ContextMenuContentType::SupportsGroup(group);
  }
}
