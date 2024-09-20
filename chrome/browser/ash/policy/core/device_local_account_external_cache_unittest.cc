// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_local_account_external_cache.h"

#include <initializer_list>
#include <iomanip>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/chromeos/extensions/external_loader/device_local_account_external_policy_loader.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_update_found_test_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using extensions::ExternalInstallInfoFile;
using extensions::ExternalInstallInfoUpdateUrl;
using extensions::mojom::ManifestLocation;
using ::testing::_;
using ::testing::Field;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::StrEq;

namespace chromeos {

namespace {

const char kCacheDir[] = "cache";
const char kExtensionId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char kExtensionUpdateManifest[] =
    "extensions/good_v1_update_manifest.xml";
const char kExtensionCRXVersion[] = "1.0.0.0";
const char kAccountId[] = "test@account.org";

class MockExternalPolicyProviderVisitor
    : public extensions::ExternalProviderInterface::VisitorInterface {
 public:
  MockExternalPolicyProviderVisitor() = default;
  MockExternalPolicyProviderVisitor(const MockExternalPolicyProviderVisitor&) =
      delete;
  MockExternalPolicyProviderVisitor& operator=(
      const MockExternalPolicyProviderVisitor&) = delete;
  ~MockExternalPolicyProviderVisitor() override = default;

  MOCK_METHOD1(OnExternalExtensionFileFound,
               bool(const ExternalInstallInfoFile&));
  MOCK_METHOD2(OnExternalExtensionUpdateUrlFound,
               bool(const ExternalInstallInfoUpdateUrl&, bool));
  MOCK_METHOD1(OnExternalProviderReady,
               void(const extensions::ExternalProviderInterface* provider));
  MOCK_METHOD4(OnExternalProviderUpdateComplete,
               void(const extensions::ExternalProviderInterface*,
                    const std::vector<ExternalInstallInfoUpdateUrl>&,
                    const std::vector<ExternalInstallInfoFile>&,
                    const std::set<std::string>& removed_extensions));
};

// A simple wrapper around a SingleThreadTaskRunner. When a task is posted
// through it, increments a counter which is decremented when the task is run.
class TrackingProxyTaskRunner : public base::SingleThreadTaskRunner {
 public:
  explicit TrackingProxyTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : wrapped_task_runner_(std::move(task_runner)) {}

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ++pending_task_count_;
    return wrapped_task_runner_->PostDelayedTask(
        from_here,
        base::BindOnce(&TrackingProxyTaskRunner::RunTask, this,
                       std::move(task)),
        delay);
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ++pending_task_count_;
    return wrapped_task_runner_->PostNonNestableDelayedTask(
        from_here,
        base::BindOnce(&TrackingProxyTaskRunner::RunTask, this,
                       std::move(task)),
        delay);
  }

  bool RunsTasksInCurrentSequence() const override {
    return wrapped_task_runner_->RunsTasksInCurrentSequence();
  }

  bool has_pending_tasks() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return pending_task_count_ != 0;
  }

 private:
  ~TrackingProxyTaskRunner() override = default;

  void RunTask(base::OnceClosure task) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_GT(pending_task_count_, 0);
    --pending_task_count_;
    std::move(task).Run();
  }

  scoped_refptr<base::SingleThreadTaskRunner> wrapped_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
  int pending_task_count_ = 0;
};

void AddExtensionToDictionary(const std::string& extension_id,
                              const std::string& update_url,
                              base::Value::Dict& dict) {
  auto value = base::Value::Dict().Set(
      extensions::ExternalProviderImpl::kExternalUpdateUrl, update_url);
  dict.Set(extension_id, std::move(value));
}

base::Value::Dict CreateExtensionsDictionary(
    std::initializer_list<std::string> extensions) {
  base::Value::Dict result;

  for (std::string extension_id : extensions) {
    AddExtensionToDictionary(extension_id, "http://download.url", result);
  }
  return result;
}

std::vector<std::string> GetKeys(const base::Value::Dict& dict) {
  std::vector<std::string> keys;
  for (auto [key, _] : dict) {
    keys.push_back(key);
  }
  return keys;
}

}  // namespace

class DeviceLocalAccountExternalCacheTest : public testing::Test {
 protected:
  DeviceLocalAccountExternalCacheTest() = default;
  ~DeviceLocalAccountExternalCacheTest() override = default;

  void SetUp() override;
  void TearDown() override;

