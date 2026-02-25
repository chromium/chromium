// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SUGGESTIONS_CACHING_ZERO_STATE_SUGGESTIONS_MANAGER_H_
#define CHROME_BROWSER_GLIC_SUGGESTIONS_CACHING_ZERO_STATE_SUGGESTIONS_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"

namespace content {
class WebContents;
}  // namespace content

namespace contextual_cueing {
class ContextualCueingService;
}  // namespace contextual_cueing

namespace glic {

class CachingZeroStateSuggestionsManager {
 public:
  virtual ~CachingZeroStateSuggestionsManager() = default;

  using GlicSuggestionsCallback =
      base::OnceCallback<void(std::vector<std::string>)>;

  virtual void GetContextualGlicZeroStateSuggestionsForFocusedTab(
      content::WebContents* focused_tab,
      bool is_fre,
      std::optional<std::vector<std::string>> supported_tools,
      GlicSuggestionsCallback callback) = 0;

  // Returns whether a new request for contextual suggestions are pending for
  // pinned tabs.
  virtual bool GetContextualGlicZeroStateSuggestionsForPinnedTabs(
      std::vector<content::WebContents*> pinned_web_contents,
      bool is_fre,
      std::optional<std::vector<std::string>> supported_tools,
      content::WebContents* focused_tab,
      GlicSuggestionsCallback callback) = 0;
};

std::unique_ptr<CachingZeroStateSuggestionsManager>
CreateCachingZeroStateSuggestionsManager(
    contextual_cueing::ContextualCueingService* service);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SUGGESTIONS_CACHING_ZERO_STATE_SUGGESTIONS_MANAGER_H_
