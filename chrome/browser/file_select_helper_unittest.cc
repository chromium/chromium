// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/file_select_helper.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/selected_file_info.h"

using blink::mojom::FileChooserParams;

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
namespace {

// A listener that remembers the list of files chosen.  The |files| argument
// to the ctor must outlive the listener.
class TestFileSelectListener : public content::FileSelectListener {
 public:
  explicit TestFileSelectListener(
      std::vector<blink::mojom::FileChooserFileInfoPtr>* files)
      : files_(files) {}

 private:
  ~TestFileSelectListener() override = default;
  // content::FileSelectListener overrides.
  void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                    const base::FilePath& base_dir,
                    blink::mojom::FileChooserParams::Mode mode) override {
    *files_ = std::move(files);
  }
  void FileSelectionCanceled() override {}

  raw_ptr<std::vector<blink::mojom::FileChooserFileInfoPtr>> files_;
};

// Fill in the arguments to be passed to the ContentAnalysisCompletionCallback()
// method based on a list of paths and the desired result for each path.
// This function simulates a path either passing the deep scan (status of true)
// or failing (status of false).
void PrepareContentAnalysisCompletionCallbackArgs(
    std::vector<base::FilePath> paths,
    std::vector<bool> status,
    std::vector<blink::mojom::FileChooserFileInfoPtr>* orig_files,
    enterprise_connectors::ContentAnalysisDelegate::Data* data,
    enterprise_connectors::ContentAnalysisDelegate::Result* result) {
  DCHECK_EQ(status.size(), paths.size());

  if (orig_files) {
    for (auto& path : paths) {
      orig_files->push_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
          blink::mojom::NativeFileInfo::New(path,
                                            path.BaseName().AsUTF16Unsafe())));
    }
  }

  data->paths = std::move(paths);
  result->paths_results = std::move(status);
}

}  // namespace
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

class FileSelectHelperTest : public testing::Test {
 public:
  FileSelectHelperTest() {}

  FileSelectHelperTest(const FileSelectHelperTest&) = delete;
  FileSelectHelperTest& operator=(const FileSelectHelperTest&) = delete;

 protected:
  void SetUp() override {
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &data_dir_));
    data_dir_ = data_dir_.AppendASCII("file_select_helper");
    ASSERT_TRUE(base::PathExists(data_dir_));
  }

  // The path to input data used in tests.
  base::FilePath data_dir_;
};

TEST_F(FileSelectHelperTest, IsAcceptTypeValid) {
  EXPECT_TRUE(FileSelectHelper::IsAcceptTypeValid("a/b"));
  EXPECT_TRUE(FileSelectHelper::IsAcceptTypeValid("abc/def"));
  EXPECT_TRUE(FileSelectHelper::IsAcceptTypeValid("abc/*"));
  EXPECT_TRUE(FileSelectHelper::IsAcceptTypeValid(".a"));
  EXPECT_TRUE(FileSelectHelper::IsAcceptTypeValid(".abc"));

  EXPECT_FALSE(FileSelectHelper::IsAcceptTypeValid("."));
  EXPECT_FALSE(FileSelectHelper::IsAcceptTypeValid("/"));
  EXPECT_FALSE(FileSelectHelper::IsAcceptTypeValid("ABC/*"));
  EXPECT_FALSE(FileSelectHelper::IsAcceptTypeValid("abc/def "));
}

#if BUILDFLAG(IS_MAC)
TEST_F(FileSelectHelperTest, ZipPackage) {
  // Zip the package.
  const char app_name[] = "CalculatorFake.app";
  base::FilePath src = data_dir_.Append(app_name);
  base::FilePath dest = FileSelectHelper::ZipPackage(src);
  ASSERT_FALSE(dest.empty());
  ASSERT_TRUE(base::PathExists(dest));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Unzip the package into a temporary directory.
  base::CommandLine cl(base::FilePath("/usr/bin/unzip"));
  cl.AppendArg(dest.value().c_str());
  cl.AppendArg("-d");
  cl.AppendArg(temp_dir.GetPath().value().c_str());
  std::string output;
  EXPECT_TRUE(base::GetAppOutput(cl, &output));

  // Verify that several key files haven't changed.
  const char* files_to_verify[] = {"Contents/Info.plist",
                                   "Contents/MacOS/Calculator",
                                   "Contents/_CodeSignature/CodeResources"};
  size_t file_count = std::size(files_to_verify);
  for (size_t i = 0; i < file_count; i++) {
    const char* relative_path = files_to_verify[i];
    base::FilePath orig_file = src.Append(relative_path);
    base::FilePath final_file =
        temp_dir.GetPath().Append(app_name).Append(relative_path);
    EXPECT_TRUE(base::ContentsEqual(orig_file, final_file));
  }
}
#endif  // BUILDFLAG(IS_MAC)

