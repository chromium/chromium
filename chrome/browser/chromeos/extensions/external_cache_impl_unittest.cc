// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/external_cache_impl.h"

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/chromeos/extensions/external_cache_delegate.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/verifier_formats.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestExtensionId1[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestExtensionId2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const char kTestExtensionId3[] = "cccccccccccccccccccccccccccccccc";
const char kTestExtensionId4[] = "dddddddddddddddddddddddddddddddd";
const char kNonWebstoreUpdateUrl[] = "https://localhost/service/update2/crx";
const char kExternalCrxPath[] = "/local/path/to/extension.crx";
const char kExternalCrxVersion[] = "1.2.3.4";

}  // namespace

namespace chromeos {

class ExternalCacheImplTest : public testing::Test,
                              public ExternalCacheDelegate {
 public:
  ExternalCacheImplTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}
  ~ExternalCacheImplTest() override = default;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() {
    return test_shared_loader_factory_;
  }

  const base::DictionaryValue* provided_prefs() { return prefs_.get(); }
  const std::set<extensions::ExtensionId>& deleted_extension_files() const {
    return deleted_extension_files_;
  }

  // ExternalCacheDelegate:
  void OnExtensionListsUpdated(const base::DictionaryValue* prefs) override {
    prefs_.reset(prefs->DeepCopy());
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
    EXPECT_EQ(base::WriteFile(file, NULL, 0), 0);
  }

  base::FilePath GetExtensionFile(const base::FilePath& dir,
                                  const std::string& id,
                                  const std::string& version) {
    return dir.Append(id + "-" + version + ".crx");
  }

  std::unique_ptr<base::DictionaryValue> CreateEntryWithUpdateUrl(
      bool from_webstore) {
    auto entry = std::make_unique<base::DictionaryValue>();
    entry->SetString(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                     from_webstore
                         ? extension_urls::GetWebstoreUpdateUrl().spec()
                         : kNonWebstoreUpdateUrl);
    return entry;
  }

