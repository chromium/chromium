// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <climits>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/check.h"
#include "base/functional/function_ref.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace {

using web_app::WebAppInstallInfo;
using web_app::WebAppProvider;
using webapps::ManifestId;

class IWAProtocolTestBase : public DevToolsProtocolTestBase {
 public:
  IWAProtocolTestBase() {
    iwa_scoped_feature_list_.InitWithFeatures(
        {
#if !BUILDFLAG(IS_CHROMEOS)
            features::kIsolatedWebApps,
#endif  // !BUILDFLAG(IS_CHROMEOS)
            features::kIsolatedWebAppDevMode},
        {});
  }

  void SetUp() override {
    auto [url, bundle_id] = SetUpIwa();

    bundle_url_ = url;
    bundle_id_ = bundle_id;

    auto url_info_ =
        web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle_id);
    app_id_ = url_info_.app_id();

    // This is strange, but the tests are running in the SetUp().
    // So all additional setup must be done before.
    DevToolsProtocolTestBase::SetUp();
  }

  virtual std::pair<GURL, web_package::SignedWebBundleId> SetUpIwa() = 0;

  void SetUpOnMainThread() override {
    DevToolsProtocolTestBase::SetUpOnMainThread();
    AttachToBrowserTarget();
    override_registration_ =
        web_app::OsIntegrationTestOverrideImpl::OverrideForTesting();
  }

  void TearDownOnMainThread() override {
    web_app::test::UninstallAllWebApps(browser()->profile());
    override_registration_.reset();
    DevToolsProtocolTestBase::TearDownOnMainThread();
  }

 protected:
  GURL InstallUrl() const { return bundle_url_; }

  std::string InstallManifestId() const {
    return base::StrCat({webapps::kIsolatedAppScheme, "://", bundle_id_.id()});
  }

  webapps::AppId AppId() const { return app_id_; }

  bool AppExists() {
    auto* provider = WebAppProvider::GetForTest(browser()->profile());
    CHECK(provider);

    return provider->registrar_unsafe().IsInRegistrar(AppId());
  }

  void InstallCommand(const GURL& url) {
    EXPECT_TRUE(SendCommandSync("PWA.install",
                                base::Value::Dict{}
                                    .Set("manifestId", InstallManifestId())
                                    .Set("installUrlOrBundleUrl", url.spec())));

    EXPECT_TRUE(AppExists());

    if (error()) {
      const std::string& message = *error()->FindString("message");
      LOG(ERROR) << message;
    }
  }

  void Install() { InstallCommand(bundle_url_); }

  void AssertErrorMessageContains(
      std::initializer_list<std::string> pieces) const {
    ASSERT_TRUE(error());
    const std::string& message = *error()->FindString("message");
    for (const auto& piece : pieces) {
      EXPECT_THAT(message, testing::HasSubstr(piece));
    }
  }

 private:
  std::unique_ptr<web_app::OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
  base::test::ScopedFeatureList iwa_scoped_feature_list_;
  GURL bundle_url_;
  webapps::AppId app_id_;
  // Will be overwritten.
  web_package::SignedWebBundleId bundle_id_ =
      web_package::SignedWebBundleId::CreateRandomForProxyMode();
};

class IWAProtocolTestLocalFile : public IWAProtocolTestBase {
  std::pair<GURL, web_package::SignedWebBundleId> SetUpIwa() override {
    bundle_ = web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder())
                  .BuildBundle();

    return std::pair(net::FilePathToFileURL(bundle_->path()),
                     bundle_->web_bundle_id());
  }

 private:
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> bundle_;
};

class IWAProtocolTestRemoteFile : public IWAProtocolTestBase {
  std::pair<GURL, web_package::SignedWebBundleId> SetUpIwa() override {
    bundle_ = web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder())
                  .BuildBundle();

    const base::FilePath& bundle_path = bundle_->path();
    embedded_test_server()->AddDefaultHandlers();
    embedded_test_server()->ServeFilesFromDirectory(bundle_path.DirName());
    test_server_closer_ = embedded_test_server()->StartAndReturnHandle();
    GURL url = embedded_test_server()->GetURL(
        "/" + bundle_path.BaseName().MaybeAsASCII());
    return std::pair(url, bundle_->web_bundle_id());
  }

 private:
  net::test_server::EmbeddedTestServerHandle test_server_closer_;
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> bundle_;
};

