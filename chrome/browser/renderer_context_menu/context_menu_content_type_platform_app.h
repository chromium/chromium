// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_PLATFORM_APP_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_PLATFORM_APP_H_

#include "components/renderer_context_menu/context_menu_content_type.h"

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

class ContextMenuContentTypePlatformApp : public ContextMenuContentType {
 public:
  ContextMenuContentTypePlatformApp(const ContextMenuContentTypePlatformApp&) =
      delete;
  ContextMenuContentTypePlatformApp& operator=(
      const ContextMenuContentTypePlatformApp&) = delete;

  ~ContextMenuContentTypePlatformApp() override;

  // ContextMenuContentType overrides.
  bool SupportsGroup(int group) override;

 protected:
  ContextMenuContentTypePlatformApp(content::WebContents* web_contents,
                                    const content::ContextMenuParams& params);

 private:
  friend class ContextMenuContentTypeFactory;

  const extensions::Extension* GetExtension();

  const raw_ptr<content::WebContents> source_web_contents_;
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_PLATFORM_APP_H_
