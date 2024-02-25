// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/paint_preview/services/paint_preview_tab_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "chrome/browser/paint_preview/services/paint_preview_tab_service_file_mixin.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace paint_preview {

namespace {

constexpr char kFeatureName[] = "tab_service_test";

class MockPaintPreviewRecorder : public mojom::PaintPreviewRecorder {
 public:
  MockPaintPreviewRecorder() = default;
  ~MockPaintPreviewRecorder() override = default;

  MockPaintPreviewRecorder(const MockPaintPreviewRecorder&) = delete;
  MockPaintPreviewRecorder& operator=(const MockPaintPreviewRecorder&) = delete;

  void CapturePaintPreview(
      mojom::PaintPreviewCaptureParamsPtr params,
      mojom::PaintPreviewRecorder::CapturePaintPreviewCallback callback)
      override {
    std::move(callback).Run(status_, mojom::PaintPreviewCaptureResponse::New());
  }

  void SetResponse(mojom::PaintPreviewStatus status) { status_ = status; }

  void BindRequest(mojo::ScopedInterfaceEndpointHandle handle) {
    binding_.reset();
    binding_.Bind(mojo::PendingAssociatedReceiver<mojom::PaintPreviewRecorder>(
        std::move(handle)));
  }

 private:
  mojom::PaintPreviewStatus status_;
  mojo::AssociatedReceiver<mojom::PaintPreviewRecorder> binding_{this};
};

std::vector<base::FilePath> ListDir(const base::FilePath& path) {
  base::FileEnumerator enumerator(
      path, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES,
      FILE_PATH_LITERAL("*.skp"));  // Ignore the proto.pb files.
  std::vector<base::FilePath> files;
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    files.push_back(name);
  }
  return files;
}

}  // namespace

class PaintPreviewTabServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  PaintPreviewTabServiceTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}
  ~PaintPreviewTabServiceTest() override = default;

  PaintPreviewTabServiceTest(const PaintPreviewTabServiceTest&) = delete;
  PaintPreviewTabServiceTest& operator=(const PaintPreviewTabServiceTest&) =
      delete;

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://www.example.com/"),
                      ui::PageTransition::PAGE_TRANSITION_FIRST);
    task_environment()->RunUntilIdle();
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    service_ = std::make_unique<PaintPreviewTabService>(
        std::make_unique<PaintPreviewTabServiceFileMixin>(temp_dir_.GetPath(),
                                                          kFeatureName),
        nullptr, false);
    task_environment()->RunUntilIdle();
    EXPECT_TRUE(service_->CacheInitialized());
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  PaintPreviewTabService* GetService() { return service_.get(); }

  void OverrideInterface(MockPaintPreviewRecorder* recorder) {
    blink::AssociatedInterfaceProvider* remote_interfaces =
        web_contents()->GetPrimaryMainFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::PaintPreviewRecorder::Name_,
        base::BindRepeating(&MockPaintPreviewRecorder::BindRequest,
                            base::Unretained(recorder)));
  }

  const base::FilePath& GetPath() const { return temp_dir_.GetPath(); }

  std::unique_ptr<PaintPreviewTabService> BuildServiceWithCache(
      const std::vector<int>& tab_ids) {
    auto path =
        GetPath().AppendASCII("paint_preview").AppendASCII(kFeatureName);
    std::string fake_content = "foobarbaz";

    for (const auto& i : tab_ids) {
      auto key_path = path.AppendASCII(base::NumberToString(i));
      EXPECT_TRUE(base::CreateDirectory(key_path));
      EXPECT_TRUE(
          base::WriteFile(key_path.AppendASCII("proto.pb"), fake_content));
    }

    return std::make_unique<PaintPreviewTabService>(
        std::make_unique<PaintPreviewTabServiceFileMixin>(GetPath(),
                                                          kFeatureName),
        nullptr, false);
  }

 private:
  std::unique_ptr<PaintPreviewTabService> service_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(PaintPreviewTabServiceTest, CaptureTab) {
  const int kTabId = 1U;

  MockPaintPreviewRecorder recorder;
  recorder.SetResponse(mojom::PaintPreviewStatus::kOk);
  OverrideInterface(&recorder);

  auto* service = GetService();
  service->CaptureTab(kTabId, web_contents(), false, 1.0, 10, 20,
                      base::BindOnce([](PaintPreviewTabService::Status status) {
                        EXPECT_EQ(status, PaintPreviewTabService::Status::kOk);
                      }));
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(service->HasCaptureForTab(kTabId));

  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(kTabId);
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  task_environment()->RunUntilIdle();

  service->TabClosed(kTabId);
  EXPECT_FALSE(service->HasCaptureForTab(kTabId));
  task_environment()->RunUntilIdle();

  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_FALSE(exists); }));
  task_environment()->RunUntilIdle();
}

