// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service_test_with_install.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/common/verifier_formats.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;

namespace extensions {

namespace {

struct ExtensionsOrder {
  bool operator()(const scoped_refptr<const Extension>& a,
                  const scoped_refptr<const Extension>& b) {
    return a->name() < b->name();
  }
};

}  // namespace

ExtensionServiceTestWithInstall::ExtensionServiceTestWithInstall()
    : ExtensionServiceTestWithInstall(
          std::make_unique<content::BrowserTaskEnvironment>(
              base::test::TaskEnvironment::MainThreadType::IO)) {}

ExtensionServiceTestWithInstall::ExtensionServiceTestWithInstall(
    std::unique_ptr<content::BrowserTaskEnvironment> task_environment)
    : ExtensionServiceUserTestBase(std::move(task_environment)),
      installed_extension_(nullptr),
      was_update_(false),
      unloaded_reason_(UnloadedExtensionReason::UNDEFINED),
      expected_extensions_count_(0),
      override_external_install_prompt_(
          FeatureSwitch::prompt_for_external_extensions(),
          false) {}

ExtensionServiceTestWithInstall::~ExtensionServiceTestWithInstall() {}

void ExtensionServiceTestWithInstall::InitializeExtensionService(
    ExtensionServiceInitParams params) {
  ExtensionServiceTestBase::InitializeExtensionService(std::move(params));

  registry_observation_.Observe(registry());
}

// static
std::vector<std::u16string> ExtensionServiceTestWithInstall::GetErrors() {
  const std::vector<std::u16string>* errors =
      LoadErrorReporter::GetInstance()->GetErrors();
  std::vector<std::u16string> ret_val;

  for (const std::u16string& error : *errors) {
    std::string utf8_error = base::UTF16ToUTF8(error);
    if (utf8_error.find(".svn") == std::string::npos) {
      ret_val.push_back(error);
    }
  }

  // The tests rely on the errors being in a certain order, which can vary
  // depending on how filesystem iteration works.
  std::stable_sort(ret_val.begin(), ret_val.end());

  return ret_val;
}

void ExtensionServiceTestWithInstall::PackCRX(const base::FilePath& dir_path,
                                              const base::FilePath& pem_path,
                                              const base::FilePath& crx_path) {
  // Use the existing pem key, if provided.
  base::FilePath pem_output_path;
  if (pem_path.value().empty()) {
    pem_output_path = crx_path.DirName().AppendASCII("temp.pem");
  } else {
    ASSERT_TRUE(base::PathExists(pem_path));
  }

  ASSERT_TRUE(base::DeleteFile(crx_path));

  std::unique_ptr<ExtensionCreator> creator(new ExtensionCreator());
  ASSERT_TRUE(creator->Run(dir_path,
                           crx_path,
                           pem_path,
                           pem_output_path,
                           ExtensionCreator::kOverwriteCRX));

  ASSERT_TRUE(base::PathExists(crx_path));
}

const Extension* ExtensionServiceTestWithInstall::PackAndInstallCRX(
    const base::FilePath& dir_path,
    const base::FilePath& pem_path,
    InstallState install_state,
    int creation_flags,
    ManifestLocation install_location) {
  base::FilePath crx_path;
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  crx_path = temp_dir.GetPath().AppendASCII("temp.crx");
  PackCRX(dir_path, pem_path, crx_path);

  return InstallCRX(crx_path, install_location, install_state, creation_flags);
}

const Extension* ExtensionServiceTestWithInstall::PackAndInstallCRX(
    const base::FilePath& dir_path,
    const base::FilePath& pem_path,
    InstallState install_state) {
  return PackAndInstallCRX(dir_path, pem_path, install_state,
                           Extension::NO_FLAGS, ManifestLocation::kInternal);
}

const Extension* ExtensionServiceTestWithInstall::PackAndInstallCRX(
    const base::FilePath& dir_path,
    InstallState install_state) {
  return PackAndInstallCRX(dir_path, base::FilePath(), install_state,
                           Extension::NO_FLAGS, ManifestLocation::kInternal);
}

const Extension* ExtensionServiceTestWithInstall::PackAndInstallCRX(
    const base::FilePath& dir_path,
    ManifestLocation install_location,
    InstallState install_state) {
  return PackAndInstallCRX(dir_path, base::FilePath(), install_state,
                           Extension::NO_FLAGS, install_location);
}

// Attempts to install an extension. Use INSTALL_FAILED if the installation
// is expected to fail.
// If |install_state| is INSTALL_UPDATED, and |expected_old_name| is
// non-empty, expects that the existing extension's title was
// |expected_old_name|.
const Extension* ExtensionServiceTestWithInstall::InstallCRX(
    const base::FilePath& path,
    InstallState install_state,
    int creation_flags,
    const std::string& expected_old_name) {
  InstallCRXInternal(path, ManifestLocation::kInternal, install_state,
                     creation_flags);
  return VerifyCrxInstall(path, install_state);
}

const Extension* ExtensionServiceTestWithInstall::InstallCRX(
    const base::FilePath& path,
    ManifestLocation install_location,
    InstallState install_state,
    int creation_flags) {
  InstallCRXInternal(path, install_location, install_state, creation_flags);
  return VerifyCrxInstall(path, install_state);
}

// Attempts to install an extension. Use INSTALL_FAILED if the installation
// is expected to fail.
const Extension* ExtensionServiceTestWithInstall::InstallCRX(
    const base::FilePath& path,
    InstallState install_state,
    int creation_flags) {
  return InstallCRX(path, install_state, creation_flags, std::string());
}

// Attempts to install an extension. Use INSTALL_FAILED if the installation
// is expected to fail.
const Extension* ExtensionServiceTestWithInstall::InstallCRX(
    const base::FilePath& path,
    InstallState install_state) {
  return InstallCRX(path, install_state, Extension::NO_FLAGS);
}

const Extension* ExtensionServiceTestWithInstall::InstallCRXFromWebStore(
    const base::FilePath& path,
    InstallState install_state) {
  InstallCRXInternal(path, ManifestLocation::kInternal, install_state,
                     Extension::FROM_WEBSTORE);
  return VerifyCrxInstall(path, install_state);
}

const Extension* ExtensionServiceTestWithInstall::VerifyCrxInstall(
    const base::FilePath& path,
    InstallState install_state) {
  return VerifyCrxInstall(path, install_state, std::string());
}

const Extension* ExtensionServiceTestWithInstall::VerifyCrxInstall(
    const base::FilePath& path,
    InstallState install_state,
    const std::string& expected_old_name) {
  std::vector<std::u16string> errors = GetErrors();
  const Extension* extension = nullptr;
  if (install_state != INSTALL_FAILED) {
    if (install_state == INSTALL_NEW || install_state == INSTALL_WITHOUT_LOAD)
      ++expected_extensions_count_;

    EXPECT_TRUE(installed_extension_) << path.value();
    // If and only if INSTALL_UPDATED, it should have the is_update flag.
    EXPECT_EQ(install_state == INSTALL_UPDATED, was_update_)
        << path.value();
    // If INSTALL_UPDATED, old_name_ should match the given string.
    if (install_state == INSTALL_UPDATED && !expected_old_name.empty())
      EXPECT_EQ(expected_old_name, old_name_);
    EXPECT_EQ(0u, errors.size()) << path.value();

    if (install_state == INSTALL_WITHOUT_LOAD) {
      EXPECT_EQ(0u, loaded_extensions_.size()) << path.value();
      extension = installed_extension_;
    } else {
      EXPECT_EQ(1u, loaded_extensions_.size()) << path.value();
      size_t actual_extension_count =
          registry()->enabled_extensions().size() +
          registry()->disabled_extensions().size();
      EXPECT_EQ(expected_extensions_count_, actual_extension_count) <<
          path.value();
      extension = loaded_extensions_[0].get();
      EXPECT_TRUE(registry()->enabled_extensions().GetByID(extension->id()))
          << path.value();
    }

    for (auto err = errors.begin(); err != errors.end(); ++err) {
      LOG(ERROR) << *err;
    }
  } else {
    EXPECT_FALSE(installed_extension_) << path.value();
    EXPECT_EQ(0u, loaded_extensions_.size()) << path.value();
    EXPECT_EQ(1u, errors.size()) << path.value();
  }

  installed_extension_ = nullptr;
  was_update_ = false;
  old_name_ = "";
  ClearLoadedExtensions();
  LoadErrorReporter::GetInstance()->ClearErrors();
  return extension;
}

void ExtensionServiceTestWithInstall::PackCRXAndUpdateExtension(
    const std::string& id,
    const base::FilePath& dir_path,
    const base::FilePath& pem_path,
    UpdateState expected_state) {
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath crx_path = temp_dir.GetPath().AppendASCII("temp.crx");

  PackCRX(dir_path, pem_path, crx_path);
  UpdateExtension(id, crx_path, expected_state);
}

void ExtensionServiceTestWithInstall::UpdateExtension(
    const std::string& id,
    const base::FilePath& in_path,
    UpdateState expected_state) {
  ASSERT_TRUE(base::PathExists(in_path));

  // We need to copy this to a temporary location because Update() will delete
  // it.
  base::FilePath path = temp_dir().GetPath();
  path = path.Append(in_path.BaseName());
  ASSERT_TRUE(base::CopyFile(in_path, path));

  int previous_enabled_extension_count =
      registry()->enabled_extensions().size();
  int previous_installed_extension_count =
      previous_enabled_extension_count +
      registry()->disabled_extensions().size();

  CRXFileInfo crx_info(path, GetTestVerifierFormat());
  crx_info.extension_id = id;

  auto installer = service()->CreateUpdateInstaller(crx_info, true);

  if (installer) {
    base::RunLoop run_loop;
    installer->AddInstallerCallback(base::BindLambdaForTesting(
        [&run_loop](const std::optional<CrxInstallError>& error) {
          run_loop.Quit();
        }));
    installer->InstallCrxFile(crx_info);
    run_loop.Run();
  } else {
    content::RunAllTasksUntilIdle();
  }

  std::vector<std::u16string> errors = GetErrors();
  int error_count = errors.size();
  int enabled_extension_count = registry()->enabled_extensions().size();
  int installed_extension_count =
      enabled_extension_count + registry()->disabled_extensions().size();

  int expected_error_count = (expected_state == FAILED) ? 1 : 0;
  EXPECT_EQ(expected_error_count, error_count) << path.value();

  if (expected_state <= FAILED) {
    EXPECT_EQ(previous_enabled_extension_count,
              enabled_extension_count);
    EXPECT_EQ(previous_installed_extension_count,
              installed_extension_count);
  } else {
    int expected_installed_extension_count =
        (expected_state >= INSTALLED) ? 1 : 0;
    int expected_enabled_extension_count =
        (expected_state >= ENABLED) ? 1 : 0;
    EXPECT_EQ(expected_installed_extension_count,
              installed_extension_count);
    EXPECT_EQ(expected_enabled_extension_count,
              enabled_extension_count);
  }

  // Verify that after running all pending tasks, the temporary file has been
  // deleted.
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(base::PathExists(path));
}

void ExtensionServiceTestWithInstall::UninstallExtension(
    const std::string& id,
    UninstallExtensionFileDeleteType delete_type) {
  // Verify that the extension is installed.
  const Extension* extension =
      registry()->GetExtensionById(id, ExtensionRegistry::EVERYTHING);
  ASSERT_TRUE(extension);
  base::FilePath extension_path = base::FilePath(extension->path());
  EXPECT_TRUE(base::PathExists(extension_path));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(prefs->GetInstalledExtensionInfo(id));

  // We make a copy of the extension's id since the extension can be deleted
  // once it's uninstalled.
  std::string extension_id = id;
  // Uninstall it.
  EXPECT_TRUE(service()->UninstallExtension(
      id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr));
  --expected_extensions_count_;

  // We should get an unload notification.
  EXPECT_FALSE(unloaded_id_.empty());
  EXPECT_EQ(extension_id, unloaded_id_);

  // The extension should not be in the service anymore.
  EXPECT_FALSE(registry()->GetInstalledExtension(extension_id));
  EXPECT_FALSE(prefs->GetInstalledExtensionInfo(extension_id));
  task_environment()->RunUntilIdle();

  switch (delete_type) {
    case kDeleteAllVersions:
      EXPECT_FALSE(base::PathExists(extension_path.DirName()));
      break;
    case kDeletePath:
      EXPECT_FALSE(base::PathExists(extension_path));
      break;
    case kDoNotDelete:
      EXPECT_TRUE(base::PathExists(extension_path));
      break;
  }
}

void ExtensionServiceTestWithInstall::TerminateExtension(
    const std::string& id) {
  if (!registry()->GetInstalledExtension(id)) {
    ADD_FAILURE();
    return;
  }
  service()->TerminateExtension(id);
}

void ExtensionServiceTestWithInstall::BlockAllExtensions() {
  service()->BlockAllExtensions();
}

void ExtensionServiceTestWithInstall::ClearLoadedExtensions() {
  loaded_extensions_.clear();
}

void ExtensionServiceTestWithInstall::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  loaded_extensions_.push_back(base::WrapRefCounted(extension));
  // The tests rely on the errors being in a certain order, which can vary
  // depending on how filesystem iteration works.
  std::stable_sort(loaded_extensions_.begin(), loaded_extensions_.end(),
                   ExtensionsOrder());
}

