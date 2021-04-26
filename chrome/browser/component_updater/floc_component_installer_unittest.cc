// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/floc_component_installer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/federated_learning/floc_constants.h"
#include "components/federated_learning/floc_sorting_lsh_clusters_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock-actions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace component_updater {

namespace {

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
  return true;
}

// This class monitors the OnSortingLshClustersFileReady method calls.
class MockFlocSortingLshClustersService
    : public federated_learning::FlocSortingLshClustersService {
 public:
  MockFlocSortingLshClustersService() = default;

  MockFlocSortingLshClustersService(const MockFlocSortingLshClustersService&) =
      delete;
  MockFlocSortingLshClustersService& operator=(
      const MockFlocSortingLshClustersService&) = delete;

  ~MockFlocSortingLshClustersService() override = default;

  void OnSortingLshClustersFileReady(const base::FilePath& file_path,
                                     const base::Version& version) override {
    file_paths_.push_back(file_path);
    versions_.push_back(version);
  }

  const std::vector<base::FilePath>& file_paths() const { return file_paths_; }
  const std::vector<base::Version>& versions() const { return versions_; }

 private:
  std::vector<base::FilePath> file_paths_;
  std::vector<base::Version> versions_;
};

}  //  namespace

class FlocComponentInstallerTest : public PlatformTest {
 public:
  FlocComponentInstallerTest() = default;

  FlocComponentInstallerTest(const FlocComponentInstallerTest&) = delete;
  FlocComponentInstallerTest& operator=(const FlocComponentInstallerTest&) =
      delete;

  ~FlocComponentInstallerTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());

    auto test_floc_sorting_lsh_clusters_service =
        std::make_unique<MockFlocSortingLshClustersService>();
    test_floc_sorting_lsh_clusters_service->SetBackgroundTaskRunnerForTesting(
        base::SequencedTaskRunnerHandle::Get());

    test_floc_sorting_lsh_clusters_service_ =
        test_floc_sorting_lsh_clusters_service.get();

    TestingBrowserProcess::GetGlobal()->SetFlocSortingLshClustersService(
        std::move(test_floc_sorting_lsh_clusters_service));

    policy_ = std::make_unique<FlocComponentInstallerPolicy>(
        test_floc_sorting_lsh_clusters_service_);
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetFlocSortingLshClustersService(
        nullptr);
    PlatformTest::TearDown();
  }

  MockFlocSortingLshClustersService* sorting_lsh_clusters_service() {
    return test_floc_sorting_lsh_clusters_service_;
  }

  void WriteStringToFile(const std::string& data, const base::FilePath& path) {
    ASSERT_EQ(base::WriteFile(path, data.data(), data.length()),
              static_cast<int32_t>(data.length()));
  }

  base::FilePath component_install_dir() {
    return component_install_dir_.GetPath();
  }

  void CreateTestFlocComponentFiles(const std::string& sorting_lsh_content) {
    base::FilePath sorting_lsh_file_path = component_install_dir().Append(
        federated_learning::kSortingLshClustersFileName);
    ASSERT_NO_FATAL_FAILURE(
        WriteStringToFile(sorting_lsh_content, sorting_lsh_file_path));
  }

  void LoadFlocComponent(const std::string& content_version,
                         int format_version) {
    auto manifest = std::make_unique<base::DictionaryValue>();
    manifest->SetInteger(federated_learning::kManifestFlocComponentFormatKey,
                         format_version);

    if (!policy_->VerifyInstallation(*manifest, component_install_dir()))
      return;

    policy_->ComponentReady(base::Version(content_version),
                            component_install_dir(), std::move(manifest));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir component_install_dir_;
  std::unique_ptr<FlocComponentInstallerPolicy> policy_;
  MockFlocSortingLshClustersService* test_floc_sorting_lsh_clusters_service_ =
      nullptr;
};

TEST_F(FlocComponentInstallerTest, TestComponentRegistration) {
  auto component_updater =
      std::make_unique<component_updater::MockComponentUpdateService>();

  base::RunLoop run_loop;
  EXPECT_CALL(*component_updater, RegisterComponent(testing::_))
      .Times(1)
      .WillOnce(QuitMessageLoop(&run_loop));

  RegisterFlocComponent(component_updater.get(),
                        sorting_lsh_clusters_service());
  run_loop.Run();
}

TEST_F(FlocComponentInstallerTest, LoadFlocComponent) {
  ASSERT_TRUE(sorting_lsh_clusters_service());

  std::string contents = "abcd";
  ASSERT_NO_FATAL_FAILURE(CreateTestFlocComponentFiles(contents));
  ASSERT_NO_FATAL_FAILURE(LoadFlocComponent(
      "1.0.1", federated_learning::kCurrentFlocComponentFormatVersion));

  ASSERT_EQ(sorting_lsh_clusters_service()->file_paths().size(), 1u);
  ASSERT_EQ(sorting_lsh_clusters_service()->versions().size(), 1u);

  // Assert that the file path is the concatenation of |component_install_dir_|
  // and the corresponding file name, which implies that the |version| argument
  // has no impact. In reality, though, the |component_install_dir_| and the
  // |version| should always match.
  ASSERT_EQ(sorting_lsh_clusters_service()->file_paths()[0].AsUTF8Unsafe(),
            component_install_dir()
                .Append(federated_learning::kSortingLshClustersFileName)
                .AsUTF8Unsafe());

  EXPECT_EQ(sorting_lsh_clusters_service()->versions()[0].GetString(), "1.0.1");

  std::string actual_contents;
  ASSERT_TRUE(base::ReadFileToString(
      sorting_lsh_clusters_service()->file_paths()[0], &actual_contents));
  EXPECT_EQ(actual_contents, contents);
}

TEST_F(FlocComponentInstallerTest, UnsupportedFormatVersionIgnored) {
  ASSERT_TRUE(sorting_lsh_clusters_service());
  const std::string contents = "future stuff";
  ASSERT_NO_FATAL_FAILURE(CreateTestFlocComponentFiles(contents));
  ASSERT_NO_FATAL_FAILURE(LoadFlocComponent(
      "1.0.0", federated_learning::kCurrentFlocComponentFormatVersion + 1));
  EXPECT_EQ(sorting_lsh_clusters_service()->file_paths().size(), 0u);
}

}  // namespace component_updater
