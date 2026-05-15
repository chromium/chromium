// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_FAKE_CONTEXTUAL_CUEING_SERVICE_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_FAKE_CONTEXTUAL_CUEING_SERVICE_H_

#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/glic/suggestions/contextual_cueing_service.h"

namespace glic {

class FakeContextualCueingService : public ContextualCueingService {
 public:
  FakeContextualCueingService();
  ~FakeContextualCueingService() override;

  // ContextualCueingService overrides:
  void GetContextualGlicZeroStateSuggestionsForFocusedTab(
      content::WebContents* web_contents,
      bool is_fre,
      std::optional<std::vector<std::string>> supported_tools,
      GlicSuggestionsCallback callback) override;

  bool GetContextualGlicZeroStateSuggestionsForPinnedTabs(
      std::vector<content::WebContents*> pinned_web_contents,
      bool is_fre,
      std::optional<std::vector<std::string>> supported_tools,
      const content::WebContents* focused_tab,
      GlicSuggestionsCallback callback) override;

  // Methods to control the fake:

  // Set the suggestions that will be returned. If not set, the suggestions will
  // be based on the URL of the web contents.
  void SetFocusedTabSuggestions(const std::vector<std::string>& suggestions) {
    focused_tab_suggestions_ = suggestions;
  }
  void SetPinnedTabsSuggestions(const std::vector<std::string>& suggestions) {
    pinned_tabs_suggestions_ = suggestions;
  }

  int focused_tab_call_count() const { return focused_tab_call_count_; }
  int pinned_tabs_call_count() const { return pinned_tabs_call_count_; }

 private:
  void ProvideFocusedTabSuggestions(
      const std::vector<std::string>& suggestions);
  void ProvidePinnedTabsSuggestions(
      const std::vector<std::string>& suggestions);

  int focused_tab_call_count_ = 0;
  int pinned_tabs_call_count_ = 0;
  std::optional<std::vector<std::string>> focused_tab_suggestions_;
  std::optional<std::vector<std::string>> pinned_tabs_suggestions_;
  std::vector<GlicSuggestionsCallback> focused_tab_callbacks_;
  std::vector<GlicSuggestionsCallback> pinned_tabs_callbacks_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_FAKE_CONTEXTUAL_CUEING_SERVICE_H_
