// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ash/components/arc/mojom/file_system.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_file_system_instance.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_mounter.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/ash/arc/fileapi/arc_media_view_util.h"
#include "chrome/browser/ash/fileapi/recent_arc_media_source.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chrome/browser/ash/fileapi/recent_source.h"
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

    source_ = std::make_unique<RecentArcMediaSource>(profile_.get());
  }

  void TearDown() override {
    arc_service_manager_->arc_bridge_service()->file_system()->CloseInstance(
        &fake_file_system_);
    arc_service_manager_->set_browser_context(nullptr);
  }

 protected:
  void AddDocumentsToFakeFileSystemInstance() {
    auto images_root_doc = MakeDocument(
        arc::kImagesRootDocumentId, "", "", arc::kAndroidDirectoryMimeType,
        base::Time::FromMillisecondsSinceUnixEpoch(1));
    auto cat_doc =
        MakeDocument("cat", arc::kImagesRootDocumentId, "cat.png", "image/png",
                     base::Time::FromMillisecondsSinceUnixEpoch(2));
    auto dog_doc =
        MakeDocument("dog", arc::kImagesRootDocumentId, "dog.jpg", "image/jpeg",
                     base::Time::FromMillisecondsSinceUnixEpoch(3));
    auto fox_doc =
        MakeDocument("fox", arc::kImagesRootDocumentId, "fox.gif", "image/gif",
                     base::Time::FromMillisecondsSinceUnixEpoch(4));
    auto elk_doc = MakeDocument("elk", arc::kImagesRootDocumentId, "elk.tiff",
                                "image/tiff",
                                base::Time::FromMillisecondsSinceUnixEpoch(5));
    auto audio_root_doc = MakeDocument(
        arc::kAudioRootDocumentId, "", "", arc::kAndroidDirectoryMimeType,
        base::Time::FromMillisecondsSinceUnixEpoch(1));
    auto god_doc =
        MakeDocument("god", arc::kAudioRootDocumentId, "god.mp3", "audio/mp3",
                     base::Time::FromMillisecondsSinceUnixEpoch(6));
    auto videos_root_doc = MakeDocument(
        arc::kVideosRootDocumentId, "", "", arc::kAndroidDirectoryMimeType,
        base::Time::FromMillisecondsSinceUnixEpoch(1));
    auto hot_doc =
        MakeDocument("hot", arc::kVideosRootDocumentId, "hot.mp4", "video/mp4",
                     base::Time::FromMillisecondsSinceUnixEpoch(7));
    auto ink_doc = MakeDocument("ink", arc::kVideosRootDocumentId, "ink.webm",
                                "video/webm",
                                base::Time::FromMillisecondsSinceUnixEpoch(8));
    auto documents_root_doc = MakeDocument(
        arc::kDocumentsRootDocumentId, "", "", arc::kAndroidDirectoryMimeType,
        base::Time::FromMillisecondsSinceUnixEpoch(1));
    auto word_doc = MakeDocument("word", arc::kDocumentsRootDocumentId,
                                 "word.doc", "application/msword",
                                 base::Time::FromMillisecondsSinceUnixEpoch(9));
    auto text_doc = MakeDocument(
        "text", arc::kDocumentsRootDocumentId, "text.txt", "text/plain",
        base::Time::FromMillisecondsSinceUnixEpoch(10));

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

    fake_file_system_.AddRecentDocument(arc::kImagesRootDocumentId,
                                        images_root_doc);
    fake_file_system_.AddRecentDocument(arc::kImagesRootDocumentId, cat_doc);
    fake_file_system_.AddRecentDocument(arc::kImagesRootDocumentId, dog_doc);
    fake_file_system_.AddRecentDocument(arc::kImagesRootDocumentId, elk_doc);
    fake_file_system_.AddRecentDocument(arc::kAudioRootDocumentId,
                                        audio_root_doc);
    fake_file_system_.AddRecentDocument(arc::kAudioRootDocumentId, god_doc);
    fake_file_system_.AddRecentDocument(arc::kVideosRootDocumentId,
                                        videos_root_doc);
    fake_file_system_.AddRecentDocument(arc::kVideosRootDocumentId, hot_doc);
    fake_file_system_.AddRecentDocument(arc::kVideosRootDocumentId, ink_doc);
    fake_file_system_.AddRecentDocument(arc::kDocumentsRootDocumentId,
                                        documents_root_doc);
    fake_file_system_.AddRecentDocument(arc::kDocumentsRootDocumentId,
                                        word_doc);
    fake_file_system_.AddRecentDocument(arc::kDocumentsRootDocumentId,
                                        text_doc);
  }

  void EnableFakeFileSystemInstance() {
    arc_service_manager_->arc_bridge_service()->file_system()->SetInstance(
        &fake_file_system_);
    arc::WaitForInstanceReady(
        arc_service_manager_->arc_bridge_service()->file_system());
  }

  std::vector<RecentFile> GetRecentFiles(
      const std::string query,
      RecentSource::FileType file_type = RecentSource::FileType::kAll) {
    std::vector<RecentFile> files;

    base::RunLoop run_loop;

    source_->GetRecentFiles(RecentSource::Params(
        nullptr /* file_system_context */, GURL() /* origin */,
        1 /* max_files: ignored */, query,
        base::Time() /* cutoff_time: ignored */,
        base::TimeTicks::Max() /* end_time: ignored */,
        file_type /* file_type */,
        base::BindOnce(
            [](base::RunLoop* run_loop, std::vector<RecentFile>* out_files,
               std::vector<RecentFile> files) {
              run_loop->Quit();
              *out_files = std::move(files);
            },
            &run_loop, &files)));

    run_loop.Run();

    return files;
  }

  void EnableDefer() { runner_->SetShouldDefer(true); }

  content::BrowserTaskEnvironment task_environment_;
  arc::FakeFileSystemInstance fake_file_system_;

  // Use the same initialization/destruction order as
  // `ChromeBrowserMainPartsAsh`.
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestingProfile> profile_;

  raw_ptr<arc::ArcFileSystemOperationRunner, ExperimentalAsh> runner_;

  std::unique_ptr<RecentArcMediaSource> source_;
};

