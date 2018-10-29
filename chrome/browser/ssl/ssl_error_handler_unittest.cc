// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_handler.h"

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/common_name_mismatch_handler.h"
#include "chrome/browser/ssl/ssl_error_assistant.h"
#include "chrome/browser/ssl/ssl_error_assistant.pb.h"
#include "chrome/common/buildflags.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/captive_portal/captive_portal_testing_utils.h"
#include "components/network_time/network_time_test_utils.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/testing_pref_service.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kCertDateErrorHistogram[] =
    "interstitial.ssl_error_handler.cert_date_error_delay";

const net::SHA256HashValue kCertPublicKeyHashValue = {{0x01, 0x02}};

const char kOkayCertName[] = "ok_cert.pem";

const uint32_t kLargeVersionId = 0xFFFFFFu;

// These certificates are self signed certificates with relevant issuer common
// names generated using the following openssl command:
//  openssl req -new -x509 -keyout server.pem -out server.pem -days 365 -nodes

// Common name: "Misconfigured Firewall_4GHPOS5412EF"
// Organization name: "Misconfigured Firewall"
const char kMisconfiguredFirewallCert[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIEKTCCAxGgAwIBAgIJAOxA1g2otzdHMA0GCSqGSIb3DQEBCwUAMIGqMQswCQYD\n"
    "VQQGEwJVUzETMBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwNU2FuIEZyYW5j\n"
    "aXNjbzEfMB0GA1UECgwWTWlzY29uZmlndXJlZCBGaXJld2FsbDEsMCoGA1UEAwwj\n"
    "TWlzY29uZmlndXJlZCBGaXJld2FsbF80R0hQT1M1NDEyRUYxHzAdBgkqhkiG9w0B\n"
    "CQEWEHRlc3RAZXhhbXBsZS5jb20wHhcNMTcwODE4MjM1MjI4WhcNMTgwODE4MjM1\n"
    "MjI4WjCBqjELMAkGA1UEBhMCVVMxEzARBgNVBAgMCkNhbGlmb3JuaWExFjAUBgNV\n"
    "BAcMDVNhbiBGcmFuY2lzY28xHzAdBgNVBAoMFk1pc2NvbmZpZ3VyZWQgRmlyZXdh\n"
    "bGwxLDAqBgNVBAMMI01pc2NvbmZpZ3VyZWQgRmlyZXdhbGxfNEdIUE9TNTQxMkVG\n"
    "MR8wHQYJKoZIhvcNAQkBFhB0ZXN0QGV4YW1wbGUuY29tMIIBIjANBgkqhkiG9w0B\n"
    "AQEFAAOCAQ8AMIIBCgKCAQEAtxh4PZ9dbqeXubutRBFSL4FschunDX/vRFzhlQdz\n"
    "3fqzIfmN2PjvwBsoX1oDaWdTTefCLad7pX08UVyX2pS0UeqYwUJL+ihXuupW0pBV\n"
    "M2VZ/soDgze7Vl9dUU43NLoODOzwvKt92QdyfS7toPEEmwFLrI4/UnzxX+QlS8qq\n"
    "naWD5ny2XZOZdNizBX1UQlvkvfYJM0wUmBZ/VUj/QQxxNHZaEBcl64t3h5jHiq1c\n"
    "gWDgp0zeYy+PbJk/LMSvF64qqMFDtujUQcniYC6HwWJ9YT7PFX2b7X9Mq4b3gtpV\n"
    "6jGXXUJqg+SfLW7XisZcWVMfHZDaVfdd35vNm61XY4sg1wIDAQABo1AwTjAdBgNV\n"
    "HQ4EFgQUmUhF2RL+A4QAEel9JiEYNbPyU+AwHwYDVR0jBBgwFoAUmUhF2RL+A4QA\n"
    "Eel9JiEYNbPyU+AwDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAO+kk\n"
    "Uin9uKD2iTkUtuoAckt+kcctmvHcP3zLe+J0m25f9HOyrjhutgIYZDwt2LiAnOTA\n"
    "CQHg3t5YyDQHove39M6I1vWhoAy9jOi0Qn+lTKkVu5H4g5ZiauO3coneqFw/dPe+\n"
    "kYye/bPKV4jNlhEYXF5+Pa7PYde0sxf7AmlDJb9NZh01xRKNFt6ScDpirhJIFdzg\n"
    "ZKram+yJyIbcZI+yd7mjzu9dSCS0NbnsZDL7xqThFFZsbhZyO98kzdDS+crip6y5\n"
    "rz3+AJpJvlGcf898Y4ibAPmeX62j6pug55TGfAdsqSVUiaQX1HcwwbmlSOYrhYTm\n"
    "lMEx5QP9TqgGU0nGwQ==\n"
    "-----END CERTIFICATE-----";

// Common name: None
// Organization name: None
const char kCertWithoutOrganizationOrCommonName[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDzzCCAregAwIBAgIJAJfHNOMLXbc4MA0GCSqGSIb3DQEBCwUAMH4xCzAJBgNV\n"
    "BAYTAlVTMRMwEQYDVQQIDApDYWxpZm9ybmlhMRYwFAYDVQQHDA1TYW4gRnJhbmNp\n"
    "c2NvMSEwHwYDVQQKDBhJbnRlcm5ldCBXaWRnaXRzIFB0eSBMdGQxHzAdBgkqhkiG\n"
    "9w0BCQEWEHRlc3RAZXhhbXBsZS5jb20wHhcNMTcwODE5MDAwNTMyWhcNMTgwODE5\n"
    "MDAwNTMyWjB+MQswCQYDVQQGEwJVUzETMBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQG\n"
    "A1UEBwwNU2FuIEZyYW5jaXNjbzEhMB8GA1UECgwYSW50ZXJuZXQgV2lkZ2l0cyBQ\n"
    "dHkgTHRkMR8wHQYJKoZIhvcNAQkBFhB0ZXN0QGV4YW1wbGUuY29tMIIBIjANBgkq\n"
    "hkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA28iX7cIS5XS+hU/0OreJXfVmEWDPVRX1\n"
    "n05AlX+ETRunnYevZOAhbSFuUeJi2cGgW4cpD6fGKrf05PpNM9GQ4yswIPlVsemR\n"
    "ickSmg8vVemPs/Hz3y0dYnRoTwzzVESh4OIVGe+rrhCUdWVHE+/HOdmHAXoBI6m1\n"
    "OhN2GgtvnEEMYzTaMRGNqb5VhRKYHwLNp8zqLtrHIbo61mi8Wl7E4NZdaVk4cTNK\n"
    "w93Y8RqlwzzpbWT9RH74JPCM+wSg0rCK+h59sa86W4yPvhXyYIGXM8WhWkMW68Ej\n"
    "jqfE0lQlEuxKPeCYZn6oC+AVRLxHCwncVxZaUtGUovMzBdV3WzsLPwIDAQABo1Aw\n"
    "TjAdBgNVHQ4EFgQUlkC11ZD66sKrb25g4mH4sob4e3MwHwYDVR0jBBgwFoAUlkC1\n"
    "1ZD66sKrb25g4mH4sob4e3MwDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQsFAAOC\n"
    "AQEAUHQZmeZdAV86TIWviPNWNqhPD+/OEGnOwgjUrBmSrQkc5hPZUhQ8the7ewNE\n"
    "V/eGjDNF72tiQqPQP7Zrhdf7i1p1Q3ufcDHpOOFbEdKd6m2DeCLg83jOLqLr/jTB\n"
    "CC7GyyWOyt+CFVRGC0yovSl3+Vxaso6DZjelO3IP5K7bT5U1f3cUZnYTpYfslh1t\n"
    "dUmxh9/MaKxnRaHkr0HDVGpWS4ZMoZUyyC6D9ZfCQ5aGJJubQEPxADc2tXHXOL73\n"
    "dspwZ8CTOlcXnfdeRIjvgxnMZLax+OFEMJdY8sgyrI9c+rk2EfOUj5JVqFDvcsYy\n"
    "ejdBhjdieIv5dTbSjIXz+ljOOA==\n"
    "-----END CERTIFICATE-----";

// Runs |quit_closure| on the UI thread once a URL request has been
// seen. Returns a request that hangs.
std::unique_ptr<net::test_server::HttpResponse> WaitForRequest(
    const base::Closure& quit_closure,
    const net::test_server::HttpRequest& request) {
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           quit_closure);
  return std::make_unique<net::test_server::HungResponse>();
}

class TestSSLErrorHandler : public SSLErrorHandler {
 public:
  TestSSLErrorHandler(
      std::unique_ptr<Delegate> delegate,
      content::WebContents* web_contents,
      Profile* profile,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      const base::Callback<void(content::CertificateRequestResultType)>&
          callback)
      : SSLErrorHandler(std::move(delegate),
                        web_contents,
                        profile,
                        cert_error,
                        ssl_info,
                        request_url,
                        callback) {}

  using SSLErrorHandler::StartHandlingError;
};

class TestSSLErrorHandlerDelegate : public SSLErrorHandler::Delegate {
 public:
  TestSSLErrorHandlerDelegate(Profile* profile,
                              content::WebContents* web_contents,
                              const net::SSLInfo& ssl_info)
      : profile_(profile),
        captive_portal_checked_(false),
        os_reports_captive_portal_(false),
        suggested_url_exists_(false),
        suggested_url_checked_(false),
        ssl_interstitial_shown_(false),
        bad_clock_interstitial_shown_(false),
        captive_portal_interstitial_shown_(false),
        mitm_software_interstitial_shown_(false),
        is_mitm_software_interstitial_enterprise_(false),
        redirected_to_suggested_url_(false),
        is_overridable_error_(true) {}

  void SendCaptivePortalNotification(
      captive_portal::CaptivePortalResult result) {
    CaptivePortalService::Results results;
    results.previous_result = captive_portal::RESULT_INTERNET_CONNECTED;
    results.result = result;
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,
        content::Source<Profile>(profile_),
        content::Details<CaptivePortalService::Results>(&results));
  }

  void SendSuggestedUrlCheckResult(
      const CommonNameMismatchHandler::SuggestedUrlCheckResult& result,
      const GURL& suggested_url) {
    suggested_url_callback_.Run(result, suggested_url);
  }

  int captive_portal_checked() const { return captive_portal_checked_; }
  int ssl_interstitial_shown() const { return ssl_interstitial_shown_; }
  int captive_portal_interstitial_shown() const {
    return captive_portal_interstitial_shown_;
  }
  int mitm_software_interstitial_shown() const {
    return mitm_software_interstitial_shown_;
  }
  bool is_mitm_software_interstitial_enterprise() const {
    return is_mitm_software_interstitial_enterprise_;
  }
  bool bad_clock_interstitial_shown() const {
    return bad_clock_interstitial_shown_;
  }
  bool suggested_url_checked() const { return suggested_url_checked_; }
  bool redirected_to_suggested_url() const {
    return redirected_to_suggested_url_;
  }

  void set_suggested_url_exists() { suggested_url_exists_ = true; }
  void set_non_overridable_error() { is_overridable_error_ = false; }
  void set_os_reports_captive_portal() { os_reports_captive_portal_ = true; }

  void ClearSeenOperations() {
    captive_portal_checked_ = false;
    os_reports_captive_portal_ = false;
    suggested_url_exists_ = false;
    suggested_url_checked_ = false;
    ssl_interstitial_shown_ = false;
    bad_clock_interstitial_shown_ = false;
    captive_portal_interstitial_shown_ = false;
    mitm_software_interstitial_shown_ = false;
    is_mitm_software_interstitial_enterprise_ = false;
    redirected_to_suggested_url_ = false;
  }

 private:
  void CheckForCaptivePortal() override {
    captive_portal_checked_ = true;
  }

  bool DoesOSReportCaptivePortal() override {
    return os_reports_captive_portal_;
  }

  bool GetSuggestedUrl(const std::vector<std::string>& dns_names,
                       GURL* suggested_url) const override {
    if (!suggested_url_exists_)
      return false;
    *suggested_url = GURL("www.example.com");
    return true;
  }

  void ShowSSLInterstitial(const GURL& support_url = GURL()) override {
    ssl_interstitial_shown_ = true;
  }

  void ShowBadClockInterstitial(const base::Time& now,
                                ssl_errors::ClockState clock_state) override {
    bad_clock_interstitial_shown_ = true;
  }

  void ShowCaptivePortalInterstitial(const GURL& landing_url) override {
    captive_portal_interstitial_shown_ = true;
  }

  void ShowMITMSoftwareInterstitial(const std::string& mitm_software_name,
                                    bool is_enterprise_managed) override {
    mitm_software_interstitial_shown_ = true;
    is_mitm_software_interstitial_enterprise_ = is_enterprise_managed;
  }

  void CheckSuggestedUrl(
      const GURL& suggested_url,
      const CommonNameMismatchHandler::CheckUrlCallback& callback) override {
    DCHECK(suggested_url_callback_.is_null());
    suggested_url_checked_ = true;
    suggested_url_callback_ = callback;
  }

  void NavigateToSuggestedURL(const GURL& suggested_url) override {
    redirected_to_suggested_url_ = true;
  }

  bool IsErrorOverridable() const override { return is_overridable_error_; }

  void ReportNetworkConnectivity(base::OnceClosure callback) override {}

  Profile* profile_;
  bool captive_portal_checked_;
  bool os_reports_captive_portal_;
  bool suggested_url_exists_;
  bool suggested_url_checked_;
  bool ssl_interstitial_shown_;
  bool bad_clock_interstitial_shown_;
  bool captive_portal_interstitial_shown_;
  bool mitm_software_interstitial_shown_;
  bool is_mitm_software_interstitial_enterprise_;
  bool redirected_to_suggested_url_;
  bool is_overridable_error_;
  CommonNameMismatchHandler::CheckUrlCallback suggested_url_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestSSLErrorHandlerDelegate);
};

}  // namespace

