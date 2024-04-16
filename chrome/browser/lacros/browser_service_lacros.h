// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_BROWSER_SERVICE_LACROS_H_
#define CHROME_BROWSER_LACROS_BROWSER_SERVICE_LACROS_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chromeos/crosapi/mojom/browser_service.mojom.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "components/policy/core/common/values_util.h"
#include "mojo/public/cpp/bindings/receiver.h"

class GURL;
class Profile;
class ScopedKeepAlive;

// BrowserService's Lacros implementation.
// This handles the requests from ash-chrome.
class BrowserServiceLacros : public crosapi::mojom::BrowserService,
                             public BrowserListObserver {
 public:
  BrowserServiceLacros();
  BrowserServiceLacros(const BrowserServiceLacros&) = delete;
  BrowserServiceLacros& operator=(const BrowserServiceLacros&) = delete;
  ~BrowserServiceLacros() override;

  // crosapi::mojom::BrowserService:
  void REMOVED_0() override;
  void REMOVED_2() override;
  void REMOVED_7(bool should_trigger_session_restore,
                 base::OnceClosure callback) override;
  void REMOVED_16(base::flat_map<policy::PolicyNamespace, std::vector<uint8_t>>
                      policy) override;
  void NewWindow(bool incognito,
                 bool should_trigger_session_restore,
                 int64_t target_display_id,
                 std::optional<uint64_t> profile_id,
                 NewWindowCallback callback) override;
  void NewFullscreenWindow(const GURL& url,
                           int64_t target_display_id,
                           NewFullscreenWindowCallback callback) override;
  void NewGuestWindow(int64_t target_display_id,
                      NewGuestWindowCallback callback) override;
  void NewWindowForDetachingTab(
      const std::u16string& tab_id,
      const std::u16string& group_id,
      NewWindowForDetachingTabCallback callback) override;
  void NewTab(std::optional<uint64_t> profile_id,
              NewTabCallback callback) override;
  void Launch(int64_t target_display_id,
              std::optional<uint64_t> profile_id,
              LaunchCallback callback) override;
  void OpenUrl(const GURL& url,
               crosapi::mojom::OpenUrlParamsPtr params,
               OpenUrlCallback callback) override;
  void RestoreTab(RestoreTabCallback callback) override;
  void HandleTabScrubbing(float x_offset, bool is_fling_scroll_event) override;
  void GetFeedbackData(GetFeedbackDataCallback callback) override;
  void GetHistograms(GetHistogramsCallback callback) override;
  void GetActiveTabUrl(GetActiveTabUrlCallback callback) override;
  void UpdateDeviceAccountPolicy(const std::vector<uint8_t>& policy) override;
  void NotifyPolicyFetchAttempt() override;
  void UpdateKeepAlive(bool enabled) override;
  void OpenForFullRestore(bool skip_crash_restore) override;
  void OpenProfileManager() override;
  void OpenCaptivePortalSignin(const GURL& url,
                               OpenUrlCallback callback) override;

 private:
  struct PendingOpenUrl;

  void OnSystemInformationReady(
      GetFeedbackDataCallback callback,
      std::unique_ptr<system_logs::SystemLogsResponse> sys_info);

  void OnGetCompressedHistograms(GetHistogramsCallback callback,
                                 const std::string& compressed_histogram);

  void OpenUrlImpl(Profile* profile,
                   const GURL& url,
                   crosapi::mojom::OpenUrlParamsPtr params,
                   OpenUrlCallback callback);

  // These *WithProfile() methods are called asynchronously by the corresponding
  // profile-less function, after loading the profile.
  void NewWindowWithProfile(bool incognito,
                            bool should_trigger_session_restore,
                            int64_t target_display_id,
                            NewWindowCallback callback,
                            Profile* profile);
  void NewFullscreenWindowWithProfile(const GURL& url,
                                      int64_t target_display_id,
                                      NewFullscreenWindowCallback callback,
                                      Profile* profile);
  void NewWindowForDetachingTabWithProfile(
      const std::u16string& tab_id,
      const std::u16string& group_id,
      NewWindowForDetachingTabCallback callback,
      Profile* profile);
  void LaunchOrNewTabWithProfile(bool should_trigger_session_restore,
                                 int64_t target_display_id,
                                 NewTabCallback callback,
                                 bool is_new_tab,
                                 Profile* profile);
  void OpenUrlWithProfile(const GURL& url,
                          crosapi::mojom::OpenUrlParamsPtr params,
                          OpenUrlCallback callback,
                          Profile* profile);
  void OpenCaptivePortalSigninWithProfile(const GURL& url,
                                          OpenUrlCallback callback,
                                          Profile* profile);
  void RestoreTabWithProfile(RestoreTabCallback callback, Profile* profile);
  void OpenForFullRestoreWithProfile(bool skip_crash_restore, Profile* profile);
  void UpdateComponentPolicy(policy::ComponentPolicyMap policy) override;

  // Called when a session is restored.
  void OnSessionRestored(Profile* profile, int num_tabs_restored);

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

  // Keeps the Lacros browser alive in the background. This is destroyed once
  // any browser window is opened.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::vector<PendingOpenUrl> pending_open_urls_;

  base::CallbackListSubscription session_restored_subscription_;
  mojo::Receiver<crosapi::mojom::BrowserService> receiver_{this};
  base::WeakPtrFactory<BrowserServiceLacros> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_BROWSER_SERVICE_LACROS_H_
