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

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace {

#if defined(USE_AURA)
bool g_root_checking_enabled = true;
#endif

bool RootCheck(gfx::NativeWindow window) {
#if defined(USE_AURA)
  // If the window is not contained in a root window, then it's not connected
  // to a display and can't be used as the context. To do otherwise results in
  // checks later on assuming context has a root.
  return !g_root_checking_enabled || (window->GetRootWindow() != nullptr);
#else
  return true;
#endif
}
}  // namespace

ExtensionInstallPromptShowParams::ExtensionInstallPromptShowParams(
    content::WebContents* contents)
    : profile_(Profile::FromBrowserContext(contents->GetBrowserContext())),
      parent_web_contents_(contents->GetWeakPtr()) {
  DCHECK(profile_);
  DCHECK(parent_web_contents_);

  if (!parent_web_contents_->GetTopLevelNativeWindow()) {
    // WebContents were created without a top-level window. This can happen when
    // the callers pass a dummy WebContents, or in some tests. There is no
    // window to track in this case. Reset the WebContents pointer and just keep
    // the profile. If we keep web contents in this case, WasParentDestroyed()
    // will always return true, even though there is no real window to check.
    parent_web_contents_.reset();
  }
}

ExtensionInstallPromptShowParams::ExtensionInstallPromptShowParams(
    Profile* profile,
    gfx::NativeWindow parent_window)
    : profile_(profile),
      parent_web_contents_(nullptr),
      parent_window_(parent_window) {
  DCHECK(profile);
  if (parent_window_) {
    native_window_tracker_ = views::NativeWindowTracker::Create(parent_window_);
  }
}

ExtensionInstallPromptShowParams::~ExtensionInstallPromptShowParams() = default;

content::WebContents* ExtensionInstallPromptShowParams::GetParentWebContents() {
  return parent_web_contents_.get();
}

gfx::NativeWindow ExtensionInstallPromptShowParams::GetParentWindow() {
  if (WasParentDestroyed()) {
    return nullptr;
  }

  if (WasConfiguredForWebContents()) {
    return parent_web_contents_->GetTopLevelNativeWindow();
  }

  return parent_window_;
}

bool ExtensionInstallPromptShowParams::WasParentDestroyed() {
  if (profile_->ShutdownStarted()) {
    return true;
  }

  if (WasConfiguredForWebContents()) {
    return !parent_web_contents_ || parent_web_contents_->IsBeingDestroyed() ||
           !parent_web_contents_->GetTopLevelNativeWindow() ||
           !RootCheck(parent_web_contents_->GetTopLevelNativeWindow());
  }

  if (native_window_tracker_) {
    return native_window_tracker_->WasNativeWindowDestroyed() ||
           !RootCheck(parent_window_);
  }

  return false;
}

bool ExtensionInstallPromptShowParams::WasConfiguredForWebContents() {
  // If we ever had a valid web contents, it means we were configured for it.
  return parent_web_contents_ || parent_web_contents_.WasInvalidated();
}

namespace test {

ScopedDisableRootChecking::ScopedDisableRootChecking() {
#if defined(USE_AURA)
  // There should be no need to support multiple ScopedDisableRootCheckings
  // at a time.
  DCHECK(g_root_checking_enabled);
  g_root_checking_enabled = false;
#endif
}

ScopedDisableRootChecking::~ScopedDisableRootChecking() {
#if defined(USE_AURA)
  DCHECK(!g_root_checking_enabled);
  g_root_checking_enabled = true;
#endif
}

}  // namespace test
