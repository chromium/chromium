// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/file_picker_interceptor.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/actor/core/actor_features.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/in_memory_file_registry.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "url/gurl.h"

namespace actor {
namespace {

using ::blink::mojom::FileChooserFileInfoPtr;
using ::blink::mojom::FileChooserParams;

constexpr char kContents[] = "staged contents";
constexpr base::TimeDelta kStagingTimeout = base::Minutes(1);

class FakeFileSelectListener : public content::FileSelectListener {
 public:
  FakeFileSelectListener() = default;

  void FileSelected(std::vector<FileChooserFileInfoPtr> files,
                    const base::FilePath& base_dir,
                    FileChooserParams::Mode mode) override {
    selected_ = true;
    files_ = std::move(files);
  }

  void FileSelectionCanceled() override { canceled_ = true; }

  bool selected() const { return selected_; }
  bool canceled() const { return canceled_; }
  const std::vector<FileChooserFileInfoPtr>& files() const { return files_; }

 private:
  ~FakeFileSelectListener() override = default;

  bool selected_ = false;
  bool canceled_ = false;
  std::vector<FileChooserFileInfoPtr> files_;
};

class FilePickerInterceptorTest : public ChromeRenderViewHostTestHarness {
 protected:
  FilePickerInterceptorTest()
      : ChromeRenderViewHostTestHarness(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://example.com/"));
  }

  FilePickerInterceptor::StagedFile Stage(std::string_view name) {
    FilePickerInterceptor::StagedFile file =
        storage::InMemoryFileRegistry::GetInstance()->Register(
            name, kContents, base::Time::Now());
    EXPECT_TRUE(file.is_valid());
    return file;
  }

  // Stages `files` for the main frame.
  void Install(std::vector<FilePickerInterceptor::StagedFile> files) {
    FilePickerInterceptor::Install(*main_rfh(), std::move(files),
                                   kStagingTimeout);
  }

  // Sends a file chooser request from `render_frame_host`, the main frame by
  // default, through MaybeInterceptFileChooser() like FileSelectHelper does.
  // The request is by default one for an `<input type=file multiple>`. Returns
  // whether the interceptor handled it.
  bool HandleFileChooser(
      scoped_refptr<content::FileSelectListener>& listener,
      FileChooserParams::Mode mode = FileChooserParams::Mode::kOpenMultiple,
      content::RenderFrameHost* render_frame_host = nullptr) {
    FileChooserParams params;
    params.mode = mode;
    // Defaults to true; a plain <input type=file> does not need a local path.
    params.need_local_path = false;
    return MaybeInterceptFileChooser(
        render_frame_host ? *render_frame_host : *main_rfh(), listener, params);
  }

  // Adds a subframe to the page and commits a cross-origin document in it.
  content::RenderFrameHost* AddCrossOriginSubframe() {
    return content::NavigationSimulator::NavigateAndCommitFromDocument(
        GURL("https://other.example/"),
        content::RenderFrameHostTester::For(main_rfh())
            ->AppendChild("subframe"));
  }

  // Whether the contents of the file staged as `filesystem_id` are still held.
  bool IsRegistered(const std::string& filesystem_id) {
    return storage::InMemoryFileRegistry::GetInstance()->Find(filesystem_id) !=
           nullptr;
  }

  // Cracks `url` the same way FileChooserImpl does before granting access.
  storage::FileSystemURL Crack(const GURL& url) {
    return main_rfh()
        ->GetStoragePartition()
        ->GetFileSystemContext()
        ->CrackURLInFirstPartyContext(url);
  }

