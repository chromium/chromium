// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/external_cache_impl.h"

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/extensions/external_cache_delegate.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/verifier_formats.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

namespace {

using ::testing::Optional;

const char kTestExtensionId1[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestExtensionId2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const char kTestExtensionId3[] = "cccccccccccccccccccccccccccccccc";
const char kTestExtensionId4[] = "dddddddddddddddddddddddddddddddd";
const char kNonWebstoreUpdateUrl[] = "https://localhost/service/update2/crx";
const char kExternalCrxPath[] = "/local/path/to/extension.crx";
const char kExternalCrxVersion[] = "1.2.3.4";

}  // namespace

class ExternalCacheImplTest : public testing::Test,
                              public ExternalCacheDelegate {
 public:
  ExternalCacheImplTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  ExternalCacheImplTest(const ExternalCacheImplTest&) = delete;
  ExternalCacheImplTest& operator=(const ExternalCacheImplTest&) = delete;

  ~ExternalCacheImplTest() override = default;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() {
    return test_shared_loader_factory_;
  }

  const absl::optional<base::Value::Dict>& provided_prefs() { return prefs_; }
  const std::set<extensions::ExtensionId>& deleted_extension_files() const {
    return deleted_extension_files_;
  }

  // ExternalCacheDelegate:
  void OnExtensionListsUpdated(const base::Value::Dict& prefs) override {
    prefs_ = prefs.Clone();
  }

  void OnCachedExtensionFileDeleted(
      const extensions::ExtensionId& id) override {
    deleted_extension_files_.insert(id);
  }

  base::FilePath CreateCacheDir(bool initialized) {
    EXPECT_TRUE(cache_dir_.CreateUniqueTempDir());
    if (initialized)
      CreateFlagFile(cache_dir_.GetPath());
    return cache_dir_.GetPath();
  }

  base::FilePath CreateTempDir() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    return temp_dir_.GetPath();
  }

  void CreateFlagFile(const base::FilePath& dir) {
    CreateFile(
        dir.Append(extensions::LocalExtensionCache::kCacheReadyFlagFileName));
  }

  void CreateExtensionFile(const base::FilePath& dir,
                           const std::string& id,
                           const std::string& version) {
    CreateFile(GetExtensionFile(dir, id, version));
  }

  void CreateFile(const base::FilePath& file) {
    EXPECT_EQ(base::WriteFile(file, nullptr, 0), 0);
  }

  base::FilePath GetExtensionFile(const base::FilePath& dir,
                                  const std::string& id,
                                  const std::string& version) {
    return dir.Append(id + "-" + version + ".crx");
  }

  base::Value CreateEntryWithUpdateUrl(bool from_webstore) {
    base::Value::Dict entry;
    entry.Set(extensions::ExternalProviderImpl::kExternalUpdateUrl,
              from_webstore ? extension_urls::GetWebstoreUpdateUrl().spec()
                            : kNonWebstoreUpdateUrl);
    return base::Value(std::move(entry));
  }

