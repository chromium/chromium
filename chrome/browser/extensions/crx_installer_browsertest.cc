// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/crx_installer.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/fake_safe_browsing_database_manager.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/scoped_database_manager_for_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/install/sandboxed_unpacker_failure_reason.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/file_util.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/test/test_extension_dir.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

namespace extensions {

namespace {

class MockInstallPrompt;

// This class holds information about things that happen with a
// MockInstallPrompt. We create the MockInstallPrompt but need to pass
// ownership of it to CrxInstaller, so it isn't safe to hang this data on
// MockInstallPrompt itself because we can't guarantee it's lifetime.
class MockPromptProxy {
 public:
  MockPromptProxy(content::WebContents* web_contents,
                  ScopedTestDialogAutoConfirm::AutoConfirm confirm_mode);

  MockPromptProxy(const MockPromptProxy&) = delete;
  MockPromptProxy& operator=(const MockPromptProxy&) = delete;

  ~MockPromptProxy();

  bool did_succeed() const { return !extension_id_.empty(); }
  const extensions::ExtensionId& extension_id() { return extension_id_; }
  bool confirmation_requested() const { return confirmation_requested_; }
  const std::u16string& error() const { return error_; }

  void set_extension_id(const extensions::ExtensionId& id) {
    extension_id_ = id;
  }
  void set_confirmation_requested(bool requested) {
    confirmation_requested_ = requested;
  }
  void set_error(const std::u16string& error) { error_ = error; }

  std::unique_ptr<ExtensionInstallPrompt> CreatePrompt();

 private:
  // Data used to create a prompt.
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;

  // Data reported back to us by the prompt we created.
  bool confirmation_requested_;
  extensions::ExtensionId extension_id_;
  std::u16string error_;

  std::unique_ptr<ScopedTestDialogAutoConfirm> auto_confirm;
};

class MockInstallPrompt : public ExtensionInstallPrompt {
 public:
  MockInstallPrompt(content::WebContents* web_contents, MockPromptProxy* proxy)
      : ExtensionInstallPrompt(web_contents), proxy_(proxy) {}

  MockInstallPrompt(const MockInstallPrompt&) = delete;
  MockInstallPrompt& operator=(const MockInstallPrompt&) = delete;

  // Overriding some of the ExtensionInstallUI API.
  void OnInstallSuccess(scoped_refptr<const Extension> extension,
                        SkBitmap* icon) override {
    proxy_->set_extension_id(extension->id());
    proxy_->set_confirmation_requested(did_call_show_dialog());
  }
  void OnInstallFailure(const CrxInstallError& error) override {
    proxy_->set_error(error.message());
    proxy_->set_confirmation_requested(did_call_show_dialog());
  }

 private:
  raw_ptr<MockPromptProxy, AcrossTasksDanglingUntriaged> proxy_;
};

MockPromptProxy::MockPromptProxy(
    content::WebContents* web_contents,
    ScopedTestDialogAutoConfirm::AutoConfirm confirm_mode)
    : web_contents_(web_contents),
      confirmation_requested_(false),
      auto_confirm(new ScopedTestDialogAutoConfirm(confirm_mode)) {}

MockPromptProxy::~MockPromptProxy() = default;

std::unique_ptr<ExtensionInstallPrompt> MockPromptProxy::CreatePrompt() {
  return std::make_unique<MockInstallPrompt>(web_contents_, this);
}

std::unique_ptr<MockPromptProxy> CreateMockPromptProxyForBrowserWithConfirmMode(
    Browser* browser,
    ScopedTestDialogAutoConfirm::AutoConfirm confirm_mode) {
  return std::make_unique<MockPromptProxy>(
      browser->tab_strip_model()->GetActiveWebContents(), confirm_mode);
}

std::unique_ptr<MockPromptProxy> CreateMockPromptProxyForBrowser(
    Browser* browser) {
  return CreateMockPromptProxyForBrowserWithConfirmMode(
      browser, ScopedTestDialogAutoConfirm::ACCEPT);
}

class ManagementPolicyMock : public ManagementPolicy::Provider {
 public:
  ManagementPolicyMock() = default;

  std::string GetDebugPolicyProviderName() const override {
    return "ManagementPolicyMock";
  }

  bool UserMayLoad(const Extension* extension,
                   std::u16string* error) const override {
    if (error)
      *error = u"Dummy error message";
    return false;
  }
};

}  // namespace

class ExtensionCrxInstallerTest : public ExtensionBrowserTest {
 protected:
  std::unique_ptr<WebstoreInstaller::Approval> GetApproval(
      const char* manifest_dir,
      const extensions::ExtensionId& id,
      bool strict_manifest_checks) {
    std::unique_ptr<WebstoreInstaller::Approval> result;

    base::ScopedAllowBlockingForTesting allow_io;
    base::FilePath ext_path = test_data_dir_.AppendASCII(manifest_dir);
    std::string error;
    std::optional<base::Value::Dict> parsed_manifest(
        file_util::LoadManifest(ext_path, &error));
    if (!parsed_manifest || !error.empty())
      return result;

    return WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
        browser()->profile(), id, std::move(*parsed_manifest),
        strict_manifest_checks);
  }

  const Extension* GetInstalledExtension(
      const extensions::ExtensionId& extension_id) {
    return extension_registry()->GetInstalledExtension(extension_id);
  }

  std::unique_ptr<base::ScopedTempDir> UnpackedCrxTempDir() {
    auto temp_dir = std::make_unique<base::ScopedTempDir>();
    EXPECT_TRUE(temp_dir->CreateUniqueTempDir());
    EXPECT_TRUE(base::PathExists(temp_dir->GetPath()));

    base::FilePath unpacked_path =
        test_data_dir_.AppendASCII("simple_with_popup");
    EXPECT_TRUE(base::PathExists(unpacked_path));
    EXPECT_TRUE(base::CopyDirectory(unpacked_path, temp_dir->GetPath(), false));

    return temp_dir;
  }

