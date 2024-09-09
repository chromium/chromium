// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_NAVIGATION_OBSERVER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/data_protection/data_protection_page_user_data.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

class Profile;

namespace content {

class NavigationHandle;
class WebContents;

}  // namespace content

namespace safe_browsing {
class RealTimeUrlLookupServiceBase;
}  // namespace safe_browsing

namespace enterprise_data_protection {

bool IsDataProtectionEnabled(Profile* profile);

// Monitors a navigation in a WebContents to determine if data protection
// settings should be enabled or not.
class DataProtectionNavigationObserver
    : public content::NavigationHandleUserData<
          DataProtectionNavigationObserver>,
      public content::WebContentsObserver {
 public:
  // Callback that is meant to update data protection settings. For now,
  // it is only accepts a std::string parameter for the watermark string.
  // change this when adding new data protection settings.
  using Callback = base::OnceCallback<void(const UrlSettings&)>;

  // Log values for source of realtime URL lookup verdict. This is used to log
  // metrics as DataProtectionURLVerdictSource, so numeric values must not be
  // changed.
  enum class URLVerdictSource {
    // Verdict has been stored in the current Page's UserData.
    kPageUserData = 0,

    // Verdict has been stored by this class's lookup callback in the
    // `watermark_text_` member.
    kCachedLookupResult = 1,

    // Verdict was not cached, so we needed to perform a lookup in
    // DidFinishNavigation().
    kPostNavigationLookup = 2,

    // Verdict was stored in the WebContents' UserData.
    kWebContentsUserData = 3,

    kMaxValue = kWebContentsUserData
  };

  // Creates a DataProtectionNavigationObserver if needed.  For example, the
  // user data may not be needed for internal chrome URLs or if the required
  // enterprise policies are not set. If this is a same doc navigation, a
  // non-primary-main frame navigation, the data protection state should remain
  // unchanged.
  //
  // This function should be called in some DidStartNavigation() function
  // so that DataProtectionNavigationObserver can be created early enough to
  // monitor the whole navigation.
  //
  // The created DataProtectionNavigationObserver will delete itself when the
  // navigation completes.
  static void CreateForNavigationIfNeeded(
      Profile* profile,
      content::NavigationHandle* navigation_handle,
      Callback callback);

  // Checks the `web_contents` url for enabled data protection settings. Note
  // that `callback` is always invoked but it be called synchronously or
  // asynchronously depending on whether the state is cached in
  // RealTimeUrlLookupService or not.
  static void GetDataProtectionSettings(Profile* profile,
                                        content::WebContents* web_contents,
                                        Callback callback);

  // public for testing
  DataProtectionNavigationObserver(
      content::NavigationHandle& navigation_handle,
      safe_browsing::RealTimeUrlLookupServiceBase* lookup_service,
      content::WebContents* web_contents,
      Callback callback);

  ~DataProtectionNavigationObserver() override;

  static void SetLookupServiceForTesting(
      safe_browsing::RealTimeUrlLookupServiceBase* lookup_service);

 private:
  friend class content::NavigationHandleUserData<
      DataProtectionNavigationObserver>;

  void OnLookupComplete(
      std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response);

  // Returns true when the "EnterpriseRealTimeUrlCheckMode" policy is enabled
  // for `browser_context`, and when a `lookup_service_` is available to make
  // URL filtering checks.
  bool ShouldPerformRealTimeUrlCheck(
      content::BrowserContext* browser_context) const;

  // content::WebContentsObserver:
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  bool is_from_cache_ = false;

  // Screenshots are allowed unless explicitly blocked.
  bool allow_screenshot_ = true;

  // The verdict indicating what watermark should be shown, if populated. Used
  // for reporting as well.
  std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response_;

  // Identifier string to show in the watermark if needed. This is either a user
  // email or a device ID.
  std::string identifier_;

  raw_ptr<safe_browsing::RealTimeUrlLookupServiceBase> lookup_service_ =
      nullptr;
  Callback pending_navigation_callback_;

  base::WeakPtrFactory<DataProtectionNavigationObserver> weak_factory_{this};

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_NAVIGATION_OBSERVER_H_