void ExtensionServiceTestWithInstall::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  unloaded_id_ = extension->id();
  unloaded_reason_ = reason;
  auto i = base::ranges::find(loaded_extensions_, extension);
  // TODO(erikkay) fix so this can be an assert.  Right now the tests
  // are manually calling `ClearLoadedExtensions` since this method is not
  // called by reloads, so this isn't doable.
  if (i == loaded_extensions_.end())
    return;
  loaded_extensions_.erase(i);
}

void ExtensionServiceTestWithInstall::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    const std::string& old_name) {
  installed_extension_ = extension;
  was_update_ = is_update;
  old_name_ = old_name;
}

// Create a CrxInstaller and install the CRX file.
// Instead of calling this method yourself, use InstallCRX(), which does extra
// error checking.
void ExtensionServiceTestWithInstall::InstallCRXInternal(
    const base::FilePath& crx_path,
    ManifestLocation install_location,
    InstallState install_state,
    int creation_flags) {
  ChromeTestExtensionLoader extension_loader(profile());
  extension_loader.set_location(install_location);
  extension_loader.set_creation_flags(creation_flags);
  extension_loader.set_should_fail(install_state == INSTALL_FAILED);
  // TODO(devlin): We shouldn't be granting permissions based on whether
  // something was installed by default. That's weird.
  extension_loader.set_grant_permissions(
      (creation_flags & Extension::WAS_INSTALLED_BY_DEFAULT) == 0);
  // TODO(devlin): We shouldn't ignore manifest warnings here, but we always
  // did so a bunch of stuff fails. Migrate this over.
  extension_loader.set_ignore_manifest_warnings(true);
  extension_loader.set_wait_for_renderers(false);
  extension_loader.LoadExtension(crx_path);
}

}  // namespace extensions
