// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_SEARCH_CHROME_CONTEXTUAL_SEARCH_SESSION_TAB_VALIDATOR_H_
#define CHROME_BROWSER_CONTEXTUAL_SEARCH_CHROME_CONTEXTUAL_SEARCH_SESSION_TAB_VALIDATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/contextual_search/contextual_search_session_handle.h"

class Profile;

namespace url_deduplication {
class URLDeduplicationHelper;
}  // namespace url_deduplication

// Chrome implementation of ContextualSearchSessionHandle::TabValidator.
// Works for both Desktop and Android.
class ChromeContextualSearchSessionTabValidator
    : public contextual_search::ContextualSearchSessionHandle::TabValidator {
 public:
  explicit ChromeContextualSearchSessionTabValidator(Profile* profile);
  ~ChromeContextualSearchSessionTabValidator() override;

  // contextual_search::ContextualSearchSessionHandle::TabValidator:
  bool IsTabValidAndPointingToUrl(
      const contextual_search::FileInfo& file_info) override;
  bool AreUrlsEquivalent(const GURL& url1,
                         const std::string& title1,
                         const GURL& url2,
                         const std::string& title2) override;

 private:
  raw_ptr<Profile> profile_;
  std::unique_ptr<url_deduplication::URLDeduplicationHelper>
      deduplication_helper_;
};

#endif  // CHROME_BROWSER_CONTEXTUAL_SEARCH_CHROME_CONTEXTUAL_SEARCH_SESSION_TAB_VALIDATOR_H_
