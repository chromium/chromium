// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt_show_params.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/native_window_tracker.h"

namespace {

gfx::NativeWindow NativeWindowForWebContents(content::WebContents* contents) {
  if (!contents)
    return nullptr;

  return contents->GetTopLevelNativeWindow();
}

}  // namespace

ExtensionInstallPromptShowParams::ExtensionInstallPromptShowParams(
    content::WebContents* contents)
    : profile_(contents
                   ? Profile::FromBrowserContext(contents->GetBrowserContext())
                   : nullptr),
      parent_web_contents_(contents ? contents->GetWeakPtr() : nullptr),
      parent_window_(NativeWindowForWebContents(contents)) {
  if (parent_window_)
    native_window_tracker_ = views::NativeWindowTracker::Create(parent_window_);
}

ExtensionInstallPromptShowParams::ExtensionInstallPromptShowParams(
    Profile* profile,
    gfx::NativeWindow parent_window)
    : profile_(profile),
      parent_web_contents_(nullptr),
      parent_window_(parent_window) {
  if (parent_window_)
    native_window_tracker_ = views::NativeWindowTracker::Create(parent_window_);
}

ExtensionInstallPromptShowParams::~ExtensionInstallPromptShowParams() = default;

content::WebContents* ExtensionInstallPromptShowParams::GetParentWebContents() {
  return parent_web_contents_.get();
}

gfx::NativeWindow ExtensionInstallPromptShowParams::GetParentWindow() {
  return (native_window_tracker_ &&
          !native_window_tracker_->WasNativeWindowDestroyed())
             ? parent_window_
             : nullptr;
}

bool ExtensionInstallPromptShowParams::WasParentDestroyed() {
  const bool parent_web_contents_destroyed =
      parent_web_contents_.WasInvalidated();
  return parent_web_contents_destroyed ||
         (native_window_tracker_ &&
          native_window_tracker_->WasNativeWindowDestroyed());
}