TEST_F(FileSelectHelperTest, GetSanitizedFileName) {
  // The empty path should be preserved.
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("")),
            FileSelectHelper::GetSanitizedFileName(base::FilePath()));

  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("ascii.txt")),
            FileSelectHelper::GetSanitizedFileName(
                base::FilePath(FILE_PATH_LITERAL("ascii.txt"))));
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("trailing-spaces_")),
            FileSelectHelper::GetSanitizedFileName(
                base::FilePath(FILE_PATH_LITERAL("trailing-spaces "))));
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("path_components_in_name")),
            FileSelectHelper::GetSanitizedFileName(
                base::FilePath(FILE_PATH_LITERAL("path/components/in/name"))));

#if BUILDFLAG(IS_WIN)
  // Invalid UTF-16. However, note that on Windows, the invalid UTF-16 will pass
  // through without error.
  base::FilePath::CharType kBadName[] = {0xd801, 0xdc37, 0xdc17, 0};
#else
  // Invalid UTF-8
  base::FilePath::CharType kBadName[] = {'\xe3', '\x81', '\x81',
                                         '\x81', '\x82', '\0'};
#endif
  base::FilePath bad_filename(kBadName);
  ASSERT_FALSE(bad_filename.empty());
  // The only thing we are testing is that if the source filename was non-empty,
  // the resulting filename is also not empty. Invalid encoded filenames can
  // cause conversions to fail. Such failures shouldn't cause the resulting
  // filename to disappear.
  EXPECT_FALSE(FileSelectHelper::GetSanitizedFileName(bad_filename).empty());
}

TEST_F(FileSelectHelperTest, LastSelectedDirectory) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  scoped_refptr<FileSelectHelper> file_select_helper =
      new FileSelectHelper(&profile);

  const int index = 0;

  const base::FilePath dir_path_1 = data_dir_.AppendASCII("dir1");
  const base::FilePath dir_path_2 = data_dir_.AppendASCII("dir2");
  const base::FilePath file_path_1 = dir_path_1.AppendASCII("file1.txt");
  const base::FilePath file_path_2 = dir_path_1.AppendASCII("file2.txt");
  const base::FilePath file_path_3 = dir_path_2.AppendASCII("file3.txt");
  std::vector<base::FilePath> files;  // Both in dir1.
  files.push_back(file_path_1);
  files.push_back(file_path_2);
  std::vector<base::FilePath> dirs;
  dirs.push_back(dir_path_1);
  dirs.push_back(dir_path_2);

  // Modes where the parent of the selection is remembered.
  const std::vector<FileChooserParams::Mode> modes = {
      FileChooserParams::Mode::kOpen, FileChooserParams::Mode::kOpenMultiple,
      FileChooserParams::Mode::kSave,
  };

  for (const auto& mode : modes) {
    file_select_helper->dialog_mode_ = mode;

    file_select_helper->AddRef();  // Normally called by RunFileChooser().
    file_select_helper->FileSelected(ui::SelectedFileInfo(file_path_1), index);
    EXPECT_EQ(dir_path_1, profile.last_selected_directory());

    file_select_helper->AddRef();  // Normally called by RunFileChooser().
    file_select_helper->FileSelected(ui::SelectedFileInfo(file_path_2), index);
    EXPECT_EQ(dir_path_1, profile.last_selected_directory());

    file_select_helper->AddRef();  // Normally called by RunFileChooser().
    file_select_helper->FileSelected(ui::SelectedFileInfo(file_path_3), index);
    EXPECT_EQ(dir_path_2, profile.last_selected_directory());

    file_select_helper->AddRef();  // Normally called by RunFileChooser().
    file_select_helper->MultiFilesSelected(
        ui::FilePathListToSelectedFileInfoList(files));
    EXPECT_EQ(dir_path_1, profile.last_selected_directory());
  }

  // Type where the selected folder itself is remembered.
  file_select_helper->dialog_mode_ = FileChooserParams::Mode::kUploadFolder;

  file_select_helper->AddRef();  // Normally called by RunFileChooser().
  file_select_helper->FileSelected(ui::SelectedFileInfo(dir_path_1), index);
  EXPECT_EQ(dir_path_1, profile.last_selected_directory());

  file_select_helper->AddRef();  // Normally called by RunFileChooser().
  file_select_helper->FileSelected(ui::SelectedFileInfo(dir_path_2), index);
  EXPECT_EQ(dir_path_2, profile.last_selected_directory());

  file_select_helper->AddRef();  // Normally called by RunFileChooser().
  file_select_helper->MultiFilesSelected(
      ui::FilePathListToSelectedFileInfoList(dirs));
  EXPECT_EQ(dir_path_1, profile.last_selected_directory());
}

