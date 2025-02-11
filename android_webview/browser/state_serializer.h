// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef ANDROID_WEBVIEW_BROWSER_STATE_SERIALIZER_H_
#define ANDROID_WEBVIEW_BROWSER_STATE_SERIALIZER_H_

#include <cstdint>
#include <memory>
#include <vector>

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

// Write and restore a WebContents to and from a pickle.
void WriteToPickle(content::WebContents& web_contents, base::Pickle* pickle);

// |web_contents| will not be modified if function returns false.
[[nodiscard]] bool RestoreFromPickle(base::PickleIterator* iterator,
                                     content::WebContents* web_contents);

namespace internal {

const uint32_t AW_STATE_VERSION_INITIAL = 20130814;
const uint32_t AW_STATE_VERSION_DATA_URL = 20151204;
const uint32_t AW_STATE_VERSION_REVERSE_ENTRIES = 20250211;

// The navigation history to be saved. Primarily exists for testing.
class NavigationHistory {
 public:
  virtual ~NavigationHistory() = default;
  virtual int GetEntryCount() = 0;
  virtual int GetCurrentEntry() = 0;
  virtual content::NavigationEntry* GetEntryAtIndex(int index) = 0;
};

// A recipient for loaded navigation entries. Primarily exists for testing.
class NavigationHistorySink {
 public:
  virtual ~NavigationHistorySink() = default;
  virtual void Restore(
      int selected_entry,
      std::vector<std::unique_ptr<content::NavigationEntry>>* entries) = 0;
};

// Functions below are individual helper functions called by functions above.
// They are broken up for unit testing, and should not be called out side of
// tests.
void WriteToPickle(NavigationHistory& history, base::Pickle* pickle);
void WriteToPickle(uint32_t state_version,
                   NavigationHistory& history,
                   base::Pickle* pickle);
void WriteHeaderToPickle(base::Pickle* pickle);
void WriteHeaderToPickle(uint32_t state_version, base::Pickle* pickle);
[[nodiscard]] uint32_t RestoreHeaderFromPickle(base::PickleIterator* iterator);
[[nodiscard]] bool IsSupportedVersion(uint32_t state_version);
void WriteNavigationEntryToPickle(content::NavigationEntry& entry,
                                  base::Pickle* pickle);
void WriteNavigationEntryToPickle(uint32_t state_version,
                                  content::NavigationEntry& entry,
                                  base::Pickle* pickle);
bool RestoreFromPickle(base::PickleIterator* iterator,
                       NavigationHistorySink& sink);
[[nodiscard]] bool RestoreNavigationEntryFromPickle(
    base::PickleIterator* iterator,
    content::NavigationEntry* entry,
    content::NavigationEntryRestoreContext* context);
[[nodiscard]] bool RestoreNavigationEntryFromPickle(
    uint32_t state_version,
    base::PickleIterator* iterator,
    content::NavigationEntry* entry,
    content::NavigationEntryRestoreContext* context);

}  // namespace internal

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_STATE_SERIALIZER_H_
