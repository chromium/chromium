// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SIDE_PANEL_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_SIDE_PANEL_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class WebContents;
}

namespace extensions {

// `SidePanelHelper` is used to provide the capability of dispatching extension
// functions for `WebContents` in `SidePanelWebUIView` or other views with
// similar functionality. Get it to work by calling
// `SidePanelHelper::CreateForWebContents(web_contents)` on the target
// `web_contents` instance.
class SidePanelHelper : public content::WebContentsUserData<SidePanelHelper>,
                        public ExtensionFunctionDispatcher::Delegate {
 public:
  SidePanelHelper(const SidePanelHelper&) = delete;
  SidePanelHelper& operator=(const SidePanelHelper&) = delete;

  ~SidePanelHelper() override = default;

 private:
  // ExtensionFunctionDispatcher::Delegate:
  WindowController* GetExtensionWindowController() override;

 private:
  explicit SidePanelHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SidePanelHelper>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SIDE_PANEL_HELPER_H_
