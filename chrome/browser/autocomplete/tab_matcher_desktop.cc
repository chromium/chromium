// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/tab_matcher_desktop.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents_user_data.h"

namespace {
class AutocompleteClientWebContentsUserData
    : public content::WebContentsUserData<
          AutocompleteClientWebContentsUserData> {
 public:
  ~AutocompleteClientWebContentsUserData() override = default;

  int GetLastCommittedEntryIndex() { return last_committed_entry_index_; }
  const GURL& GetLastCommittedStrippedURL() {
    return last_committed_stripped_url_;
  }
  void UpdateLastCommittedStrippedURL(
      int last_committed_index,
      const GURL& last_committed_url,
      const TemplateURLService* template_url_service,
      const bool keep_search_intent_params) {
    if (last_committed_url.is_valid()) {
      last_committed_entry_index_ = last_committed_index;
      // Use a blank input as the stripped URL will be reused with other inputs.
      // Also keep the search intent params. Otherwise, this can result in over
      // triggering of the Switch to Tab action on plain-text suggestions for
      // open entity SRPs, or vice versa, on entity suggestions for open
      // plain-text SRPs.
      last_committed_stripped_url_ = AutocompleteMatch::GURLToStrippedGURL(
          last_committed_url, AutocompleteInput(), template_url_service,
          std::u16string(), keep_search_intent_params);
    }
  }

 private:
  explicit AutocompleteClientWebContentsUserData(
      content::WebContents* contents);
  friend class content::WebContentsUserData<
      AutocompleteClientWebContentsUserData>;

  int last_committed_entry_index_ = -1;
  GURL last_committed_stripped_url_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

AutocompleteClientWebContentsUserData::AutocompleteClientWebContentsUserData(
    content::WebContents* web_contents)
    : content::WebContentsUserData<AutocompleteClientWebContentsUserData>(
          *web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutocompleteClientWebContentsUserData);
}  // namespace

bool TabMatcherDesktop::IsTabOpenWithURL(const GURL& url,
                                         const AutocompleteInput* input) const {
  const AutocompleteInput empty_input;
  if (!input)
    input = &empty_input;

  // Use a blank input as the stripped URL will be reused with other inputs.
  // Also keep the search intent params. Otherwise, this can result in over
  // triggering of the Switch to Tab action on plain-text suggestions for
  // open entity SRPs, or vice versa, on entity suggestions for open plain-text
  // SRPs.
  const GURL stripped_url = AutocompleteMatch::GURLToStrippedGURL(
      url, *input, template_url_service_, /*keyword=*/std::u16string(),
      /*keep_search_intent_params=*/true);
  for (auto* web_contents : GetOpenWebContents()) {
    if (IsStrippedURLEqualToWebContentsURL(stripped_url, web_contents)) {
      return true;
    }
  }
  return false;
}

std::vector<TabMatcher::TabWrapper> TabMatcherDesktop::GetOpenTabs(
    const AutocompleteInput* input,
    bool exclude_active_tab) const {
  std::vector<TabMatcher::TabWrapper> open_tabs;
  for (auto* web_contents : GetOpenWebContents(exclude_active_tab)) {
    open_tabs.emplace_back(web_contents->GetTitle(),
                           web_contents->GetLastCommittedURL(),
                           web_contents->GetLastActiveTime());
  }

  return open_tabs;
}

std::vector<content::WebContents*> TabMatcherDesktop::GetOpenWebContents(
    bool exclude_active_tab) const {
  content::WebContents* active_tab = nullptr;
  if (BrowserWindowInterface* const active_bwi =
          GetLastActiveBrowserWindowInterfaceWithAnyProfile()) {
    active_tab = active_bwi->GetTabStripModel()->GetActiveWebContents();
  }

  std::vector<content::WebContents*> all_tabs;
  for (BrowserWindowInterface* bwi : GetAllBrowserWindowInterfaces()) {
    if (profile_ != bwi->GetProfile()) {
      // Only look at the same profile (and anonymity level).
      continue;
    }

    TabStripModel* const tab_strip_model = bwi->GetTabStripModel();
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      auto* web_contents = tab_strip_model->GetWebContentsAt(i);
      if (web_contents != active_tab || !exclude_active_tab) {
        all_tabs.push_back(web_contents);
      }
    }
  }
  return all_tabs;
}

bool TabMatcherDesktop::IsStrippedURLEqualToWebContentsURL(
    const GURL& stripped_url,
    content::WebContents* web_contents) const {
  AutocompleteClientWebContentsUserData::CreateForWebContents(web_contents);
  AutocompleteClientWebContentsUserData* user_data =
      AutocompleteClientWebContentsUserData::FromWebContents(web_contents);
  DCHECK(user_data);
  if (user_data->GetLastCommittedEntryIndex() !=
      web_contents->GetController().GetLastCommittedEntryIndex()) {
    user_data->UpdateLastCommittedStrippedURL(
        web_contents->GetController().GetLastCommittedEntryIndex(),
        web_contents->GetLastCommittedURL(), template_url_service_,
        /*keep_search_intent_params=*/true);
  }
  return stripped_url == user_data->GetLastCommittedStrippedURL();
}
