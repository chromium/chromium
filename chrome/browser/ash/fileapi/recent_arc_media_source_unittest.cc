// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/fileapi/recent_arc_media_source.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/components/arc/mojom/file_system.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_file_system_instance.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_mounter.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/ash/arc/fileapi/arc_media_view_util.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chrome/browser/ash/fileapi/recent_source.h"
#include "chrome/browser/ash/fileapi/test/recent_file_matcher.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

std::unique_ptr<KeyedService> CreateFileSystemOperationRunnerForTesting(
    content::BrowserContext* context) {
  return arc::ArcFileSystemOperationRunner::CreateForTesting(
      context, arc::ArcServiceManager::Get()->arc_bridge_service());
}

base::FilePath GetPathForRoot(const std::string& root,
                              const std::string& name) {
  return arc::GetDocumentsProviderMountPath(
             arc::kMediaDocumentsProviderAuthority, root)
      .Append(name);
}

base::FilePath GetDocumentPath(const std::string& name) {
  return GetPathForRoot(arc::kDocumentsRootId, name);
}

base::FilePath GetImagePath(const std::string& name) {
  return GetPathForRoot(arc::kImagesRootId, name);
}

base::FilePath GetVideoPath(const std::string& name) {
  return GetPathForRoot(arc::kVideosRootId, name);
}

base::Time ModifiedTime(int64_t millis) {
  return base::Time::FromMillisecondsSinceUnixEpoch(millis);
}

arc::FakeFileSystemInstance::Document MakeDocument(
    const std::string& document_id,
    const std::string& parent_document_id,
    const std::string& display_name,
    const std::string& mime_type,
    const base::Time& last_modified) {
  return arc::FakeFileSystemInstance::Document(
      arc::kMediaDocumentsProviderAuthority,          // authority
      document_id,                                    // document_id
      parent_document_id,                             // parent_document_id
      display_name,                                   // display_name
      mime_type,                                      // mime_type
      0,                                              // size
      last_modified.InMillisecondsSinceUnixEpoch());  // last_modified
}

}  // namespace

class RecentArcMediaSourceTest : public testing::Test {
 public:
  RecentArcMediaSourceTest() = default;

  void SetUp() override {
    arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
    profile_ = std::make_unique<TestingProfile>();
    arc_service_manager_->set_browser_context(profile_.get());
    runner_ = static_cast<arc::ArcFileSystemOperationRunner*>(
        arc::ArcFileSystemOperationRunner::GetFactory()
            ->SetTestingFactoryAndUse(
                profile_.get(),
                base::BindRepeating(
                    &CreateFileSystemOperationRunnerForTesting)));

    // Mount ARC file systems.
    arc::ArcFileSystemMounter::GetForBrowserContext(profile_.get());

    // Add documents to FakeFileSystemInstance. Note that they are not available
    // until EnableFakeFileSystemInstance() is called.
    AddDocumentsToFakeFileSystemInstance();
  }

  void TearDown() override {
    arc_service_manager_->arc_bridge_service()->file_system()->CloseInstance(
        &fake_file_system_);
    arc_service_manager_->set_browser_context(nullptr);
  }

