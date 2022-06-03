// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef ANDROID_WEBVIEW_BROWSER_STATE_SERIALIZER_H_
#define ANDROID_WEBVIEW_BROWSER_STATE_SERIALIZER_H_

#include <cstdint>

#include "base/compiler_specific.h"

namespace base {

class Pickle;
class PickleIterator;

}  // namespace base

namespace content {

class NavigationEntry;
class NavigationEntryRestoreContext;
class WebContents;

}  // namespace content

namespace android_webview {

// Write and restore a WebContents to and from a pickle. Return true on
// success.

// Note that |pickle| may be changed even if function returns false.
void WriteToPickle(content::WebContents& web_contents, base::Pickle* pickle);

// |web_contents| will not be modified if function returns false.
bool RestoreFromPickle(base::PickleIterator* iterator,
                       content::WebContents* web_contents) WARN_UNUSED_RESULT;

namespace internal {

const uint32_t AW_STATE_VERSION_INITIAL = 20130814;
const uint32_t AW_STATE_VERSION_DATA_URL = 20151204;

// Functions below are individual helper functions called by functions above.
// They are broken up for unit testing, and should not be called out side of
// tests.
void WriteHeaderToPickle(base::Pickle* pickle);
void WriteHeaderToPickle(uint32_t state_version, base::Pickle* pickle);
uint32_t RestoreHeaderFromPickle(base::PickleIterator* iterator)
    WARN_UNUSED_RESULT;
bool IsSupportedVersion(uint32_t state_version) WARN_UNUSED_RESULT;
void WriteNavigationEntryToPickle(content::NavigationEntry& entry,
                                  base::Pickle* pickle);
void WriteNavigationEntryToPickle(uint32_t state_version,
                                  content::NavigationEntry& entry,
                                  base::Pickle* pickle);
bool RestoreNavigationEntryFromPickle(
    base::PickleIterator* iterator,
    content::NavigationEntry* entry,
    content::NavigationEntryRestoreContext* context) WARN_UNUSED_RESULT;
bool RestoreNavigationEntryFromPickle(
    uint32_t state_version,
    base::PickleIterator* iterator,
    content::NavigationEntry* entry,
    content::NavigationEntryRestoreContext* context) WARN_UNUSED_RESULT;

}  // namespace internal

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_STATE_SERIALIZER_H_
