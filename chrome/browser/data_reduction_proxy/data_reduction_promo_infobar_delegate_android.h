// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROMO_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROMO_INFOBAR_DELEGATE_ANDROID_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"

namespace content {
class WebContents;
}

namespace infobars {
class InfoBarManager;
}

// An infobar that prompts the user to enable the data reduction proxy.
class DataReductionPromoInfoBarDelegateAndroid : public ConfirmInfoBarDelegate {
 public:
  // Creates the infobar and adds it to the provided |web_contents|.
  static void Create(content::WebContents* web_contents);

  DataReductionPromoInfoBarDelegateAndroid();
  ~DataReductionPromoInfoBarDelegateAndroid() override;

  static void Launch(JNIEnv* env,
                     const base::android::JavaRef<jobject>& jweb_contents);

  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(JNIEnv* env);

 private:
  // Returns a Data Reduction Proxy infobar that owns |delegate|.
  static std::unique_ptr<infobars::InfoBar> CreateInfoBar(
      infobars::InfoBarManager* infobar_manager,
      std::unique_ptr<DataReductionPromoInfoBarDelegateAndroid> delegate);

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetMessageText() const override;
  bool Accept() override;

  DISALLOW_COPY_AND_ASSIGN(DataReductionPromoInfoBarDelegateAndroid);
};

#endif  // CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROMO_INFOBAR_DELEGATE_ANDROID_H_