 protected:
  void AddDocumentsToFakeFileSystemInstance() {
    auto images_root_doc =
        MakeDocument(arc::kImagesRootId, "", "", arc::kAndroidDirectoryMimeType,
                     base::Time::FromMillisecondsSinceUnixEpoch(1));
    auto cat_doc = MakeDocument("cat", arc::kImagesRootId, "cat.png",
                                "image/png", ModifiedTime(2));
    auto dog_doc = MakeDocument("dog", arc::kImagesRootId, "dog.jpg",
                                "image/jpeg", ModifiedTime(3));
    auto fox_doc = MakeDocument("fox", arc::kImagesRootId, "fox.gif",
                                "image/gif", ModifiedTime(4));
    auto elk_doc = MakeDocument("elk", arc::kImagesRootId, "elk.tiff",
                                "image/tiff", ModifiedTime(5));
    auto audio_root_doc =
        MakeDocument(arc::kAudioRootId, "", "", arc::kAndroidDirectoryMimeType,
                     ModifiedTime(1));
    auto god_doc = MakeDocument("god", arc::kAudioRootId, "god.mp3",
                                "audio/mp3", ModifiedTime(6));
    auto videos_root_doc =
        MakeDocument(arc::kVideosRootId, "", "", arc::kAndroidDirectoryMimeType,
                     ModifiedTime(1));
    auto hot_doc = MakeDocument("hot", arc::kVideosRootId, "hot.mp4",
                                "video/mp4", ModifiedTime(7));
    auto ink_doc = MakeDocument("ink", arc::kVideosRootId, "ink.webm",
                                "video/webm", ModifiedTime(8));
    auto documents_root_doc =
        MakeDocument(arc::kDocumentsRootId, "", "",
                     arc::kAndroidDirectoryMimeType, ModifiedTime(1));
    auto word_doc = MakeDocument("word", arc::kDocumentsRootId, "word.doc",
                                 "application/msword", ModifiedTime(9));
    auto text_doc = MakeDocument("text", arc::kDocumentsRootId, "text.txt",
                                 "text/plain", ModifiedTime(10));

    fake_file_system_.AddDocument(images_root_doc);
    fake_file_system_.AddDocument(cat_doc);
    fake_file_system_.AddDocument(dog_doc);
    fake_file_system_.AddDocument(fox_doc);
    fake_file_system_.AddDocument(audio_root_doc);
    fake_file_system_.AddDocument(god_doc);
    fake_file_system_.AddDocument(videos_root_doc);
    fake_file_system_.AddDocument(hot_doc);
    fake_file_system_.AddDocument(ink_doc);
    fake_file_system_.AddDocument(documents_root_doc);
    fake_file_system_.AddDocument(word_doc);
    fake_file_system_.AddDocument(text_doc);

    fake_file_system_.AddRecentDocument(arc::kImagesRootId, images_root_doc);
    fake_file_system_.AddRecentDocument(arc::kImagesRootId, cat_doc);
    fake_file_system_.AddRecentDocument(arc::kImagesRootId, dog_doc);
    fake_file_system_.AddRecentDocument(arc::kImagesRootId, elk_doc);
    fake_file_system_.AddRecentDocument(arc::kAudioRootId, audio_root_doc);
    fake_file_system_.AddRecentDocument(arc::kAudioRootId, god_doc);
    fake_file_system_.AddRecentDocument(arc::kVideosRootId, videos_root_doc);
    fake_file_system_.AddRecentDocument(arc::kVideosRootId, hot_doc);
    fake_file_system_.AddRecentDocument(arc::kVideosRootId, ink_doc);
    fake_file_system_.AddRecentDocument(arc::kDocumentsRootId,
                                        documents_root_doc);
    fake_file_system_.AddRecentDocument(arc::kDocumentsRootId, word_doc);
    fake_file_system_.AddRecentDocument(arc::kDocumentsRootId, text_doc);
  }

  void EnableFakeFileSystemInstance() {
    arc_service_manager_->arc_bridge_service()->file_system()->SetInstance(
        &fake_file_system_);
    arc::WaitForInstanceReady(
        arc_service_manager_->arc_bridge_service()->file_system());
  }

  // Fetches files matching the given query and specified `file_type` from
  // the `source_`. If the `root_lag` has value it can be used to cause one
  // of the roots (documents, images, videos) to experience an artificial lag.
  // If the lag is specified, it must be greater than 100ms.
  std::vector<RecentFile> GetRecentFiles(
      RecentArcMediaSource* source,
      const std::string& query,
      RecentSource::FileType file_type = RecentSource::FileType::kAll,
      std::optional<std::pair<const char*, base::TimeDelta>> root_lag = {},
      size_t max_files = 10) {
    std::vector<RecentFile> files;
    base::RunLoop run_loop;
    base::OneShotTimer timer;

    const int32_t call_id = 0;
    if (root_lag.has_value()) {
      auto [root_id, lag] = root_lag.value();
      base::TimeDelta stop_delta = lag - base::Milliseconds(100);
      source->SetLagForTesting(lag);
      EXPECT_TRUE(stop_delta.is_positive());
      timer.Start(FROM_HERE, stop_delta, base::BindLambdaForTesting([&]() {
                    files = source->Stop(call_id);
                    run_loop.Quit();
                  }));
    }

    source->GetRecentFiles(
        RecentSource::Params(
            /*file_system_context=*/nullptr, call_id,
            /*origin=*/GURL(), query,
            /*max_files=*/max_files,
            /*cutoff_time=*/base::Time(),
            /*end_time=*/base::TimeTicks::Max(),
            /*file_type=*/file_type),
        base::BindOnce(
            [](base::RunLoop* run_loop, std::vector<RecentFile>* out_files,
               std::vector<RecentFile> files) {
              *out_files = std::move(files);
              run_loop->Quit();
            },
            &run_loop, &files));

    run_loop.Run();

    return files;
  }

