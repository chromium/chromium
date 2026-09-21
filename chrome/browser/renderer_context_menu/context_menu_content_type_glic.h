// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_GLIC_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_GLIC_H_

#include "components/renderer_context_menu/context_menu_content_type.h"

// A ContextMenuContentType for Glic surfaces (guest and overlay WebUI).
class ContextMenuContentTypeGlic : public ContextMenuContentType {
 public:
  explicit ContextMenuContentTypeGlic(const content::ContextMenuParams& params);
  ContextMenuContentTypeGlic(const ContextMenuContentTypeGlic&) = delete;
  ContextMenuContentTypeGlic& operator=(const ContextMenuContentTypeGlic&) =
      delete;

  ~ContextMenuContentTypeGlic() override;

  // ContextMenuContentType overrides.
  bool SupportsGroup(int group) override;
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_GLIC_H_
