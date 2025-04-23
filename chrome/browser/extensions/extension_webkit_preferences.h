// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBKIT_PREFERENCES_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBKIT_PREFERENCES_H_

namespace blink {
namespace web_pref {
struct WebPreferences;
}
}  // namespace blink

namespace extensions {
class Extension;
}

namespace extension_webkit_preferences {

void SetPreferences(const extensions::Extension* extension,
                    blink::web_pref::WebPreferences* webkit_prefs);

}  // namespace extension_webkit_preferences

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBKIT_PREFERENCES_H_