  base::Value CreateEntryWithExternalCrx() {
    base::Value::Dict entry;
    entry.Set(extensions::ExternalProviderImpl::kExternalCrx, kExternalCrxPath);
    entry.Set(extensions::ExternalProviderImpl::kExternalVersion,
              kExternalCrxVersion);
    return base::Value(std::move(entry));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  base::ScopedTempDir cache_dir_;
  base::ScopedTempDir temp_dir_;
  absl::optional<base::Value::Dict> prefs_;
  std::set<extensions::ExtensionId> deleted_extension_files_;

  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
};

TEST_F(ExternalCacheImplTest, Basic) {
  base::FilePath cache_dir(CreateCacheDir(false));
  ExternalCacheImpl external_cache(
      cache_dir, url_loader_factory(),
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}), this,
      true, false, false);

  base::Value::Dict prefs;
  prefs.Set(kTestExtensionId1, CreateEntryWithUpdateUrl(true));
  CreateExtensionFile(cache_dir, kTestExtensionId1, "1");
  prefs.Set(kTestExtensionId2, CreateEntryWithUpdateUrl(true));
  prefs.Set(kTestExtensionId3, CreateEntryWithUpdateUrl(false));
  CreateExtensionFile(cache_dir, kTestExtensionId3, "3");
  prefs.Set(kTestExtensionId4, CreateEntryWithUpdateUrl(false));

  external_cache.UpdateExtensionsList(std::move(prefs));
  content::RunAllTasksUntilIdle();

  ASSERT_TRUE(provided_prefs());
  EXPECT_EQ(provided_prefs()->size(), 2ul);

  // File in cache from Webstore.
  const base::Value::Dict* entry1 =
      provided_prefs()->FindDictByDottedPath(kTestExtensionId1);
  ASSERT_TRUE(entry1);
  EXPECT_EQ(entry1->Find(extensions::ExternalProviderImpl::kExternalUpdateUrl),
            nullptr);
  EXPECT_NE(entry1->Find(extensions::ExternalProviderImpl::kExternalCrx),
            nullptr);
  EXPECT_NE(entry1->Find(extensions::ExternalProviderImpl::kExternalVersion),
            nullptr);
  EXPECT_THAT(
      entry1->FindBool(extensions::ExternalProviderImpl::kIsFromWebstore),
      Optional(true));

  // File in cache not from Webstore.
  const base::Value::Dict* entry3 =
      provided_prefs()->FindDictByDottedPath(kTestExtensionId3);
  ASSERT_TRUE(entry3);
  EXPECT_EQ(entry3->Find(extensions::ExternalProviderImpl::kExternalUpdateUrl),
            nullptr);
  EXPECT_NE(entry3->Find(extensions::ExternalProviderImpl::kExternalCrx),
            nullptr);
  EXPECT_NE(entry3->Find(extensions::ExternalProviderImpl::kExternalVersion),
            nullptr);
  EXPECT_EQ(entry3->Find(extensions::ExternalProviderImpl::kIsFromWebstore),
            nullptr);

  // Update from Webstore.
  base::FilePath temp_dir(CreateTempDir());
  base::FilePath temp_file2 = temp_dir.Append("b.crx");
  CreateFile(temp_file2);
  extensions::CRXFileInfo crx_info_v2(temp_file2,
                                      extensions::GetTestVerifierFormat());
  crx_info_v2.extension_id = kTestExtensionId2;
  crx_info_v2.expected_version = base::Version("2");
  external_cache.OnExtensionDownloadFinished(
      crx_info_v2, true, GURL(),
      extensions::ExtensionDownloaderDelegate::PingResult(), std::set<int>(),
      extensions::ExtensionDownloaderDelegate::InstallCallback());

  content::RunAllTasksUntilIdle();
  EXPECT_EQ(provided_prefs()->size(), 3ul);

  const base::Value::Dict* entry2 =
      provided_prefs()->FindDictByDottedPath(kTestExtensionId2);
  ASSERT_TRUE(entry2);
  EXPECT_EQ(entry2->Find(extensions::ExternalProviderImpl::kExternalUpdateUrl),
            nullptr);
  EXPECT_NE(entry2->Find(extensions::ExternalProviderImpl::kExternalCrx),
            nullptr);
  EXPECT_NE(entry2->Find(extensions::ExternalProviderImpl::kExternalVersion),
            nullptr);
  EXPECT_THAT(
      entry2->FindBool(extensions::ExternalProviderImpl::kIsFromWebstore),
      Optional(true));
  EXPECT_TRUE(
      base::PathExists(GetExtensionFile(cache_dir, kTestExtensionId2, "2")));

  // Update not from Webstore.
  base::FilePath temp_file4 = temp_dir.Append("d.crx");
  CreateFile(temp_file4);
  {
    extensions::CRXFileInfo crx_info_v4(temp_file4,
                                        extensions::GetTestVerifierFormat());
    crx_info_v4.extension_id = kTestExtensionId4;
    crx_info_v4.expected_version = base::Version("4");
    external_cache.OnExtensionDownloadFinished(
        crx_info_v4, true, GURL(),
        extensions::ExtensionDownloaderDelegate::PingResult(), std::set<int>(),
        extensions::ExtensionDownloaderDelegate::InstallCallback());
  }

  content::RunAllTasksUntilIdle();
  EXPECT_EQ(provided_prefs()->size(), 4ul);

  const base::Value::Dict* entry4 =
      provided_prefs()->FindDictByDottedPath(kTestExtensionId4);
  ASSERT_TRUE(entry4);
  EXPECT_EQ(entry4->Find(extensions::ExternalProviderImpl::kExternalUpdateUrl),
            nullptr);
  EXPECT_NE(entry4->Find(extensions::ExternalProviderImpl::kExternalCrx),
            nullptr);
  EXPECT_NE(entry4->Find(extensions::ExternalProviderImpl::kExternalVersion),
            nullptr);
  EXPECT_EQ(entry4->Find(extensions::ExternalProviderImpl::kIsFromWebstore),
            nullptr);
  EXPECT_TRUE(
      base::PathExists(GetExtensionFile(cache_dir, kTestExtensionId4, "4")));

  // Damaged file should be removed from disk.
  EXPECT_EQ(0ul, deleted_extension_files().size());
  external_cache.OnDamagedFileDetected(
      GetExtensionFile(cache_dir, kTestExtensionId2, "2"));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(3ul, provided_prefs()->size());
  EXPECT_FALSE(
      base::PathExists(GetExtensionFile(cache_dir, kTestExtensionId2, "2")));
  EXPECT_EQ(1ul, deleted_extension_files().size());
  EXPECT_EQ(1ul, deleted_extension_files().count(kTestExtensionId2));

  // Shutdown with callback OnExtensionListsUpdated that clears prefs.
  external_cache.Shutdown(
      base::BindOnce(&ExternalCacheImplTest::OnExtensionListsUpdated,
                     base::Unretained(this), base::Value::Dict()));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(provided_prefs()->size(), 0ul);

  // After Shutdown directory shouldn't be touched.
  external_cache.OnDamagedFileDetected(
      GetExtensionFile(cache_dir, kTestExtensionId4, "4"));
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(
      base::PathExists(GetExtensionFile(cache_dir, kTestExtensionId4, "4")));
}