// A class to test name mismatch errors. Creates an error handler with a name
// mismatch error.
class SSLErrorHandlerNameMismatchTest : public ChromeRenderViewHostTestHarness {
 public:
  SSLErrorHandlerNameMismatchTest() {}
  ~SSLErrorHandlerNameMismatchTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SSLErrorHandler::ResetConfigForTesting();
    SSLErrorHandler::SetInterstitialDelayForTesting(base::TimeDelta());
    ssl_info_.cert = GetCertificate();
    ssl_info_.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
    ssl_info_.public_key_hashes.push_back(
        net::HashValue(kCertPublicKeyHashValue));

    delegate_ =
        new TestSSLErrorHandlerDelegate(profile(), web_contents(), ssl_info_);
    error_handler_.reset(new TestSSLErrorHandler(
        std::unique_ptr<SSLErrorHandler::Delegate>(delegate_), web_contents(),
        profile(), net::MapCertStatusToNetError(ssl_info_.cert_status),
        ssl_info_,
        GURL(),  // request_url
        base::Callback<void(content::CertificateRequestResultType)>()));
  }

  void TearDown() override {
    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
    error_handler_.reset(nullptr);
    SSLErrorHandler::ResetConfigForTesting();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestSSLErrorHandler* error_handler() { return error_handler_.get(); }
  TestSSLErrorHandlerDelegate* delegate() { return delegate_; }

  const net::SSLInfo& ssl_info() { return ssl_info_; }

 private:
  // Returns a certificate for the test. Virtual to allow derived fixtures to
  // use a certificate with different characteristics.
  virtual scoped_refptr<net::X509Certificate> GetCertificate() {
    return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                   "subjectAltName_www_example_com.pem");
  }

  net::SSLInfo ssl_info_;
  std::unique_ptr<TestSSLErrorHandler> error_handler_;
  TestSSLErrorHandlerDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorHandlerNameMismatchTest);
};