  // Helper function that creates a file at |relative_path| within |directory|
  // and fills it with |content|.
  bool AddFileToDirectory(const base::FilePath& directory,
                          const base::FilePath& relative_path,
                          const std::string& content) const {
    const base::FilePath full_path = directory.Append(relative_path);
    if (!CreateDirectory(full_path.DirName()))
      return false;
    return base::WriteFile(full_path, content);
  }

  void AddExtension(const extensions::ExtensionId& extension_id,
                    const std::string& version) {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    ASSERT_TRUE(base::PathExists(temp_dir.GetPath()));

    base::FilePath foo_js(FILE_PATH_LITERAL("foo.js"));
    base::FilePath bar_html(FILE_PATH_LITERAL("bar/bar.html"));
    ASSERT_TRUE(AddFileToDirectory(temp_dir.GetPath(), foo_js, "hello"))
        << "Failed to write " << temp_dir.GetPath().value() << "/"
        << foo_js.value();
    ASSERT_TRUE(AddFileToDirectory(temp_dir.GetPath(), bar_html, "world"));

    ExtensionBuilder builder;
    builder.SetManifest(base::Value::Dict()
                            .Set("name", "My First Extension")
                            .Set("version", version)
                            .Set("manifest_version", 2));
    builder.SetID(extension_id);
    builder.SetPath(temp_dir.GetPath());
    extension_service()->AddExtension(builder.Build().get());

    const Extension* extension = GetInstalledExtension(extension_id);
    ASSERT_NE(nullptr, extension);
    ASSERT_EQ(version, extension->VersionString());
  }

  static void InstallerCallback(base::OnceClosure quit_closure,
                                CrxInstaller::InstallerResultCallback callback,
                                const std::optional<CrxInstallError>& error) {
    if (!callback.is_null())
      std::move(callback).Run(error);
    std::move(quit_closure).Run();
  }

  void RunCrxInstaller(const WebstoreInstaller::Approval* approval,
                       std::unique_ptr<ExtensionInstallPrompt> prompt,
                       CrxInstaller::InstallerResultCallback callback,
                       const base::FilePath& crx_path) {
    base::RunLoop run_loop;

    scoped_refptr<CrxInstaller> installer(
        CrxInstaller::Create(extension_service(), std::move(prompt), approval));
    installer->set_allow_silent_install(true);
    installer->set_is_gallery_install(true);
    installer->AddInstallerCallback(
        base::BindOnce(&ExtensionCrxInstallerTest::InstallerCallback,
                       run_loop.QuitWhenIdleClosure(), std::move(callback)));
    installer->InstallCrx(crx_path);

    run_loop.Run();
  }

  void RunCrxInstallerFromUnpackedDirectory(
      std::unique_ptr<ExtensionInstallPrompt> prompt,
      CrxInstaller::InstallerResultCallback callback,
      const extensions::ExtensionId& extension_id,
      const std::string& public_key,
      const base::FilePath& crx_directory) {
    base::RunLoop run_loop;

    scoped_refptr<CrxInstaller> installer(
        CrxInstaller::Create(extension_service(), std::move(prompt)));
    installer->set_allow_silent_install(true);
    installer->set_is_gallery_install(true);
    installer->AddInstallerCallback(
        base::BindOnce(&ExtensionCrxInstallerTest::InstallerCallback,
                       run_loop.QuitWhenIdleClosure(), std::move(callback)));
    installer->set_delete_source(true);
    installer->InstallUnpackedCrx(extension_id, public_key, crx_directory);

    run_loop.Run();
  }

  void RunUpdateExtension(std::unique_ptr<ExtensionInstallPrompt> prompt,
                          const extensions::ExtensionId& extension_id,
                          const std::string& public_key,
                          const base::FilePath& unpacked_dir,
                          CrxInstaller::InstallerResultCallback callback) {
    base::RunLoop run_loop;

    scoped_refptr<CrxInstaller> installer(
        CrxInstaller::Create(extension_service(), std::move(prompt)));
    installer->set_delete_source(true);
    installer->AddInstallerCallback(
        base::BindOnce(&ExtensionCrxInstallerTest::InstallerCallback,
                       run_loop.QuitWhenIdleClosure(), std::move(callback)));
    installer->UpdateExtensionFromUnpackedCrx(extension_id, public_key,
                                              unpacked_dir);

    run_loop.Run();
  }

  // Installs a crx from |crx_relpath| (a path relative to the extension test
  // data dir) with expected id |id|.
  void InstallWithPrompt(const char* ext_relpath,
                         const extensions::ExtensionId& id,
                         CrxInstaller::InstallerResultCallback callback,
                         MockPromptProxy* mock_install_prompt) {
    base::FilePath ext_path = test_data_dir_.AppendASCII(ext_relpath);

    std::unique_ptr<WebstoreInstaller::Approval> approval;
    if (!id.empty())
      approval = GetApproval(ext_relpath, id, true);

    base::FilePath crx_path = PackExtension(ext_path);
    EXPECT_FALSE(crx_path.empty());
    RunCrxInstaller(approval.get(), mock_install_prompt->CreatePrompt(),
                    std::move(callback), crx_path);

    EXPECT_TRUE(mock_install_prompt->did_succeed());
  }

  // Installs an extension and checks that it has scopes granted IFF
  // |record_oauth2_grant| is true.
  void CheckHasEmptyScopesAfterInstall(
      const std::string& ext_relpath,
      CrxInstaller::InstallerResultCallback callback,
      bool record_oauth2_grant) {
    std::unique_ptr<MockPromptProxy> mock_prompt =
        CreateMockPromptProxyForBrowser(browser());

    InstallWithPrompt("browsertest/scopes", std::string(), std::move(callback),
                      mock_prompt.get());

    std::unique_ptr<const PermissionSet> permissions =
        ExtensionPrefs::Get(browser()->profile())
            ->GetGrantedPermissions(mock_prompt->extension_id());
    ASSERT_TRUE(permissions.get());
  }
};

