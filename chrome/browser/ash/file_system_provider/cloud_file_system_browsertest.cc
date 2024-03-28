// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/cloud_file_system.h"

#include "base/test/bind.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"

namespace ash::file_system_provider {
namespace {
const char kFileSystemId[] = "cloud-fs-id";
const char kDisplayName[] = "Cloud FS";

// Fake extension provider to create a `a `CloudFileSystem` wrapping by a
// `FakeProvidedFileSystem`. This also observes the `CacheManager`, if it is
// provided.
class FakeExtensionProviderForCloudFileSystem : public FakeExtensionProvider {
 public:
  static std::unique_ptr<ProviderInterface> Create(
      const extensions::ExtensionId& extension_id,
      base::OnceCallback<void(CacheManager* cache_manager)> on_cache_manager) {
    Capabilities default_capabilities(false, false, false,
                                      extensions::SOURCE_NETWORK);
    return std::make_unique<FakeExtensionProviderForCloudFileSystem>(
        extension_id, default_capabilities, std::move(on_cache_manager));
  }

  FakeExtensionProviderForCloudFileSystem(
      const extensions::ExtensionId& extension_id,
      const Capabilities& capabilities,
      base::OnceCallback<void(CacheManager* cache_manager)> on_cache_manager)
      : FakeExtensionProvider(extension_id, capabilities),
        on_cache_manager_(std::move(on_cache_manager)) {}

  // This will be called by the `Service::MountFileSystem()` and will lead to a
  // `CloudFileSystem` being mounted. Return a `CloudFileSystem` wrapping by a
  // `FakeProvidedFileSystem`. This will include a content cache if the
  // `FileSystemProviderContentCache` flag is set and the provider is ODFS
  // because the `cache_manager` would have been created. This logic mirrors the
  // real `ExtensionProvider::CreateProvidedFileSystem()`. Also call the
  // `on_cache_manager_` callback with the provided `cache_manager`.
  std::unique_ptr<ProvidedFileSystemInterface> CreateProvidedFileSystem(
      Profile* profile,
      const ProvidedFileSystemInfo& file_system_info,
      CacheManager* cache_manager) override {
    EXPECT_TRUE(
        chromeos::features::IsFileSystemProviderCloudFileSystemEnabled());
    std::move(on_cache_manager_).Run(cache_manager);
    if (file_system_info.cache_type() != CacheType::NONE) {
      // CloudFileSystem with cache.
      return std::make_unique<CloudFileSystem>(
          std::make_unique<FakeProvidedFileSystem>(file_system_info),
          cache_manager);
    }
    // CloudFileSystem without cache.
    return std::make_unique<CloudFileSystem>(
        std::make_unique<FakeProvidedFileSystem>(file_system_info));
  }

 private:
  base::OnceCallback<void(CacheManager* cache_manager)> on_cache_manager_;
};

}  // namespace

class CloudFileSystemBrowserTest : public InProcessBrowserTest {
 public:
  CloudFileSystemBrowserTest() = default;

  Profile* profile() { return browser()->profile(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

class CloudFileSystemBrowserTestWithContentCache
    : public CloudFileSystemBrowserTest {
 protected:
  CloudFileSystemBrowserTestWithContentCache() {
    feature_list_.InitWithFeatures(
        {chromeos::features::kFileSystemProviderCloudFileSystem,
         chromeos::features::kFileSystemProviderContentCache},
        {});
  }
};

// Tests that a CacheManager is created when mounting a CloudFileSystem when
// `kFileSystemProviderCloudFileSystem` and `kFileSystemProviderContentCache`
// are enabled.
IN_PROC_BROWSER_TEST_F(CloudFileSystemBrowserTestWithContentCache,
                       CacheManagerCreated) {
  // Mount a CloudFileSystem.
  MountOptions mount_options(kFileSystemId, kDisplayName);
  base::OnceCallback<void(CacheManager * cache_manager)> on_cache_manager =
      base::BindLambdaForTesting([](CacheManager* cache_manager) {
        // Expect the cache manager to have been created.
        ASSERT_TRUE(cache_manager);
      });

  // Specifically mount ODFS as Service::MountFileSystem() will only use a
  // CacheManager if kFileSystemProviderCloudFileSystem and
  // kFileSystemProviderContentCache are set and the extension is ODFS.
  file_manager::test::MountProvidedFileSystem(
      profile(), extension_misc::kODFSExtensionId, mount_options,
      FakeExtensionProviderForCloudFileSystem::Create(
          extension_misc::kODFSExtensionId, std::move(on_cache_manager)));
}

class CloudFileSystemBrowserTestWithoutContentCache
    : public CloudFileSystemBrowserTest {
 protected:
  CloudFileSystemBrowserTestWithoutContentCache() {
    feature_list_.InitWithFeatures(
        {chromeos::features::kFileSystemProviderCloudFileSystem},
        {chromeos::features::kFileSystemProviderContentCache});
  }
};

// Tests that a CacheManager is not created when mounting a CloudFileSystem when
// `kFileSystemProviderCloudFileSystem` is enabled but
// `kFileSystemProviderContentCache` isn't.
IN_PROC_BROWSER_TEST_F(CloudFileSystemBrowserTestWithoutContentCache,
                       CacheManagerNotCreated) {
  // Mount a CloudFileSystem.
  MountOptions mount_options(kFileSystemId, kDisplayName);
  base::OnceCallback<void(CacheManager * cache_manager)> on_cache_manager =
      base::BindLambdaForTesting([](CacheManager* cache_manager) {
        // Expect the cache manager to not have been created.
        ASSERT_FALSE(cache_manager);
      });

  // Specifically mount ODFS as Service::MountFileSystem() will only use a
  // CacheManager if kFileSystemProviderCloudFileSystem and
  // kFileSystemProviderContentCache are set and the extension is ODFS.
  file_manager::test::MountProvidedFileSystem(
      profile(), extension_misc::kODFSExtensionId, mount_options,
      FakeExtensionProviderForCloudFileSystem::Create(
          extension_misc::kODFSExtensionId, std::move(on_cache_manager)));
}

}  // namespace ash::file_system_provider