// A class to test name mismatch errors, where the certificate lacks a
// SubjectAltName. Creates an error handler with a name mismatch error.
class SSLErrorHandlerNameMismatchNoSANTest
    : public SSLErrorHandlerNameMismatchTest {
 public:
  SSLErrorHandlerNameMismatchNoSANTest() {}

 private:
  // Return a certificate that contains no SubjectAltName field.
  scoped_refptr<net::X509Certificate> GetCertificate() override {
    return net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  }

  DISALLOW_COPY_AND_ASSIGN(SSLErrorHandlerNameMismatchNoSANTest);
};

// A class to test the captive portal certificate list feature. Creates an error
// handler with a name mismatch error by default. The error handler can be
// recreated by calling ResetErrorHandler() with an appropriate cert status.
class SSLErrorAssistantProtoTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SSLErrorHandler::ResetConfigForTesting();
    SSLErrorHandler::SetErrorAssistantProto(
        SSLErrorAssistant::GetErrorAssistantProtoFromResourceBundle());

    SSLErrorHandler::SetInterstitialDelayForTesting(base::TimeDelta());
    ResetErrorHandlerFromFile(kOkayCertName,
                              net::CERT_STATUS_COMMON_NAME_INVALID);
  }

  void TearDown() override {
    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
    error_handler_.reset(nullptr);
    SSLErrorHandler::ResetConfigForTesting();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestSSLErrorHandler* error_handler() { return error_handler_.get(); }
  TestSSLErrorHandlerDelegate* delegate() { return delegate_; }

  const net::SSLInfo& ssl_info() { return ssl_info_; }

 protected:
  SSLErrorAssistantProtoTest() {}
  ~SSLErrorAssistantProtoTest() override {}

  void SetCaptivePortalFeatureEnabled(bool enabled) {
    if (enabled) {
      scoped_feature_list_.InitFromCommandLine(
          "CaptivePortalCertificateList" /* enabled */,
          std::string() /* disabled */);
    } else {
      scoped_feature_list_.InitFromCommandLine(
          std::string(), "CaptivePortalCertificateList" /* disabled */);
    }
  }

  void SetMITMSoftwareFeatureEnabled(bool enabled) {
    if (enabled) {
      scoped_feature_list_.InitFromCommandLine(
          "MITMSoftwareInterstitial" /* enabled */,
          std::string() /* disabled */);
    } else {
      scoped_feature_list_.InitFromCommandLine(
          std::string(), "MITMSoftwareInterstitial" /* disabled */);
    }
  }

  void ResetErrorHandlerFromString(const std::string& cert_data,
                                   net::CertStatus cert_status) {
    net::CertificateList certs =
        net::X509Certificate::CreateCertificateListFromBytes(
            cert_data.data(), cert_data.size(),
            net::X509Certificate::FORMAT_AUTO);
    ASSERT_FALSE(certs.empty());
    ResetErrorHandler(certs[0], cert_status);
  }

  void ResetErrorHandlerFromFile(const std::string& cert_name,
                                 net::CertStatus cert_status) {
    ResetErrorHandler(
        net::ImportCertFromFile(net::GetTestCertsDirectory(), cert_name),
        cert_status);
  }

  // Set up an error assistant proto with mock captive portal hash data and
  // begin handling the certificate error.
  void RunCaptivePortalTest() {
    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
    EXPECT_EQ(1u, ssl_info().public_key_hashes.size());

    auto config_proto =
        std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
    config_proto->set_version_id(kLargeVersionId);

    config_proto->add_captive_portal_cert()->set_sha256_hash(
        "sha256/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    config_proto->add_captive_portal_cert()->set_sha256_hash(
        ssl_info().public_key_hashes[0].ToString());
    config_proto->add_captive_portal_cert()->set_sha256_hash(
        "sha256/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));

    error_handler()->StartHandlingError();
  }

  void TestNoCaptivePortalInterstitial() {
    base::HistogramTester histograms;

    RunCaptivePortalTest();

#if !defined(OS_ANDROID)
    // On non-Android platforms (except for iOS where this code is disabled),
    // timer should start for captive portal detection.
    EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
    EXPECT_TRUE(delegate()->captive_portal_checked());
    EXPECT_FALSE(delegate()->ssl_interstitial_shown());
    EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
    EXPECT_FALSE(delegate()->suggested_url_checked());

    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());

    // Captive portal should be checked on non-Android platforms.
    EXPECT_TRUE(delegate()->captive_portal_checked());
    EXPECT_TRUE(delegate()->ssl_interstitial_shown());
    EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
    EXPECT_FALSE(delegate()->suggested_url_checked());
#else
    // On Android there is no custom captive portal detection logic, so the
    // timer should not start and an SSL interstitial should be shown
    // immediately.
    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
    EXPECT_FALSE(delegate()->captive_portal_checked());
    EXPECT_TRUE(delegate()->ssl_interstitial_shown());
    EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
    EXPECT_FALSE(delegate()->suggested_url_checked());

    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
    EXPECT_FALSE(delegate()->captive_portal_checked());
    EXPECT_TRUE(delegate()->ssl_interstitial_shown());
    EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
    EXPECT_FALSE(delegate()->suggested_url_checked());
#endif

    // Check that the histogram for the captive portal cert was recorded.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  }

  // Set up a mock SSL Error Assistant config with regexes that match the
  // outdated antivirus and misconfigured firewall certificate.
  void InitMITMSoftwareList() {
    auto config_proto =
        std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
    config_proto->set_version_id(kLargeVersionId);

    chrome_browser_ssl::MITMSoftware* filter =
        config_proto->add_mitm_software();
    filter->set_name("Misconfigured Firewall");
    filter->set_issuer_common_name_regex("Misconfigured Firewall_[A-Z0-9]+");
    filter->set_issuer_organization_regex("Misconfigured Firewall");

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  }

  void TestMITMSoftwareInterstitial() {
    base::HistogramTester histograms;

    delegate()->set_non_overridable_error();
    error_handler()->StartHandlingError();
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(delegate()->ssl_interstitial_shown());
    EXPECT_TRUE(delegate()->mitm_software_interstitial_shown());
    EXPECT_FALSE(delegate()->suggested_url_checked());

    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 0);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 0);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL, 1);
  }

  void TestNoMITMSoftwareInterstitial() {
    base::HistogramTester histograms;

    delegate()->set_non_overridable_error();
    error_handler()->StartHandlingError();
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
    EXPECT_TRUE(delegate()->ssl_interstitial_shown());
    EXPECT_FALSE(delegate()->mitm_software_interstitial_shown());
    EXPECT_FALSE(delegate()->suggested_url_checked());

    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL, 0);
  }

 private:
  void ResetErrorHandler(scoped_refptr<net::X509Certificate> cert,
                         net::CertStatus cert_status) {
    ssl_info_.Reset();
    ssl_info_.cert = cert;
    ssl_info_.cert_status = cert_status;
    ssl_info_.public_key_hashes.push_back(
        net::HashValue(kCertPublicKeyHashValue));

    delegate_ =
        new TestSSLErrorHandlerDelegate(profile(), web_contents(), ssl_info_);
    error_handler_.reset(new TestSSLErrorHandler(
        std::unique_ptr<SSLErrorHandler::Delegate>(delegate_), web_contents(),
        profile(), net::MapCertStatusToNetError(ssl_info_.cert_status),
        ssl_info_,
        GURL(),  // request_url
        base::Callback<void(content::CertificateRequestResultType)>()));
  }

  net::SSLInfo ssl_info_;
  std::unique_ptr<TestSSLErrorHandler> error_handler_;
  TestSSLErrorHandlerDelegate* delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorAssistantProtoTest);
};