  std::unique_ptr<base::DictionaryValue> CreateEntryWithExternalCrx() {
    auto entry = std::make_unique<base::DictionaryValue>();
    entry->SetString(extensions::ExternalProviderImpl::kExternalCrx,
                     kExternalCrxPath);
    entry->SetString(extensions::ExternalProviderImpl::kExternalVersion,
                     kExternalCrxVersion);
    return entry;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  base::ScopedTempDir cache_dir_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::DictionaryValue> prefs_;
  std::set<extensions::ExtensionId> deleted_extension_files_;

  ScopedCrosSettingsTestHelper cros_settings_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(ExternalCacheImplTest);
};

TEST_F(ExternalCacheImplTest, Basic) {
  base::FilePath cache_dir(CreateCacheDir(false));
  ExternalCacheImpl external_cache(
      cache_dir, url_loader_factory(),
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}), this,
      true, false);

  std::unique_ptr<base::DictionaryValue> prefs(new base::DictionaryValue);
  prefs->Set(kTestExtensionId1, CreateEntryWithUpdateUrl(true));
  CreateExtensionFile(cache_dir, kTestExtensionId1, "1");
  prefs->Set(kTestExtensionId2, CreateEntryWithUpdateUrl(true));
  prefs->Set(kTestExtensionId3, CreateEntryWithUpdateUrl(false));
  CreateExtensionFile(cache_dir, kTestExtensionId3, "3");
  prefs->Set(kTestExtensionId4, CreateEntryWithUpdateUrl(false));

  external_cache.UpdateExtensionsList(std::move(prefs));
  content::RunAllTasksUntilIdle();

  ASSERT_TRUE(provided_prefs());
  EXPECT_EQ(provided_prefs()->size(), 2ul);

  // File in cache from Webstore.
  const base::DictionaryValue* entry1 = NULL;
  ASSERT_TRUE(provided_prefs()->GetDictionary(kTestExtensionId1, &entry1));
  EXPECT_FALSE(
      entry1->HasKey(extensions::ExternalProviderImpl::kExternalUpdateUrl));
  EXPECT_TRUE(entry1->HasKey(extensions::ExternalProviderImpl::kExternalCrx));
  EXPECT_TRUE(
      entry1->HasKey(extensions::ExternalProviderImpl::kExternalVersion));
  bool from_webstore = false;
  EXPECT_TRUE(entry1->GetBoolean(
      extensions::ExternalProviderImpl::kIsFromWebstore, &from_webstore));
  EXPECT_TRUE(from_webstore);

  // File in cache not from Webstore.
  const base::DictionaryValue* entry3 = NULL;
  ASSERT_TRUE(provided_prefs()->GetDictionary(kTestExtensionId3, &entry3));
  EXPECT_FALSE(
      entry3->HasKey(extensions::ExternalProviderImpl::kExternalUpdateUrl));
  EXPECT_TRUE(entry3->HasKey(extensions::ExternalProviderImpl::kExternalCrx));
  EXPECT_TRUE(
      entry3->HasKey(extensions::ExternalProviderImpl::kExternalVersion));
  EXPECT_FALSE(
      entry3->HasKey(extensions::ExternalProviderImpl::kIsFromWebstore));

  // Update from Webstore.
  base::FilePath temp_dir(CreateTempDir());
  base::FilePath temp_file2 = temp_dir.Append("b.crx");
  CreateFile(temp_file2);
  extensions::CRXFileInfo crx_info(temp_file2,
                                   extensions::GetTestVerifierFormat());
  crx_info.extension_id = kTestExtensionId2;
  crx_info.expected_version = base::Version("2");
  external_cache.OnExtensionDownloadFinished(
      crx_info, true, GURL(),
      extensions::ExtensionDownloaderDelegate::PingResult(), std::set<int>(),
      extensions::ExtensionDownloaderDelegate::InstallCallback());

  content::RunAllTasksUntilIdle();
  EXPECT_EQ(provided_prefs()->size(), 3ul);

  const base::DictionaryValue* entry2 = NULL;
  ASSERT_TRUE(provided_prefs()->GetDictionary(kTestExtensionId2, &entry2));
  EXPECT_FALSE(
      entry2->HasKey(extensions::ExternalProviderImpl::kExternalUpdateUrl));
  EXPECT_TRUE(entry2->HasKey(extensions::ExternalProviderImpl::kExternalCrx));
  EXPECT_TRUE(
      entry2->HasKey(extensions::ExternalProviderImpl::kExternalVersion));
  from_webstore = false;
  EXPECT_TRUE(entry2->GetBoolean(
      extensions::ExternalProviderImpl::kIsFromWebstore, &from_webstore));
  EXPECT_TRUE(from_webstore);
  EXPECT_TRUE(
      base::PathExists(GetExtensionFile(cache_dir, kTestExtensionId2, "2")));

  // Update not from Webstore.
  base::FilePath temp_file4 = temp_dir.Append("d.crx");
  CreateFile(temp_file4);
  {
    extensions::CRXFileInfo crx_info(temp_file4,
                                     extensions::GetTestVerifierFormat());
    crx_info.extension_id = kTestExtensionId4;
    crx_info.expected_version = base::Version("4");
    external_cache.OnExtensionDownloadFinished(
        crx_info, true, GURL(),
        extensions::ExtensionDownloaderDelegate::PingResult(), std::set<int>(),
        extensions::ExtensionDownloaderDelegate::InstallCallback());
  }

  content::RunAllTasksUntilIdle();
  EXPECT_EQ(provided_prefs()->size(), 4ul);

  const base::DictionaryValue* entry4 = NULL;
  ASSERT_TRUE(provided_prefs()->GetDictionary(kTestExtensionId4, &entry4));
  EXPECT_FALSE(
      entry4->HasKey(extensions::ExternalProviderImpl::kExternalUpdateUrl));
  EXPECT_TRUE(entry4->HasKey(extensions::ExternalProviderImpl::kExternalCrx));
  EXPECT_TRUE(
      entry4->HasKey(extensions::ExternalProviderImpl::kExternalVersion));
  EXPECT_FALSE(
      entry4->HasKey(extensions::ExternalProviderImpl::kIsFromWebstore));
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
  std::unique_ptr<base::DictionaryValue> empty(new base::DictionaryValue);
  external_cache.Shutdown(
      base::BindOnce(&ExternalCacheImplTest::OnExtensionListsUpdated,
                     base::Unretained(this), base::Unretained(empty.get())));
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
      true, false);

  std::unique_ptr<base::DictionaryValue> prefs(new base::DictionaryValue);
  prefs->Set(kTestExtensionId1, CreateEntryWithExternalCrx());
  prefs->Set(kTestExtensionId2, CreateEntryWithUpdateUrl(true));

  external_cache.UpdateExtensionsList(std::move(prefs));
  content::RunAllTasksUntilIdle();

  ASSERT_TRUE(provided_prefs());
  EXPECT_EQ(provided_prefs()->size(), 1ul);

  // Extensions downloaded from update url will only be visible in the provided
  // prefs once the download of the .crx has finished. Extensions that are
  // provided as external crx path directly should also be visible in the
  // provided prefs directly.
  const base::DictionaryValue* entry1 = NULL;
  ASSERT_TRUE(provided_prefs()->GetDictionary(kTestExtensionId1, &entry1));
  EXPECT_FALSE(
      entry1->HasKey(extensions::ExternalProviderImpl::kExternalUpdateUrl));
  EXPECT_TRUE(entry1->HasKey(extensions::ExternalProviderImpl::kExternalCrx));
  EXPECT_TRUE(
      entry1->HasKey(extensions::ExternalProviderImpl::kExternalVersion));
}

}  // namespace chromeos
