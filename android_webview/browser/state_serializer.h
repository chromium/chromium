// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef ANDROID_WEBVIEW_BROWSER_STATE_SERIALIZER_H_
#define ANDROID_WEBVIEW_BROWSER_STATE_SERIALIZER_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

#include "base/containers/fixed_flat_set.h"

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

// Writes the navigation history to a Pickle. If `max_size` is provided, older
// entries will be dropped to ensure the returned Pickle is within the limit.
// If `include_forward_state` is false, only entries before the selected entry
// are saved. This is useful for embedders who only have a Back button (not
// a Forward one).
std::optional<base::Pickle> WriteToPickle(content::WebContents& web_contents,
                                          size_t max_size,
                                          bool include_forward_state);

// |web_contents| will not be modified if function returns false.
[[nodiscard]] bool RestoreFromPickle(base::PickleIterator* iterator,
                                     content::WebContents* web_contents);

namespace internal {

const uint32_t AW_STATE_VERSION_INITIAL = 20130814;
const uint32_t AW_STATE_VERSION_DATA_URL = 20151204;
const uint32_t AW_STATE_VERSION_MOST_RECENT_FIRST = 20250213;
// Note: This version always writes the headers field to the bundle,
// but whether the actual headers or an empty string is saved is dependant
// on the feature kWebViewSaveStateIncludeHeaders
const uint32_t AW_STATE_VERSION_INCLUDE_HEADERS = 20260226;

inline constexpr auto AW_STATE_SUPPORTED_VERSIONS =
    base::MakeFixedFlatSet<uint32_t>({
        AW_STATE_VERSION_INITIAL,
        AW_STATE_VERSION_DATA_URL,
        AW_STATE_VERSION_MOST_RECENT_FIRST,
        AW_STATE_VERSION_INCLUDE_HEADERS,
    });
const uint32_t AW_STATE_VERSION = internal::AW_STATE_VERSION_INCLUDE_HEADERS;

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

[[nodiscard]] bool IsSupportedVersion(uint32_t state_version);

void WriteHeaderToPickle(base::Pickle* pickle, uint32_t state_version);
std::optional<base::Pickle> WriteToPickle(
    NavigationHistory& history,
    uint32_t state_version,
    size_t max_size = std::numeric_limits<size_t>::max(),
    bool save_forward_history = true);
void WriteNavigationEntryToPickle(content::NavigationEntry& entry,
                                  base::Pickle* pickle,
                                  uint32_t state_version);

[[nodiscard]] uint32_t RestoreHeaderFromPickle(base::PickleIterator* iterator);
bool RestoreFromPickle(base::PickleIterator* iterator,
                       NavigationHistorySink& sink);
[[nodiscard]] bool RestoreNavigationEntryFromPickle(
    base::PickleIterator* iterator,
    content::NavigationEntry* entry,
    content::NavigationEntryRestoreContext* context,
    uint32_t state_version);

}  // namespace internal

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_STATE_SERIALIZER_H_