class SSLErrorHandlerDateInvalidTest : public ChromeRenderViewHostTestHarness {
 public:
  SSLErrorHandlerDateInvalidTest()
      : ChromeRenderViewHostTestHarness(
            content::TestBrowserThreadBundle::REAL_IO_THREAD),
        field_trial_test_(new network_time::FieldTrialTest()),
        clock_(new base::SimpleTestClock),
        tick_clock_(new base::SimpleTestTickClock),
        test_server_(new net::EmbeddedTestServer) {
    network_time::NetworkTimeTracker::RegisterPrefs(pref_service_.registry());
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SSLErrorHandler::ResetConfigForTesting();

    field_trial_test()->SetNetworkQueriesWithVariationsService(
        false, 0.0,
        network_time::NetworkTimeTracker::FETCHES_IN_BACKGROUND_ONLY);

    base::RunLoop run_loop;
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info;
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(CreateURLLoaderFactory, &url_loader_factory_info),
        run_loop.QuitClosure());
    run_loop.Run();

    shared_url_loader_factory_ = network::SharedURLLoaderFactory::Create(
        std::move(url_loader_factory_info));

    tracker_.reset(new network_time::NetworkTimeTracker(
        std::unique_ptr<base::Clock>(clock_),
        std::unique_ptr<base::TickClock>(tick_clock_), &pref_service_,
        shared_url_loader_factory_));
    // Do this to be sure that |is_null| returns false.
    clock_->Advance(base::TimeDelta::FromDays(111));
    tick_clock_->Advance(base::TimeDelta::FromDays(222));

    SSLErrorHandler::SetInterstitialDelayForTesting(base::TimeDelta());
    ssl_info_.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ssl_info_.cert_status = net::CERT_STATUS_DATE_INVALID;

    delegate_ =
        new TestSSLErrorHandlerDelegate(profile(), web_contents(), ssl_info_);
    error_handler_.reset(new TestSSLErrorHandler(
        std::unique_ptr<SSLErrorHandler::Delegate>(delegate_), web_contents(),
        profile(), net::MapCertStatusToNetError(ssl_info_.cert_status),
        ssl_info_,
        GURL(),  // request_url
        base::Callback<void(content::CertificateRequestResultType)>()));
    error_handler_->SetNetworkTimeTrackerForTesting(tracker_.get());