TEST_F(PaintPreviewTabServiceTest, CaptureTabFailed) {
  const int kTabId = 1U;

  MockPaintPreviewRecorder recorder;
  recorder.SetResponse(mojom::PaintPreviewStatus::kFailed);
  OverrideInterface(&recorder);

  auto* service = GetService();
  service->CaptureTab(
      kTabId, web_contents(), false, 1.0, 10, 20,
      base::BindOnce([](PaintPreviewTabService::Status status) {
        EXPECT_EQ(status, PaintPreviewTabService::Status::kCaptureFailed);
      }));
  task_environment()->RunUntilIdle();

  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(kTabId);
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(service->HasCaptureForTab(kTabId));

  service->TabClosed(kTabId);
  task_environment()->RunUntilIdle();
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_FALSE(exists); }));
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(service->HasCaptureForTab(kTabId));
}

TEST_F(PaintPreviewTabServiceTest, CaptureTabTwice) {
  const int kTabId = 1U;

  MockPaintPreviewRecorder recorder;
  recorder.SetResponse(mojom::PaintPreviewStatus::kOk);
  OverrideInterface(&recorder);

  auto* service = GetService();
  service->CaptureTab(kTabId, web_contents(), false, 1.0, 10, 20,
                      base::BindOnce([](PaintPreviewTabService::Status status) {
                        EXPECT_EQ(status, PaintPreviewTabService::Status::kOk);
                      }));
  task_environment()->RunUntilIdle();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(kTabId);
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  task_environment()->RunUntilIdle();
  base::FilePath path_1;
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::CreateOrGetDirectory, file_manager, key,
                     false),
      base::BindOnce(
          [](base::FilePath* out, const std::optional<base::FilePath>& path) {
            EXPECT_TRUE(path.has_value());
            *out = path.value();
          },
          &path_1));
  task_environment()->RunUntilIdle();
  auto files_1 = ListDir(path_1);
  ASSERT_EQ(1U, files_1.size());

  service->CaptureTab(kTabId, web_contents(), false, 1.0, 10, 20,
                      base::BindOnce([](PaintPreviewTabService::Status status) {
                        EXPECT_EQ(status, PaintPreviewTabService::Status::kOk);
                      }));
  task_environment()->RunUntilIdle();

  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  base::FilePath path_2;
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::CreateOrGetDirectory, file_manager, key,
                     false),
      base::BindOnce(
          [](base::FilePath* out, const std::optional<base::FilePath>& path) {
            EXPECT_TRUE(path.has_value());
            *out = path.value();
          },
          &path_2));
  task_environment()->RunUntilIdle();
  EXPECT_EQ(path_2, path_1);
  auto files_2 = ListDir(path_2);
  ASSERT_EQ(1U, files_2.size());
  // The embedding token is used in the filename of the captured SkPicture.
  // Since the embedding token is the same the filenames should also be the
  // same.
  EXPECT_EQ(files_1, files_2);
  EXPECT_TRUE(service->HasCaptureForTab(kTabId));

  service->TabClosed(kTabId);
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_FALSE(exists); }));
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(service->HasCaptureForTab(kTabId));
}

TEST_F(PaintPreviewTabServiceTest, TestUnityAudit) {
  std::vector<int> tab_ids = {1, 2, 3};
  auto service = BuildServiceWithCache(tab_ids);
  auto file_manager = service->GetFileMixin()->GetFileManager();
  task_environment()->RunUntilIdle();

  service->AuditArtifacts(tab_ids);
  task_environment()->RunUntilIdle();

  for (const auto& id : tab_ids) {
    EXPECT_TRUE(service->HasCaptureForTab(id));
    auto key = file_manager->CreateKey(id);
    service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
        base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  }
  task_environment()->RunUntilIdle();
}

TEST_F(PaintPreviewTabServiceTest, TestDisjointAudit) {
  std::vector<int> tab_ids = {1, 2, 3};
  auto service = BuildServiceWithCache(tab_ids);
  auto file_manager = service->GetFileMixin()->GetFileManager();
  task_environment()->RunUntilIdle();

  service->AuditArtifacts({4});
  task_environment()->RunUntilIdle();

  for (const auto& id : tab_ids) {
    EXPECT_FALSE(service->HasCaptureForTab(id));
    auto key = file_manager->CreateKey(id);
    service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
        base::BindOnce([](bool exists) { EXPECT_FALSE(exists); }));
  }
  task_environment()->RunUntilIdle();
}

TEST_F(PaintPreviewTabServiceTest, TestPartialAudit) {
  auto service = BuildServiceWithCache({1, 2, 3});
  auto file_manager = service->GetFileMixin()->GetFileManager();
  task_environment()->RunUntilIdle();

  std::vector<int> kept_tab_ids = {1, 3};
  service->AuditArtifacts(kept_tab_ids);
  task_environment()->RunUntilIdle();

  for (const auto& id : kept_tab_ids) {
    EXPECT_TRUE(service->HasCaptureForTab(id));
    auto key = file_manager->CreateKey(id);
    service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
        base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  }
  EXPECT_FALSE(service->HasCaptureForTab(2));
  auto key = file_manager->CreateKey(2);
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_FALSE(exists); }));
  task_environment()->RunUntilIdle();
}

