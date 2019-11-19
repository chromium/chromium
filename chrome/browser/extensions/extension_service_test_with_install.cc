// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service_test_with_install.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/verifier_formats.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

struct ExtensionsOrder {
  bool operator()(const scoped_refptr<const Extension>& a,
                  const scoped_refptr<const Extension>& b) {
    return a->name() < b->name();
  }
};

// Helper method to set up a WindowedNotificationObserver to wait for a
// specific CrxInstaller to finish if we don't know the value of the
// |installer| yet.
bool IsCrxInstallerDone(extensions::CrxInstaller** installer,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  return content::Source<extensions::CrxInstaller>(source).ptr() == *installer;
}

}  // namespace

ExtensionServiceTestWithInstall::ExtensionServiceTestWithInstall()
    : installed_(nullptr),
      was_update_(false),
      unloaded_reason_(UnloadedExtensionReason::UNDEFINED),
      expected_extensions_count_(0),
      override_external_install_prompt_(
          FeatureSwitch::prompt_for_external_extensions(),
          false) {}

ExtensionServiceTestWithInstall::~ExtensionServiceTestWithInstall() {}

void ExtensionServiceTestWithInstall::InitializeExtensionService(
    const ExtensionServiceInitParams& params) {
  ExtensionServiceTestBase::InitializeExtensionService(params);

  registry_observer_.Add(registry());
}

// static
std::vector<base::string16> ExtensionServiceTestWithInstall::GetErrors() {
  const std::vector<base::string16>* errors =
      LoadErrorReporter::GetInstance()->GetErrors();
  std::vector<base::string16> ret_val;

  for (const base::string16& error : *errors) {
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

  ASSERT_TRUE(base::DeleteFile(crx_path, false));

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
    Manifest::Location install_location) {
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
                           Extension::NO_FLAGS, Manifest::Location::INTERNAL);
}

const Extension* ExtensionServiceTestWithInstall::PackAndInstallCRX(
    const base::FilePath& dir_path,
    InstallState install_state) {
  return PackAndInstallCRX(dir_path, base::FilePath(), install_state,
                           Extension::NO_FLAGS, Manifest::Location::INTERNAL);
}

const Extension* ExtensionServiceTestWithInstall::PackAndInstallCRX(
    const base::FilePath& dir_path,
    Manifest::Location install_location,
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
  InstallCRXInternal(path, Manifest::Location::INTERNAL, install_state,
                     creation_flags);
  return VerifyCrxInstall(path, install_state);
}

const Extension* ExtensionServiceTestWithInstall::InstallCRX(
    const base::FilePath& path,
    Manifest::Location install_location,
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
  InstallCRXInternal(path, Manifest::Location::INTERNAL, install_state,
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
  std::vector<base::string16> errors = GetErrors();
  const Extension* extension = nullptr;
  if (install_state != INSTALL_FAILED) {
    if (install_state == INSTALL_NEW || install_state == INSTALL_WITHOUT_LOAD)
      ++expected_extensions_count_;

    EXPECT_TRUE(installed_) << path.value();
    // If and only if INSTALL_UPDATED, it should have the is_update flag.
    EXPECT_EQ(install_state == INSTALL_UPDATED, was_update_)
        << path.value();
    // If INSTALL_UPDATED, old_name_ should match the given string.
    if (install_state == INSTALL_UPDATED && !expected_old_name.empty())
      EXPECT_EQ(expected_old_name, old_name_);
    EXPECT_EQ(0u, errors.size()) << path.value();

    if (install_state == INSTALL_WITHOUT_LOAD) {
      EXPECT_EQ(0u, loaded_.size()) << path.value();
      extension = installed_;
    } else {
      EXPECT_EQ(1u, loaded_.size()) << path.value();
      size_t actual_extension_count =
          registry()->enabled_extensions().size() +
          registry()->disabled_extensions().size();
      EXPECT_EQ(expected_extensions_count_, actual_extension_count) <<
          path.value();
      extension = loaded_[0].get();
      EXPECT_TRUE(registry()->GetExtensionById(extension->id(),
                                               ExtensionRegistry::ENABLED))
          << path.value();
    }

    for (auto err = errors.begin(); err != errors.end(); ++err) {
      LOG(ERROR) << *err;
    }
  } else {
    EXPECT_FALSE(installed_) << path.value();
    EXPECT_EQ(0u, loaded_.size()) << path.value();
    EXPECT_EQ(1u, errors.size()) << path.value();
  }

  installed_ = nullptr;
  was_update_ = false;
  old_name_ = "";
  loaded_.clear();
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

  extensions::CrxInstaller* installer = nullptr;
  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      base::Bind(&IsCrxInstallerDone, &installer));
  service()->UpdateExtension(CRXFileInfo(id, GetTestVerifierFormat(), path),
                             true, &installer);

  if (installer)
    observer.Wait();
  else
    content::RunAllTasksUntilIdle();

  std::vector<base::string16> errors = GetErrors();
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
    const std::string& id) {
  // Verify that the extension is installed.
  ASSERT_TRUE(registry()->GetExtensionById(id, ExtensionRegistry::EVERYTHING));
  base::FilePath extension_path = extensions_install_dir().AppendASCII(id);
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
  content::RunAllTasksUntilIdle();

  // The directory should be gone.
  EXPECT_FALSE(base::PathExists(extension_path));
}

void ExtensionServiceTestWithInstall::TerminateExtension(
    const std::string& id) {
  if (!registry()->GetInstalledExtension(id)) {
    ADD_FAILURE();
    return;
  }
  service()->TerminateExtension(id);
}

void ExtensionServiceTestWithInstall::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  loaded_.push_back(base::WrapRefCounted(extension));
  // The tests rely on the errors being in a certain order, which can vary
  // depending on how filesystem iteration works.
  std::stable_sort(loaded_.begin(), loaded_.end(), ExtensionsOrder());
}

void ExtensionServiceTestWithInstall::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  unloaded_id_ = extension->id();
  unloaded_reason_ = reason;
  auto i = std::find(loaded_.begin(), loaded_.end(), extension);
  // TODO(erikkay) fix so this can be an assert.  Right now the tests
  // are manually calling clear() on loaded_, so this isn't doable.
  if (i == loaded_.end())
    return;
  loaded_.erase(i);
}

void ExtensionServiceTestWithInstall::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    const std::string& old_name) {
  installed_ = extension;
  was_update_ = is_update;
  old_name_ = old_name;
}

// Create a CrxInstaller and install the CRX file.
// Instead of calling this method yourself, use InstallCRX(), which does extra
// error checking.
void ExtensionServiceTestWithInstall::InstallCRXInternal(
    const base::FilePath& crx_path,
    Manifest::Location install_location,
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
  extension_loader.LoadExtension(crx_path);
}

}  // namespace extensions