    // Fix flakiness in case system time is off and triggers a bad clock
    // interstitial. https://crbug.com/666821#c50
    ssl_errors::SetBuildTimeForTesting(base::Time::Now());
  }

  void TearDown() override {
    // Release the reference on TestSharedURLLoaderFactory before the test
    // thread bundle flushes the IO thread so that it's destructed.
    shared_url_loader_factory_ = nullptr;

    if (error_handler()) {
      EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
      error_handler_.reset(nullptr);
    }
    SSLErrorHandler::ResetConfigForTesting();

    // ChromeRenderViewHostTestHarness::TearDown() simulates shutdown and as
    // such destroys parts of the task environment required in these
    // destructors.
    test_server_.reset();
    tracker_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestSSLErrorHandler* error_handler() { return error_handler_.get(); }
  TestSSLErrorHandlerDelegate* delegate() { return delegate_; }

  network_time::FieldTrialTest* field_trial_test() {
    return field_trial_test_.get();
  }

  network_time::NetworkTimeTracker* tracker() { return tracker_.get(); }

  net::EmbeddedTestServer* test_server() { return test_server_.get(); }

  void ClearErrorHandler() { error_handler_.reset(nullptr); }

 private:
  static void CreateURLLoaderFactory(
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>*
          url_loader_factory_info) {
    scoped_refptr<network::TestSharedURLLoaderFactory> factory =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>();
    // Holds a reference to |factory|.
    *url_loader_factory_info = factory->Clone();
  }

  net::SSLInfo ssl_info_;
  std::unique_ptr<TestSSLErrorHandler> error_handler_;
  TestSSLErrorHandlerDelegate* delegate_;

  std::unique_ptr<network_time::FieldTrialTest> field_trial_test_;
  base::SimpleTestClock* clock_;
  base::SimpleTestTickClock* tick_clock_;
  TestingPrefServiceSimple pref_service_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<network_time::NetworkTimeTracker> tracker_;
  std::unique_ptr<net::EmbeddedTestServer> test_server_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorHandlerDateInvalidTest);
};

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnTimerExpired) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());

  delegate()->ClearSeenOperations();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowCustomInterstitialOnCaptivePortalResult) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
  // Fake a captive portal result.
  delegate()->ClearSeenOperations();
  delegate()->SendCaptivePortalNotification(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_TRUE(delegate()->captive_portal_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnNoCaptivePortalResult) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
  // Fake a "connected to internet" result for the captive portal check.
  // This should immediately trigger an SSL interstitial without waiting for
  // the timer to expire.
  delegate()->ClearSeenOperations();
  delegate()->SendCaptivePortalNotification(
      captive_portal::RESULT_INTERNET_CONNECTED);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldNotCheckSuggestedUrlIfNoSuggestedUrl) {
  base::HistogramTester histograms;
  error_handler()->StartHandlingError();

  EXPECT_TRUE(delegate()->captive_portal_checked());
  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->suggested_url_checked());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldNotCheckCaptivePortalIfSuggestedUrlExists) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->suggested_url_checked());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());

  // Note that the suggested URL check is never completed, so there is no entry
  // for WWW_MISMATCH_URL_AVAILABLE or WWW_MISMATCH_URL_NOT_AVAILABLE.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_FOUND_IN_SAN, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldNotHandleNameMismatchOnNonOverridableError) {
  base::HistogramTester histograms;
  delegate()->set_non_overridable_error();
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_FALSE(delegate()->suggested_url_checked());
  EXPECT_TRUE(delegate()->captive_portal_checked());
  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
}

#else  // #if !BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnCaptivePortalDetectionDisabled) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  error_handler()->StartHandlingError();
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

#endif  // BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

// Test that a captive portal interstitial is shown if the OS reports a portal.
TEST_F(SSLErrorHandlerNameMismatchTest, OSReportsCaptivePortal) {
  base::HistogramTester histograms;
  delegate()->set_os_reports_captive_portal();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  error_handler()->StartHandlingError();
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_TRUE(delegate()->captive_portal_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::OS_REPORTS_CAPTIVE_PORTAL, 1);
}

// Test that a captive portal interstitial isn't shown if the OS reports a
// portal but CaptivePortalInterstitial feature is disabled.
TEST_F(SSLErrorHandlerNameMismatchTest,
       OSReportsCaptivePortal_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      std::string(), "CaptivePortalInterstitial" /* disabled */);

  base::HistogramTester histograms;
  delegate()->set_os_reports_captive_portal();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  error_handler()->StartHandlingError();
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::OS_REPORTS_CAPTIVE_PORTAL, 0);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnTimerExpiredWhenSuggestedUrlExists) {
  base::HistogramTester histograms;
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->suggested_url_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());

  // Note that the suggested URL check is never completed, so there is no entry
  // for WWW_MISMATCH_URL_AVAILABLE or WWW_MISMATCH_URL_NOT_AVAILABLE.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_FOUND_IN_SAN, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldRedirectOnSuggestedUrlCheckResult) {
  base::HistogramTester histograms;
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->suggested_url_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());
  // Fake a valid suggested URL check result.
  // The URL returned by |SuggestedUrlCheckResult| can be different from
  // |suggested_url|, if there is a redirect.
  delegate()->SendSuggestedUrlCheckResult(
      CommonNameMismatchHandler::SuggestedUrlCheckResult::
          SUGGESTED_URL_AVAILABLE,
      GURL("https://random.example.com"));

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_TRUE(delegate()->redirected_to_suggested_url());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_FOUND_IN_SAN, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_URL_AVAILABLE, 1);
}

// No suggestions should be requested if certificate lacks a SubjectAltName.
TEST_F(SSLErrorHandlerNameMismatchNoSANTest,
       SSLCommonNameMismatchHandlingRequiresSubjectAltName) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_FALSE(delegate()->suggested_url_checked());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_FOUND_IN_SAN, 0);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnInvalidUrlCheckResult) {
  base::HistogramTester histograms;
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->suggested_url_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());
  // Fake an Invalid Suggested URL Check result.
  delegate()->SendSuggestedUrlCheckResult(
      CommonNameMismatchHandler::SuggestedUrlCheckResult::
          SUGGESTED_URL_NOT_AVAILABLE,
      GURL());

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 4);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_FOUND_IN_SAN, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_URL_NOT_AVAILABLE,
                               1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerDateInvalidTest, TimeQueryStarted) {
  base::HistogramTester histograms;
  base::Time network_time;
  base::TimeDelta uncertainty;
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));
  EXPECT_EQ(network_time::NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker()->GetNetworkTime(&network_time, &uncertainty));

  // Enable network time queries and handle the error. A bad clock interstitial
  // should be shown.
  test_server()->RegisterRequestHandler(
      base::Bind(&network_time::GoodTimeResponseHandler));
  EXPECT_TRUE(test_server()->Start());
  tracker()->SetTimeServerURLForTesting(test_server()->GetURL("/"));
  field_trial_test()->SetNetworkQueriesWithVariationsService(
      true, 0.0, network_time::NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY);
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  tracker()->WaitForFetchForTesting(123123123);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate()->bad_clock_interstitial_shown());
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  // Check that the histogram for the delay was recorded.
  histograms.ExpectTotalCount(kCertDateErrorHistogram, 1);
}

