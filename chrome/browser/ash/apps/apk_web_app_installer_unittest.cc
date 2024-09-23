// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "ash/components/arc/test/fake_app_instance.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/apps/apk_web_app_installer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "url/gurl.h"

namespace {

arc::mojom::WebAppInfoPtr GetWebAppInfo() {
  return arc::mojom::WebAppInfo::New("Fake App Title",
                                     "https://www.google.com/index.html",
                                     "https://www.google.com/", 0xFFAABBCC);
}

constexpr int kGeneratedIconSize = 128;

arc::mojom::RawIconPngDataPtr GetIconBytes() {
  auto fake_app_instance =
      std::make_unique<arc::FakeAppInstance>(/*app_host=*/nullptr);
  arc::mojom::RawIconPngDataPtr icon = fake_app_instance->GenerateIconResponse(
      kGeneratedIconSize, /*app_icon=*/true);
  EXPECT_TRUE(icon);
  if (icon)
    EXPECT_TRUE(icon->icon_png_data.has_value());
  return icon;
}

}  // namespace

namespace ash {

class FakeApkWebAppInstaller : public ApkWebAppInstaller {
 public:
  FakeApkWebAppInstaller(Profile* profile,
                         base::WeakPtr<ApkWebAppInstaller::Owner> weak_owner,
                         base::OnceClosure quit_closure)
      : ApkWebAppInstaller(profile, base::DoNothing(), weak_owner),
        quit_closure_(std::move(quit_closure)) {}

  FakeApkWebAppInstaller(const FakeApkWebAppInstaller&) = delete;
  FakeApkWebAppInstaller& operator=(const FakeApkWebAppInstaller&) = delete;

  ~FakeApkWebAppInstaller() override = default;

  using ApkWebAppInstaller::has_web_app_install_info;
  using ApkWebAppInstaller::Start;
  using ApkWebAppInstaller::web_app_install_info;

  const webapps::AppId& id() const { return id_; }
  bool complete_installation_called() const {
    return complete_installation_called_;
  }
  bool do_install_called() const { return do_install_called_; }

 private:
  void CompleteInstallation(const webapps::AppId& id,
                            webapps::InstallResultCode code) override {
    id_ = id;
    complete_installation_called_ = true;
    std::move(quit_closure_).Run();
  }

  void DoInstall() override {
    do_install_called_ = true;
    std::move(quit_closure_).Run();
  }

  webapps::AppId id_;
  bool complete_installation_called_ = false;
  bool do_install_called_ = false;
  base::OnceClosure quit_closure_;
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

  apk_web_app_installer.Start("package", GetWebAppInfo(), GetIconBytes());
  run_loop.Run();

  EXPECT_FALSE(apk_web_app_installer.complete_installation_called());
  EXPECT_TRUE(apk_web_app_installer.do_install_called());

  EXPECT_EQ(u"Fake App Title",
            apk_web_app_installer.web_app_install_info().title);
  EXPECT_EQ(GURL("https://www.google.com/index.html"),
            apk_web_app_installer.web_app_install_info().start_url());
  EXPECT_EQ(GURL("https://www.google.com/"),
            apk_web_app_installer.web_app_install_info().scope);
  EXPECT_EQ(
      0xFFAABBCC,
      static_cast<uint32_t>(
          apk_web_app_installer.web_app_install_info().theme_color.value()));

  EXPECT_EQ(
      1u, apk_web_app_installer.web_app_install_info().icon_bitmaps.any.size());
  EXPECT_FALSE(apk_web_app_installer.web_app_install_info()
                   .icon_bitmaps.any.at(kGeneratedIconSize)
                   .drawsNothing());
}

TEST_F(ApkWebAppInstallerTest,
       InvalidatedWeakPtrBeforeStartCallsCompleteInstallation) {
  base::RunLoop run_loop;
  FakeApkWebAppInstaller apk_web_app_installer(
      profile(), weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure());

  weak_ptr_factory_.InvalidateWeakPtrs();
  apk_web_app_installer.Start("package", GetWebAppInfo(), GetIconBytes());
  run_loop.Run();

  EXPECT_EQ("", apk_web_app_installer.id());
  EXPECT_TRUE(apk_web_app_installer.complete_installation_called());
  EXPECT_FALSE(apk_web_app_installer.do_install_called());

  EXPECT_FALSE(apk_web_app_installer.has_web_app_install_info());
}

TEST_F(ApkWebAppInstallerTest,
       InvalidatedWeakPtrAfterStartCallsCompleteInstallation) {
  base::RunLoop run_loop;
  FakeApkWebAppInstaller apk_web_app_installer(
      profile(), weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure());

  apk_web_app_installer.Start("package", GetWebAppInfo(), GetIconBytes());
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

  apk_web_app_installer.Start("package", /*web_app_install_info=*/nullptr,
                              GetIconBytes());
  run_loop.Run();

  EXPECT_EQ("", apk_web_app_installer.id());
  EXPECT_TRUE(apk_web_app_installer.complete_installation_called());
  EXPECT_FALSE(apk_web_app_installer.do_install_called());

  EXPECT_FALSE(apk_web_app_installer.has_web_app_install_info());
}

TEST_F(ApkWebAppInstallerTest, NullIconCallsCompleteInstallation) {
  base::RunLoop run_loop;
  FakeApkWebAppInstaller apk_web_app_installer(
      profile(), weak_ptr_factory_.GetWeakPtr(), run_loop.QuitClosure());

  apk_web_app_installer.Start("package", GetWebAppInfo(), {});
  run_loop.Run();

  EXPECT_EQ("", apk_web_app_installer.id());
  EXPECT_TRUE(apk_web_app_installer.complete_installation_called());
  EXPECT_FALSE(apk_web_app_installer.do_install_called());

  EXPECT_FALSE(apk_web_app_installer.has_web_app_install_info());
}

}  // namespace ash
