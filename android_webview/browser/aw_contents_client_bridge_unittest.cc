// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_contents_client_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/test/android_webview_unittests_jni/MockAwContentsClientBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
using net::SSLCertRequestInfo;
using net::SSLPrivateKey;
using net::X509Certificate;
using testing::NotNull;
using testing::Test;

namespace android_webview {

namespace {

// Tests the android_webview contents client bridge.
class AwContentsClientBridgeTest : public Test {
 public:
  AwContentsClientBridgeTest() {}

  // Callback method called when a cert is selected.
  void CertSelected(scoped_refptr<X509Certificate> cert,
                    scoped_refptr<SSLPrivateKey> key);

 protected:
  void SetUp() override;
  void TestSignatureAlgorithms(const std::vector<uint16_t>& algorithms,
                               const std::vector<std::string>& expected_names);
  // Create the TestBrowserThreads. Just instantiate the member variable.
  content::BrowserTaskEnvironment task_environment_;
  base::android::ScopedJavaGlobalRef<jobject> jbridge_;
  std::unique_ptr<AwContentsClientBridge> bridge_;
  scoped_refptr<SSLCertRequestInfo> cert_request_info_;
  scoped_refptr<X509Certificate> selected_cert_;
  scoped_refptr<SSLPrivateKey> selected_key_;
  int cert_selected_callbacks_;
  raw_ptr<JNIEnv> env_;
};

class TestClientCertificateDelegate
    : public content::ClientCertificateDelegate {
 public:
  explicit TestClientCertificateDelegate(AwContentsClientBridgeTest* test)
      : test_(test) {}

  TestClientCertificateDelegate(const TestClientCertificateDelegate&) = delete;
  TestClientCertificateDelegate& operator=(
      const TestClientCertificateDelegate&) = delete;

  // content::ClientCertificateDelegate.
  void ContinueWithCertificate(scoped_refptr<net::X509Certificate> cert,
                               scoped_refptr<net::SSLPrivateKey> key) override {
    test_->CertSelected(std::move(cert), std::move(key));
    test_ = nullptr;
  }

 private:
  raw_ptr<AwContentsClientBridgeTest> test_;
};

}  // namespace

void AwContentsClientBridgeTest::SetUp() {
  env_ = AttachCurrentThread();
  ASSERT_THAT(env_, NotNull());
  jbridge_.Reset(
      env_,
      Java_MockAwContentsClientBridge_getAwContentsClientBridge(env_).obj());
  bridge_ = std::make_unique<AwContentsClientBridge>(env_, jbridge_);
  selected_cert_ = nullptr;
  cert_selected_callbacks_ = 0;
  cert_request_info_ = base::MakeRefCounted<net::SSLCertRequestInfo>();
}

void AwContentsClientBridgeTest::CertSelected(
    scoped_refptr<X509Certificate> cert,
    scoped_refptr<SSLPrivateKey> key) {
  selected_cert_ = std::move(cert);
  selected_key_ = std::move(key);
  cert_selected_callbacks_++;
}

TEST_F(AwContentsClientBridgeTest, TestSignatureAlgorithmsCorrectlyEncoded) {
  TestSignatureAlgorithms(
      {SSL_SIGN_RSA_PSS_RSAE_SHA256, SSL_SIGN_RSA_PSS_RSAE_SHA384,
       SSL_SIGN_ECDSA_SECP256R1_SHA256},
      {"RSA", "EC"});
  TestSignatureAlgorithms({SSL_SIGN_RSA_PSS_RSAE_SHA256}, {"RSA"});
  TestSignatureAlgorithms(
      {SSL_SIGN_ECDSA_SECP256R1_SHA256, SSL_SIGN_ECDSA_SECP384R1_SHA384},
      {"EC"});
  TestSignatureAlgorithms({SSL_SIGN_ED25519}, {});
}

void AwContentsClientBridgeTest::TestSignatureAlgorithms(
    const std::vector<uint16_t>& algorithms,
    const std::vector<std::string>& expected_names) {
  cert_request_info_->signature_algorithms = algorithms;
  bridge_->SelectClientCertificate(
      cert_request_info_.get(),
      std::make_unique<TestClientCertificateDelegate>(this));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, cert_selected_callbacks_);
  ScopedJavaLocalRef<jobjectArray> key_types =
      Java_MockAwContentsClientBridge_getKeyTypes(env_, jbridge_);
  std::vector<std::string> vec;
  base::android::AppendJavaStringArrayToStringVector(env_, key_types, &vec);
  EXPECT_EQ(expected_names, vec);
}

// Verify that ProvideClientCertificateResponse works properly when the client
// responds with a null key.
TEST_F(AwContentsClientBridgeTest,
       TestProvideClientCertificateResponseCallsCallbackOnNullKey) {
  // Call SelectClientCertificate to create a callback id that mock java object
  // can call on.
  bridge_->SelectClientCertificate(
      cert_request_info_.get(),
      base::WrapUnique(new TestClientCertificateDelegate(this)));
  bridge_->ProvideClientCertificateResponse(
      env_, jbridge_,
      Java_MockAwContentsClientBridge_getRequestId(env_, jbridge_),
      Java_MockAwContentsClientBridge_createTestCertChain(env_, jbridge_),
      nullptr);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, selected_cert_.get());
  EXPECT_EQ(nullptr, selected_key_.get());
  EXPECT_EQ(1, cert_selected_callbacks_);
}

// Verify that ProvideClientCertificateResponse calls the callback with
// null parameters when private key is not provided.
TEST_F(AwContentsClientBridgeTest,
       TestProvideClientCertificateResponseCallsCallbackOnNullChain) {
  // Call SelectClientCertificate to create a callback id that mock java object
  // can call on.
  bridge_->SelectClientCertificate(
      cert_request_info_.get(),
      base::WrapUnique(new TestClientCertificateDelegate(this)));
  int requestId = Java_MockAwContentsClientBridge_getRequestId(env_, jbridge_);
  bridge_->ProvideClientCertificateResponse(env_, jbridge_, requestId, nullptr,
                                            nullptr);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, selected_cert_.get());
  EXPECT_EQ(nullptr, selected_key_.get());
  EXPECT_EQ(1, cert_selected_callbacks_);
}

}  // namespace android_webview