// Tests that an SSL interstitial is shown if the accuracy of the system
// clock can't be determined because network time is unavailable.
TEST_F(SSLErrorHandlerDateInvalidTest, NoTimeQueries) {
  base::HistogramTester histograms;
  base::Time network_time;
  base::TimeDelta uncertainty;
  EXPECT_EQ(network_time::NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker()->GetNetworkTime(&network_time, &uncertainty));

  // Handle the error without enabling time queries. A bad clock interstitial
  // should not be shown.
  error_handler()->StartHandlingError();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->bad_clock_interstitial_shown());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  // Check that the histogram for the delay was recorded.
  histograms.ExpectTotalCount(kCertDateErrorHistogram, 1);
}

// Tests that an SSL interstitial is shown if determing the accuracy of
// the system clock times out (e.g. because a network time query hangs).
TEST_F(SSLErrorHandlerDateInvalidTest, TimeQueryHangs) {
  base::HistogramTester histograms;
  base::Time network_time;
  base::TimeDelta uncertainty;
  EXPECT_EQ(network_time::NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker()->GetNetworkTime(&network_time, &uncertainty));

  // Enable network time queries and handle the error. Because the
  // network time cannot be determined before the timer elapses, an SSL
  // interstitial should be shown.
  base::RunLoop wait_for_time_query_loop;
  test_server()->RegisterRequestHandler(
      base::Bind(&WaitForRequest, wait_for_time_query_loop.QuitClosure()));
  EXPECT_TRUE(test_server()->Start());
  tracker()->SetTimeServerURLForTesting(test_server()->GetURL("/"));
  field_trial_test()->SetNetworkQueriesWithVariationsService(
      true, 0.0, network_time::NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY);
  error_handler()->StartHandlingError();
  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  wait_for_time_query_loop.Run();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(delegate()->bad_clock_interstitial_shown());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());

  // Check that the histogram for the delay was recorded.
  histograms.ExpectTotalCount(kCertDateErrorHistogram, 1);

  // Clear the error handler to test that, when the request completes,
  // it doesn't try to call a callback on a deleted SSLErrorHandler.
  ClearErrorHandler();

  // Shut down the server to cancel the pending request.
  ASSERT_TRUE(test_server()->ShutdownAndWaitUntilComplete());
}

// Tests that a certificate marked as a known captive portal certificate causes
// the captive portal interstitial to be shown.
TEST_F(SSLErrorAssistantProtoTest, CaptivePortal_FeatureEnabled) {
  SetCaptivePortalFeatureEnabled(true);

  base::HistogramTester histograms;

  RunCaptivePortalTest();

  // Timer shouldn't start for a known captive portal certificate.
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_TRUE(delegate()->captive_portal_interstitial_shown());
  EXPECT_FALSE(delegate()->suggested_url_checked());

  // A buggy SSL error handler might have incorrectly started the timer. Run
  // to completion to ensure the timer is expired.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_TRUE(delegate()->captive_portal_interstitial_shown());
  EXPECT_FALSE(delegate()->suggested_url_checked());

  // Check that the histogram for the captive portal cert was recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::CAPTIVE_PORTAL_CERT_FOUND, 1);
}

// Tests that a certificate marked as a known captive portal certificate does
// not cause the captive portal interstitial to be shown, if the feature is
// disabled.
TEST_F(SSLErrorAssistantProtoTest, CaptivePortal_FeatureDisabled) {
  SetCaptivePortalFeatureEnabled(false);

  // Default error for SSLErrorHandlerNameMismatchTest tests is name mismatch.
  TestNoCaptivePortalInterstitial();
}

// Tests that an error other than name mismatch does not cause a captive portal
// interstitial to be shown, even if the certificate is marked as a known
// captive portal certificate.
TEST_F(SSLErrorAssistantProtoTest,
       CaptivePortal_AuthorityInvalidError_NoInterstitial) {
  SetCaptivePortalFeatureEnabled(true);

  ResetErrorHandlerFromFile(kOkayCertName, net::CERT_STATUS_AUTHORITY_INVALID);
  TestNoCaptivePortalInterstitial();
}

// Tests that an authority invalid error in addition to name mismatch error does
// not cause a captive portal interstitial to be shown, even if the certificate
// is marked as a known captive portal certificate. The resulting error is
// authority-invalid.
TEST_F(SSLErrorAssistantProtoTest, CaptivePortal_TwoErrors_NoInterstitial) {
  SetCaptivePortalFeatureEnabled(true);

  const net::CertStatus cert_status =
      net::CERT_STATUS_COMMON_NAME_INVALID | net::CERT_STATUS_AUTHORITY_INVALID;
  // Sanity check that AUTHORITY_INVALID is seen as the net error.
  ASSERT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            net::MapCertStatusToNetError(cert_status));
  ResetErrorHandlerFromFile(kOkayCertName, cert_status);
  TestNoCaptivePortalInterstitial();
}

// Tests that another error in addition to name mismatch error does not cause a
// captive portal interstitial to be shown, even if the certificate is marked as
// a known captive portal certificate. Similar to
// NameMismatchAndAuthorityInvalid, except the resulting error is name mismatch.
TEST_F(SSLErrorAssistantProtoTest,
       CaptivePortal_TwoErrorsIncludingNameMismatch_NoInterstitial) {
  SetCaptivePortalFeatureEnabled(true);

  const net::CertStatus cert_status =
      net::CERT_STATUS_COMMON_NAME_INVALID | net::CERT_STATUS_WEAK_KEY;
  // Sanity check that COMMON_NAME_INVALID is seen as the net error, since the
  // test is designed to verify that SSLErrorHandler notices other errors in the
  // CertStatus even when COMMON_NAME_INVALID is the net error.
  ASSERT_EQ(net::ERR_CERT_COMMON_NAME_INVALID,
            net::MapCertStatusToNetError(cert_status));
  ResetErrorHandlerFromFile(kOkayCertName, cert_status);
  TestNoCaptivePortalInterstitial();
}

// Tests that if a certificate matches the issuer common name regex of a MITM
// software entry but not the issuer organization name a MITM software
// interstitial will not be displayed.
TEST_F(SSLErrorAssistantProtoTest,
       MITMSoftware_CertificateDoesNotMatchOrganizationName_NoInterstitial) {
  SetMITMSoftwareFeatureEnabled(true);

  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);

  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  chrome_browser_ssl::MITMSoftware* filter = config_proto->add_mitm_software();
  filter->set_name("Misconfigured Firewall");
  filter->set_issuer_common_name_regex("Misconfigured Firewall_[A-Z0-9]+");
  filter->set_issuer_organization_regex("Non-Matching Organization Name");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  TestNoMITMSoftwareInterstitial();
}

