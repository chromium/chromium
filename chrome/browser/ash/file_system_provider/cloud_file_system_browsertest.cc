// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/cloud_file_system.h"

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
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

class MockCacheManagerObserver : public CacheManager::Observer {
 public:
  MOCK_METHOD(void,
              OnProviderInitializationComplete,
              (base::FilePath base64_encoded_provider_folder_name,
               base::File::Error result),
              (override));
  MOCK_METHOD(void,
              OnProviderUninitialized,
              (base::FilePath base64_encoded_provider_folder_name,
               base::File::Error result),
              (override));
};

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

  const base::FilePath GetProviderMountPath(
      base::FilePath base64_encoded_provider_folder_name) {
    return profile()
        ->GetPath()
        .Append(kFspContentCacheDirName)
        .Append(base64_encoded_provider_folder_name);
  }
};

// Tests that a CacheManager is created when mounting a CloudFileSystem when
// `kFileSystemProviderCloudFileSystem` and `kFileSystemProviderContentCache`
// are enabled. Tests that a ContentCache is initialized on the CacheManager
// upon mount and is uninitialized upon unmount via the user.
IN_PROC_BROWSER_TEST_F(
    CloudFileSystemBrowserTestWithContentCache,
    ContentCacheInitializedOnMountAndUninitializedOnUnmountByUser) {
  // Catch the cache_manager pointer before it's passed to the CloudFileSystem
  // constructor.
  MockCacheManagerObserver observer;
  base::OnceCallback<void(CacheManager * cache_manager)> on_cache_manager =
      base::BindLambdaForTesting([&](CacheManager* cache_manager) {
        // Expect the cache manager to have been created.
        ASSERT_TRUE(cache_manager);
        cache_manager->AddObserver(&observer);
      });

  // Mount a CloudFileSystem.
  // Specifically mount ODFS as Service::MountFileSystem() will only use a
  // CacheManager if kFileSystemProviderCloudFileSystem and
  // kFileSystemProviderContentCache are set and the extension is ODFS.
  MountOptions mount_options(kFileSystemId, kDisplayName);
  ProvidedFileSystemInterface* cloud_file_system =
      file_manager::test::MountProvidedFileSystem(
          profile(), extension_misc::kODFSExtensionId, mount_options,
          FakeExtensionProviderForCloudFileSystem::Create(
              extension_misc::kODFSExtensionId, std::move(on_cache_manager)));

  // Wait for the provider to be initialized on the CacheManager.
  base::RunLoop run_loop1;
  const base::FilePath base64_encoded_provider_folder_name(base::Base64Encode(
      cloud_file_system->GetFileSystemInfo().mount_path().BaseName().value()));
  EXPECT_CALL(observer,
              OnProviderInitializationComplete(
                  base64_encoded_provider_folder_name, base::File::FILE_OK))
      .WillOnce(base::test::RunClosure(run_loop1.QuitClosure()));
  run_loop1.Run();

  // Check provider cache folder exists.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(
        GetProviderMountPath(base64_encoded_provider_folder_name)));
  }

  // Unmount.
  Service* service = Service::Get(profile());
  service->UnmountFileSystem(
      cloud_file_system->GetFileSystemInfo().provider_id(),
      cloud_file_system->GetFileSystemInfo().file_system_id(),
      Service::UnmountReason::UNMOUNT_REASON_USER);

  // Wait for the provider to be uninitialized on the CacheManager.
  base::RunLoop run_loop2;
  EXPECT_CALL(observer,
              OnProviderUninitialized(base64_encoded_provider_folder_name,
                                      base::File::FILE_OK))
      .WillOnce(base::test::RunClosure(run_loop2.QuitClosure()));
  run_loop2.Run();

  // Check provider cache folder deleted.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(
        GetProviderMountPath(base64_encoded_provider_folder_name)));
  }
}

// Tests that the content cache remains if unmount is performed by the system.
IN_PROC_BROWSER_TEST_F(CloudFileSystemBrowserTestWithContentCache,
                       ContentCacheNotUninitializedOnUnmountByShutdown) {
  // Catch the cache_manager pointer before it's passed to the CloudFileSystem
  // constructor.
  MockCacheManagerObserver observer;
  base::OnceCallback<void(CacheManager * cache_manager)> on_cache_manager =
      base::BindLambdaForTesting([&](CacheManager* cache_manager) {
        // Expect the cache manager to have been created.
        ASSERT_TRUE(cache_manager);
        cache_manager->AddObserver(&observer);
      });

  // Mount a CloudFileSystem.
  // Specifically mount ODFS as Service::MountFileSystem() will only use a
  // CacheManager if kFileSystemProviderCloudFileSystem and
  // kFileSystemProviderContentCache are set and the extension is ODFS.
  MountOptions mount_options(kFileSystemId, kDisplayName);
  ProvidedFileSystemInterface* cloud_file_system =
      file_manager::test::MountProvidedFileSystem(
          profile(), extension_misc::kODFSExtensionId, mount_options,
          FakeExtensionProviderForCloudFileSystem::Create(
              extension_misc::kODFSExtensionId, std::move(on_cache_manager)));

  // Wait for the provider to be initialized on the CacheManager.
  base::RunLoop run_loop;
  const base::FilePath base64_encoded_provider_folder_name(base::Base64Encode(
      cloud_file_system->GetFileSystemInfo().mount_path().BaseName().value()));
  EXPECT_CALL(observer,
              OnProviderInitializationComplete(
                  base64_encoded_provider_folder_name, base::File::FILE_OK))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  run_loop.Run();

  // Expect that the Provider won't be uninitialized.
  EXPECT_CALL(observer, OnProviderUninitialized(testing::_, testing::_))
      .Times(0);

  // Unmount.
  Service* service = Service::Get(profile());
  service->UnmountFileSystem(
      cloud_file_system->GetFileSystemInfo().provider_id(),
      cloud_file_system->GetFileSystemInfo().file_system_id(),
      Service::UnmountReason::UNMOUNT_REASON_SHUTDOWN);
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
