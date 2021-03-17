// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt_show_params.h"

#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/native_window_tracker.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace {

gfx::NativeWindow NativeWindowForWebContents(content::WebContents* contents) {
  if (!contents)
    return nullptr;

  return contents->GetTopLevelNativeWindow();
}

}  // namespace

class ExtensionInstallPromptShowParams::WebContentsDestructionObserver
    : public content::WebContentsObserver {
 public:
  explicit WebContentsDestructionObserver(
      ExtensionInstallPromptShowParams* params)
      : content::WebContentsObserver(params->GetParentWebContents()),
        params_(params) {
  }

  ~WebContentsDestructionObserver() override {}

  void WebContentsDestroyed() override { params_->WebContentsDestroyed(); }

 private:
  // Not owned.
  ExtensionInstallPromptShowParams* params_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDestructionObserver);
};

ExtensionInstallPromptShowParams::ExtensionInstallPromptShowParams(
    content::WebContents* contents)
    : profile_(contents
                   ? Profile::FromBrowserContext(contents->GetBrowserContext())
                   : nullptr),
      parent_web_contents_(contents),
      parent_web_contents_destroyed_(false),
      parent_window_(NativeWindowForWebContents(contents)) {
  if (contents) {
    web_contents_destruction_observer_.reset(
        new WebContentsDestructionObserver(this));
  }
  if (parent_window_)
    native_window_tracker_ = NativeWindowTracker::Create(parent_window_);
}

ExtensionInstallPromptShowParams::ExtensionInstallPromptShowParams(
    Profile* profile,
    gfx::NativeWindow parent_window)
  : profile_(profile),
    parent_web_contents_(nullptr),
    parent_web_contents_destroyed_(false),
    parent_window_(parent_window) {
  if (parent_window_)
    native_window_tracker_ = NativeWindowTracker::Create(parent_window_);
}

ExtensionInstallPromptShowParams::~ExtensionInstallPromptShowParams() {
}

content::WebContents* ExtensionInstallPromptShowParams::GetParentWebContents() {
  return parent_web_contents_;
}

gfx::NativeWindow ExtensionInstallPromptShowParams::GetParentWindow() {
  return (native_window_tracker_ &&
          !native_window_tracker_->WasNativeWindowClosed())
             ? parent_window_
             : nullptr;
}

bool ExtensionInstallPromptShowParams::WasParentDestroyed() {
  return parent_web_contents_destroyed_ ||
         (native_window_tracker_ &&
          native_window_tracker_->WasNativeWindowClosed());
}

void ExtensionInstallPromptShowParams::WebContentsDestroyed() {
  parent_web_contents_ = nullptr;
  parent_web_contents_destroyed_ = true;
}
