// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webkit_preferences.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace extension_webkit_preferences {

void SetPreferences(const extensions::Extension* extension,
                    extensions::ViewType render_view_type,
                    blink::web_pref::WebPreferences* webkit_prefs) {
  if (!extension)
    return;

  if (!extension->is_hosted_app()) {
    // Extensions are trusted so we override any user preferences for disabling
    // javascript or images.
    webkit_prefs->loads_images_automatically = true;
    webkit_prefs->javascript_enabled = true;

    // Tabs aren't typically allowed to close windows. But extensions shouldn't
    // be subject to that.
    webkit_prefs->allow_scripts_to_close_windows = true;
  }

  if (extension->is_platform_app()) {
    webkit_prefs->databases_enabled = false;
    webkit_prefs->local_storage_enabled = false;
    webkit_prefs->sync_xhr_in_documents_enabled = false;
    webkit_prefs->cookie_enabled = false;
  }

  // Enable WebGL features that regular pages can't access, since they add
  // more risk of fingerprinting.
  webkit_prefs->privileged_webgl_extensions_enabled = true;
}

}  // namespace extension_webkit_preferences
