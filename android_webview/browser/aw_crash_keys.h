// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_CRASH_KEYS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_CRASH_KEYS_H_

#include <set>
#include <string>

namespace android_webview {
/**
 * Adds sets of switches and features as crash keys.
 * Features must be formatted "name-of-feature:{enabled/disabled}"
 * Note: Only intended to be called from aw_contents_statics. Visible for
 * testing purposes.
 */
void SetCrashKeysFromFeaturesAndSwitches(const std::set<std::string>& switches,
                                         const std::set<std::string>& features);
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_CRASH_KEYS_H_
