// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_WEB_CONTENTS_OBSERVER_H_

#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/common/stack_frame.h"

namespace content {
class RenderFrameHost;
}

namespace extensions {

// An ExtensionWebContentsObserver that adds support for the extension error
// console, reloading crashed extensions, routing extension messages between
// renderers and updating autoplay policy.
class ChromeExtensionWebContentsObserver
    : public ExtensionWebContentsObserver,
      public content::WebContentsUserData<ChromeExtensionWebContentsObserver> {
 public:
  ~ChromeExtensionWebContentsObserver() override;

  // Creates and initializes an instance of this class for the given
  // |web_contents|, if it doesn't already exist.
  static void CreateForWebContents(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<ChromeExtensionWebContentsObserver>;

  explicit ChromeExtensionWebContentsObserver(
      content::WebContents* web_contents);

  // ExtensionWebContentsObserver:
  void InitializeRenderFrame(
      content::RenderFrameHost* render_frame_host) override;
  std::unique_ptr<ExtensionFrameHost> CreateExtensionFrameHost(
      content::WebContents* web_contents) override;

  // content::WebContentsObserver overrides.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;

  // Silence a warning about hiding a virtual function.
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;

  // Adds a message to the extensions ErrorConsole.
  void OnDetailedConsoleMessageAdded(
      content::RenderFrameHost* render_frame_host,
      const std::u16string& message,
      const std::u16string& source,
      const StackTrace& stack_trace,
      int32_t severity_level);

  // Reloads an extension if it is on the terminated list.
  void ReloadIfTerminated(content::RenderFrameHost* render_frame_host);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionWebContentsObserver);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_WEB_CONTENTS_OBSERVER_H_
