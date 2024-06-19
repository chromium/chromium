// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/profiles/profile.h"
#include "components/value_store/value_store_factory_impl.h"
#include "extensions/browser/api/storage/storage_api.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

namespace {

std::unique_ptr<KeyedService> CreateStorageFrontendForTesting(
    content::BrowserContext* context) {
  auto factory = base::MakeRefCounted<value_store::ValueStoreFactoryImpl>(
      context->GetPath());
  return StorageFrontend::CreateForTesting(factory, context);
}

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return std::make_unique<extensions::EventRouter>(
      profile, ExtensionPrefs::Get(profile));
}

}  // namespace

class SessionStorageApiUnittest : public ExtensionServiceTestWithInstall {
 public:
  SessionStorageApiUnittest() = default;
  ~SessionStorageApiUnittest() override = default;
  SessionStorageApiUnittest(const SessionStorageApiUnittest& other) = delete;
  SessionStorageApiUnittest& operator=(SessionStorageApiUnittest& other) =
      delete;

 protected:
  // A wrapper around api_test_utils::RunFunction that runs the given function
  // and args with the associated profile for the session storage.
  void RunFunction(scoped_refptr<ExtensionFunction> function,
                   const std::string& args,
                   scoped_refptr<const Extension> extension);

  // Returns the session storage of the given extension with the associated
  // profile.
  std::optional<base::Value> GetStorage(
      scoped_refptr<const Extension> extension);

  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

 private:
  void SetFunctionProperties(scoped_refptr<ExtensionFunction> function,
                             scoped_refptr<const Extension> extension);
};

void SessionStorageApiUnittest::RunFunction(
    scoped_refptr<ExtensionFunction> function,
    const std::string& args,
    scoped_refptr<const Extension> extension) {
  SetFunctionProperties(function, extension);
  ASSERT_TRUE(api_test_utils::RunFunction(
      function.get(), base::StringPrintf("[\"session\", %s]", args.c_str()),
      profile()));
}

std::optional<base::Value> SessionStorageApiUnittest::GetStorage(
    scoped_refptr<const Extension> extension) {
  scoped_refptr<ExtensionFunction> function =
      base::MakeRefCounted<StorageStorageAreaGetFunction>();
  SetFunctionProperties(function, extension);
  return api_test_utils::RunFunctionAndReturnSingleResult(
      function.get(), R"(["session", null])", profile());
}

void SessionStorageApiUnittest::SetUp() {
  ExtensionServiceTestWithInstall::SetUp();
  InitializeEmptyExtensionService();

  EventRouterFactory::GetInstance()->SetTestingFactory(
      browser_context(), base::BindRepeating(&BuildEventRouter));

  // Ensure a StorageFrontend can be created on demand. The StorageFrontend
  // will be owned by the KeyedService system.
  StorageFrontend::GetFactoryInstance()->SetTestingFactory(
      browser_context(), base::BindRepeating(&CreateStorageFrontendForTesting));
}

void SessionStorageApiUnittest::TearDown() {
  ExtensionServiceTestWithInstall::TearDown();
}

void SessionStorageApiUnittest::SetFunctionProperties(
    scoped_refptr<ExtensionFunction> function,
    scoped_refptr<const Extension> extension) {
  function->set_extension(extension);
  function->set_source_context_type(mojom::ContextType::kPrivilegedExtension);
}

TEST_F(SessionStorageApiUnittest,
       SessionStorageClearedWhenExtensionIsReloaded) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
          "name": "Sample extension",
          "manifest_version": 3,
          "version": "0.1",
          "permissions": ["storage"]
        })");
  const Extension* extension = InstallCRX(test_dir.Pack(), INSTALL_NEW);
  ExtensionId extension_id = extension->id();

  // Set a value in the session storage and check it can be retrieved.
  RunFunction(base::MakeRefCounted<StorageStorageAreaSetFunction>().get(),
              R"({"foo": "bar"})", extension);
  EXPECT_THAT(*GetStorage(extension), base::test::IsJson(R"({"foo": "bar"})"));

  // Reload the extension and check the session storage is cleared.
  TestExtensionRegistryObserver registry_observer(registry(), extension_id);
  service()->ReloadExtension(extension_id);
  scoped_refptr<const Extension> reloaded_extension =
      registry_observer.WaitForExtensionLoaded();
  EXPECT_THAT(*GetStorage(reloaded_extension), base::test::IsJson(R"({})"));
}

}  // namespace extensions