class IWAProtocolTestRemoteProxy : public IWAProtocolTestBase {
  std::pair<GURL, web_package::SignedWebBundleId> SetUpIwa() override {
    proxy_app_ = web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder())
                     .BuildAndStartProxyServer();

    auto bundle_id = web_package::SignedWebBundleId::CreateRandomForProxyMode();

    return std::pair(proxy_app_->proxy_server().GetOrigin().GetURL(),
                     bundle_id);
  }

 private:
  std::unique_ptr<web_app::ScopedProxyIsolatedWebApp> proxy_app_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(IWAProtocolTestLocalFile, Install) {
  Install();
}
IN_PROC_BROWSER_TEST_F(IWAProtocolTestRemoteFile, Install) {
  Install();
}
IN_PROC_BROWSER_TEST_F(IWAProtocolTestRemoteProxy, Install) {
  Install();
}

// In comparison with PWA, we cannot install IWA twice.
IN_PROC_BROWSER_TEST_F(IWAProtocolTestLocalFile, Install_Twice) {
  Install();

  ASSERT_FALSE(SendCommandSync(
      "PWA.install", base::Value::Dict{}
                         .Set("manifestId", InstallManifestId())
                         .Set("installUrlOrBundleUrl", InstallUrl().spec())));

  AssertErrorMessageContains({InstallManifestId(), "already installed"});
  ASSERT_TRUE(AppExists());
}

IN_PROC_BROWSER_TEST_F(IWAProtocolTestLocalFile, Install_UrlUnreachable) {
  ASSERT_FALSE(SendCommandSync(
      "PWA.install",
      base::Value::Dict{}
          .Set("manifestId", InstallManifestId())
          .Set("installUrlOrBundleUrl", "http://hello/this/is/not/existing")));
  AssertErrorMessageContains(
      {InstallManifestId(), "http://hello/this/is/not/existing"});
  ASSERT_FALSE(AppExists());
}

IN_PROC_BROWSER_TEST_F(IWAProtocolTestLocalFile, Install_InvalidBundleId) {
  std::string garbage_id = "isolated-app://garbage_id";

  ASSERT_FALSE(SendCommandSync(
      "PWA.install", base::Value::Dict{}
                         .Set("manifestId", garbage_id)
                         .Set("installUrlOrBundleUrl", InstallUrl().spec())));

  AssertErrorMessageContains(
      {"must be a valid signed web bundle id", garbage_id});
  ASSERT_FALSE(AppExists());
}

IN_PROC_BROWSER_TEST_F(IWAProtocolTestLocalFile, Install_UnmatchManifestId) {
  std::string unmatched_id =
      "isolated-app://"
      "aiv4bxauvcu3zvbu6r5yynoh5atkzqqaoeof5mwz54b4zfywcrjuoaacai";

  ASSERT_FALSE(SendCommandSync(
      "PWA.install", base::Value::Dict{}
                         .Set("manifestId", unmatched_id)
                         .Set("installUrlOrBundleUrl", InstallUrl().spec())));

  AssertErrorMessageContains(
      {InstallUrl().spec(), unmatched_id, "Web bundle id mismatch"});
  ASSERT_FALSE(AppExists());
}

IN_PROC_BROWSER_TEST_F(IWAProtocolTestRemoteFile, Install_UnmatchManifestId) {
  std::string unmatched_id =
      "isolated-app://"
      "aiv4bxauvcu3zvbu6r5yynoh5atkzqqaoeof5mwz54b4zfywcrjuoaacai";

  ASSERT_FALSE(SendCommandSync(
      "PWA.install", base::Value::Dict{}
                         .Set("manifestId", unmatched_id)
                         .Set("installUrlOrBundleUrl", InstallUrl().spec())));

  AssertErrorMessageContains(
      {InstallUrl().spec(), unmatched_id, "Web bundle id mismatch"});
  ASSERT_FALSE(AppExists());
}

IN_PROC_BROWSER_TEST_F(IWAProtocolTestLocalFile, Install_Uninstall) {
  ASSERT_FALSE(AppExists());
  Install();
  ASSERT_TRUE(AppExists());

  ASSERT_TRUE(SendCommandSync(
      "PWA.uninstall",
      base::Value::Dict{}.Set("manifestId", InstallManifestId())));
  ASSERT_FALSE(AppExists());
}