  void EnableDefer() { runner_->SetShouldDefer(true); }

  // NOTE: In local run real time was used. However, that causes the test to run
  // for over 6s. With MOCK_TIME, this is reduced to 3s with no difference to
  // test outcomes (UmaStats taking the longest).
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::REAL_IO_THREAD,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  arc::FakeFileSystemInstance fake_file_system_;

  // Use the same initialization/destruction order as
  // `ChromeBrowserMainPartsAsh`.
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestingProfile> profile_;

  raw_ptr<arc::ArcFileSystemOperationRunner> runner_;
};

TEST_F(RecentArcMediaSourceTest, Normal) {
  EnableFakeFileSystemInstance();

  auto doc_source = std::make_unique<RecentArcMediaSource>(
      profile_.get(), arc::kDocumentsRootId);
  std::vector<RecentFile> doc_files = GetRecentFiles(doc_source.get(), "");

  ASSERT_EQ(2u, doc_files.size());
  EXPECT_THAT(doc_files[0],
              IsRecentFile(GetDocumentPath("text.txt"), ModifiedTime(10)));
  EXPECT_THAT(doc_files[1],
              IsRecentFile(GetDocumentPath("word.doc"), ModifiedTime(9)));

  auto video_source = std::make_unique<RecentArcMediaSource>(
      profile_.get(), arc::kVideosRootId);
  std::vector<RecentFile> video_files = GetRecentFiles(video_source.get(), "");

  ASSERT_EQ(2u, video_files.size());
  EXPECT_THAT(video_files[0],
              IsRecentFile(GetVideoPath("ink.webm"), ModifiedTime(8)));
  EXPECT_THAT(video_files[1],
              IsRecentFile(GetVideoPath("hot.mp4"), ModifiedTime(7)));

  auto image_source = std::make_unique<RecentArcMediaSource>(
      profile_.get(), arc::kImagesRootId);
  std::vector<RecentFile> image_files = GetRecentFiles(image_source.get(), "");

  EXPECT_THAT(image_files[0],
              IsRecentFile(GetImagePath("dog.jpg"), ModifiedTime(3)));
  EXPECT_THAT(image_files[1],
              IsRecentFile(GetImagePath("cat.png"), ModifiedTime(2)));

  doc_files = GetRecentFiles(doc_source.get(), "text");
  ASSERT_EQ(1u, doc_files.size());
  EXPECT_THAT(doc_files[0],
              IsRecentFile(GetDocumentPath("text.txt"), ModifiedTime(10)));
  ASSERT_EQ(0u, GetRecentFiles(video_source.get(), "text").size());
  ASSERT_EQ(0u, GetRecentFiles(image_source.get(), "text").size());
}

TEST_F(RecentArcMediaSourceTest, ArcNotAvailable) {
  // By not enabling fake file system instance we make Arc unavailable.
  auto doc_source = std::make_unique<RecentArcMediaSource>(
      profile_.get(), arc::kDocumentsRootId);
  std::vector<RecentFile> files = GetRecentFiles(doc_source.get(), "");
  EXPECT_EQ(0u, files.size());

  files = GetRecentFiles(doc_source.get(), "hot");
  EXPECT_EQ(0u, files.size());
}

TEST_F(RecentArcMediaSourceTest, Deferred) {
  EnableFakeFileSystemInstance();
  EnableDefer();

  auto doc_source = std::make_unique<RecentArcMediaSource>(
      profile_.get(), arc::kDocumentsRootId);
  std::vector<RecentFile> files = GetRecentFiles(doc_source.get(), "");
  EXPECT_EQ(0u, files.size());

  files = GetRecentFiles(doc_source.get(), "word");
  EXPECT_EQ(0u, files.size());
}

TEST_F(RecentArcMediaSourceTest, GetAudioFiles) {
  EnableFakeFileSystemInstance();

  auto doc_source = std::make_unique<RecentArcMediaSource>(
      profile_.get(), arc::kDocumentsRootId);
  std::vector<RecentFile> files =
      GetRecentFiles(doc_source.get(), "", RecentSource::FileType::kAudio);
  // Query for recently-modified audio files should be ignored, since
  // MediaDocumentsProvider doesn't support queryRecentDocuments for audio.
  ASSERT_EQ(0u, files.size());
}

