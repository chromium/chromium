// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_PATHS_H__
#define ANDROID_WEBVIEW_COMMON_AW_PATHS_H__

// This file declares path keys for webview. These can be used with
// the PathService to access various special directories and files.

namespace android_webview {

enum {
  PATH_START = 11000,

  DIR_CRASH_DUMPS = PATH_START,  // Directory where crash dumps are written.

  DIR_COMPONENTS_ROOT,  // Directory where components installed via component
                        // updater.

  DIR_COMPONENTS_TEMP,  // Directory where temporary copies of components are
                        // made.

  DIR_SAFE_BROWSING,  // Directory where safe browsing related cookies are
                      // stored.

  DIR_LOCAL_TRACES,  // Directory where local traces are written.

  PATH_END
};

// Call once to register the provider for the path keys defined above.
void RegisterPathProvider();

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_COMMON_AW_PATHS_H__