// The following tests depend on the enterprise cloud content analysis feature
// set.
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

TEST_F(FileSelectHelperTest, ContentAnalysisCompletionCallback_NoFiles) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  scoped_refptr<FileSelectHelper> file_select_helper =
      new FileSelectHelper(&profile);

  std::vector<blink::mojom::FileChooserFileInfoPtr> files;
  auto listener = base::MakeRefCounted<TestFileSelectListener>(&files);
  file_select_helper->SetFileSelectListenerForTesting(std::move(listener));
  file_select_helper->DontAbortOnMissingWebContentsForTesting();

  std::vector<blink::mojom::FileChooserFileInfoPtr> orig_files;
  enterprise_connectors::ContentAnalysisDelegate::Data data;
  enterprise_connectors::ContentAnalysisDelegate::Result result;
  file_select_helper->AddRef();  // Normally called by RunFileChooser().
  file_select_helper->ContentAnalysisCompletionCallback(std::move(orig_files),
                                                        data, result);

  EXPECT_EQ(0u, files.size());
}

TEST_F(FileSelectHelperTest, ContentAnalysisCompletionCallback_OneOKFile) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  scoped_refptr<FileSelectHelper> file_select_helper =
      new FileSelectHelper(&profile);

  std::vector<blink::mojom::FileChooserFileInfoPtr> files;
  auto listener = base::MakeRefCounted<TestFileSelectListener>(&files);
  file_select_helper->SetFileSelectListenerForTesting(std::move(listener));
  file_select_helper->DontAbortOnMissingWebContentsForTesting();

  std::vector<blink::mojom::FileChooserFileInfoPtr> orig_files;
  enterprise_connectors::ContentAnalysisDelegate::Data data;
  enterprise_connectors::ContentAnalysisDelegate::Result result;
  PrepareContentAnalysisCompletionCallbackArgs(
      {data_dir_.AppendASCII("foo.doc")}, {true}, &orig_files, &data, &result);

  file_select_helper->AddRef();  // Normally called by RunFileChooser().
  file_select_helper->ContentAnalysisCompletionCallback(std::move(orig_files),
                                                        data, result);

  EXPECT_EQ(1u, files.size());
}

TEST_F(FileSelectHelperTest, ContentAnalysisCompletionCallback_TwoOKFiles) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  scoped_refptr<FileSelectHelper> file_select_helper =
      new FileSelectHelper(&profile);

  std::vector<blink::mojom::FileChooserFileInfoPtr> files;
  auto listener = base::MakeRefCounted<TestFileSelectListener>(&files);
  file_select_helper->SetFileSelectListenerForTesting(std::move(listener));
  file_select_helper->DontAbortOnMissingWebContentsForTesting();

  std::vector<blink::mojom::FileChooserFileInfoPtr> orig_files;
  enterprise_connectors::ContentAnalysisDelegate::Data data;
  enterprise_connectors::ContentAnalysisDelegate::Result result;
  PrepareContentAnalysisCompletionCallbackArgs(
      {data_dir_.AppendASCII("foo.doc"), data_dir_.AppendASCII("bar.doc")},
      {true, true}, &orig_files, &data, &result);

  file_select_helper->AddRef();  // Normally called by RunFileChooser().
  file_select_helper->ContentAnalysisCompletionCallback(std::move(orig_files),
                                                        data, result);

  EXPECT_EQ(2u, files.size());
}