TEST_F(ExternalCacheImplTest, PreserveExternalCrx) {
  base::FilePath cache_dir(CreateCacheDir(false));
  ExternalCacheImpl external_cache(
      cache_dir, url_loader_factory(),
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}), this,
      true, false, false);

  base::Value::Dict prefs;
  prefs.Set(kTestExtensionId1, CreateEntryWithExternalCrx());
  prefs.Set(kTestExtensionId2, CreateEntryWithUpdateUrl(true));

  external_cache.UpdateExtensionsList(std::move(prefs));
  content::RunAllTasksUntilIdle();

  ASSERT_TRUE(provided_prefs());
  EXPECT_EQ(provided_prefs()->size(), 1ul);

  // Extensions downloaded from update url will only be visible in the provided
  // prefs once the download of the .crx has finished. Extensions that are
  // provided as external crx path directly should also be visible in the
  // provided prefs directly.
  const base::Value::Dict* entry1 =
      provided_prefs()->FindDictByDottedPath(kTestExtensionId1);
  ASSERT_TRUE(entry1);
  EXPECT_EQ(entry1->Find(extensions::ExternalProviderImpl::kExternalUpdateUrl),
            nullptr);
  EXPECT_NE(entry1->Find(extensions::ExternalProviderImpl::kExternalCrx),
            nullptr);
  EXPECT_NE(entry1->Find(extensions::ExternalProviderImpl::kExternalVersion),
            nullptr);
}

}  // namespace chromeos