TEST_F(RecentArcMediaSourceTest, GetImageFiles) {
  EnableFakeFileSystemInstance();

  auto image_source = std::make_unique<RecentArcMediaSource>(
      profile_.get(), arc::kImagesRootId);
  std::vector<RecentFile> files =
      GetRecentFiles(image_source.get(), "", RecentSource::FileType::kImage);

  ASSERT_EQ(2u, files.size());
  EXPECT_THAT(files[0], IsRecentFile(GetImagePath("dog.jpg"), ModifiedTime(3)));
  EXPECT_THAT(files[1], IsRecentFile(GetImagePath("cat.png"), ModifiedTime(2)));
}

TEST_F(RecentArcMediaSourceTest, GetVideoFiles) {
  EnableFakeFileSystemInstance();

  auto video_source = std::make_unique<RecentArcMediaSource>(
      profile_.get(), arc::kVideosRootId);
  std::vector<RecentFile> files =
      GetRecentFiles(video_source.get(), "", RecentSource::FileType::kVideo);

  ASSERT_EQ(2u, files.size());
  EXPECT_THAT(files[0],
              IsRecentFile(GetVideoPath("ink.webm"), ModifiedTime(8)));
  EXPECT_THAT(files[1], IsRecentFile(GetVideoPath("hot.mp4"), ModifiedTime(7)));
}

TEST_F(RecentArcMediaSourceTest, GetDocumentFiles) {
  EnableFakeFileSystemInstance();

  auto doc_source = std::make_unique<RecentArcMediaSource>(
      profile_.get(), arc::kDocumentsRootId);
  std::vector<RecentFile> files =
      GetRecentFiles(doc_source.get(), "", RecentSource::FileType::kDocument);

  ASSERT_EQ(2u, files.size());
  EXPECT_THAT(files[0],
              IsRecentFile(GetDocumentPath("text.txt"), ModifiedTime(10)));
  EXPECT_THAT(files[1],
              IsRecentFile(GetDocumentPath("word.doc"), ModifiedTime(9)));

  files = GetRecentFiles(doc_source.get(), "word",
                         RecentSource::FileType::kDocument);
  ASSERT_EQ(1u, files.size());
  EXPECT_THAT(files[0],
              IsRecentFile(GetDocumentPath("word.doc"), ModifiedTime(9)));

  files = GetRecentFiles(doc_source.get(), "no-match",
                         RecentSource::FileType::kDocument);
  ASSERT_EQ(0u, files.size());
}

TEST_F(RecentArcMediaSourceTest, LaggyDocuments) {
  EnableFakeFileSystemInstance();
  // Find all recent files containing 'd' in their name.
  auto doc_source = std::make_unique<RecentArcMediaSource>(
      profile_.get(), arc::kDocumentsRootId);
  std::vector<RecentFile> files_no_lag = GetRecentFiles(doc_source.get(), "d");
  ASSERT_EQ(1u, files_no_lag.size());
  EXPECT_THAT(files_no_lag[0],
              IsRecentFile(GetDocumentPath("word.doc"), ModifiedTime(9)));

  // Now search again; but with an artificial lag for documents. Expect that
  // word.doc is no longer found.
  std::vector<RecentFile> files = GetRecentFiles(
      doc_source.get(), "d", RecentSource::FileType::kAll,
      std::make_pair(arc::kDocumentsRootId, base::Milliseconds(500)));

  ASSERT_EQ(0u, files.size());
}

