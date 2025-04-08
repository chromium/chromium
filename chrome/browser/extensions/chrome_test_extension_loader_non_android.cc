// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/load_error_waiter.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

ExtensionService* GetExtensionService(
    content::BrowserContext* browser_context) {
  return ExtensionSystem::Get(browser_context)->extension_service();
}

class ExtensionLoadedObserver : public ExtensionRegistryObserver {
 public:
  using ExtensionCallback = base::OnceCallback<void(const Extension*)>;

  ExtensionLoadedObserver(ExtensionRegistry* registry,
                          const base::FilePath& file_path)
      : file_path_(file_path) {
    extension_registry_observation_.Observe(registry);
  }

  static void ObserveOnce(std::unique_ptr<ExtensionLoadedObserver> observer,
                          ExtensionCallback callback) {
    auto* observer_ptr = observer.get();
    observer_ptr->ObserveOnce(std::move(callback).Then(
        // Keep |observer| alive until it made its observation.
        base::BindOnce([](std::unique_ptr<ExtensionLoadedObserver>) {},
                       std::move(observer))));
  }

 private:
  void ObserveOnce(ExtensionCallback callback) {
    if (extension_) {
      std::move(callback).Run(extension_.get());
      // |this| will be deleted here.
      return;
    }
    callback_ = std::move(callback);
  }

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override {
    if (extension->path() == file_path_) {
      extension_registry_observation_.Reset();
      if (callback_) {
        std::move(callback_).Run(extension);
        // |this| will be deleted here.
        return;
      }
      extension_ = extension;
    }
  }

  const base::FilePath file_path_;
  scoped_refptr<const Extension> extension_;
  ExtensionCallback callback_;
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
};

}  // namespace

void ChromeTestExtensionLoader::LoadUnpackedExtensionAsync(
    const base::FilePath& file_path,
    base::OnceCallback<void(const Extension*)> callback) {
  auto observer =
      std::make_unique<ExtensionLoadedObserver>(extension_registry_, file_path);
  UnpackedInstaller::Create(GetExtensionService(browser_context_))
      ->Load(file_path);
  ExtensionLoadedObserver::ObserveOnce(std::move(observer),
                                       std::move(callback));
}

scoped_refptr<const Extension> ChromeTestExtensionLoader::LoadUnpacked(
    const base::FilePath& file_path) {
  scoped_refptr<const Extension> extension;
  TestExtensionRegistryObserver registry_observer(extension_registry_);
  scoped_refptr<UnpackedInstaller> installer =
      UnpackedInstaller::Create(GetExtensionService(browser_context_));
  installer->set_require_modern_manifest_version(
      require_modern_manifest_version_);
  if (allow_file_access_.has_value()) {
    installer->set_allow_file_access(*allow_file_access_);
  }
  if (allow_incognito_access_.has_value()) {
    installer->set_allow_incognito_access(*allow_incognito_access_);
  }
  if (install_param_.has_value()) {
    installer->set_install_param(*install_param_);
  }
  LoadErrorWaiter waiter;
  installer->Load(file_path);
  if (!should_fail_) {
    extension = registry_observer.WaitForExtensionLoaded();
  } else {
    EXPECT_TRUE(waiter.Wait()) << "No load error observed";
  }

  return extension;
}

}  // namespace extensions
