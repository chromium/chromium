// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_search/chrome_contextual_search_session_tab_validator.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_tasks/public/utils.h"
#include "components/tabs/public/tab_handle_factory.h"
#include "components/tabs/public/tab_interface.h"
#include "components/url_deduplication/url_deduplication_helper.h"

ChromeContextualSearchSessionTabValidator::
    ChromeContextualSearchSessionTabValidator(Profile* profile)
    : profile_(profile),
      deduplication_helper_(
          contextual_tasks::CreateURLDeduplicationHelperForContextualTask()) {}

ChromeContextualSearchSessionTabValidator::
    ~ChromeContextualSearchSessionTabValidator() = default;

bool ChromeContextualSearchSessionTabValidator::IsTabValidAndPointingToUrl(
    const contextual_search::FileInfo& file_info) {
  if (!file_info.tab_session_id.has_value() || !file_info.tab_url.has_value()) {
    return false;
  }
  SessionID target_session_id = *file_info.tab_session_id;

  // Use the shared tab handle factory to find the tab.
  int32_t handle_val =
      tabs::SessionMappedTabHandleFactory::GetInstance().GetHandleForSessionId(
          target_session_id.id());
  if (handle_val == tabs::TabHandle::NullValue) {
    return false;  // Tab closed or not tracked.
  }

  tabs::TabHandle handle(handle_val);
  tabs::TabInterface* tab = handle.Get();
  if (!tab) {
    return false;  // Tab destroyed.
  }

  // Tab handles are sequential integers and thus guessable. Since the
  // WebUI/renderer could potentially pass a guessed handle value, we must
  // verify that the resolved tab belongs to the profile associated with this
  // session to prevent cross-profile information leakage.
  if (tab->GetProfile() != profile_) {
    return false;
  }

  std::string current_key = deduplication_helper_->ComputeURLDeduplicationKey(
      tab->GetURL(), base::UTF16ToUTF8(tab->GetTitle()));
  std::string expected_key = deduplication_helper_->ComputeURLDeduplicationKey(
      *file_info.tab_url, file_info.tab_title.value_or(""));

  return current_key == expected_key;
}

bool ChromeContextualSearchSessionTabValidator::AreUrlsEquivalent(
    const GURL& url1,
    const std::string& title1,
    const GURL& url2,
    const std::string& title2) {
  std::string key1 =
      deduplication_helper_->ComputeURLDeduplicationKey(url1, title1);
  std::string key2 =
      deduplication_helper_->ComputeURLDeduplicationKey(url2, title2);
  return key1 == key2;
}