TEST_F(RecentArcMediaSourceTest, OverlappingLaggySearches) {
  EnableFakeFileSystemInstance();
  // The number of times laggy, overlapping search is repeated.
  constexpr int32_t reps = 10;

  auto doc_source = std::make_unique<RecentArcMediaSource>(
      profile_.get(), arc::kDocumentsRootId);
  std::vector<RecentFile> results[reps];
  base::OneShotTimer timers[reps];

  // Prepare timers; timers are stopping searches at 250ms + 100ms * call_id.
  // Whenever a source is stopped, the code just collects its partial results
  // for later analysis.
  for (int32_t call_id = 0; call_id < reps; ++call_id) {
    base::TimeDelta stop_delta = base::Milliseconds(250 + 100 * call_id);
    base::TimeDelta lag_delta = base::Milliseconds(500 + 100 * call_id);
    doc_source->SetLagForTesting(lag_delta);
    timers[call_id].Start(
        FROM_HERE, stop_delta,
        base::BindOnce(
            [](const int32_t my_call_id, std::vector<RecentFile>* out_files,
               RecentArcMediaSource* source) {
              *out_files = source->Stop(my_call_id);
            },
            call_id, &results[call_id], doc_source.get()));
    doc_source->GetRecentFiles(RecentSource::Params(
                                   /*file_system_context=*/nullptr,
                                   /*call_id=*/call_id,
                                   /*origin=*/GURL(),
                                   /*query=*/"d",
                                   /*max_files=*/10,
                                   /*cutoff_time=*/base::Time(),
                                   /*end_time=*/base::TimeTicks::Max(),
                                   /*file_type=*/RecentSource::FileType::kAll),
                               base::BindOnce(
                                   [](std::vector<RecentFile>* out_files,
                                      std::vector<RecentFile> files) {
                                     *out_files = std::move(files);
                                   },
                                   &results[call_id]));
  }

  // Last call; wait for the results; these results are not interrupted and take
  // longer than any of the request requested above.
  doc_source->SetLagForTesting(base::Milliseconds(500 + 100 * reps));
  base::RunLoop run_loop;
  std::vector<RecentFile> final_result;
  doc_source->GetRecentFiles(
      RecentSource::Params(
          /*file_system_context=*/nullptr,
          /*call_id=*/reps,
          /*origin=*/GURL(),
          /*query=*/"d",
          /*max_files=*/10,
          /*cutoff_time=*/base::Time(),
          /*end_time=*/base::TimeTicks::Max(),
          /*file_type=*/RecentSource::FileType::kAll),
      base::BindOnce(
          [](base::RunLoop* run_loop, std::vector<RecentFile>* out_files,
             std::vector<RecentFile> files) {
            *out_files = std::move(files);
            run_loop->Quit();
          },
          &run_loop, &final_result));

  run_loop.Run();
  ASSERT_EQ(1u, final_result.size());
  EXPECT_THAT(final_result[0],
              IsRecentFile(GetDocumentPath("word.doc"), ModifiedTime(9)));
  for (int32_t call_id = 0; call_id < reps; ++call_id) {
    ASSERT_EQ(0u, results[call_id].size());
  }
}

TEST_F(RecentArcMediaSourceTest, UmaStats) {
  EnableFakeFileSystemInstance();

  auto doc_source = std::make_unique<RecentArcMediaSource>(
      profile_.get(), arc::kDocumentsRootId);
  base::HistogramTester histogram_tester;

  GetRecentFiles(doc_source.get(), "");

  histogram_tester.ExpectTotalCount(RecentArcMediaSource::kLoadHistogramName,
                                    1);
}

TEST_F(RecentArcMediaSourceTest, UmaStats_Deferred) {
  EnableFakeFileSystemInstance();
  EnableDefer();

  auto doc_source = std::make_unique<RecentArcMediaSource>(
      profile_.get(), arc::kDocumentsRootId);
  base::HistogramTester histogram_tester;

  GetRecentFiles(doc_source.get(), "");

  histogram_tester.ExpectTotalCount(RecentArcMediaSource::kLoadHistogramName,
                                    0);
}

TEST_F(RecentArcMediaSourceTest, MaxFiles) {
  EnableFakeFileSystemInstance();

  // Maximum one image can be returned per query, regardless of matched numbers.
  auto image_source = std::make_unique<RecentArcMediaSource>(
      profile_.get(), arc::kImagesRootId);
  std::vector<RecentFile> files = GetRecentFiles(
      image_source.get(), "", RecentSource::FileType::kImage, {}, 1);

  ASSERT_EQ(1u, files.size());
  EXPECT_THAT(files[0], IsRecentFile(GetImagePath("dog.jpg"), ModifiedTime(3)));
}

TEST_F(RecentArcMediaSourceTest, CallStopLate) {
  EnableFakeFileSystemInstance();

  // Maximum one image can be returned per query, relardless of matched numbers.
  auto image_source = std::make_unique<RecentArcMediaSource>(
      profile_.get(), arc::kImagesRootId);
  std::vector<RecentFile> files =
      GetRecentFiles(image_source.get(), "", RecentSource::FileType::kImage);

  ASSERT_EQ(2u, files.size());
  EXPECT_THAT(files[0], IsRecentFile(GetImagePath("dog.jpg"), ModifiedTime(3)));
  EXPECT_THAT(files[1], IsRecentFile(GetImagePath("cat.png"), ModifiedTime(2)));

  std::vector<RecentFile> stop_files = image_source->Stop(0);
  ASSERT_EQ(0u, stop_files.size());
}

}  // namespace ash
