// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SSL_MOCK_CERT_VERIFIER_RULE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_SSL_MOCK_CERT_VERIFIER_RULE_ANDROID_H_

#include "base/android/jni_android.h"
#include "content/public/test/content_mock_cert_verifier.h"

// Enables tests to force certificate verification results.
class MockCertVerifierRuleAndroid {
 public:
  MockCertVerifierRuleAndroid();

  MockCertVerifierRuleAndroid(const MockCertVerifierRuleAndroid&) = delete;
  MockCertVerifierRuleAndroid& operator=(const MockCertVerifierRuleAndroid&) =
      delete;

  // Sets the certificate verification result to force.
  void SetResult(JNIEnv* env, int result);
  void SetUp(JNIEnv* env);
  void TearDown(JNIEnv* env);

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
};

#endif  // CHROME_BROWSER_ANDROID_SSL_MOCK_CERT_VERIFIER_RULE_ANDROID_H_
