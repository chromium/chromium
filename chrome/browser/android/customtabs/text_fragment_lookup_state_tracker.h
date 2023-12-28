// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CUSTOMTABS_TEXT_FRAGMENT_LOOKUP_STATE_TRACKER_H_
#define CHROME_BROWSER_ANDROID_CUSTOMTABS_TEXT_FRAGMENT_LOOKUP_STATE_TRACKER_H_

#include "base/gtest_prod_util.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace customtabs {

class TextFragmentLookupStateTracker
    : public content::WebContentsObserver,
      public content::WebContentsUserData<TextFragmentLookupStateTracker> {
 public:
  ~TextFragmentLookupStateTracker() override;

  // `state_key` is opaque id used by client to keep track of the request.
  // `lookup_results` is the mapping between the text fragments that were looked
  // up and whether they were found on the page or not.
  using OnResultCallback = base::OnceCallback<void(
      const std::string& state_key,
      const std::vector<std::pair<std::string, bool>>& lookup_results)>;

  // Looks up whether `text_directives` are present on the page or not. Once
  // lookups are finished, `on_result_callback` is called with results.
  // `state_key` is opaque id used by client to keep track of the request.
  void LookupTextFragment(const std::string& state_key,
                          const std::vector<std::string>& text_directives,
                          OnResultCallback on_result_callback);

  void FindScrollAndHighlight(const std::string& text_directive) const;

 private:
  friend class content::WebContentsUserData<TextFragmentLookupStateTracker>;
  FRIEND_TEST_ALL_PREFIXES(TextFragmentLookupStateTrackerTest,
                           ExtractAllowedTextDirectives);

  explicit TextFragmentLookupStateTracker(content::WebContents* web_contents);

  // Extracts the first allowed number of text directives based on the remaining
  // quota of lookups of the current page.
  std::vector<std::string> ExtractAllowedTextDirectives(
      const std::vector<std::string>& text_directives) const;

  // Implements `content::WebContentsObserver`:
  void PrimaryPageChanged(content::Page& page) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  size_t lookup_count_ = 0;
};

}  // namespace customtabs

#endif  // CHROME_BROWSER_ANDROID_CUSTOMTABS_TEXT_FRAGMENT_LOOKUP_STATE_TRACKER_H_
