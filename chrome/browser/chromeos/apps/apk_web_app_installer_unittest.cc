// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/apps/apk_web_app_installer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/test/fake_app_instance.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "url/gurl.h"

namespace {

arc::mojom::WebAppInfoPtr GetWebAppInfo() {
  return arc::mojom::WebAppInfo::New("Fake App Title",
                                     "https://www.google.com/index.html",
                                     "https://www.google.com/", 10000);
}

const std::vector<uint8_t> GetIconBytes() {
  auto fake_app_instance =
      std::make_unique<arc::FakeAppInstance>(/*app_host=*/nullptr);
  std::string png_data_as_string;
  EXPECT_TRUE(fake_app_instance->GenerateIconResponse(128, /*app_icon=*/true,
                                                      &png_data_as_string));
  return std::vector<uint8_t>(png_data_as_string.begin(),
                              png_data_as_string.end());
}

}  // namespace

namespace chromeos {

class FakeApkWebAppInstaller : public ApkWebAppInstaller {
 public:
  FakeApkWebAppInstaller(Profile* profile,
                         base::WeakPtr<ApkWebAppInstaller::Owner> weak_owner,
                         base::OnceClosure quit_closure)
      : ApkWebAppInstaller(profile, base::DoNothing(), weak_owner),
        quit_closure_(std::move(quit_closure)) {}

  ~FakeApkWebAppInstaller() override = default;

  using ApkWebAppInstaller::has_web_app_info;
  using ApkWebAppInstaller::Start;
  using ApkWebAppInstaller::web_app_info;

  const web_app::AppId& id() const { return id_; }
  bool complete_installation_called() const {
    return complete_installation_called_;
  }
  bool do_install_called() const { return do_install_called_; }

 private:
  void CompleteInstallation(const web_app::AppId& id,
                            web_app::InstallResultCode code) override {
    id_ = id;
    complete_installation_called_ = true;
    std::move(quit_closure_).Run();
  }

  void DoInstall() override {
    do_install_called_ = true;
    std::move(quit_closure_).Run();
  }

  web_app::AppId id_;
  bool complete_installation_called_ = false;
  bool do_install_called_ = false;
  base::OnceClosure quit_closure_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeApkWebAppInstaller);
};

class ApkWebAppInstallerTest : public ChromeRenderViewHostTestHarness,
                               public ApkWebAppInstaller::Owner {
 public:
  ApkWebAppInstallerTest() {}
  ~ApkWebAppInstallerTest() override = default;

 protected:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  // Must stay as last member.
  base::WeakPtrFactory<ApkWebAppInstallerTest> weak_ptr_factory_{this};
};

TEST_F(ApkWebAppInstallerTest, IconDecodeCallsWebAppInstallManager) {
  base::RunLoop run_loop;
  FakeApkWebAppInstaller apk_web_app_installer(
      profile(), weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure());

  apk_web_app_installer.Start(GetWebAppInfo(), GetIconBytes());
  run_loop.Run();

  EXPECT_FALSE(apk_web_app_installer.complete_installation_called());
  EXPECT_TRUE(apk_web_app_installer.do_install_called());

  EXPECT_EQ(base::ASCIIToUTF16("Fake App Title"),
            apk_web_app_installer.web_app_info().title);
  EXPECT_EQ(GURL("https://www.google.com/index.html"),
            apk_web_app_installer.web_app_info().app_url);
  EXPECT_EQ(GURL("https://www.google.com/"),
            apk_web_app_installer.web_app_info().scope);
  EXPECT_EQ(10000,
            static_cast<int32_t>(
                apk_web_app_installer.web_app_info().theme_color.value()));

  EXPECT_EQ(1u, apk_web_app_installer.web_app_info().icons.size());
  EXPECT_FALSE(
      apk_web_app_installer.web_app_info().icons[0].data.drawsNothing());
}

TEST_F(ApkWebAppInstallerTest,
       InvalidatedWeakPtrBeforeStartCallsCompleteInstallation) {
  base::RunLoop run_loop;
  FakeApkWebAppInstaller apk_web_app_installer(
      profile(), weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure());

  weak_ptr_factory_.InvalidateWeakPtrs();
  apk_web_app_installer.Start(GetWebAppInfo(), GetIconBytes());
  run_loop.Run();

  EXPECT_EQ("", apk_web_app_installer.id());
  EXPECT_TRUE(apk_web_app_installer.complete_installation_called());
  EXPECT_FALSE(apk_web_app_installer.do_install_called());

  EXPECT_FALSE(apk_web_app_installer.has_web_app_info());
}

TEST_F(ApkWebAppInstallerTest,
       InvalidatedWeakPtrAfterStartCallsCompleteInstallation) {
  base::RunLoop run_loop;
  FakeApkWebAppInstaller apk_web_app_installer(
      profile(), weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure());

  apk_web_app_installer.Start(GetWebAppInfo(), GetIconBytes());
  weak_ptr_factory_.InvalidateWeakPtrs();
  run_loop.Run();

  EXPECT_EQ("", apk_web_app_installer.id());
  EXPECT_TRUE(apk_web_app_installer.complete_installation_called());
  EXPECT_FALSE(apk_web_app_installer.do_install_called());
}

TEST_F(ApkWebAppInstallerTest, NullWebAppInfoCallsCompleteInstallation) {
  base::RunLoop run_loop;
  FakeApkWebAppInstaller apk_web_app_installer(
      profile(), weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure());

  apk_web_app_installer.Start(/*web_app_info=*/nullptr, GetIconBytes());
  run_loop.Run();

  EXPECT_EQ("", apk_web_app_installer.id());
  EXPECT_TRUE(apk_web_app_installer.complete_installation_called());
  EXPECT_FALSE(apk_web_app_installer.do_install_called());

  EXPECT_FALSE(apk_web_app_installer.has_web_app_info());
}

TEST_F(ApkWebAppInstallerTest, NullIconCallsCompleteInstallation) {
  base::RunLoop run_loop;
  FakeApkWebAppInstaller apk_web_app_installer(
      profile(), weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure());

  apk_web_app_installer.Start(GetWebAppInfo(), {});
  run_loop.Run();

  EXPECT_EQ("", apk_web_app_installer.id());
  EXPECT_TRUE(apk_web_app_installer.complete_installation_called());
  EXPECT_FALSE(apk_web_app_installer.do_install_called());

  EXPECT_FALSE(apk_web_app_installer.has_web_app_info());
}

}  // namespace chromeos
