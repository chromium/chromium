// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SSL_MOCK_CERT_VERIFIER_RULE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_SSL_MOCK_CERT_VERIFIER_RULE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "content/public/test/content_mock_cert_verifier.h"

// Enables tests to force certificate verification results.
class MockCertVerifierRuleAndroid {
 public:
  MockCertVerifierRuleAndroid();

  // Sets the certificate verification result to force.
  void SetResult(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& obj,
                 int result);

  void SetUp(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void TearDown(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;

  DISALLOW_COPY_AND_ASSIGN(MockCertVerifierRuleAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_SSL_MOCK_CERT_VERIFIER_RULE_ANDROID_H_