class ExtensionCrxInstallerTestWithExperimentalApis
    : public ExtensionCrxInstallerTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionCrxInstallerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       ExperimentalExtensionFromGallery) {
  // Gallery-installed extensions should have their experimental permission
  // preserved, since we allow the Webstore to make that decision.
  const Extension* extension = InstallExtensionFromWebstore(
      test_data_dir_.AppendASCII("experimental.crx"), 1);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      mojom::APIPermissionID::kExperimental));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       ExperimentalExtensionFromOutsideGallery) {
  // Non-gallery-installed extensions should lose their experimental
  // permission if the flag isn't enabled.
  const Extension* extension =
      InstallExtension(test_data_dir_.AppendASCII("experimental.crx"), 1);
  ASSERT_TRUE(extension);
  EXPECT_FALSE(extension->permissions_data()->HasAPIPermission(
      mojom::APIPermissionID::kExperimental));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTestWithExperimentalApis,
                       ExperimentalExtensionFromOutsideGalleryWithFlag) {
  // Non-gallery-installed extensions should maintain their experimental
  // permission if the flag is enabled.
  const Extension* extension =
      InstallExtension(test_data_dir_.AppendASCII("experimental.crx"), 1);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      mojom::APIPermissionID::kExperimental));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTestWithExperimentalApis,
                       PlatformAppCrx) {
  EXPECT_TRUE(InstallExtension(
      test_data_dir_.AppendASCII("minimal_platform_app.crx"), 1));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, BlockedFileTypes) {
  const Extension* extension =
      InstallExtension(test_data_dir_.AppendASCII("blocked_file_types.crx"), 1);
  base::ScopedAllowBlockingForTesting allow_io;
  EXPECT_TRUE(base::PathExists(extension->path().AppendASCII("test.html")));
  EXPECT_TRUE(base::PathExists(extension->path().AppendASCII("test.nexe")));
  EXPECT_FALSE(base::PathExists(extension->path().AppendASCII("test1.EXE")));
  EXPECT_FALSE(base::PathExists(extension->path().AppendASCII("test2.exe")));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, AllowedThemeFileTypes) {
  const Extension* extension = InstallExtension(
      test_data_dir_.AppendASCII("theme_with_extension.crx"), 1);
  ASSERT_TRUE(extension);
  const base::FilePath& path = extension->path();
  base::ScopedAllowBlockingForTesting allow_io;
  EXPECT_TRUE(
      base::PathExists(path.AppendASCII("images/theme_frame_camo.PNG")));
  EXPECT_TRUE(
      base::PathExists(path.AppendASCII("images/theme_ntp_background.png")));
  EXPECT_TRUE(base::PathExists(
      path.AppendASCII("images/theme_ntp_background_norepeat.png")));
  EXPECT_TRUE(
      base::PathExists(path.AppendASCII("images/theme_toolbar_camo.png")));
  EXPECT_TRUE(base::PathExists(path.AppendASCII("images/redirect_target.GIF")));
  EXPECT_TRUE(base::PathExists(path.AppendASCII("test.image.bmp")));
  EXPECT_TRUE(
      base::PathExists(path.AppendASCII("test_image_with_no_extension")));

  EXPECT_FALSE(base::PathExists(path.AppendASCII("non_images/test.html")));
  EXPECT_FALSE(base::PathExists(path.AppendASCII("non_images/test.nexe")));
  EXPECT_FALSE(base::PathExists(path.AppendASCII("non_images/test1.EXE")));
  EXPECT_FALSE(base::PathExists(path.AppendASCII("non_images/test2.exe")));
  EXPECT_FALSE(base::PathExists(path.AppendASCII("non_images/test.txt")));
  EXPECT_FALSE(base::PathExists(path.AppendASCII("non_images/test.css")));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       PackAndInstallExtensionFromDownload) {
  std::unique_ptr<base::AutoReset<bool>> allow_offstore_install =
      download_crx_util::OverrideOffstoreInstallAllowedForTesting(true);

  const int kNumDownloadsExpected = 1;

  base::FilePath crx_path =
      PackExtension(test_data_dir_.AppendASCII("common/background_page"));
  ASSERT_FALSE(crx_path.empty());
  std::string crx_path_string(crx_path.value().begin(), crx_path.value().end());
  GURL url = GURL(std::string("file:///").append(crx_path_string));

  std::unique_ptr<MockPromptProxy> mock_prompt =
      CreateMockPromptProxyForBrowser(browser());
  download_crx_util::SetMockInstallPromptForTesting(
      mock_prompt->CreatePrompt());

  content::DownloadManager* download_manager =
      browser()->profile()->GetDownloadManager();

  std::unique_ptr<content::DownloadTestObserver> observer(
      new content::DownloadTestObserverTerminal(
          download_manager, kNumDownloadsExpected,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));

  TestExtensionRegistryObserver registry_observer(extension_registry());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  EXPECT_TRUE(registry_observer.WaitForExtensionInstalled());
  EXPECT_TRUE(mock_prompt->confirmation_requested());
}

