// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/shared_module_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "components/crx_file/id_util.h"
#include "components/sync/model/string_ordinal.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"

namespace extensions {

namespace {

// Return an extension with |id| which imports all the modules that are in the
// container |import_ids|.
scoped_refptr<const Extension> CreateExtensionImportingModules(
    const std::vector<std::string>& import_ids,
    const std::string& id,
    const std::string& version) {
  auto builder = base::Value::Dict()
                     .Set("name", "Has Dependent Modules")
                     .Set("version", version)
                     .Set("manifest_version", 2);
  if (!import_ids.empty()) {
    base::Value::List import_list;
    for (const std::string& import_id : import_ids)
      import_list.Append(base::Value::Dict().Set("id", import_id));
    builder.Set("import", std::move(import_list));
  }
  return ExtensionBuilder()
      .SetManifest(std::move(builder))
      .AddFlags(Extension::FROM_WEBSTORE)
      .SetID(id)
      .Build();
}

scoped_refptr<const Extension> CreateSharedModule(
    const std::string& module_id) {
  base::Value::Dict manifest =
      base::Value::Dict()
          .Set("name", "Shared Module")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Set("export",
               base::Value::Dict().Set("resources",
                                       base::Value::List().Append("foo.js")));

  return ExtensionBuilder()
      .SetManifest(std::move(manifest))
      .AddFlags(Extension::FROM_WEBSTORE)
      .SetID(crx_file::id_util::GenerateId(module_id))
      .Build();
}

}  // namespace

class SharedModuleServiceUnitTest : public ExtensionServiceTestBase {
 public:
  SharedModuleServiceUnitTest() :
      // The "export" key is open for dev-channel only, but unit tests
      // run as stable channel on the official Windows build.
      current_channel_(version_info::Channel::UNKNOWN) {}
 protected:
  void SetUp() override;

  // Install an extension and notify the ExtensionService.
  testing::AssertionResult InstallExtension(const Extension* extension,
                                            bool is_update);
  ScopedCurrentChannel current_channel_;
};

void SharedModuleServiceUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeGoodInstalledExtensionService();
  service()->Init();
}

testing::AssertionResult SharedModuleServiceUnitTest::InstallExtension(
    const Extension* extension,
    bool is_update) {

  const Extension* old = registry()->GetExtensionById(
      extension->id(),
      ExtensionRegistry::ENABLED);

  // Verify the extension is not already installed, if it is not update.
  if (!is_update) {
    if (old)
      return testing::AssertionFailure() << "Extension already installed.";
  } else {
    if (!old)
      return testing::AssertionFailure() << "The extension does not exist.";
  }

  // Notify the service that the extension is installed. This adds it to the
  // registry, notifies interested parties, etc.
  service()->OnExtensionInstalled(
      extension, syncer::StringOrdinal(), kInstallFlagInstallImmediately);

  // Verify that the extension is now installed.
  if (!registry()->enabled_extensions().Contains(extension->id())) {
    return testing::AssertionFailure() << "Could not install extension.";
  }

  return testing::AssertionSuccess();
}

TEST_F(SharedModuleServiceUnitTest, AddDependentSharedModules) {
  // Create an extension that has a dependency.
  std::string import_id = crx_file::id_util::GenerateId("id");
  std::string extension_id = crx_file::id_util::GenerateId("extension_id");
  scoped_refptr<const Extension> extension = CreateExtensionImportingModules(
      std::vector<std::string>(1, import_id), extension_id, "1.0");

  PendingExtensionManager* pending_extension_manager =
      service()->pending_extension_manager();

  // Verify that we don't currently want to install the imported module.
  EXPECT_FALSE(pending_extension_manager->IsIdPending(import_id));

  // Try to satisfy imports for the extension. This should queue the imported
  // module's installation.
  service()->shared_module_service()->SatisfyImports(extension.get());
  EXPECT_TRUE(pending_extension_manager->IsIdPending(import_id));
}

TEST_F(SharedModuleServiceUnitTest, PruneSharedModulesOnUninstall) {
  // Create a module which exports a resource, and install it.
  scoped_refptr<const Extension> shared_module =
      CreateSharedModule("shared_module");

  EXPECT_TRUE(InstallExtension(shared_module.get(), false));

  std::string extension_id = crx_file::id_util::GenerateId("extension_id");
  // Create and install an extension that imports our new module.
  scoped_refptr<const Extension> importing_extension =
      CreateExtensionImportingModules(
          std::vector<std::string>(1, shared_module->id()), extension_id,
          "1.0");
  EXPECT_TRUE(InstallExtension(importing_extension.get(), false));

  // Uninstall the extension that imports our module.
  std::u16string error;
  service()->UninstallExtension(importing_extension->id(),
                                UNINSTALL_REASON_FOR_TESTING, &error);
  EXPECT_TRUE(error.empty());

  // Since the module was only referenced by that single extension, it should
  // have been uninstalled as a side-effect of uninstalling the extension that
  // depended upon it.
  EXPECT_FALSE(registry()->GetExtensionById(shared_module->id(),
                                            ExtensionRegistry::EVERYTHING));
}

