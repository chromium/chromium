// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_INFOBAR_DELEGATE_H_

#include <string>

#include "base/android/jni_android.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents_observer.h"

class InstantAppsInfoBarDelegate : public ConfirmInfoBarDelegate,
                                   public content::WebContentsObserver {
 public:
  ~InstantAppsInfoBarDelegate() override;

  static void Create(content::WebContents* web_contents,
                     const jobject jdata,
                     const std::string& url,
                     const bool instant_app_is_default);

  base::android::ScopedJavaGlobalRef<jobject> data() { return data_; }

  // ConfirmInfoBarDelegate:
  bool ShouldExpire(const NavigationDetails& details) const override;

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  explicit InstantAppsInfoBarDelegate(content::WebContents* web_contents,
                                      const jobject jdata,
                                      const std::string& url,
                                      const bool instant_app_is_default);

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetMessageText() const override;
  bool Accept() override;
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;
  void InfoBarDismissed() override;

  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;
  base::android::ScopedJavaGlobalRef<jobject> data_;
  std::string url_;
  bool user_navigated_away_from_launch_url_;
  bool instant_app_is_default_;

  DISALLOW_COPY_AND_ASSIGN(InstantAppsInfoBarDelegate);
};

#endif  // CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_INFOBAR_DELEGATE_H_