  void VerifyAndResetVisitorCallExpectations();
  base::FilePath SimulateExtensionDownload(
      const std::string& id,
      const std::string& manifest_file,
      extensions::ExtensionUpdateFoundTestObserver&
          extension_update_found_observer);

  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP};
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;
  base::FilePath cache_dir_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_loader_factory_{
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)};
  base::FilePath test_dir_;

  scoped_refptr<DeviceLocalAccountExternalPolicyLoader> extension_loader_;
  std::unique_ptr<DeviceLocalAccountExternalCache> external_cache_;
  MockExternalPolicyProviderVisitor visitor_;
  std::unique_ptr<extensions::ExternalProviderImpl> provider_;

  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};

  content::InProcessUtilityThreadHelper in_process_utility_thread_helper_;
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
};

void DeviceLocalAccountExternalCacheTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  cache_dir_ = temp_dir_.GetPath().Append(kCacheDir);
  ASSERT_TRUE(base::CreateDirectoryAndGetError(cache_dir_, /*error=*/nullptr));
  TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
      test_shared_loader_factory_);
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir_));

  ASSERT_TRUE(testing_profile_manager_.SetUp());
  Profile* profile = testing_profile_manager_.CreateTestingProfile("Default");

  extension_loader_ =
      base::MakeRefCounted<chromeos::DeviceLocalAccountExternalPolicyLoader>();

  external_cache_ = std::make_unique<DeviceLocalAccountExternalCache>(
      /*ash_loader=*/
      base::BindRepeating(
          [](scoped_refptr<chromeos::DeviceLocalAccountExternalPolicyLoader>
                 loader,
             const std::string&, base::Value::Dict cached_extensions) {
            loader->OnExtensionListsUpdated(cached_extensions);
          },
          extension_loader_),
      /*lacros_loader=*/
      base::DoNothing(), kAccountId, cache_dir_);
  provider_ = std::make_unique<extensions::ExternalProviderImpl>(
      &visitor_, extension_loader_, profile, ManifestLocation::kExternalPolicy,
      ManifestLocation::kExternalPolicyDownload,
      extensions::Extension::NO_FLAGS);

  VerifyAndResetVisitorCallExpectations();
}

void DeviceLocalAccountExternalCacheTest::TearDown() {
  testing_profile_manager_.DeleteAllTestingProfiles();
  TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(nullptr);
}

void DeviceLocalAccountExternalCacheTest::
    VerifyAndResetVisitorCallExpectations() {
  Mock::VerifyAndClearExpectations(&visitor_);
  EXPECT_CALL(visitor_, OnExternalExtensionFileFound(_)).Times(0);
  EXPECT_CALL(visitor_, OnExternalExtensionUpdateUrlFound(_, _)).Times(0);
  EXPECT_CALL(visitor_, OnExternalProviderReady(_)).Times(0);
  EXPECT_CALL(visitor_, OnExternalProviderUpdateComplete(_, _, _, _)).Times(0);
}

base::FilePath DeviceLocalAccountExternalCacheTest::SimulateExtensionDownload(
    const std::string& id,
    const std::string& manifest_file,
    extensions::ExtensionUpdateFoundTestObserver&
        extension_update_found_observer) {
  // Return a manifest to the downloader.
  std::string manifest;
  EXPECT_TRUE(
      base::ReadFileToString(test_dir_.Append(manifest_file), &manifest));

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.AddResponse(pending_request->request.url.spec(),
                                       manifest);

  // Wait for the manifest to be parsed.
  extension_update_found_observer.Wait();

  // Verify that the downloader is attempting to download a CRX file.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  // Trigger downloading of the temporary CRX file.
  pending_request = test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.AddResponse(pending_request->request.url.spec(),
                                       "Content is irrelevant.");

  return cache_dir_.Append(
      base::StringPrintf("%s-%s.crx", id.c_str(), kExtensionCRXVersion));
}

// Verifies that when the cache is not explicitly started, the loader does not
// serve any extensions, even if the force-install list policy is set or a load
// is manually requested.
TEST_F(DeviceLocalAccountExternalCacheTest, CacheNotStarted) {
  // Manually request a load.
  extension_loader_->StartLoading();

  EXPECT_FALSE(external_cache_->IsCacheRunning());
}