TEST_F(SharedModuleServiceUnitTest, PruneSharedModulesOnUpdate) {
  // Create two modules which export a resource, and install them.
  scoped_refptr<const Extension> shared_module_1 =
      CreateSharedModule("shared_module_1");
  EXPECT_TRUE(InstallExtension(shared_module_1.get(), false));

  base::Value::Dict manifest_2 =
      base::Value::Dict()
          .Set("name", "Shared Module 2")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Set("export",
               base::Value::Dict().Set("resources",
                                       base::Value::List().Append("foo.js")));
  scoped_refptr<const Extension> shared_module_2 =
      CreateSharedModule("shared_module_2");
  EXPECT_TRUE(InstallExtension(shared_module_2.get(), false));

  std::string extension_id = crx_file::id_util::GenerateId("extension_id");

  // Create and install an extension v1.0 that imports our new module 1.
  scoped_refptr<const Extension> importing_extension_1 =
      CreateExtensionImportingModules(
          std::vector<std::string>(1, shared_module_1->id()), extension_id,
          "1.0");
  EXPECT_TRUE(InstallExtension(importing_extension_1.get(), false));

  // Create and install a new version of the extension that imports our new
  // module 2.
  scoped_refptr<const Extension> importing_extension_2 =
      CreateExtensionImportingModules(
          std::vector<std::string>(1, shared_module_2->id()), extension_id,
          "1.1");
  EXPECT_TRUE(InstallExtension(importing_extension_2.get(), true));

  // Since the extension v1.1 depends the module 2 insteand module 1.
  // So the module 1 should be uninstalled.
  EXPECT_FALSE(registry()->GetExtensionById(shared_module_1->id(),
                                            ExtensionRegistry::EVERYTHING));
  EXPECT_TRUE(registry()->GetExtensionById(shared_module_2->id(),
                                            ExtensionRegistry::EVERYTHING));

  // Create and install a new version of the extension that does not import any
  // module.
  scoped_refptr<const Extension> importing_extension_3 =
      CreateExtensionImportingModules(std::vector<std::string>(), extension_id,
                                      "1.2");
  EXPECT_TRUE(InstallExtension(importing_extension_3.get(), true));

  // Since the extension v1.2 does not depend any module, so the all models
  // should have been uninstalled.
  EXPECT_FALSE(registry()->GetExtensionById(shared_module_1->id(),
                                            ExtensionRegistry::EVERYTHING));
  EXPECT_FALSE(registry()->GetExtensionById(shared_module_2->id(),
                                            ExtensionRegistry::EVERYTHING));

}

TEST_F(SharedModuleServiceUnitTest, AllowlistedImports) {
  std::string allowlisted_id = crx_file::id_util::GenerateId("allowlisted");
  std::string nonallowlisted_id =
      crx_file::id_util::GenerateId("nonallowlisted");
  // Create a module which exports to a restricted allowlist.
  base::Value::Dict manifest =
      base::Value::Dict()
          .Set("name", "Shared Module")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Set("export",
               base::Value::Dict()
                   .Set("allowlist", base::Value::List().Append(allowlisted_id))
                   .Set("resources", base::Value::List().Append("*")));
  scoped_refptr<const Extension> shared_module =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .AddFlags(Extension::FROM_WEBSTORE)
          .SetID(crx_file::id_util::GenerateId("shared_module"))
          .Build();

  EXPECT_TRUE(InstallExtension(shared_module.get(), false));

  // Create and install an extension with the allowlisted ID.
  scoped_refptr<const Extension> allowlisted_extension =
      CreateExtensionImportingModules(
          std::vector<std::string>(1, shared_module->id()), allowlisted_id,
          "1.0");
  EXPECT_TRUE(InstallExtension(allowlisted_extension.get(), false));

  // Try to install an extension with an ID that is not allowlisted.
  scoped_refptr<const Extension> nonallowlisted_extension =
      CreateExtensionImportingModules(
          std::vector<std::string>(1, shared_module->id()), nonallowlisted_id,
          "1.0");
  // This should succeed because only CRX installer (and by extension the
  // WebStore Installer) checks the shared module allowlist.  InstallExtension
  // bypasses the allowlist check because the SharedModuleService does not
  // care about allowlists.
  EXPECT_TRUE(InstallExtension(nonallowlisted_extension.get(), false));
}

TEST_F(SharedModuleServiceUnitTest, PruneMultipleSharedModules) {
  // Create two modules which export a resource each, and install it.
  scoped_refptr<const Extension> shared_module_one =
      CreateSharedModule("shared_module_one");
  EXPECT_TRUE(InstallExtension(shared_module_one.get(), false));
  scoped_refptr<const Extension> shared_module_two =
      CreateSharedModule("shared_module_two");
  EXPECT_TRUE(InstallExtension(shared_module_two.get(), false));

  std::string extension_id = crx_file::id_util::GenerateId("extension_id");
  std::vector<std::string> module_ids;
  module_ids.push_back(shared_module_one->id());
  module_ids.push_back(shared_module_two->id());
  // Create and install an extension that imports both the modules.
  scoped_refptr<const Extension> importing_extension =
      CreateExtensionImportingModules(module_ids, extension_id, "1.0");
  EXPECT_TRUE(InstallExtension(importing_extension.get(), false));

  // Uninstall the extension that imports our modules.
  std::u16string error;
  service()->UninstallExtension(importing_extension->id(),
                                UNINSTALL_REASON_FOR_TESTING, &error);
  EXPECT_TRUE(error.empty());

  // Since the modules were only referenced by that single extension, they
  // should have been uninstalled as a side-effect of uninstalling the extension
  // that depended upon it.
  EXPECT_FALSE(registry()->GetExtensionById(shared_module_one->id(),
                                            ExtensionRegistry::EVERYTHING));
  EXPECT_FALSE(registry()->GetExtensionById(shared_module_two->id(),
                                            ExtensionRegistry::EVERYTHING));
}

}  // namespace extensions