TEST_F(PaintPreviewTabServiceTest, LoadCache) {
  auto service = BuildServiceWithCache({1, 3, 4});
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(service->CacheInitialized());
  EXPECT_TRUE(service->HasCaptureForTab(1));
  EXPECT_FALSE(service->HasCaptureForTab(2));
  EXPECT_TRUE(service->HasCaptureForTab(3));
  EXPECT_TRUE(service->HasCaptureForTab(4));
  EXPECT_FALSE(service->HasCaptureForTab(5));
}

TEST_F(PaintPreviewTabServiceTest, EarlyDeletion) {
  auto service = BuildServiceWithCache({1, 3});
  // This should queue a deferred deletion so that the cache is in the right
  // state.
  service->TabClosed(1);
  EXPECT_FALSE(service->CacheInitialized());
  EXPECT_FALSE(service->HasCaptureForTab(1));
  task_environment()->RunUntilIdle();
  task_environment()->AdvanceClock(base::Seconds(10));
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(service->CacheInitialized());
  EXPECT_FALSE(service->HasCaptureForTab(1));
  EXPECT_TRUE(service->HasCaptureForTab(3));
}

TEST_F(PaintPreviewTabServiceTest, EarlyAudit) {
  auto service = BuildServiceWithCache({1, 3});
  // This should queue a deferred deletion so that the cache is in the right
  // state.
  service->AuditArtifacts({1, 2, 4});
  EXPECT_FALSE(service->CacheInitialized());
  EXPECT_FALSE(service->HasCaptureForTab(1));
  EXPECT_FALSE(service->HasCaptureForTab(3));
  task_environment()->RunUntilIdle();
  task_environment()->AdvanceClock(base::Seconds(10));
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(service->CacheInitialized());
  EXPECT_TRUE(service->HasCaptureForTab(1));
  EXPECT_FALSE(service->HasCaptureForTab(3));
}

TEST_F(PaintPreviewTabServiceTest, EarlyCapture) {
  const int kTabId = 1U;

  MockPaintPreviewRecorder recorder;
  recorder.SetResponse(mojom::PaintPreviewStatus::kOk);
  OverrideInterface(&recorder);

  auto service = BuildServiceWithCache({});
  service->CaptureTab(kTabId, web_contents(), false, 1.0, 10, 20,
                      base::BindOnce([](PaintPreviewTabService::Status status) {
                        EXPECT_EQ(status, PaintPreviewTabService::Status::kOk);
                      }));
  EXPECT_FALSE(service->HasCaptureForTab(kTabId));
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(service->HasCaptureForTab(kTabId));

  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(kTabId);
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  task_environment()->RunUntilIdle();

  service->TabClosed(kTabId);
  EXPECT_FALSE(service->HasCaptureForTab(kTabId));
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_FALSE(exists); }));
  task_environment()->RunUntilIdle();
}

TEST_F(PaintPreviewTabServiceTest, CaptureTabAndCleanup) {
  const int kTabId = 1U;

  MockPaintPreviewRecorder recorder;
  recorder.SetResponse(mojom::PaintPreviewStatus::kOk);
  OverrideInterface(&recorder);

  auto service = BuildServiceWithCache({kTabId + 1});
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(service->CacheInitialized());
  base::FilePath old_path = GetPath()
                                .AppendASCII("paint_preview")
                                .AppendASCII(kFeatureName)
                                .AppendASCII(base::NumberToString(kTabId + 1));
  // The threshold for cleanup is 25 MB.
  std::string data(25 * 1000 * 1000, 'x');
  EXPECT_TRUE(base::WriteFile(old_path.AppendASCII("foo.txt"), data));
  EXPECT_TRUE(base::PathExists(old_path));
  EXPECT_TRUE(service->HasCaptureForTab(kTabId + 1));

  service->CaptureTab(kTabId, web_contents(), false, 1.0, 10, 20,
                      base::BindOnce([](PaintPreviewTabService::Status status) {
                        EXPECT_EQ(status, PaintPreviewTabService::Status::kOk);
                      }));
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(service->HasCaptureForTab(kTabId));

  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(kTabId);
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(old_path));
  EXPECT_FALSE(service->HasCaptureForTab(kTabId + 1));
  EXPECT_TRUE(service->HasCaptureForTab(kTabId));

  service->TabClosed(kTabId);
  EXPECT_FALSE(service->HasCaptureForTab(kTabId));
  task_environment()->RunUntilIdle();
  service->GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_FALSE(exists); }));
  task_environment()->RunUntilIdle();
}

}  // namespace paint_preview