// Verifies that the cache can be started and stopped correctly.
TEST_F(DeviceLocalAccountExternalCacheTest, ForceInstallListEmpty) {
  // Start the cache. Verify that the loader announces an empty extension list.
  EXPECT_CALL(visitor_, OnExternalProviderReady(provider_.get())).Times(1);
  external_cache_->StartCache(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  external_cache_->UpdateExtensionsList(base::Value::Dict(),
                                        base::Value::Dict());
  base::RunLoop().RunUntilIdle();
  VerifyAndResetVisitorCallExpectations();

  // Stop the cache. Verify that the loader announces an empty extension list.
  EXPECT_CALL(visitor_, OnExternalProviderReady(provider_.get())).Times(1);
  base::RunLoop run_loop;
  external_cache_->StopCache(run_loop.QuitClosure());
  VerifyAndResetVisitorCallExpectations();

  // Spin the loop until the cache shutdown callback is invoked. Verify that at
  // that point, no further file I/O tasks are pending.
  run_loop.Run();
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

// Verifies that when a force-install list policy referencing an extension is
// set and the cache is started, the loader downloads, caches and serves the
// extension.
TEST_F(DeviceLocalAccountExternalCacheTest, ForceInstallListSet) {
  extensions::ExtensionUpdateFoundTestObserver extension_update_found_observer;
  base::Value::Dict dict;
  AddExtensionToDictionary(kExtensionId,
                           extension_urls::GetWebstoreUpdateUrl().spec(), dict);

  // Start the cache.
  auto cache_task_runner = base::MakeRefCounted<TrackingProxyTaskRunner>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  external_cache_->StartCache(cache_task_runner);
  external_cache_->UpdateExtensionsList(std::move(dict), base::Value::Dict());

  // Spin the loop, allowing the loader to process the force-install list.
  // Verify that the loader announces an empty extension list.
  EXPECT_CALL(visitor_, OnExternalProviderReady(provider_.get())).Times(1);
  base::RunLoop().RunUntilIdle();

  // Verify that a downloader has started and is attempting to download an
  // update manifest.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  const base::FilePath cached_crx_path = SimulateExtensionDownload(
      kExtensionId, kExtensionUpdateManifest, extension_update_found_observer);

  base::RunLoop cache_run_loop;
  EXPECT_CALL(
      visitor_,
      OnExternalExtensionFileFound(AllOf(
          Field(&extensions::ExternalInstallInfoFile::extension_id,
                StrEq(kExtensionId)),
          Field(&extensions::ExternalInstallInfoFile::path, cached_crx_path),
          Field(&extensions::ExternalInstallInfoFile::crx_location,
                ManifestLocation::kExternalPolicy))));
  EXPECT_CALL(visitor_, OnExternalProviderReady(provider_.get()))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(&cache_run_loop, &base::RunLoop::Quit));
  cache_run_loop.Run();
  VerifyAndResetVisitorCallExpectations();

  // Stop the cache. Verify that the loader announces an empty extension list.
  EXPECT_CALL(visitor_, OnExternalProviderReady(provider_.get())).Times(1);
  base::RunLoop shutdown_run_loop;
  external_cache_->StopCache(shutdown_run_loop.QuitClosure());
  VerifyAndResetVisitorCallExpectations();

  // Spin the loop until the cache shutdown callback is invoked. Verify that at
  // that point, no further file I/O tasks are pending.
  shutdown_run_loop.Run();
  EXPECT_FALSE(cache_task_runner->has_pending_tasks());
}

TEST_F(DeviceLocalAccountExternalCacheTest,
       ShouldSeparateAshAndLacrosExtensions) {
  base::Value::Dict ash_extension_prefs =
      CreateExtensionsDictionary({"ash-extension", "shared-extension"});
  base::Value::Dict lacros_extension_prefs =
      CreateExtensionsDictionary({"lacros-extension", "shared-extension"});

  base::test::TestFuture<const std::string&, base::Value::Dict> ash_loader;
  base::test::TestFuture<const std::string&, base::Value::Dict> lacros_loader;

  DeviceLocalAccountExternalCache cache{ash_loader.GetRepeatingCallback(),
                                        lacros_loader.GetRepeatingCallback(),
                                        "<the-user-id>", cache_dir_};

  cache.UpdateExtensionsList(ash_extension_prefs.Clone(),
                             lacros_extension_prefs.Clone());

  // Pretend the extensions have been downloaded by the cache.
  cache.SetCacheResponseForTesting(CreateExtensionsDictionary(
      {"ash-extension", "lacros-extension", "shared-extension"}));

  auto [user_id_sent_to_ash, extensions_sent_to_ash] = ash_loader.Take();
  EXPECT_EQ(user_id_sent_to_ash, "<the-user-id>");
  EXPECT_THAT(
      GetKeys(extensions_sent_to_ash),
      ::testing::UnorderedElementsAre("ash-extension", "shared-extension"));

  auto [user_id_sent_to_lacros, extensions_sent_to_lacros] =
      lacros_loader.Take();
  EXPECT_EQ(user_id_sent_to_lacros, "<the-user-id>");
  EXPECT_THAT(
      GetKeys(extensions_sent_to_lacros),
      ::testing::UnorderedElementsAre("lacros-extension", "shared-extension"));
}

}  // namespace chromeos
