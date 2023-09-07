// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_SUPPORT_TPCD_SUPPORT_MANAGER_H_
#define CHROME_BROWSER_TPCD_SUPPORT_TPCD_SUPPORT_MANAGER_H_

#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class TpcdSupportDelegate {
 public:
  explicit TpcdSupportDelegate(content::BrowserContext* browser_context)
      : browser_context_(browser_context) {}

  virtual ~TpcdSupportDelegate() = default;
  // Updates ContentSettingsForOneType::TPCD_SUPPORT to reflect
  // |origin|'s enrollment status (when embedded by |partition_origin|).
  void Update3pcdSupportSettings(const url::Origin& origin,
                                 const url::Origin& partition_origin,
                                 bool enrolled);

 private:
  raw_ptr<content::BrowserContext> browser_context_;
};

// Observes a WebContents to detect changes in enrollment and
// update TPCD_SUPPORT content settings appropriately.
class TpcdSupportManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<TpcdSupportManager> {
 public:
  using ContentSettingUpdateCallback =
      base::OnceCallback<void(const url::Origin& request_origin,
                              const url::Origin& partition_origin,
                              bool enrolled)>;

  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  ~TpcdSupportManager() override;
  TpcdSupportManager(const TpcdSupportManager&) = delete;
  TpcdSupportManager& operator=(const TpcdSupportManager&) = delete;

 private:
  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class content::WebContentsUserData<TpcdSupportManager>;

  explicit TpcdSupportManager(content::WebContents* web_contents,
                              std::unique_ptr<TpcdSupportDelegate> delegate);

  // Updates ContentSettingsForOneType::TPCD_SUPPORT to reflect
  // |origin|'s enrollment status (when embedded by |partition_origin|).
  void Update3pcdSupportSettings(const url::Origin& origin,
                                 const url::Origin& partition_origin,
                                 bool enrolled);
  void Check3pcdTrialOnUiThread(ContentSettingUpdateCallback done_callback,
                                const url::Origin& request_origin,
                                const url::Origin& partition_origin);
  // Post a call to the UI thread to check the enrollment status of
  // |request_origin| (when embedded by |partition_origin|).
  void Check3pcdTrialAsync(ContentSettingUpdateCallback done_callback,
                           const url::Origin& request_origin,
                           const url::Origin& partition_origin);

  void OnNavigationResponse(content::NavigationHandle* navigation_handle);

  // WebContentsObserver overrides:
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  std::unique_ptr<TpcdSupportDelegate> delegate_;
  base::WeakPtrFactory<TpcdSupportManager> weak_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_TPCD_SUPPORT_TPCD_SUPPORT_MANAGER_H_