TEST_F(RecentArcMediaSourceTest, Normal) {
  EnableFakeFileSystemInstance();

  std::vector<RecentFile> files = GetRecentFiles("");

  ASSERT_EQ(6u, files.size());
  EXPECT_EQ(
      arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                         arc::kImagesRootDocumentId)
          .Append("cat.png"),
      files[0].url().path());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(2),
            files[0].last_modified());
  EXPECT_EQ(
      arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                         arc::kImagesRootDocumentId)
          .Append("dog.jpg"),
      files[1].url().path());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(3),
            files[1].last_modified());
  EXPECT_EQ(
      arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                         arc::kVideosRootDocumentId)
          .Append("hot.mp4"),
      files[2].url().path());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(7),
            files[2].last_modified());
  EXPECT_EQ(
      arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                         arc::kVideosRootDocumentId)
          .Append("ink.webm"),
      files[3].url().path());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(8),
            files[3].last_modified());
  EXPECT_EQ(
      arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                         arc::kDocumentsRootDocumentId)
          .Append("text.txt"),
      files[4].url().path());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(10),
            files[4].last_modified());
  EXPECT_EQ(
      arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                         arc::kDocumentsRootDocumentId)
          .Append("word.doc"),
      files[5].url().path());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(9),
            files[5].last_modified());

  files = GetRecentFiles("text");
  ASSERT_EQ(1u, files.size());
  EXPECT_EQ(
      arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                         arc::kDocumentsRootDocumentId)
          .Append("text.txt"),
      files[0].url().path());
}

TEST_F(RecentArcMediaSourceTest, ArcNotAvailable) {
  std::vector<RecentFile> files = GetRecentFiles("");
  EXPECT_EQ(0u, files.size());

  files = GetRecentFiles("hot");
  EXPECT_EQ(0u, files.size());
}

