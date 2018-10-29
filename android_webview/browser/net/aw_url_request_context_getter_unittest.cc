// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/aw_url_request_context_getter.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/task/post_task.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/test_root_certs.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_config_service_android.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace android_webview {

namespace {

// A ProtocolHandler that will immediately fail all jobs.
class FailingProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new net::URLRequestFailedJob(request, network_delegate,
                                        net::URLRequestFailedJob::START,
                                        net::ERR_FAILED);
  }
};

}  // namespace

class AwURLRequestContextGetterTest : public ::testing::Test {
 public:
  AwURLRequestContextGetterTest() = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    env_ = base::android::AttachCurrentThread();
    ASSERT_TRUE(env_);

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    android_webview::AwURLRequestContextGetter::RegisterPrefs(
        pref_service_->registry());

    std::unique_ptr<net::ProxyConfigServiceAndroid> config_service_android;
    config_service_android.reset(static_cast<net::ProxyConfigServiceAndroid*>(
        net::ProxyResolutionService::CreateSystemProxyConfigService(
            base::CreateSingleThreadTaskRunnerWithTraits(
                {content::BrowserThread::IO}))
            .release()));

    getter_ = base::MakeRefCounted<android_webview::AwURLRequestContextGetter>(
        temp_dir_.GetPath(), temp_dir_.GetPath().AppendASCII("ChannelID"),
        std::move(config_service_android), pref_service_.get(), &net_log_);

    // AwURLRequestContextGetter implicitly depends on having protocol handlers
    // provided for url::kBlobScheme, url::kFileSystemScheme, and
    // content::kChromeUIScheme, so provide testing values here.
    content::ProtocolHandlerMap fake_handlers;
    fake_handlers[url::kBlobScheme].reset(new FailingProtocolHandler());
    fake_handlers[url::kFileSystemScheme].reset(new FailingProtocolHandler());
    fake_handlers[content::kChromeUIScheme].reset(new FailingProtocolHandler());
    content::URLRequestInterceptorScopedVector interceptors;
    getter_->SetHandlersAndInterceptors(&fake_handlers,
                                        std::move(interceptors));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  JNIEnv* env_;
  net::NetLog net_log_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  scoped_refptr<android_webview::AwURLRequestContextGetter> getter_;
};

// Tests that constraints on trust for Symantec-issued certificates are not
// enforced for the AwURLRequestContext(Getter), as it should behave like
// the Android system.
TEST_F(AwURLRequestContextGetterTest, SymantecPoliciesExempted) {
  net::URLRequestContext* context = getter_->GetURLRequestContext();
  ASSERT_TRUE(context);

  scoped_refptr<net::X509Certificate> cert =
      net::CreateCertificateChainFromFile(net::GetTestCertsDirectory(),
                                          "www.ahrn.com.pem",
                                          net::X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert);
  ASSERT_EQ(2u, cert->intermediate_buffers().size());

  scoped_refptr<net::X509Certificate> root =
      net::X509Certificate::CreateFromBuffer(
          bssl::UpRef(cert->intermediate_buffers().back()), {});
  ASSERT_TRUE(root);
  net::ScopedTestRoot test_root(root.get());

  net::CertVerifyResult result;
  int flags = 0;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = context->cert_verifier()->Verify(
      net::CertVerifier::RequestParams(cert, "www.ahrn.com", flags,
                                       std::string()),
      &result, callback.callback(), &request, net::NetLogWithSource());
  EXPECT_THAT(error, net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, net::test::IsError(net::OK));
}

// Tests that SHA-1 is still allowed for locally-installed trust anchors,
// including those in application manifests, as it should behave like
// the Android system.
TEST_F(AwURLRequestContextGetterTest, SHA1LocalAnchorsAllowed) {
  net::URLRequestContext* context = getter_->GetURLRequestContext();
  ASSERT_TRUE(context);

  net::CertificateList certs;
  ASSERT_TRUE(net::LoadCertificateFiles(
      {"weak_digest_sha1_ee.pem", "weak_digest_sha1_intermediate.pem",
       "weak_digest_sha1_root.pem"},
      &certs));
  ASSERT_EQ(3u, certs.size());

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(certs[1]->cert_buffer()));
  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateFromBuffer(
          bssl::UpRef(certs[0]->cert_buffer()), std::move(intermediates));
  ASSERT_TRUE(cert);

  net::ScopedTestRoot test_root(certs[2].get());

  net::CertVerifyResult result;
  int flags = 0;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = context->cert_verifier()->Verify(
      net::CertVerifier::RequestParams(cert, "127.0.0.1", flags, std::string()),
      &result, callback.callback(), &request, net::NetLogWithSource());
  EXPECT_THAT(error, net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, net::test::IsError(net::OK));
}

}  // namespace android_webview