// Tests that scopes are only granted if |record_oauth2_grant_| on the prompt is
// true.
IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTestWithExperimentalApis,
                       GrantScopes) {
  EXPECT_NO_FATAL_FAILURE(CheckHasEmptyScopesAfterInstall(
      "browsertest/scopes", CrxInstaller::InstallerResultCallback(), true));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTestWithExperimentalApis,
                       GrantScopes_WithCallback) {
  EXPECT_NO_FATAL_FAILURE(CheckHasEmptyScopesAfterInstall(
      "browsertest/scopes",
      base::BindOnce([](const std::optional<CrxInstallError>& error) {
        EXPECT_EQ(std::nullopt, error);
      }),
      true));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTestWithExperimentalApis,
                       DoNotGrantScopes) {
  EXPECT_NO_FATAL_FAILURE(CheckHasEmptyScopesAfterInstall(
      "browsertest/scopes", CrxInstaller::InstallerResultCallback(), false));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTestWithExperimentalApis,
                       DoNotGrantScopes_WithCallback) {
  EXPECT_NO_FATAL_FAILURE(CheckHasEmptyScopesAfterInstall(
      "browsertest/scopes",
      base::BindOnce([](const std::optional<CrxInstallError>& error) {
        EXPECT_EQ(std::nullopt, error);
      }),
      false));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, AllowOffStore) {
  const bool kTestData[] = {false, true};

  for (size_t i = 0; i < std::size(kTestData); ++i) {
    std::unique_ptr<MockPromptProxy> mock_prompt =
        CreateMockPromptProxyForBrowser(browser());

    scoped_refptr<CrxInstaller> crx_installer(
        CrxInstaller::Create(extension_service(), mock_prompt->CreatePrompt()));
    crx_installer->set_install_cause(
        extension_misc::INSTALL_CAUSE_USER_DOWNLOAD);

    if (kTestData[i]) {
      crx_installer->set_off_store_install_allow_reason(
          CrxInstaller::OffStoreInstallAllowedInTest);
    }

    base::RunLoop run_loop;
    crx_installer->AddInstallerCallback(
        base::BindOnce(&ExtensionCrxInstallerTest::InstallerCallback,
                       run_loop.QuitWhenIdleClosure(),
                       CrxInstaller::InstallerResultCallback()));
    crx_installer->InstallCrx(test_data_dir_.AppendASCII("good.crx"));
    // The |mock_prompt| will quit running the loop once the |crx_installer|
    // is done.
    run_loop.Run();
    EXPECT_EQ(kTestData[i], mock_prompt->did_succeed());
    EXPECT_EQ(kTestData[i], mock_prompt->confirmation_requested())
        << kTestData[i];
    if (kTestData[i]) {
      EXPECT_EQ(std::u16string(), mock_prompt->error()) << kTestData[i];
    } else {
      EXPECT_EQ(
          l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALL_DISALLOWED_ON_SITE),
          mock_prompt->error())
          << kTestData[i];
    }
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, HiDpiThemeTest) {
  base::FilePath crx_path = test_data_dir_.AppendASCII("theme_hidpi_crx");
  crx_path = crx_path.AppendASCII("theme_hidpi.crx");

  ASSERT_TRUE(InstallExtension(crx_path, 1));

  const extensions::ExtensionId extension_id(
      "gllekhaobjnhgeagipipnkpmmmpchacm");
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  ASSERT_TRUE(extension);
  EXPECT_EQ(extension_id, extension->id());

  UninstallExtension(extension_id);
  EXPECT_FALSE(registry->enabled_extensions().GetByID(extension_id));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       InstallDelayedUntilNextUpdate) {
  const extensions::ExtensionId extension_id(
      "ldnnhddmnhbkjipkidpdiheffobcpfmf");
  base::FilePath base_path = test_data_dir_.AppendASCII("delayed_install");

  ExtensionService* service = extension_service();
  ASSERT_TRUE(service);
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  ASSERT_TRUE(registry);

  // Install version 1 of the test extension. This extension does not have
  // a background page but does have a browser action.
  base::FilePath v1_path = PackExtension(base_path.AppendASCII("v1"));
  ASSERT_FALSE(v1_path.empty());
  ASSERT_TRUE(InstallExtension(v1_path, 1));
  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension_id, extension->id());
  ASSERT_EQ("1.0", extension->version().GetString());

  // Make test extension non-idle by opening the extension's options page.
  ExtensionTabUtil::OpenOptionsPage(extension, browser());
  WaitForExtensionNotIdle(extension_id);

  // Install version 2 of the extension and check that it is indeed delayed.
  base::FilePath v2_path = PackExtension(base_path.AppendASCII("v2"));
  ASSERT_FALSE(v2_path.empty());
  ASSERT_TRUE(UpdateExtensionWaitForIdle(extension_id, v2_path, 0));

  ASSERT_EQ(1u, service->delayed_installs()->size());
  extension = registry->enabled_extensions().GetByID(extension_id);
  ASSERT_EQ("1.0", extension->version().GetString());

  // Make the extension idle again by navigating away from the options page.
  // This should not trigger the delayed install.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  WaitForExtensionIdle(extension_id);
  ASSERT_EQ(1u, service->delayed_installs()->size());
  extension = registry->enabled_extensions().GetByID(extension_id);
  ASSERT_EQ("1.0", extension->version().GetString());

  // Install version 3 of the extension. Because the extension is idle,
  // this install should succeed.
  base::FilePath v3_path = PackExtension(base_path.AppendASCII("v3"));
  ASSERT_FALSE(v3_path.empty());
  ASSERT_TRUE(UpdateExtensionWaitForIdle(extension_id, v3_path, 0));
  extension = registry->enabled_extensions().GetByID(extension_id);
  ASSERT_EQ("3.0", extension->version().GetString());

  // The version 2 delayed install should be cleaned up, and finishing
  // delayed extension installation shouldn't break anything.
  ASSERT_EQ(0u, service->delayed_installs()->size());
  service->MaybeFinishDelayedInstallations();
  extension = registry->enabled_extensions().GetByID(extension_id);
  ASSERT_EQ("3.0", extension->version().GetString());
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, Blocklist) {
  scoped_refptr<FakeSafeBrowsingDatabaseManager> blocklist_db(
      new FakeSafeBrowsingDatabaseManager(true));
  ScopedDatabaseManagerForTest scoped_blocklist_db(blocklist_db);

  const extensions::ExtensionId extension_id =
      "gllekhaobjnhgeagipipnkpmmmpchacm";
  blocklist_db->SetUnsafe(extension_id);

  base::FilePath crx_path = test_data_dir_.AppendASCII("theme_hidpi_crx")
                                .AppendASCII("theme_hidpi.crx");
  EXPECT_FALSE(InstallExtension(crx_path, 0));

  auto installation_failure =
      InstallStageTracker::Get(profile())->Get(extension_id);
  EXPECT_EQ(InstallStageTracker::FailureReason::CRX_INSTALL_ERROR_DECLINED,
            installation_failure.failure_reason);
  EXPECT_EQ(CrxInstallErrorDetail::EXTENSION_IS_BLOCKLISTED,
            installation_failure.install_error_detail);
}
#endif

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, NonStrictManifestCheck) {
  std::unique_ptr<MockPromptProxy> mock_prompt =
      CreateMockPromptProxyForBrowser(browser());

  // We want to simulate the case where the webstore sends a more recent
  // version of the manifest, but the downloaded .crx file is old since
  // the newly published version hasn't fully propagated to all the download
  // servers yet. So load the v2 manifest, but then install the v1 crx file.
  extensions::ExtensionId id = "ooklpoaelmiimcjipecogjfcejghbogp";
  std::unique_ptr<WebstoreInstaller::Approval> approval =
      GetApproval("crx_installer/v2_no_permission_change/", id, false);

  RunCrxInstaller(approval.get(), mock_prompt->CreatePrompt(),
                  CrxInstaller::InstallerResultCallback(),
                  test_data_dir_.AppendASCII("crx_installer/v1.crx"));

  EXPECT_TRUE(mock_prompt->did_succeed());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       NonStrictManifestCheck_WithCallback) {
  std::unique_ptr<MockPromptProxy> mock_prompt =
      CreateMockPromptProxyForBrowser(browser());

  // We want to simulate the case where the webstore sends a more recent
  // version of the manifest, but the downloaded .crx file is old since
  // the newly published version hasn't fully propagated to all the download
  // servers yet. So load the v2 manifest, but then install the v1 crx file.
  const extensions::ExtensionId id = "ooklpoaelmiimcjipecogjfcejghbogp";
  std::unique_ptr<WebstoreInstaller::Approval> approval =
      GetApproval("crx_installer/v2_no_permission_change/", id, false);

  RunCrxInstaller(
      approval.get(), mock_prompt->CreatePrompt(),
      base::BindOnce([](const std::optional<CrxInstallError>& error) {
        EXPECT_EQ(std::nullopt, error);
      }),
      test_data_dir_.AppendASCII("crx_installer/v1.crx"));

  EXPECT_TRUE(mock_prompt->did_succeed());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       InstallUnpackedCrx_FolderDoesNotExist) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::unique_ptr<MockPromptProxy> mock_prompt =
      CreateMockPromptProxyForBrowser(browser());

  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath folder = temp_dir.GetPath().AppendASCII("abcdef");
  EXPECT_FALSE(base::PathExists(folder));

  const std::string public_key = "123456";
  RunCrxInstallerFromUnpackedDirectory(
      mock_prompt->CreatePrompt(),
      base::BindOnce([](const std::optional<CrxInstallError>& error) {
        ASSERT_NE(std::nullopt, error);
        ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
                  error->type());
        EXPECT_EQ(SandboxedUnpackerFailureReason::DIRECTORY_MOVE_FAILED,
                  error->sandbox_failure_detail());
      }),
      std::string(), public_key, folder);

  EXPECT_FALSE(mock_prompt->did_succeed());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       InstallUnpackedCrx_EmptyFolder) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::unique_ptr<MockPromptProxy> mock_prompt =
      CreateMockPromptProxyForBrowser(browser());

  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  EXPECT_TRUE(base::PathExists(temp_dir.GetPath()));

  const std::string public_key = "123456";
  RunCrxInstallerFromUnpackedDirectory(
      mock_prompt->CreatePrompt(),
      base::BindOnce([](const std::optional<CrxInstallError>& error) {
        ASSERT_NE(std::nullopt, error);
        ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
                  error->type());
        EXPECT_EQ(SandboxedUnpackerFailureReason::UNPACKER_CLIENT_FAILED,
                  error->sandbox_failure_detail());
      }),
      std::string(), public_key, temp_dir.GetPath());

  EXPECT_FALSE(mock_prompt->did_succeed());
  EXPECT_FALSE(base::PathExists(temp_dir.GetPath()));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       InstallUnpackedCrx_InvalidPublicKey) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::unique_ptr<MockPromptProxy> mock_prompt =
      CreateMockPromptProxyForBrowser(browser());

  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  EXPECT_TRUE(base::PathExists(temp_dir.GetPath()));

  const base::FilePath unpacked_path =
      test_data_dir_.AppendASCII("simple_with_popup");
  EXPECT_TRUE(base::PathExists(unpacked_path));
  EXPECT_TRUE(base::CopyDirectory(unpacked_path, temp_dir.GetPath(), false));

  const std::string public_key = "123456";
  RunCrxInstallerFromUnpackedDirectory(
      mock_prompt->CreatePrompt(),
      base::BindOnce([](const std::optional<CrxInstallError>& error) {
        ASSERT_NE(std::nullopt, error);
        ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
                  error->type());
        EXPECT_EQ(SandboxedUnpackerFailureReason::INVALID_MANIFEST,
                  error->sandbox_failure_detail());
      }),
      std::string(), public_key, temp_dir.GetPath());

  EXPECT_FALSE(mock_prompt->did_succeed());
  EXPECT_FALSE(base::PathExists(temp_dir.GetPath()));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, InstallUnpackedCrx_Success) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::unique_ptr<MockPromptProxy> mock_prompt =
      CreateMockPromptProxyForBrowser(browser());

  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  EXPECT_TRUE(base::PathExists(temp_dir.GetPath()));

  const base::FilePath unpacked_path =
      test_data_dir_.AppendASCII("simple_with_popup");
  EXPECT_TRUE(base::PathExists(unpacked_path));
  EXPECT_TRUE(base::CopyDirectory(unpacked_path, temp_dir.GetPath(), false));

  const std::string public_key =
      "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC8c4fBSPZ6utYoZ8NiWF/"
      "DSaimBhihjwgOsskyleFGaurhi3TDClTVSGPxNkgCzrz0wACML7M4aNjpd05qupdbR2d294j"
      "kDuI7caxEGUucpP7GJRRHnm8Sx+"
      "y0ury28n8jbN0PnInKKWcxpIXXmNQyC19HBuO3QIeUq9Dqc+7YFQIDAQAB";
  RunCrxInstallerFromUnpackedDirectory(
      mock_prompt->CreatePrompt(),
      base::BindOnce([](const std::optional<CrxInstallError>& error) {
        EXPECT_EQ(std::nullopt, error);
      }),
      std::string(), public_key, temp_dir.GetPath());

  EXPECT_TRUE(mock_prompt->did_succeed());
  EXPECT_FALSE(base::PathExists(temp_dir.GetPath()));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       UpdateExtensionFromUnpackedCrx_NewExtension) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::unique_ptr<MockPromptProxy> mock_prompt =
      CreateMockPromptProxyForBrowser(browser());

  // Update won't work as the extension doesn't exist.
  const extensions::ExtensionId extension_id =
      "ldnnhddmnhbkjipkidpdiheffobcpfmf";
  const std::string public_key =
      "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC8c4fBSPZ6utYoZ8NiWF/"
      "DSaimBhihjwgOsskyleFGaurhi3TDClTVSGPxNkgCzrz0wACML7M4aNjpd05qupdbR2d294j"
      "kDuI7caxEGUucpP7GJRRHnm8Sx+"
      "y0ury28n8jbN0PnInKKWcxpIXXmNQyC19HBuO3QIeUq9Dqc+7YFQIDAQAB";
  ASSERT_EQ(nullptr, GetInstalledExtension(extension_id));
  auto temp_dir = UnpackedCrxTempDir();
  RunUpdateExtension(
      mock_prompt->CreatePrompt(), extension_id, public_key,
      temp_dir->GetPath(),
      base::BindOnce([](const std::optional<CrxInstallError>& error) {
        ASSERT_NE(std::nullopt, error);
        EXPECT_EQ(CrxInstallErrorType::OTHER, error->type());
        EXPECT_EQ(CrxInstallErrorDetail::UPDATE_NON_EXISTING_EXTENSION,
                  error->detail());
      }));

  // The unpacked folder should be deleted.
  EXPECT_FALSE(mock_prompt->did_succeed());
  EXPECT_FALSE(base::PathExists(temp_dir->GetPath()));
  EXPECT_EQ(nullptr, GetInstalledExtension(extension_id));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       UpdateExtensionFromUnpackedCrx_UpdateExistingExtension) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::unique_ptr<MockPromptProxy> mock_prompt =
      CreateMockPromptProxyForBrowser(browser());

  const extensions::ExtensionId extension_id =
      "ldnnhddmnhbkjipkidpdiheffobcpfmf";
  const std::string public_key =
      "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC8c4fBSPZ6utYoZ8NiWF/"
      "DSaimBhihjwgOsskyleFGaurhi3TDClTVSGPxNkgCzrz0wACML7M4aNjpd05qupdbR2d294j"
      "kDuI7caxEGUucpP7GJRRHnm8Sx+"
      "y0ury28n8jbN0PnInKKWcxpIXXmNQyC19HBuO3QIeUq9Dqc+7YFQIDAQAB";

  // Test updating an existing extension.
  AddExtension(extension_id, "0.0");

  auto temp_dir = UnpackedCrxTempDir();
  RunUpdateExtension(
      mock_prompt->CreatePrompt(), extension_id, public_key,
      temp_dir->GetPath(),
      base::BindOnce([](const std::optional<CrxInstallError>& error) {
        EXPECT_EQ(std::nullopt, error);
      }));

  EXPECT_TRUE(mock_prompt->did_succeed());

  // The unpacked folder should be deleted.
  EXPECT_FALSE(base::PathExists(temp_dir->GetPath()));

  const Extension* extension = GetInstalledExtension(extension_id);
  ASSERT_NE(nullptr, extension);
  EXPECT_EQ("1.0", extension->VersionString());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       UpdateExtensionFromUnpackedCrx_InvalidPublicKey) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::unique_ptr<MockPromptProxy> mock_prompt =
      CreateMockPromptProxyForBrowser(browser());

  const extensions::ExtensionId extension_id =
      "ldnnhddmnhbkjipkidpdiheffobcpfmf";
  const std::string public_key = "invalid public key";

  // Test updating an existing extension.
  AddExtension(extension_id, "0.0");

  auto temp_dir = UnpackedCrxTempDir();
  RunUpdateExtension(
      mock_prompt->CreatePrompt(), extension_id, public_key,
      temp_dir->GetPath(),
      base::BindOnce([](const std::optional<CrxInstallError>& error) {
        ASSERT_NE(std::nullopt, error);
        ASSERT_EQ(CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE,
                  error->type());
        EXPECT_EQ(SandboxedUnpackerFailureReason::INVALID_MANIFEST,
                  error->sandbox_failure_detail());
      }));

  EXPECT_FALSE(mock_prompt->did_succeed());

  // The unpacked folder should be deleted.
  EXPECT_FALSE(base::PathExists(temp_dir->GetPath()));

  const Extension* extension = GetInstalledExtension(extension_id);
  ASSERT_NE(nullptr, extension);
  EXPECT_EQ("0.0", extension->VersionString());

  auto installation_failure =
      InstallStageTracker::Get(profile())->Get(extension_id);
  EXPECT_EQ(InstallStageTracker::FailureReason::
                CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE,
            installation_failure.failure_reason);
  EXPECT_EQ(std::nullopt, installation_failure.install_error_detail);
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       UpdateExtensionFromUnpackedCrx_WrongPublicKey) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::unique_ptr<MockPromptProxy> mock_prompt =
      CreateMockPromptProxyForBrowser(browser());

  const extensions::ExtensionId extension_id =
      "gllekhaobjnhgeagipipnkpmmmpchacm";
  const std::string public_key =
      "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC8c4fBSPZ6utYoZ8NiWF/"
      "DSaimBhihjwgOsskyleFGaurhi3TDClTVSGPxNkgCzrz0wACML7M4aNjpd05qupdbR2d294j"
      "kDuI7caxEGUucpP7GJRRHnm8Sx+"
      "y0ury28n8jbN0PnInKKWcxpIXXmNQyC19HBuO3QIeUq9Dqc+7YFQIDAQAB";

  // Test updating an existing extension.
  AddExtension(extension_id, "0.0");

  auto temp_dir = UnpackedCrxTempDir();
  RunUpdateExtension(
      mock_prompt->CreatePrompt(), extension_id, public_key,
      temp_dir->GetPath(),
      base::BindOnce([](const std::optional<CrxInstallError>& error) {
        ASSERT_NE(std::nullopt, error);
        EXPECT_EQ(CrxInstallErrorType::OTHER, error->type());
        EXPECT_EQ(CrxInstallErrorDetail::UNEXPECTED_ID, error->detail());
      }));

  EXPECT_FALSE(mock_prompt->did_succeed());

  // The unpacked folder should be deleted.
  EXPECT_FALSE(base::PathExists(temp_dir->GetPath()));

  const Extension* extension = GetInstalledExtension(extension_id);
  ASSERT_NE(nullptr, extension);
  EXPECT_EQ("0.0", extension->VersionString());

  auto installation_failure =
      InstallStageTracker::Get(profile())->Get(extension_id);
  EXPECT_EQ(InstallStageTracker::FailureReason::CRX_INSTALL_ERROR_OTHER,
            installation_failure.failure_reason);
  EXPECT_EQ(CrxInstallErrorDetail::UNEXPECTED_ID,
            *installation_failure.install_error_detail);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, KioskOnlyTest) {
  base::ScopedAllowBlockingForTesting allow_io;
  // kiosk_only is allowlisted from non-chromeos.
  base::FilePath crx_path = test_data_dir_.AppendASCII("kiosk/kiosk_only.crx");
  EXPECT_FALSE(InstallExtension(crx_path, 0));
  // Simulate ChromeOS kiosk mode. |scoped_user_manager| will take over
  // lifetime of |user_manager|.
  auto* fake_user_manager = new ash::FakeChromeUserManager();
  const AccountId account_id(AccountId::FromUserEmail("example@example.com"));
  fake_user_manager->AddKioskAppUser(account_id);
  fake_user_manager->LoginUser(account_id);
  user_manager::ScopedUserManager scoped_user_manager(
      base::WrapUnique(fake_user_manager));
  EXPECT_TRUE(InstallExtension(crx_path, 1));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, InstallToSharedLocation) {
  base::ScopedAllowBlockingForTesting allow_io;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableExtensionAssetsSharing);
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  ExtensionAssetsManagerChromeOS::SetSharedInstallDirForTesting(
      cache_dir.GetPath());

  base::FilePath crx_path = test_data_dir_.AppendASCII("crx_installer/v1.crx");
  const Extension* extension =
      InstallExtension(crx_path, 1, mojom::ManifestLocation::kExternalPref);
  base::FilePath extension_path = extension->path();
  EXPECT_TRUE(cache_dir.GetPath().IsParent(extension_path));
  EXPECT_TRUE(base::PathExists(extension_path));

  extensions::ExtensionId extension_id = extension->id();
  UninstallExtension(extension_id);
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  EXPECT_FALSE(registry->enabled_extensions().GetByID(extension_id));

  content::RunAllTasksUntilIdle();

  EXPECT_FALSE(base::PathExists(extension_path));
}
#endif

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, DoNotSync) {
  scoped_refptr<CrxInstaller> crx_installer(
      CrxInstaller::CreateSilent(extension_service()));
  crx_installer->set_do_not_sync(true);

  base::test::TestFuture<std::optional<CrxInstallError>> installer_done_future;
  crx_installer->AddInstallerCallback(
      installer_done_future
          .GetCallback<const std::optional<CrxInstallError>&>());
  crx_installer->InstallCrx(test_data_dir_.AppendASCII("good.crx"));
  EXPECT_FALSE(installer_done_future.Get().has_value());
  ASSERT_TRUE(crx_installer->extension());

  const ExtensionPrefs* extension_prefs =
      ExtensionPrefs::Get(browser()->profile());
  EXPECT_TRUE(extension_prefs->DoNotSync(crx_installer->extension()->id()));
  EXPECT_FALSE(
      util::ShouldSync(crx_installer->extension(), browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, ManagementPolicy) {
  ManagementPolicyMock policy;
  ExtensionSystem::Get(profile())->management_policy()->RegisterProvider(
      &policy);

  base::FilePath crx_path = test_data_dir_.AppendASCII("crx_installer/v1.crx");
  EXPECT_FALSE(InstallExtension(crx_path, 0));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, UpdateWithFileAccess) {
  base::FilePath ext_source =
      test_data_dir_.AppendASCII("permissions").AppendASCII("files");
  base::FilePath crx_with_file_permission = PackExtension(ext_source);
  ASSERT_FALSE(crx_with_file_permission.empty());

  ExtensionService* service = extension_service();

  const extensions::ExtensionId extension_id(
      "bdkapipdccfifhdghmblnenbbncfcpid");
  {
    // Install extension.
    scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(service));
    base::test::TestFuture<std::optional<CrxInstallError>>
        installer_done_future;
    installer->AddInstallerCallback(
        installer_done_future
            .GetCallback<const std::optional<CrxInstallError>&>());
    installer->InstallCrx(crx_with_file_permission);
    EXPECT_FALSE(installer_done_future.Get().has_value());
    const Extension* extension = installer->extension();
    ASSERT_TRUE(extension);
    // IDs must match, otherwise the test doesn't make any sense.
    ASSERT_EQ(extension_id, extension->id());
    // Sanity check: File access should be disabled by default.
    EXPECT_FALSE(ExtensionPrefs::Get(profile())->AllowFileAccess(extension_id));
    EXPECT_FALSE(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS);
  }

  {
    // Uninstall and re-install the extension. Any previously granted file
    // permissions should be gone.
    ExtensionPrefs::Get(profile())->SetAllowFileAccess(extension_id, true);
    EXPECT_TRUE(ExtensionPrefs::Get(profile())->AllowFileAccess(extension_id));
    UninstallExtension(extension_id);
    EXPECT_FALSE(ExtensionPrefs::Get(profile())->AllowFileAccess(extension_id));

    scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(service));
    base::test::TestFuture<std::optional<CrxInstallError>>
        installer_done_future;
    installer->AddInstallerCallback(
        installer_done_future
            .GetCallback<const std::optional<CrxInstallError>&>());
    installer->InstallCrx(crx_with_file_permission);
    EXPECT_FALSE(installer_done_future.Get().has_value());
    const Extension* extension = installer->extension();
    ASSERT_TRUE(extension);
    ASSERT_EQ(extension_id, extension->id());
    EXPECT_FALSE(ExtensionPrefs::Get(profile())->AllowFileAccess(extension_id));
    EXPECT_FALSE(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS);
  }

  {
    // Grant file access and update the extension. File access should be kept.
    ExtensionPrefs::Get(profile())->SetAllowFileAccess(extension_id, true);
    EXPECT_TRUE(ExtensionPrefs::Get(profile())->AllowFileAccess(extension_id));
    base::FilePath crx_update_with_file_permission = PackExtension(ext_source);

    scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(service));
    base::test::TestFuture<std::optional<CrxInstallError>>
        installer_done_future;
    installer->AddInstallerCallback(
        installer_done_future
            .GetCallback<const std::optional<CrxInstallError>&>());
    installer->InstallCrx(crx_update_with_file_permission);
    EXPECT_FALSE(installer_done_future.Get().has_value());
    const Extension* extension = installer->extension();
    ASSERT_TRUE(extension);
    ASSERT_EQ(extension_id, extension->id());
    EXPECT_TRUE(ExtensionPrefs::Get(profile())->AllowFileAccess(extension_id));
    EXPECT_TRUE(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS);
  }
}

