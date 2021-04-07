// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener.h"

#include "content/public/browser/browser_thread.h"

#if defined(USE_OZONE)
#include "chrome/browser/extensions/global_shortcut_listener_ozone.h"
#include "ui/base/ui_base_features.h"
#endif

#if defined(USE_X11)
#include "chrome/browser/extensions/global_shortcut_listener_x11.h"
#endif

using content::BrowserThread;

namespace extensions {

// static
GlobalShortcutListener* GlobalShortcutListener::GetInstance() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    static GlobalShortcutListenerOzone* instance =
        new GlobalShortcutListenerOzone();
    return instance;
  }
#endif
#if defined(USE_X11)
  static GlobalShortcutListenerX11* instance = new GlobalShortcutListenerX11();
  return instance;
#else
  NOTREACHED();
  return nullptr;
#endif
}

}  // namespace extensions
