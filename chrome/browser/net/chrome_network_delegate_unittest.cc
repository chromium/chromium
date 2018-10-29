// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/net/reporting_permissions_checker.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/event_router_forwarder.h"
#endif

namespace {

// Helper function to make the IsAccessAllowed test concise.
bool IsAccessAllowed(const std::string& path,
                     const std::string& profile_path) {
  return ChromeNetworkDelegate::IsAccessAllowed(
      base::FilePath::FromUTF8Unsafe(path),
      base::FilePath::FromUTF8Unsafe(profile_path));
}

}  // namespace

class ChromeNetworkDelegateTest : public testing::Test {
 public:
  ChromeNetworkDelegateTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    forwarder_ = new extensions::EventRouterForwarder();
#endif
  }

  void SetUp() override {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
  }

  virtual void Initialize() {
    network_delegate_.reset(new ChromeNetworkDelegate(forwarder()));
  }

  net::NetworkDelegate* network_delegate() { return network_delegate_.get(); }

  ChromeNetworkDelegate* chrome_network_delegate() {
    return network_delegate_.get();
  }

  extensions::EventRouterForwarder* forwarder() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    return forwarder_.get();
#else
    return nullptr;
#endif
  }

 private:
  std::unique_ptr<TestingProfileManager> profile_manager_;
  content::TestBrowserThreadBundle thread_bundle_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::EventRouterForwarder> forwarder_;
#endif
  TestingProfile profile_;
  std::unique_ptr<ChromeNetworkDelegate> network_delegate_;
};

class ChromeNetworkDelegatePolicyTest : public testing::Test {
 public:
  ChromeNetworkDelegatePolicyTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    forwarder_ = new extensions::EventRouterForwarder();
#endif
  }

 protected:
   void SetDelegate(net::NetworkDelegate* delegate) {
     context_.set_network_delegate(delegate);
   }

  extensions::EventRouterForwarder* forwarder() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    return forwarder_.get();
#else
    return nullptr;
#endif
  }

  content::TestBrowserThreadBundle thread_bundle_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::EventRouterForwarder> forwarder_;
#endif
  TestingProfile profile_;
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
  DISALLOW_COPY_AND_ASSIGN(ChromeNetworkDelegatePolicyTest);
};

// Privacy Mode disables Channel Id if cookies are blocked (cr223191)
class ChromeNetworkDelegatePrivacyModeTest : public testing::Test {
 public:
  ChromeNetworkDelegatePrivacyModeTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
#if BUILDFLAG(ENABLE_EXTENSIONS)
        forwarder_(new extensions::EventRouterForwarder()),
#endif
        cookie_settings_(CookieSettingsFactory::GetForProfile(&profile_).get()),
        kBlockedSite("http://ads.thirdparty.com"),
        kAllowedSite("http://good.allays.com"),
        kFirstPartySite("http://cool.things.com"),
        kBlockedFirstPartySite("http://no.thirdparties.com") {
  }

 protected:
  std::unique_ptr<ChromeNetworkDelegate> CreateNetworkDelegate() {
    std::unique_ptr<ChromeNetworkDelegate> network_delegate(
        new ChromeNetworkDelegate(forwarder()));
    network_delegate->set_cookie_settings(cookie_settings_);
    return network_delegate;
  }

  void SetDelegate(net::NetworkDelegate* delegate) {
    network_delegate_ = delegate;
    context_.set_network_delegate(network_delegate_);
  }

 protected:
  extensions::EventRouterForwarder* forwarder() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    return forwarder_.get();
#else
    return NULL;
#endif
  }

  content::TestBrowserThreadBundle thread_bundle_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::EventRouterForwarder> forwarder_;
#endif
  TestingProfile profile_;
  content_settings::CookieSettings* cookie_settings_;
  std::unique_ptr<net::URLRequest> request_;
  net::TestURLRequestContext context_;
  net::NetworkDelegate* network_delegate_;

  const GURL kBlockedSite;
  const GURL kAllowedSite;
  const GURL kEmptyFirstPartySite;
  const GURL kFirstPartySite;
  const GURL kBlockedFirstPartySite;
};