// Tests that if a certificate matches the issuer organization name regex of a
// MITM software entry but not the issuer common name a MITM software
// interstitial will not be displayed.
TEST_F(SSLErrorAssistantProtoTest,
       MITMSoftware_CertificateDoesNotMatchCommonName_NoInterstitial) {
  SetMITMSoftwareFeatureEnabled(true);

  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);

  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  chrome_browser_ssl::MITMSoftware* filter = config_proto->add_mitm_software();
  filter->set_name("Misconfigured Firewall");
  filter->set_issuer_common_name_regex("Non-Matching Issuer Common Name");
  filter->set_issuer_organization_regex("Misconfigured Firewall");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  TestNoMITMSoftwareInterstitial();
}

// Tests that a certificate with no organization name or common name will not
// trigger a MITM software interstitial.
TEST_F(SSLErrorAssistantProtoTest,
       MITMSoftware_CertificateWithNoOrganizationOrCommonName_NoInterstitial) {
  SetMITMSoftwareFeatureEnabled(true);

  ResetErrorHandlerFromString(kCertWithoutOrganizationOrCommonName,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  InitMITMSoftwareList();
  TestNoMITMSoftwareInterstitial();
}

// Tests that when everything else is in order, a matching MITM software
// certificate will trigger the MITM software interstitial.
TEST_F(SSLErrorAssistantProtoTest,
       MITMSoftware_CertificateMatchesCommonNameAndOrganizationName) {
  SetMITMSoftwareFeatureEnabled(true);

  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  InitMITMSoftwareList();
  TestMITMSoftwareInterstitial();
}

// Tests that a known MITM software entry in the SSL error assistant proto that
// has a common name regex but not an organization name regex can still trigger
// a MITM software interstitial.
TEST_F(SSLErrorAssistantProtoTest, MITMSoftware_CertificateMatchesCommonName) {
  SetMITMSoftwareFeatureEnabled(true);
  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  // Register a MITM Software entry in the SSL error assistant proto that has a
  // common name regex but not an organization name regex.
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  chrome_browser_ssl::MITMSoftware* filter = config_proto->add_mitm_software();
  filter->set_name("Misconfigured Firewall");
  filter->set_issuer_common_name_regex("Misconfigured Firewall_[A-Z0-9]+");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  TestMITMSoftwareInterstitial();
}

// Tests that a known MITM software entry in the SSL error assistant proto that
// has an organization name regex but not a common name name regex can still
// trigger a MITM software interstitial.
TEST_F(SSLErrorAssistantProtoTest,
       MITMSoftware_CertificateMatchesOrganizationName) {
  SetMITMSoftwareFeatureEnabled(true);
  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  // Register a MITM Software entry in the SSL error assistant proto that has an
  // organization name regex, but not a common name regex.
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  chrome_browser_ssl::MITMSoftware* filter = config_proto->add_mitm_software();
  filter->set_name("Misconfigured Firewall");
  filter->set_issuer_organization_regex("Misconfigured Firewall");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  TestMITMSoftwareInterstitial();
}

// Tests that only a full regex match will trigger the MITM software
// interstitial. For example, a common name regex "Match" should not trigger the
// MITM software interstitial on a certificate that's common name is
// "Full Match".
TEST_F(SSLErrorAssistantProtoTest,
       MITMSoftware_PartialRegexMatch_NoInterstitial) {
  SetMITMSoftwareFeatureEnabled(true);
  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  // Register a MITM software entry with common name and organization name
  // regexes that will match part of each the certificate's common name and
  // organization name fields but not the entire field.
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  chrome_browser_ssl::MITMSoftware* filter = config_proto->add_mitm_software();
  filter->set_name("Misconfigured Firewall");
  filter->set_issuer_common_name_regex("Misconfigured");
  filter->set_issuer_organization_regex("Misconfigured");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  TestNoMITMSoftwareInterstitial();
}

// Tests that a MITM software interstitial is not triggered when neither the
// common name or the organization name match.
TEST_F(SSLErrorAssistantProtoTest,
       MITMSoftware_NonMatchingCertificate_NoInterstitial) {
  SetMITMSoftwareFeatureEnabled(true);

  ResetErrorHandlerFromFile(kOkayCertName, net::CERT_STATUS_AUTHORITY_INVALID);
  InitMITMSoftwareList();
  TestNoMITMSoftwareInterstitial();
}

// Tests that when a user's machine is enterprise managed the correct MITM
// software interstitial is triggered.
TEST_F(SSLErrorAssistantProtoTest, MITMSoftware_EnterpriseManaged) {
  SetMITMSoftwareFeatureEnabled(true);

  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  SSLErrorHandler::SetEnterpriseManagedForTesting(true);
  ASSERT_TRUE(SSLErrorHandler::IsEnterpriseManagedFlagSetForTesting());
  InitMITMSoftwareList();
  TestMITMSoftwareInterstitial();

  EXPECT_TRUE(delegate()->is_mitm_software_interstitial_enterprise());
}

// Tests that when a user's machine is not enterprise managed the correct MITM
// software interstitial is triggered.
TEST_F(SSLErrorAssistantProtoTest, MITMSoftware_NotEnterpriseManaged) {
  SetMITMSoftwareFeatureEnabled(true);

  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  SSLErrorHandler::SetEnterpriseManagedForTesting(false);
  ASSERT_TRUE(SSLErrorHandler::IsEnterpriseManagedFlagSetForTesting());
  InitMITMSoftwareList();
  TestMITMSoftwareInterstitial();

  EXPECT_FALSE(delegate()->is_mitm_software_interstitial_enterprise());
}

// Tests that the MITM software interstitial is not triggered when the feature
// is disabled by Finch.
TEST_F(SSLErrorAssistantProtoTest, MITMSoftware_FeatureDisabled) {
  SetMITMSoftwareFeatureEnabled(false);

  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  InitMITMSoftwareList();
  TestNoMITMSoftwareInterstitial();
}

// Tests that the MITM software interstitial is not triggered when an error
// other than net::CERT_STATUS_AUTHORITY_INVALID is thrown.
TEST_F(SSLErrorAssistantProtoTest, MITMSoftware_WrongError_NoInterstitial) {
  SetMITMSoftwareFeatureEnabled(true);

  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_COMMON_NAME_INVALID);
  InitMITMSoftwareList();
  TestNoMITMSoftwareInterstitial();
}

// Tests that the MITM software interstitial is not triggered when more than one
// error is thrown.
TEST_F(SSLErrorAssistantProtoTest, MITMSoftware_TwoErrors_NoInterstitial) {
  SetMITMSoftwareFeatureEnabled(true);

  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID |
                                  net::CERT_STATUS_COMMON_NAME_INVALID);
  InitMITMSoftwareList();
  TestNoMITMSoftwareInterstitial();
}