TEST_F(RecentArcMediaSourceTest, Deferred) {
  EnableFakeFileSystemInstance();
  EnableDefer();

  std::vector<RecentFile> files = GetRecentFiles("");
  EXPECT_EQ(0u, files.size());

  files = GetRecentFiles("word");
  EXPECT_EQ(0u, files.size());
}

TEST_F(RecentArcMediaSourceTest, GetAudioFiles) {
  EnableFakeFileSystemInstance();

  std::vector<RecentFile> files =
      GetRecentFiles("", RecentSource::FileType::kAudio);
  // Query for recently-modified audio files should be ignored, since
  // MediaDocumentsProvider doesn't support queryRecentDocuments for audio.
  ASSERT_EQ(0u, files.size());
}

TEST_F(RecentArcMediaSourceTest, GetImageFiles) {
  EnableFakeFileSystemInstance();

  std::vector<RecentFile> files =
      GetRecentFiles("", RecentSource::FileType::kImage);

  ASSERT_EQ(2u, files.size());
  EXPECT_EQ(
      arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                         arc::kImagesRootDocumentId)
          .Append("cat.png"),
      files[0].url().path());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(2),
            files[0].last_modified());
  EXPECT_EQ(
      arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                         arc::kImagesRootDocumentId)
          .Append("dog.jpg"),
      files[1].url().path());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(3),
            files[1].last_modified());
}

TEST_F(RecentArcMediaSourceTest, GetVideoFiles) {
  EnableFakeFileSystemInstance();

  std::vector<RecentFile> files =
      GetRecentFiles("", RecentSource::FileType::kVideo);

  ASSERT_EQ(2u, files.size());
  EXPECT_EQ(
      arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                         arc::kVideosRootDocumentId)
          .Append("hot.mp4"),
      files[0].url().path());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(7),
            files[0].last_modified());
  EXPECT_EQ(
      arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                         arc::kVideosRootDocumentId)
          .Append("ink.webm"),
      files[1].url().path());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(8),
            files[1].last_modified());
}

TEST_F(RecentArcMediaSourceTest, GetDocumentFiles) {
  EnableFakeFileSystemInstance();

  std::vector<RecentFile> files =
      GetRecentFiles("", RecentSource::FileType::kDocument);

  ASSERT_EQ(2u, files.size());
  EXPECT_EQ(
      arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                         arc::kDocumentsRootDocumentId)
          .Append("text.txt"),
      files[0].url().path());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(10),
            files[0].last_modified());
  EXPECT_EQ(
      arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                         arc::kDocumentsRootDocumentId)
          .Append("word.doc"),
      files[1].url().path());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(9),
            files[1].last_modified());

  files = GetRecentFiles("word", RecentSource::FileType::kDocument);
  ASSERT_EQ(1u, files.size());
  EXPECT_EQ(
      arc::GetDocumentsProviderMountPath(arc::kMediaDocumentsProviderAuthority,
                                         arc::kDocumentsRootDocumentId)
          .Append("word.doc"),
      files[0].url().path());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(9),
            files[0].last_modified());

  files = GetRecentFiles("no-match", RecentSource::FileType::kDocument);
  ASSERT_EQ(0u, files.size());
}

TEST_F(RecentArcMediaSourceTest, UmaStats) {
  EnableFakeFileSystemInstance();

  base::HistogramTester histogram_tester;

  GetRecentFiles("");

  histogram_tester.ExpectTotalCount(RecentArcMediaSource::kLoadHistogramName,
                                    1);
}

TEST_F(RecentArcMediaSourceTest, UmaStats_Deferred) {
  EnableFakeFileSystemInstance();
  EnableDefer();

  base::HistogramTester histogram_tester;

  GetRecentFiles("");

  histogram_tester.ExpectTotalCount(RecentArcMediaSource::kLoadHistogramName,
                                    0);
}

}  // namespace ash