TEST_F(FileSelectHelperTest, ContentAnalysisCompletionCallback_TwoBadFiles) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  scoped_refptr<FileSelectHelper> file_select_helper =
      new FileSelectHelper(&profile);

  std::vector<blink::mojom::FileChooserFileInfoPtr> files;
  auto listener = base::MakeRefCounted<TestFileSelectListener>(&files);
  file_select_helper->SetFileSelectListenerForTesting(std::move(listener));
  file_select_helper->DontAbortOnMissingWebContentsForTesting();

  std::vector<blink::mojom::FileChooserFileInfoPtr> orig_files;
  enterprise_connectors::ContentAnalysisDelegate::Data data;
  enterprise_connectors::ContentAnalysisDelegate::Result result;
  PrepareContentAnalysisCompletionCallbackArgs(
      {data_dir_.AppendASCII("foo.doc"), data_dir_.AppendASCII("bar.doc")},
      {false, false}, &orig_files, &data, &result);

  file_select_helper->AddRef();  // Normally called by RunFileChooser().
  file_select_helper->ContentAnalysisCompletionCallback(std::move(orig_files),
                                                        data, result);

  EXPECT_EQ(0u, files.size());
}

TEST_F(FileSelectHelperTest, ContentAnalysisCompletionCallback_OKBadFiles) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  scoped_refptr<FileSelectHelper> file_select_helper =
      new FileSelectHelper(&profile);

  std::vector<blink::mojom::FileChooserFileInfoPtr> files;
  auto listener = base::MakeRefCounted<TestFileSelectListener>(&files);
  file_select_helper->SetFileSelectListenerForTesting(std::move(listener));
  file_select_helper->DontAbortOnMissingWebContentsForTesting();

  std::vector<blink::mojom::FileChooserFileInfoPtr> orig_files;
  enterprise_connectors::ContentAnalysisDelegate::Data data;
  enterprise_connectors::ContentAnalysisDelegate::Result result;
  PrepareContentAnalysisCompletionCallbackArgs(
      {data_dir_.AppendASCII("foo.doc"), data_dir_.AppendASCII("bar.doc")},
      {false, true}, &orig_files, &data, &result);

  file_select_helper->AddRef();  // Normally called by RunFileChooser().
  file_select_helper->ContentAnalysisCompletionCallback(std::move(orig_files),
                                                        data, result);

  ASSERT_EQ(1u, files.size());
  EXPECT_EQ(data_dir_.AppendASCII("bar.doc"),
            files[0]->get_native_file()->file_path);
}

TEST_F(FileSelectHelperTest,
       ContentAnalysisCompletionCallback_SystemFilesSkipped) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  scoped_refptr<FileSelectHelper> file_select_helper =
      new FileSelectHelper(&profile);

  std::vector<blink::mojom::FileChooserFileInfoPtr> files;
  auto listener = base::MakeRefCounted<TestFileSelectListener>(&files);
  file_select_helper->SetFileSelectListenerForTesting(std::move(listener));
  file_select_helper->DontAbortOnMissingWebContentsForTesting();

  std::vector<blink::mojom::FileChooserFileInfoPtr> orig_files;
  enterprise_connectors::ContentAnalysisDelegate::Data data;
  enterprise_connectors::ContentAnalysisDelegate::Result result;

  for (int i = 0; i < 5; ++i) {
    orig_files.push_back(blink::mojom::FileChooserFileInfo::NewFileSystem(
        blink::mojom::FileSystemFileInfo::New()));
  }

  file_select_helper->AddRef();  // Normally called by RunFileChooser().
  file_select_helper->ContentAnalysisCompletionCallback(std::move(orig_files),
                                                        data, result);

  ASSERT_EQ(5u, files.size());
  for (int i = 0; i < 5; ++i)
    EXPECT_TRUE(files[i]->is_file_system());
}