// Tests that the MITM software interstitial is not triggered if the error
// thrown is overridable.
TEST_F(SSLErrorAssistantProtoTest, MITMSoftware_Overridable_NoInterstitial) {
  base::HistogramTester histograms;

  SetMITMSoftwareFeatureEnabled(true);
  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  InitMITMSoftwareList();
  error_handler()->StartHandlingError();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->mitm_software_interstitial_shown());
  EXPECT_FALSE(delegate()->suggested_url_checked());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 0);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL,
                               0);
}

TEST_F(SSLErrorAssistantProtoTest,
       MITMSoftware_IgnoreDynamicUpdateWithSmallVersionId) {
  SetMITMSoftwareFeatureEnabled(true);
  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);

  // Register a MITM Software entry in the SSL error assistant proto that has a
  // common name regex but not an organization name regex. This should normally
  // trigger a MITM software interstitial, but the version_id is zero which is
  // less than the version_id of the local resource bundle, so the dynamic
  // update will be ignored.
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(0u);

  chrome_browser_ssl::MITMSoftware* filter = config_proto->add_mitm_software();
  filter->set_name("Misconfigured Firewall");
  filter->set_issuer_common_name_regex("Misconfigured Firewall_[A-Z0-9]+");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));

  TestNoMITMSoftwareInterstitial();
}

TEST(SSLErrorHandlerTest, CalculateOptionsMask) {
  int mask;

  // Non-overridable cert error.
  mask = SSLErrorHandler::CalculateOptionsMask(
      net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN, /* cert_error */
      false,                                     /* hard_override_disabled */
      false, /* should_ssl_errors_be_fatal */
      false, /* is_superfish */
      false /* expired_previous_decision */);
  EXPECT_EQ(0, mask);
  mask = SSLErrorHandler::CalculateOptionsMask(
      net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN, /* cert_error */
      true,                                      /* hard_override_disabled */
      false, /* should_ssl_errors_be_fatal */
      false, /* is_superfish */
      false /* expired_previous_decision */);
  EXPECT_EQ(security_interstitials::SSLErrorUI::HARD_OVERRIDE_DISABLED, mask);
  mask = SSLErrorHandler::CalculateOptionsMask(
      net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN, /* cert_error */
      false,                                     /* hard_override_disabled */
      true,  /* should_ssl_errors_be_fatal */
      false, /* is_superfish */
      false /* expired_previous_decision */);
  EXPECT_EQ(security_interstitials::SSLErrorUI::STRICT_ENFORCEMENT, mask);
  mask = SSLErrorHandler::CalculateOptionsMask(
      net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN, /* cert_error */
      false,                                     /* hard_override_disabled */
      false, /* should_ssl_errors_be_fatal */
      true,  /* is_superfish */
      false /* expired_previous_decision */);
  EXPECT_EQ(0, mask);
  mask = SSLErrorHandler::CalculateOptionsMask(
      net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN, /* cert_error */
      false,                                     /* hard_override_disabled */
      false, /* should_ssl_errors_be_fatal */
      false, /* is_superfish */
      true /* expired_previous_decision */);
  EXPECT_EQ(security_interstitials::SSLErrorUI::EXPIRED_BUT_PREVIOUSLY_ALLOWED,
            mask);
  mask = SSLErrorHandler::CalculateOptionsMask(
      net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN, /* cert_error */
      true,                                      /* hard_override_disabled */
      true, /* should_ssl_errors_be_fatal */
      true, /* is_superfish */
      true /* expired_previous_decision */);
  EXPECT_EQ(
      security_interstitials::SSLErrorUI::HARD_OVERRIDE_DISABLED |
          security_interstitials::SSLErrorUI::STRICT_ENFORCEMENT |
          security_interstitials::SSLErrorUI::EXPIRED_BUT_PREVIOUSLY_ALLOWED,
      mask);

  // Overridable cert error.
  mask = SSLErrorHandler::CalculateOptionsMask(
      net::ERR_CERT_DATE_INVALID, /* cert_error */
      false,                      /* hard_override_disabled */
      false,                      /* should_ssl_errors_be_fatal */
      false,                      /* is_superfish */
      false /* expired_previous_decision */);
  EXPECT_EQ(security_interstitials::SSLErrorUI::SOFT_OVERRIDE_ENABLED, mask);
  mask = SSLErrorHandler::CalculateOptionsMask(
      net::ERR_CERT_DATE_INVALID, /* cert_error */
      true,                       /* hard_override_disabled */
      false,                      /* should_ssl_errors_be_fatal */
      false,                      /* is_superfish */
      false /* expired_previous_decision */);
  EXPECT_EQ(security_interstitials::SSLErrorUI::HARD_OVERRIDE_DISABLED, mask);
  mask = SSLErrorHandler::CalculateOptionsMask(
      net::ERR_CERT_DATE_INVALID, /* cert_error */
      false,                      /* hard_override_disabled */
      true,                       /* should_ssl_errors_be_fatal */
      false,                      /* is_superfish */
      false /* expired_previous_decision */);
  EXPECT_EQ(security_interstitials::SSLErrorUI::STRICT_ENFORCEMENT, mask);
  mask = SSLErrorHandler::CalculateOptionsMask(
      net::ERR_CERT_DATE_INVALID, /* cert_error */
      false,                      /* hard_override_disabled */
      false,                      /* should_ssl_errors_be_fatal */
      true,                       /* is_superfish */
      false /* expired_previous_decision */);
  EXPECT_EQ(0, mask);
  mask = SSLErrorHandler::CalculateOptionsMask(
      net::ERR_CERT_DATE_INVALID, /* cert_error */
      false,                      /* hard_override_disabled */
      false,                      /* should_ssl_errors_be_fatal */
      false,                      /* is_superfish */
      true /* expired_previous_decision */);
  EXPECT_EQ(
      security_interstitials::SSLErrorUI::SOFT_OVERRIDE_ENABLED |
          security_interstitials::SSLErrorUI::EXPIRED_BUT_PREVIOUSLY_ALLOWED,
      mask);
  mask = SSLErrorHandler::CalculateOptionsMask(
      net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN, /* cert_error */
      true,                                      /* hard_override_disabled */
      true, /* should_ssl_errors_be_fatal */
      true, /* is_superfish */
      true /* expired_previous_decision */);
  EXPECT_EQ(
      security_interstitials::SSLErrorUI::HARD_OVERRIDE_DISABLED |
          security_interstitials::SSLErrorUI::STRICT_ENFORCEMENT |
          security_interstitials::SSLErrorUI::EXPIRED_BUT_PREVIOUSLY_ALLOWED,
      mask);
}
