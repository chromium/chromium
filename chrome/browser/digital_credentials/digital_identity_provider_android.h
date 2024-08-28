// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "content/public/browser/digital_identity_provider.h"
#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom.h"
#include "url/origin.h"

namespace content {
class WebContents;
}

// Android specific implementation of `DigitalIdentityProvider`. It
// communicates with native apps via JNI. Once an identity is returned from
// Android apps, it sends it back to the browser where the API is initiated.
class DigitalIdentityProviderAndroid : public content::DigitalIdentityProvider {
 public:
  DigitalIdentityProviderAndroid();
  ~DigitalIdentityProviderAndroid() override;

  DigitalIdentityProviderAndroid(const DigitalIdentityProviderAndroid&) =
      delete;
  DigitalIdentityProviderAndroid& operator=(
      const DigitalIdentityProviderAndroid&) = delete;

  // Implementation of corresponding JNI methods in
  // DigitalIdentityProviderAndroid.Natives.*
  void OnReceive(JNIEnv*,
                 jstring j_digital_identity,
                 jint j_status_for_metrics);

  bool IsLowRiskOrigin(const url::Origin& to_check) const override;
  DigitalIdentityInterstitialAbortCallback ShowDigitalIdentityInterstitial(
      content::WebContents& web_contents,
      const url::Origin& origin,
      content::DigitalIdentityInterstitialType interstitial_type,
      DigitalIdentityInterstitialCallback callback) override;
  void Request(content::WebContents* web_contents,
               const url::Origin& origin,
               const base::Value request,
               DigitalIdentityCallback callback) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject>
      j_digital_identity_provider_android_;
  DigitalIdentityCallback callback_;
};

#endif  // CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_ANDROID_H_