TEST_F(FileSelectHelperTest,
       ContentAnalysisCompletionCallback_SystemOKBadFiles) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  scoped_refptr<FileSelectHelper> file_select_helper =
      new FileSelectHelper(&profile);

  std::vector<blink::mojom::FileChooserFileInfoPtr> files;
  auto listener = base::MakeRefCounted<TestFileSelectListener>(&files);
  file_select_helper->SetFileSelectListenerForTesting(std::move(listener));
  file_select_helper->DontAbortOnMissingWebContentsForTesting();

  std::vector<blink::mojom::FileChooserFileInfoPtr> orig_files;
  enterprise_connectors::ContentAnalysisDelegate::Data data;
  enterprise_connectors::ContentAnalysisDelegate::Result result;

  // Add 1 non-native file at the start and end of the files list, which should
  // be skipped.
  orig_files.push_back(blink::mojom::FileChooserFileInfo::NewFileSystem(
      blink::mojom::FileSystemFileInfo::New()));
  PrepareContentAnalysisCompletionCallbackArgs(
      {data_dir_.AppendASCII("foo.doc"), data_dir_.AppendASCII("bar.doc")},
      {false, true}, &orig_files, &data, &result);
  orig_files.push_back(blink::mojom::FileChooserFileInfo::NewFileSystem(
      blink::mojom::FileSystemFileInfo::New()));

  file_select_helper->AddRef();  // Normally called by RunFileChooser().
  file_select_helper->ContentAnalysisCompletionCallback(std::move(orig_files),
                                                        data, result);

  ASSERT_EQ(3u, files.size());
  EXPECT_TRUE(files[0]->is_file_system());
  EXPECT_TRUE(files[1]->is_native_file());
  EXPECT_EQ(data_dir_.AppendASCII("bar.doc"),
            files[1]->get_native_file()->file_path);
  EXPECT_TRUE(files[2]->is_file_system());
}

TEST_F(FileSelectHelperTest,
       ContentAnalysisCompletionCallback_FolderUpload_OK) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  scoped_refptr<FileSelectHelper> file_select_helper =
      new FileSelectHelper(&profile);

  std::vector<blink::mojom::FileChooserFileInfoPtr> files;
  auto listener = base::MakeRefCounted<TestFileSelectListener>(&files);
  file_select_helper->SetFileSelectListenerForTesting(std::move(listener));
  file_select_helper->DontAbortOnMissingWebContentsForTesting();

  std::vector<blink::mojom::FileChooserFileInfoPtr> orig_files;
  enterprise_connectors::ContentAnalysisDelegate::Data data;
  enterprise_connectors::ContentAnalysisDelegate::Result result;

  // Set the dialog type to folder upload to test folder handling logic.
  file_select_helper->dialog_type_ = ui::SelectFileDialog::SELECT_UPLOAD_FOLDER;
  PrepareContentAnalysisCompletionCallbackArgs(
      {data_dir_.AppendASCII("foo.doc"), data_dir_.AppendASCII("bar.doc")},
      {true, true}, &orig_files, &data, &result);

  // Calling the content analysis completion callback would normally
  // release `file_select_helper`, so we add a reference and validate that
  // it goes down to 1 after the call.
  file_select_helper->AddRef();
  EXPECT_FALSE(file_select_helper->HasOneRef());
  file_select_helper->ContentAnalysisCompletionCallback(std::move(orig_files),
                                                        data, result);

  EXPECT_TRUE(file_select_helper->HasOneRef());
  EXPECT_EQ(2u, files.size());
}

TEST_F(FileSelectHelperTest,
       ContentAnalysisCompletionCallback_FolderUpload_Bad) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  scoped_refptr<FileSelectHelper> file_select_helper =
      new FileSelectHelper(&profile);

  std::vector<blink::mojom::FileChooserFileInfoPtr> files;
  auto listener = base::MakeRefCounted<TestFileSelectListener>(&files);
  file_select_helper->SetFileSelectListenerForTesting(std::move(listener));
  file_select_helper->DontAbortOnMissingWebContentsForTesting();

  std::vector<blink::mojom::FileChooserFileInfoPtr> orig_files;
  enterprise_connectors::ContentAnalysisDelegate::Data data;
  enterprise_connectors::ContentAnalysisDelegate::Result result;

  // Set the dialog type to folder upload to test folder handling logic.
  file_select_helper->dialog_type_ = ui::SelectFileDialog::SELECT_UPLOAD_FOLDER;
  PrepareContentAnalysisCompletionCallbackArgs(
      {data_dir_.AppendASCII("foo.doc"), data_dir_.AppendASCII("bar.doc")},
      {false, false}, &orig_files, &data, &result);

  // Calling the content analysis completion callback would normally
  // release `file_select_helper`, so we add a reference and validate that
  // it goes down to 1 after the call.
  file_select_helper->AddRef();
  EXPECT_FALSE(file_select_helper->HasOneRef());
  file_select_helper->ContentAnalysisCompletionCallback(std::move(orig_files),
                                                        data, result);
  EXPECT_TRUE(file_select_helper->HasOneRef());
  EXPECT_EQ(0u, files.size());
}

