// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_SHOW_PARAMS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_SHOW_PARAMS_H_

#include <memory>

#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"

class NativeWindowTracker;
class Profile;

namespace content {
class WebContents;
}

// Parameters to show an install prompt dialog. The parameters control:
// - The dialog's parent window
// - The browser window to use to open a new tab if a user clicks a link in the
//   dialog.
class ExtensionInstallPromptShowParams {
 public:
  explicit ExtensionInstallPromptShowParams(content::WebContents* web_contents);

  // The most recently active browser window (or a new browser window if there
  // are no browser windows) is used if a new tab needs to be opened.
  ExtensionInstallPromptShowParams(Profile* profile, gfx::NativeWindow window);

  virtual ~ExtensionInstallPromptShowParams();

  Profile* profile() {
    return profile_;
  }

  // The parent web contents for the dialog. Returns NULL if the web contents
  // have been destroyed.
  content::WebContents* GetParentWebContents();

  // The parent window for the dialog. Returns NULL if the window has been
  // destroyed.
  gfx::NativeWindow GetParentWindow();

  // Returns true if either the parent web contents or the parent window were
  // destroyed.
  bool WasParentDestroyed();

 private:
  void WebContentsDestroyed();

  Profile* profile_;
  content::WebContents* parent_web_contents_;
  bool parent_web_contents_destroyed_;
  gfx::NativeWindow parent_window_;

  class WebContentsDestructionObserver;
  std::unique_ptr<WebContentsDestructionObserver>
      web_contents_destruction_observer_;

  std::unique_ptr<NativeWindowTracker> native_window_tracker_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallPromptShowParams);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_SHOW_PARAMS_H_