class ExtensionCrxInstallerTestWithWithholdingUI
    : public ExtensionCrxInstallerTest,
      public testing::WithParamInterface<bool> {
 public:
  ExtensionCrxInstallerTestWithWithholdingUI() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kAllowWithholdingExtensionPermissionsOnInstall);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ExtensionCrxInstallerTestWithWithholdingUI,
                       WithholdingHostsOnInstall) {
  // Permissions should be withhold when the dialog is accepted with the option
  // selected.
  bool should_withhold_permissions = GetParam();
  ScopedTestDialogAutoConfirm::AutoConfirm mode =
      should_withhold_permissions
          ? ScopedTestDialogAutoConfirm::ACCEPT
          : ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION;
  std::unique_ptr<MockPromptProxy> mock_prompt =
      CreateMockPromptProxyForBrowserWithConfirmMode(browser(), mode);

  scoped_refptr<CrxInstaller> crx_installer(
      CrxInstaller::Create(extension_service(), mock_prompt->CreatePrompt()));

  // Install a simple extension with google.com as a permission.
  base::RunLoop run_loop;
  crx_installer->AddInstallerCallback(base::BindOnce(
      &ExtensionCrxInstallerTest::InstallerCallback,
      run_loop.QuitWhenIdleClosure(), CrxInstaller::InstallerResultCallback()));
  base::FilePath crx_with_host =
      PackExtension(test_data_dir_.AppendASCII("simple_with_host"));
  crx_installer->InstallCrx(crx_with_host);
  run_loop.Run();

  EXPECT_TRUE(mock_prompt->did_succeed());
  EXPECT_TRUE(mock_prompt->confirmation_requested());

  // Access to google.com should be withheld only when
  // `should_withhold_permissions` is true.
  const Extension* extension =
      GetInstalledExtension(mock_prompt->extension_id());
  PermissionsManager* permissions_manager =
      PermissionsManager::Get(browser()->profile());
  EXPECT_EQ(should_withhold_permissions,
            permissions_manager->HasWithheldHostPermissions(*extension));

  const PermissionsManager::ExtensionSiteAccess site_access =
      PermissionsManager::Get(profile())->GetSiteAccess(
          *extension, GURL("https://google.com"));
  EXPECT_EQ(should_withhold_permissions, site_access.withheld_site_access);
  EXPECT_EQ(!should_withhold_permissions, site_access.has_site_access);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExtensionCrxInstallerTestWithWithholdingUI,
                         testing::Bool());

}  // namespace extensions
