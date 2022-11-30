// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_EXTENSION_POPUP_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_EXTENSION_POPUP_H_

#include "components/renderer_context_menu/context_menu_content_type.h"

class ContextMenuContentTypeExtensionPopup : public ContextMenuContentType {
 public:
  ContextMenuContentTypeExtensionPopup(
      const ContextMenuContentTypeExtensionPopup&) = delete;
  ContextMenuContentTypeExtensionPopup& operator=(
      const ContextMenuContentTypeExtensionPopup&) = delete;

  ~ContextMenuContentTypeExtensionPopup() override;

  // ContextMenuContentType overrides.
  bool SupportsGroup(int group) override;

 protected:
  ContextMenuContentTypeExtensionPopup(
      const content::ContextMenuParams& params);

 private:
  friend class ContextMenuContentTypeFactory;
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_EXTENSION_POPUP_H_
