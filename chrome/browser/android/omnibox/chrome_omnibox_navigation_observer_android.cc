// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/chrome_omnibox_navigation_observer_android.h"

#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace {

// Helper to keep ChromeOmniboxNavigationObserverAndroid alive while the
// initiated navigation is pending.
struct NavigationUserData
    : public content::NavigationHandleUserData<NavigationUserData> {
  NavigationUserData(
      content::NavigationHandle& navigation,
      scoped_refptr<ChromeOmniboxNavigationObserverAndroid> observer)
      : observer(std::move(observer)) {}
  ~NavigationUserData() override = default;

  scoped_refptr<ChromeOmniboxNavigationObserverAndroid> observer;

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(NavigationUserData);

}  // namespace

// static
void ChromeOmniboxNavigationObserverAndroid::Create(
    content::NavigationHandle* navigation_handle,
    Profile* profile,
    const std::u16string& omnibox_user_input,
    const AutocompleteMatch& selected_match) {
  if (!navigation_handle) {
    return;
  }

  new ChromeOmniboxNavigationObserverAndroid(
      navigation_handle, profile, omnibox_user_input, selected_match);
}

void ChromeOmniboxNavigationObserverAndroid::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!NavigationEligible(navigation_handle)) {
    return;
  }

  auto shortcuts_backend = ShortcutsBackendFactory::GetForProfile(profile_);
  if (!shortcuts_backend) {
    return;
  }
  shortcuts_backend->AddOrUpdateShortcut(omnibox_user_input_, selected_match_);
}

ChromeOmniboxNavigationObserverAndroid::ChromeOmniboxNavigationObserverAndroid(
    content::NavigationHandle* navigation_handle,
    Profile* profile,
    const std::u16string& omnibox_user_input,
    const AutocompleteMatch& selected_match)
    : content::WebContentsObserver(navigation_handle->GetWebContents()),
      profile_(profile),
      omnibox_user_input_(omnibox_user_input),
      selected_match_(selected_match) {
  NavigationUserData::CreateForNavigationHandle(*navigation_handle, this);
}

ChromeOmniboxNavigationObserverAndroid::
    ~ChromeOmniboxNavigationObserverAndroid() = default;

bool ChromeOmniboxNavigationObserverAndroid::NavigationEligible(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      !navigation_handle->GetResponseHeaders()) {
    return false;
  }

  int response_code = navigation_handle->GetResponseHeaders()->response_code();

  // HTTP 2xx, 401, and 407 all indicate that the target address exists.
  return (response_code >= 200 && response_code < 300) ||
         (response_code == 401) || (response_code == 407);
}