 private:
  base::test::ScopedFeatureList feature_list_{kGlicActorFileUploadTool};
};

TEST_F(FilePickerInterceptorTest, HandsOutStagedFile) {
  FilePickerInterceptor::StagedFile staged = Stage("report.txt");
  const std::string filesystem_id = staged.filesystem_id();
  std::vector<FilePickerInterceptor::StagedFile> files;
  files.push_back(std::move(staged));
  Install(std::move(files));

  auto fake_listener = base::MakeRefCounted<FakeFileSelectListener>();
  scoped_refptr<content::FileSelectListener> listener = fake_listener;
  ASSERT_TRUE(HandleFileChooser(listener));
  EXPECT_FALSE(listener);

  ASSERT_TRUE(fake_listener->selected());
  ASSERT_EQ(1u, fake_listener->files().size());
  const FileChooserFileInfoPtr& info = fake_listener->files()[0];
  ASSERT_TRUE(info->is_file_system());
  EXPECT_EQ(static_cast<int64_t>(std::string(kContents).size()),
            info->get_file_system()->length);

  const storage::FileSystemURL cracked = Crack(info->get_file_system()->url);
  ASSERT_TRUE(cracked.is_valid());
  EXPECT_EQ(storage::kFileSystemTypeInMemory, cracked.type());
  EXPECT_EQ(filesystem_id, cracked.mount_filesystem_id());
}

// Names with characters that are significant in URLs are escaped, so the URL
// still cracks to the file and its last component is the original name.
TEST_F(FilePickerInterceptorTest, EscapesFileNameInUrl) {
  constexpr char kName[] = "Q&A #1 100%2F?.txt";
  FilePickerInterceptor::StagedFile staged = Stage(kName);
  const std::string filesystem_id = staged.filesystem_id();
  std::vector<FilePickerInterceptor::StagedFile> files;
  files.push_back(std::move(staged));
  Install(std::move(files));

  auto fake_listener = base::MakeRefCounted<FakeFileSelectListener>();
  scoped_refptr<content::FileSelectListener> listener = fake_listener;
  ASSERT_TRUE(HandleFileChooser(listener));
  ASSERT_EQ(1u, fake_listener->files().size());

  const GURL& url = fake_listener->files()[0]->get_file_system()->url;
  EXPECT_FALSE(url.has_query());
  EXPECT_FALSE(url.has_ref());

  const storage::FileSystemURL cracked = Crack(url);
  ASSERT_TRUE(cracked.is_valid());
  EXPECT_EQ(filesystem_id, cracked.mount_filesystem_id());
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe(kName),
            cracked.virtual_path().BaseName());
}

// A staged file whose URL no longer cracks to its file system is never handed
// to the page: FileChooserImpl would otherwise grant access to an empty file
// system id, which crashes.
TEST_F(FilePickerInterceptorTest, SkipsFileThatDoesNotResolve) {
  FilePickerInterceptor::StagedFile revoked = Stage("revoked.txt");
  storage::IsolatedContext::GetInstance()->RevokeFileSystem(
      revoked.filesystem_id());
  FilePickerInterceptor::StagedFile valid = Stage("valid.txt");
  const std::string valid_filesystem_id = valid.filesystem_id();

  std::vector<FilePickerInterceptor::StagedFile> files;
  files.push_back(std::move(revoked));
  files.push_back(std::move(valid));
  Install(std::move(files));

  auto fake_listener = base::MakeRefCounted<FakeFileSelectListener>();
  scoped_refptr<content::FileSelectListener> listener = fake_listener;
  ASSERT_TRUE(HandleFileChooser(listener));

  // Only the file that still resolves is handed out.
  ASSERT_EQ(1u, fake_listener->files().size());
  EXPECT_EQ(valid_filesystem_id,
            Crack(fake_listener->files()[0]->get_file_system()->url)
                .mount_filesystem_id());
}

// If no staged file resolves, the request is left to the regular file picker
// and the listener is not consumed.
TEST_F(FilePickerInterceptorTest, FallsBackWhenNoFileResolves) {
  FilePickerInterceptor::StagedFile revoked = Stage("revoked.txt");
  storage::IsolatedContext::GetInstance()->RevokeFileSystem(
      revoked.filesystem_id());
  std::vector<FilePickerInterceptor::StagedFile> files;
  files.push_back(std::move(revoked));
  Install(std::move(files));

  auto fake_listener = base::MakeRefCounted<FakeFileSelectListener>();
  scoped_refptr<content::FileSelectListener> listener = fake_listener;
  EXPECT_FALSE(HandleFileChooser(listener));
  EXPECT_EQ(fake_listener.get(), listener.get());
  EXPECT_FALSE(fake_listener->selected());
  EXPECT_FALSE(fake_listener->canceled());
}

// Files staged for a page are never handed to the page that replaces it: the
// staging is discarded as soon as the page is navigated away from.
TEST_F(FilePickerInterceptorTest, DiscardsStagedFilesWhenNavigatedAway) {
  FilePickerInterceptor::StagedFile staged = Stage("report.txt");
  const std::string filesystem_id = staged.filesystem_id();
  std::vector<FilePickerInterceptor::StagedFile> files;
  files.push_back(std::move(staged));
  Install(std::move(files));

  NavigateAndCommit(GURL("https://example.com/next"));

  EXPECT_FALSE(FilePickerInterceptor::FromWebContents(web_contents()));
  EXPECT_FALSE(IsRegistered(filesystem_id));
}

// A same-document navigation, e.g. to a fragment, does not leave the page, so
// its file chooser is still answered with the staged files.
TEST_F(FilePickerInterceptorTest, KeepsStagedFilesOnSameDocumentNavigation) {
  std::vector<FilePickerInterceptor::StagedFile> files;
  files.push_back(Stage("report.txt"));
  Install(std::move(files));

  content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com/#section"), main_rfh())
      ->CommitSameDocument();

  auto fake_listener = base::MakeRefCounted<FakeFileSelectListener>();
  scoped_refptr<content::FileSelectListener> listener = fake_listener;
  EXPECT_TRUE(HandleFileChooser(listener));
  EXPECT_TRUE(fake_listener->selected());
}

// Answering a request uses up the staging, but the files handed out stay
// readable, even once another upload is staged, e.g. for a second input of the
// same form.
TEST_F(FilePickerInterceptorTest, HandedOutFilesOutliveTheStaging) {
  FilePickerInterceptor::StagedFile staged = Stage("first.txt");
  const std::string filesystem_id = staged.filesystem_id();
  std::vector<FilePickerInterceptor::StagedFile> files;
  files.push_back(std::move(staged));
  Install(std::move(files));

  auto fake_listener = base::MakeRefCounted<FakeFileSelectListener>();
  scoped_refptr<content::FileSelectListener> listener = fake_listener;
  ASSERT_TRUE(HandleFileChooser(listener));
  ASSERT_EQ(1u, fake_listener->files().size());
  EXPECT_FALSE(FilePickerInterceptor::FromWebContents(web_contents()));

  std::vector<FilePickerInterceptor::StagedFile> more_files;
  more_files.push_back(Stage("second.txt"));
  Install(std::move(more_files));

  EXPECT_TRUE(IsRegistered(filesystem_id));
  EXPECT_TRUE(
      Crack(fake_listener->files()[0]->get_file_system()->url).is_valid());
}

// Staged files that are not handed out, e.g. extra files for an input that
// accepts a single file, are discarded right away.
TEST_F(FilePickerInterceptorTest, DiscardsStagedFilesThatWereNotHandedOut) {
  FilePickerInterceptor::StagedFile first = Stage("first.txt");
  const std::string first_filesystem_id = first.filesystem_id();
  FilePickerInterceptor::StagedFile second = Stage("second.txt");
  const std::string second_filesystem_id = second.filesystem_id();
  std::vector<FilePickerInterceptor::StagedFile> files;
  files.push_back(std::move(first));
  files.push_back(std::move(second));
  Install(std::move(files));

  auto fake_listener = base::MakeRefCounted<FakeFileSelectListener>();
  scoped_refptr<content::FileSelectListener> listener = fake_listener;
  ASSERT_TRUE(HandleFileChooser(listener, FileChooserParams::Mode::kOpen));
  ASSERT_EQ(1u, fake_listener->files().size());

  EXPECT_TRUE(IsRegistered(first_filesystem_id));
  EXPECT_FALSE(IsRegistered(second_filesystem_id));
}

// Handed-out files are discarded once the document they were handed to is
// gone, since nothing can read them anymore.
TEST_F(FilePickerInterceptorTest, DiscardsHandedOutFilesWithTheirDocument) {
  // A page in the back/forward cache keeps its document, and with it the files.
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  FilePickerInterceptor::StagedFile staged = Stage("report.txt");
  const std::string filesystem_id = staged.filesystem_id();
  std::vector<FilePickerInterceptor::StagedFile> files;
  files.push_back(std::move(staged));
  Install(std::move(files));

  auto fake_listener = base::MakeRefCounted<FakeFileSelectListener>();
  scoped_refptr<content::FileSelectListener> listener = fake_listener;
  ASSERT_TRUE(HandleFileChooser(listener));
  ASSERT_TRUE(IsRegistered(filesystem_id));

  NavigateAndCommit(GURL("https://example.com/next"));

  EXPECT_FALSE(IsRegistered(filesystem_id));
}

// Files staged for one frame are never handed to another frame in the tab,
// e.g. a cross-origin subframe, and stay staged for the frame they are for.
TEST_F(FilePickerInterceptorTest, IgnoresRequestsFromOtherFrames) {
  content::RenderFrameHost* subframe = AddCrossOriginSubframe();
  ASSERT_TRUE(subframe);
  std::vector<FilePickerInterceptor::StagedFile> files;
  files.push_back(Stage("report.txt"));
  Install(std::move(files));

  auto fake_listener = base::MakeRefCounted<FakeFileSelectListener>();
  scoped_refptr<content::FileSelectListener> listener = fake_listener;
  EXPECT_FALSE(HandleFileChooser(
      listener, FileChooserParams::Mode::kOpenMultiple, subframe));
  EXPECT_EQ(fake_listener.get(), listener.get());
  EXPECT_FALSE(fake_listener->selected());

  EXPECT_TRUE(HandleFileChooser(listener));
  EXPECT_TRUE(fake_listener->selected());
}

// Files staged for a subframe, e.g. one embedding an upload widget, are handed
// to that subframe only.
TEST_F(FilePickerInterceptorTest, HandsOutFilesStagedForSubframe) {
  content::RenderFrameHost* subframe = AddCrossOriginSubframe();
  ASSERT_TRUE(subframe);
  std::vector<FilePickerInterceptor::StagedFile> files;
  files.push_back(Stage("report.txt"));
  FilePickerInterceptor::Install(*subframe, std::move(files), kStagingTimeout);

  auto fake_listener = base::MakeRefCounted<FakeFileSelectListener>();
  scoped_refptr<content::FileSelectListener> listener = fake_listener;
  EXPECT_FALSE(HandleFileChooser(listener));
  EXPECT_FALSE(fake_listener->selected());

  EXPECT_TRUE(HandleFileChooser(
      listener, FileChooserParams::Mode::kOpenMultiple, subframe));
  EXPECT_TRUE(fake_listener->selected());
}

// If the page never opens a file chooser, the staged files are discarded once
// the staging timeout fires.
TEST_F(FilePickerInterceptorTest, DiscardsStagedFilesOnTimeout) {
  FilePickerInterceptor::StagedFile staged = Stage("report.txt");
  const std::string filesystem_id = staged.filesystem_id();
  std::vector<FilePickerInterceptor::StagedFile> files;
  files.push_back(std::move(staged));
  Install(std::move(files));

  task_environment()->FastForwardBy(kStagingTimeout - base::Seconds(1));
  EXPECT_TRUE(FilePickerInterceptor::FromWebContents(web_contents()));
  EXPECT_TRUE(IsRegistered(filesystem_id));

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(FilePickerInterceptor::FromWebContents(web_contents()));
  EXPECT_FALSE(IsRegistered(filesystem_id));
}

}  // namespace
}  // namespace actor
