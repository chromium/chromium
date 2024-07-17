// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_SHOW_PARAMS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_SHOW_PARAMS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace content {
class WebContents;
}

namespace views {
class NativeWindowTracker;
}

// Parameters to show an install prompt dialog. The parameters control:
// - The dialog's parent window
// - The browser window to use to open a new tab if a user clicks a link in the
//   dialog.
//
// This can either be created with a content::WebContents or a
// gfx::NativeWindow. If this is created for WebContents, GetParentWindow() will
// return the outermost window hosting the WebContents.
class ExtensionInstallPromptShowParams {
 public:
  explicit ExtensionInstallPromptShowParams(content::WebContents* web_contents);

  // The most recently active browser window (or a new browser window if there
  // are no browser windows) is used if a new tab needs to be opened.
  ExtensionInstallPromptShowParams(Profile* profile, gfx::NativeWindow window);

  ExtensionInstallPromptShowParams(const ExtensionInstallPromptShowParams&) =
      delete;
  ExtensionInstallPromptShowParams& operator=(
      const ExtensionInstallPromptShowParams&) = delete;

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
  // Returns trues if the current object was configured for WebContents.
  bool WasConfiguredForWebContents();

  raw_ptr<Profile, DanglingUntriaged> profile_;

  // Only one of these will be non-null.
  base::WeakPtr<content::WebContents> parent_web_contents_;
  gfx::NativeWindow parent_window_;

  // Used to track the parent_window_'s lifetime. We need to explicitly track it
  // because aura::Window does not expose a WeakPtr like WebContents.
  std::unique_ptr<views::NativeWindowTracker> native_window_tracker_;
};

namespace test {

// Unit test may use this to disable root window checking in
// ExtensionInstallPromptShowParams.
class ScopedDisableRootChecking {
 public:
  ScopedDisableRootChecking();
  ~ScopedDisableRootChecking();
};

}  // namespace test

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_SHOW_PARAMS_H_
