// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ssl/mock_cert_verifier_rule_android.h"

#include "base/command_line.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/test_support_jni_headers/MockCertVerifierRuleAndroid_jni.h"

jlong JNI_MockCertVerifierRuleAndroid_Init(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(new MockCertVerifierRuleAndroid());
}

MockCertVerifierRuleAndroid::MockCertVerifierRuleAndroid() = default;

void MockCertVerifierRuleAndroid::SetResult(JNIEnv* env, int result) {
  mock_cert_verifier_.mock_cert_verifier()->set_default_result(result);
}

void MockCertVerifierRuleAndroid::SetUp(JNIEnv* env) {
  mock_cert_verifier_.SetUpCommandLine(base::CommandLine::ForCurrentProcess());
  mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
}

void MockCertVerifierRuleAndroid::TearDown(JNIEnv* env) {
  mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
}