TEST(ChromeNetworkDelegateStaticTest, IsAccessAllowed) {
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  // Platforms other than Chrome OS and Android have access to any files.
  EXPECT_TRUE(IsAccessAllowed("/", ""));
  EXPECT_TRUE(IsAccessAllowed("/foo.txt", ""));
#endif

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  // Chrome OS and Android don't have access to random files.
  EXPECT_FALSE(IsAccessAllowed("/", ""));
  EXPECT_FALSE(IsAccessAllowed("/foo.txt", ""));
  // Empty path should not be allowed.
  EXPECT_FALSE(IsAccessAllowed("", ""));
#endif

#if defined(OS_CHROMEOS)
  base::FilePath temp_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_TEMP, &temp_dir));
  // Chrome OS allows the following directories.
  EXPECT_TRUE(IsAccessAllowed("/home/chronos/user/Downloads", ""));
  EXPECT_TRUE(IsAccessAllowed("/home/chronos/user/log", ""));
  EXPECT_TRUE(IsAccessAllowed("/home/chronos/user/WebRTC Logs", ""));
  EXPECT_TRUE(IsAccessAllowed("/media", ""));
  EXPECT_TRUE(IsAccessAllowed("/opt/oem", ""));
  EXPECT_TRUE(IsAccessAllowed("/usr/share/chromeos-assets", ""));
  EXPECT_TRUE(IsAccessAllowed(temp_dir.AsUTF8Unsafe(), ""));
  EXPECT_TRUE(IsAccessAllowed("/var/log", ""));
  // Files under the directories are allowed.
  EXPECT_TRUE(IsAccessAllowed("/var/log/foo.txt", ""));
  // Make sure similar paths are not allowed.
  EXPECT_FALSE(IsAccessAllowed("/home/chronos/user/log.txt", ""));
  EXPECT_FALSE(IsAccessAllowed("/home/chronos/user", ""));
  EXPECT_FALSE(IsAccessAllowed("/home/chronos", ""));

  // If profile path is given, the following additional paths are allowed.
  EXPECT_TRUE(IsAccessAllowed("/profile/Downloads", "/profile"));
  EXPECT_TRUE(IsAccessAllowed("/profile/WebRTC Logs", "/profile"));

  // GCache/v2/<opaque ID>/Logs is allowed.
  EXPECT_TRUE(IsAccessAllowed("/profile/GCache/v2/id/Logs", "/profile"));
  EXPECT_TRUE(
      IsAccessAllowed("/profile/GCache/v2/id/Logs/drivefs.txt", "/profile"));
  EXPECT_FALSE(
      IsAccessAllowed("/profile/GCache/v2/id/logs/drivefs.txt", "/profile"));
  EXPECT_FALSE(
      IsAccessAllowed("/profile/GCache/v2/id/something_else", "/profile"));
  EXPECT_FALSE(IsAccessAllowed("/profile/GCache/v2/id", "/profile"));
  EXPECT_FALSE(IsAccessAllowed("/profile/GCache/v2", "/profile"));
  EXPECT_FALSE(IsAccessAllowed("/home/chronos/user/GCache/v2/id/Logs", ""));

#elif defined(OS_ANDROID)
  // Android allows the following directories.
  EXPECT_TRUE(IsAccessAllowed("/sdcard", ""));
  EXPECT_TRUE(IsAccessAllowed("/mnt/sdcard", ""));
  // Files under the directories are allowed.
  EXPECT_TRUE(IsAccessAllowed("/sdcard/foo.txt", ""));
  // Make sure similar paths are not allowed.
  EXPECT_FALSE(IsAccessAllowed("/mnt/sdcard.txt", ""));
  EXPECT_FALSE(IsAccessAllowed("/mnt", ""));

  // Files in external storage are allowed.
  base::FilePath external_storage_path;
  base::PathService::Get(base::DIR_ANDROID_EXTERNAL_STORAGE,
                         &external_storage_path);
  EXPECT_TRUE(IsAccessAllowed(
      external_storage_path.AppendASCII("foo.txt").AsUTF8Unsafe(), ""));
  // The external storage root itself is not allowed.
  EXPECT_FALSE(IsAccessAllowed(external_storage_path.AsUTF8Unsafe(), ""));
#endif
}

namespace {

class TestingPermissionProfile : public TestingProfile {
 public:
  TestingPermissionProfile() = default;

  content::MockPermissionManager* mock_permission_manager() {
    return &mock_permission_manager_;
  }

  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override {
    return &mock_permission_manager_;
  }

 private:
  content::MockPermissionManager mock_permission_manager_;
};

}  // namespace

class ChromeNetworkDelegateReportingTest : public ChromeNetworkDelegateTest {
 public:
  ChromeNetworkDelegateReportingTest() : factory_(&profile_) {}

  content::MockPermissionManager* mock_permission_manager() {
    return profile_.mock_permission_manager();
  }

  void Initialize() override {
    ChromeNetworkDelegateTest::Initialize();
    chrome_network_delegate()->set_reporting_permissions_checker(
        factory_.CreateChecker());
  }

 protected:
  TestingPermissionProfile profile_;
  ReportingPermissionsCheckerFactory factory_;
  DISALLOW_COPY_AND_ASSIGN(ChromeNetworkDelegateReportingTest);
};

TEST_F(ChromeNetworkDelegateReportingTest, ChecksReportingPermissions) {
  using testing::_;
  using testing::Return;

  Initialize();

  auto origin1 = url::Origin::Create(GURL("https://example.com/"));
  auto origin2 = url::Origin::Create(GURL("https://foo.example.com/"));

  std::set<url::Origin> origins = {origin1, origin2};

  EXPECT_CALL(*mock_permission_manager(),
              GetPermissionStatus(_, origin1.GetURL(), _))
      .WillOnce(Return(blink::mojom::PermissionStatus::GRANTED));
  EXPECT_CALL(*mock_permission_manager(),
              GetPermissionStatus(_, origin2.GetURL(), _))
      .WillOnce(Return(blink::mojom::PermissionStatus::DENIED));

  std::set<url::Origin> allowed_origins;
  chrome_network_delegate()->CanSendReportingReports(
      std::move(origins),
      base::BindOnce(
          [](std::set<url::Origin>* dest, std::set<url::Origin> result) {
            *dest = std::move(result);
          },
          &allowed_origins));
  content::RunAllTasksUntilIdle();

  ASSERT_EQ(1u, allowed_origins.size());
  EXPECT_EQ(origin1, *allowed_origins.begin());
}