TEST_F(FileSelectHelperTest,
       ContentAnalysisCompletionCallback_FolderUpload_OKBad) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  scoped_refptr<FileSelectHelper> file_select_helper =
      new FileSelectHelper(&profile);

  std::vector<blink::mojom::FileChooserFileInfoPtr> files;
  auto listener = base::MakeRefCounted<TestFileSelectListener>(&files);
  file_select_helper->SetFileSelectListenerForTesting(std::move(listener));
  file_select_helper->DontAbortOnMissingWebContentsForTesting();

  std::vector<blink::mojom::FileChooserFileInfoPtr> orig_files;
  enterprise_connectors::ContentAnalysisDelegate::Data data;
  enterprise_connectors::ContentAnalysisDelegate::Result result;

  // Set the dialog type to folder upload to test folder handling logic.
  file_select_helper->dialog_type_ = ui::SelectFileDialog::SELECT_UPLOAD_FOLDER;
  PrepareContentAnalysisCompletionCallbackArgs(
      {data_dir_.AppendASCII("foo.doc"), data_dir_.AppendASCII("bar.doc")},
      {true, false}, &orig_files, &data, &result);

  // Calling the content analysis completion callback would normally
  // release `file_select_helper`, so we add a reference and validate that
  // it goes down to 1 after the call.
  file_select_helper->AddRef();
  EXPECT_FALSE(file_select_helper->HasOneRef());
  file_select_helper->ContentAnalysisCompletionCallback(std::move(orig_files),
                                                        data, result);

  EXPECT_TRUE(file_select_helper->HasOneRef());
  // Files should be cleared.
  EXPECT_EQ(0u, files.size());
}

TEST_F(FileSelectHelperTest, GetFileTypesFromAcceptType) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  scoped_refptr<FileSelectHelper> file_select_helper =
      new FileSelectHelper(&profile);

  std::vector<std::u16string> accept_types{
      // normal file extension
      u".mp4",
      // file extension with some chinese
      u".斤拷锟",
      // file extension with fire emoji
      u".🔥",
      // mime type
      u"image/png",
      // non-ascii mime type which should be ignored
      u"text/斤拷锟"};

  std::unique_ptr<ui::SelectFileDialog::FileTypeInfo> file_type_info =
      file_select_helper->GetFileTypesFromAcceptType(accept_types);

  std::vector<std::vector<base::FilePath::StringType>> expected_extensions{
      std::vector<base::FilePath::StringType>{
#if BUILDFLAG(IS_WIN)
          L"mp4", L"斤拷锟", L"🔥", L"png"}};
#else
          "mp4", "斤拷锟", "🔥", "png"}};
#endif
  ASSERT_EQ(expected_extensions, file_type_info->extensions);
}

// This test depends on platform-specific mappings from mime types to file
// extensions in PlatformMimeUtil. It would seem that Linux does not offer a way
// to get extensions, and our Windows implementation still needs to be updated.
#if BUILDFLAG(IS_MAC)
TEST_F(FileSelectHelperTest, MultipleFileExtensionsForMime) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  scoped_refptr<FileSelectHelper> file_select_helper =
      new FileSelectHelper(&profile);

  std::vector<std::u16string> accept_types{u"application/vnd.ms-powerpoint"};
  std::unique_ptr<ui::SelectFileDialog::FileTypeInfo> file_type_info =
      file_select_helper->GetFileTypesFromAcceptType(accept_types);

  std::vector<base::FilePath::StringType> expected_extensions {
#if BUILDFLAG(IS_WIN)
    L"ppt", L"pot", L"pps"
  };
#else
    "ppt", "pot", "pps"
  };
#endif
  std::sort(expected_extensions.begin(), expected_extensions.end());

  ASSERT_EQ(file_type_info->extensions.size(), 1u);
  std::vector<base::FilePath::StringType> actual_extensions =
      file_type_info->extensions[0];
  std::sort(actual_extensions.begin(), actual_extensions.end());

  EXPECT_EQ(expected_extensions, actual_extensions);
}
#endif

#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
