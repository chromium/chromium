// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_INTERNAL_AUTHENTICATOR_ANDROID_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_INTERNAL_AUTHENTICATOR_ANDROID_H_

#include <memory>
#include <string>

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "components/autofill/core/browser/payments/internal_authenticator.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}  // namespace content

// Implementation of the public InternalAuthenticator interface.
// This class is meant only for trusted and internal components of Chrome to
// use. The Android implementation is in
// org.chromium.chrome.browser.webauth.AuthenticatorImpl.
// When MakeCredential() or GetAssertion() is called, the Java implementation
// passes the response through InvokeMakeCredentialResponse() and
// InvokeGetAssertionResponse(), which eventually invokes the callback given by
// the original caller.
class InternalAuthenticatorAndroid : public autofill::InternalAuthenticator {
 public:
  explicit InternalAuthenticatorAndroid(
      content::RenderFrameHost* render_frame_host);

  ~InternalAuthenticatorAndroid() override;

  // InternalAuthenticator:
  void SetEffectiveOrigin(const url::Origin& origin) override;
  void MakeCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      blink::mojom::Authenticator::MakeCredentialCallback callback) override;
  void GetAssertion(
      blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
      blink::mojom::Authenticator::GetAssertionCallback callback) override;
  void IsUserVerifyingPlatformAuthenticatorAvailable(
      blink::mojom::Authenticator::
          IsUserVerifyingPlatformAuthenticatorAvailableCallback callback)
      override;
  void Cancel() override;
  content::RenderFrameHost* GetRenderFrameHost() override;

  void InvokeMakeCredentialResponse(
      JNIEnv* env,
      jint status,
      const base::android::JavaParamRef<jobject>& byte_buffer);
  void InvokeGetAssertionResponse(
      JNIEnv* env,
      jint status,
      const base::android::JavaParamRef<jobject>& byte_buffer);
  void InvokeIsUserVerifyingPlatformAuthenticatorAvailableResponse(
      JNIEnv* env,
      jboolean is_uvpaa);

 private:
  // Returns the associated AuthenticatorImpl Java object. Initializes new
  // instance if not done so already in order to avoid possibility of any null
  // pointer issues.
  base::android::JavaRef<jobject>& GetJavaObject();

  base::android::ScopedJavaGlobalRef<jobject> java_authenticator_impl_ref_;
  content::RenderFrameHost* render_frame_host_;
  blink::mojom::Authenticator::MakeCredentialCallback
      make_credential_response_callback_;
  blink::mojom::Authenticator::GetAssertionCallback
      get_assertion_response_callback_;
  blink::mojom::Authenticator::
      IsUserVerifyingPlatformAuthenticatorAvailableCallback is_uvpaa_callback_;
};

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_INTERNAL_AUTHENTICATOR_ANDROID_H_
