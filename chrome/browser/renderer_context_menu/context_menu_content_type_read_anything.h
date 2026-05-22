// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_READ_ANYTHING_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_READ_ANYTHING_H_

#include "components/renderer_context_menu/context_menu_content_type.h"

// Context menu content type for Reading Mode.
class ContextMenuContentTypeReadAnything : public ContextMenuContentType {
 public:
  explicit ContextMenuContentTypeReadAnything(
      const content::ContextMenuParams& params);
  ContextMenuContentTypeReadAnything(
      const ContextMenuContentTypeReadAnything&) = delete;
  ContextMenuContentTypeReadAnything& operator=(
      const ContextMenuContentTypeReadAnything&) = delete;

  ~ContextMenuContentTypeReadAnything() override;

  // ContextMenuContentType overrides.
  bool SupportsGroup(int group) override;
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_READ_ANYTHING_H_
