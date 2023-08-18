// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <ImageCaptureCore/ImageCaptureCore.h>
#include "base/files/file_path.h"

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/media_galleries/mac/mtp_device_delegate_impl_mac.h"
#include "components/storage_monitor/image_capture_device_manager.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kDeviceId[] = "id";
const char kDevicePath[] = "/ic:id";
const char kTestFileContents[] = "test";

}  // namespace

@interface ICCameraDevice ()
- (instancetype)initWithDictionary:(id)properties NS_DESIGNATED_INITIALIZER;
@end

@interface MockMTPICCameraDevice : ICCameraDevice {
 @private
  NSMutableArray* __strong _allMediaFiles;
}

- (void)addMediaFile:(ICCameraFile*)file;

@end

@implementation MockMTPICCameraDevice

- (instancetype)init {
  if (self = [super initWithDictionary:@{}]) {
    _allMediaFiles = [NSMutableArray array];
  }
  return self;
}

- (NSString*)mountPoint {
  return @"mountPoint";
}

- (NSString*)name {
  return @"name";
}

- (NSString*)UUIDString {
  return base::SysUTF8ToNSString(kDeviceId);
}

- (ICDeviceType)type {
  return ICDeviceTypeCamera;
}

- (void)requestOpenSession {
}

- (void)requestCloseSession {
}

- (NSArray*)mediaFiles {
  return _allMediaFiles;
}

- (void)addMediaFile:(ICCameraFile*)file {
  [_allMediaFiles addObject:file];
}

- (void)requestDownloadFile:(ICCameraFile*)file
                    options:(NSDictionary*)options
           downloadDelegate:(id<ICCameraDeviceDownloadDelegate>)downloadDelegate
        didDownloadSelector:(SEL)selector
                contextInfo:(void*)contextInfo {
  base::FilePath saveDir =
      base::apple::NSURLToFilePath(options[ICDownloadsDirectoryURL]);
  std::string saveAsFilename =
      base::SysNSStringToUTF8(options[ICSaveAsFilename]);
  // It appears that the ImageCapture library adds an extension to the requested
  // filename. Do that here to require a rename.
  saveAsFilename += ".jpg";
  base::FilePath toBeSaved = saveDir.Append(saveAsFilename);
  ASSERT_TRUE(base::WriteFile(toBeSaved, kTestFileContents));

  NSMutableDictionary* returnOptions = [options mutableCopy];
  returnOptions[ICSavedFilename] = base::SysUTF8ToNSString(saveAsFilename);

  [static_cast<NSObject<ICCameraDeviceDownloadDelegate>*>(downloadDelegate)
   didDownloadFile:file
             error:nil
           options:returnOptions
       contextInfo:contextInfo];
}

@end

@interface MockMTPICCameraFile : ICCameraFile {
 @private
  NSString* __strong _name;
  NSDate* __strong _date;
}

- (instancetype)init:(NSString*)name;

@end

@implementation MockMTPICCameraFile

- (instancetype)init:(NSString*)name {
  if ((self = [super init])) {
    NSDateFormatter* iso8601day = [[NSDateFormatter alloc] init];
    iso8601day.dateFormat = @"yyyy-MM-dd";
    _name = [name copy];
    _date = [iso8601day dateFromString:@"2012-12-12"];
  }
  return self;
}

- (NSString*)name {
  return _name;
}

- (NSString*)UTI {
  return base::apple::CFToNSPtrCast(kUTTypeImage);
}

- (NSDate*)modificationDate {
  return _date;
}

- (NSDate*)creationDate {
  return _date;
}

- (off_t)fileSize {
  return 1000;
}

@end

class MTPDeviceDelegateImplMacTest : public testing::Test {
 public:
  MTPDeviceDelegateImplMacTest() = default;

  MTPDeviceDelegateImplMacTest(const MTPDeviceDelegateImplMacTest&) = delete;
  MTPDeviceDelegateImplMacTest& operator=(const MTPDeviceDelegateImplMacTest&) =
      delete;

  void SetUp() override {
    storage_monitor::TestStorageMonitor* monitor =
        storage_monitor::TestStorageMonitor::CreateAndInstall();
    manager_.SetNotifications(monitor->receiver());

    camera_ = [[MockMTPICCameraDevice alloc] init];
    id<ICDeviceBrowserDelegate> delegate = manager_.device_browser_delegate();
    [delegate deviceBrowser:manager_.device_browser_for_test()
               didAddDevice:camera_
                 moreComing:NO];

    delegate_ = new MTPDeviceDelegateImplMac(kDeviceId, kDevicePath);
  }

  void TearDown() override {
    id<ICDeviceBrowserDelegate> delegate = manager_.device_browser_delegate();
    [delegate deviceBrowser:manager_.device_browser_for_test()
            didRemoveDevice:camera_
                  moreGoing:NO];

    delegate_->CancelPendingTasksAndDeleteDelegate();

    storage_monitor::TestStorageMonitor::Destroy();
  }

  void OnError(base::WaitableEvent* event, base::File::Error error) {
    error_ = error;
    event->Signal();
  }

  void OverlappedOnError(base::WaitableEvent* event,
                         base::File::Error error) {
    overlapped_error_ = error;
    event->Signal();
  }

  void OnFileInfo(base::WaitableEvent* event,
                  const base::File::Info& info) {
    error_ = base::File::FILE_OK;
    info_ = info;
    event->Signal();
  }

  void OnReadDir(base::WaitableEvent* event,
                 storage::AsyncFileUtil::EntryList files,
                 bool has_more) {
    error_ = base::File::FILE_OK;
    ASSERT_FALSE(has_more);
    file_list_ = std::move(files);
    event->Signal();
  }

  void OverlappedOnReadDir(base::WaitableEvent* event,
                           storage::AsyncFileUtil::EntryList files,
                           bool has_more) {
    overlapped_error_ = base::File::FILE_OK;
    ASSERT_FALSE(has_more);
    overlapped_file_list_ = std::move(files);
    event->Signal();
  }

  void OnDownload(base::WaitableEvent* event,
                  const base::File::Info& file_info,
                  const base::FilePath& local_path) {
    error_ = base::File::FILE_OK;
    event->Signal();
  }

  base::File::Error GetFileInfo(const base::FilePath& path,
                                base::File::Info* info) {
    base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    delegate_->GetFileInfo(
        path,
        base::BindOnce(&MTPDeviceDelegateImplMacTest::OnFileInfo,
                       base::Unretained(this), &wait),
        base::BindOnce(&MTPDeviceDelegateImplMacTest::OnError,
                       base::Unretained(this), &wait));
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(wait.IsSignaled());
    *info = info_;
    return error_;
  }

  base::File::Error ReadDir(const base::FilePath& path) {
    base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    delegate_->ReadDirectory(
        path,
        base::BindRepeating(&MTPDeviceDelegateImplMacTest::OnReadDir,
                            base::Unretained(this), &wait),
        base::BindOnce(&MTPDeviceDelegateImplMacTest::OnError,
                       base::Unretained(this), &wait));
    task_environment_.RunUntilIdle();
    wait.Wait();
    return error_;
  }

  base::File::Error DownloadFile(
      const base::FilePath& path,
      const base::FilePath& local_path) {
    base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    delegate_->CreateSnapshotFile(
        path, local_path,
        base::BindOnce(&MTPDeviceDelegateImplMacTest::OnDownload,
                       base::Unretained(this), &wait),
        base::BindOnce(&MTPDeviceDelegateImplMacTest::OnError,
                       base::Unretained(this), &wait));
    task_environment_.RunUntilIdle();
    wait.Wait();
    return error_;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  storage_monitor::ImageCaptureDeviceManager manager_;
  MockMTPICCameraDevice* __strong camera_ = nullptr;

  // This object needs special deletion inside the above |task_runner_|.
  raw_ptr<MTPDeviceDelegateImplMac> delegate_ = nullptr;

  base::File::Error error_;
  base::File::Info info_;
  storage::AsyncFileUtil::EntryList file_list_;

  base::File::Error overlapped_error_;
  storage::AsyncFileUtil::EntryList overlapped_file_list_;
};

TEST_F(MTPDeviceDelegateImplMacTest, TestGetRootFileInfo) {
  base::File::Info info;
  // Making a fresh delegate should have a single file entry for the synthetic
  // root directory, with the name equal to the device id string.
  EXPECT_EQ(base::File::FILE_OK,
            GetFileInfo(base::FilePath(kDevicePath), &info));
  EXPECT_TRUE(info.is_directory);
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            GetFileInfo(base::FilePath("/nonexistent"), &info));

  // Signal the delegate that no files are coming.
  delegate_->NoMoreItems();

  EXPECT_EQ(base::File::FILE_OK, ReadDir(base::FilePath(kDevicePath)));
  EXPECT_EQ(0U, file_list_.size());
}

TEST_F(MTPDeviceDelegateImplMacTest, TestOverlappedReadDir) {
  base::Time time1 = base::Time::Now();
  base::File::Info info1;
  info1.size = 1;
  info1.is_directory = false;
  info1.is_symbolic_link = false;
  info1.last_modified = time1;
  info1.last_accessed = time1;
  info1.creation_time = time1;
  delegate_->ItemAdded("name1", info1);

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);

  delegate_->ReadDirectory(
      base::FilePath(kDevicePath),
      base::BindRepeating(&MTPDeviceDelegateImplMacTest::OnReadDir,
                          base::Unretained(this), &wait),
      base::BindOnce(&MTPDeviceDelegateImplMacTest::OnError,
                     base::Unretained(this), &wait));

  delegate_->ReadDirectory(
      base::FilePath(kDevicePath),
      base::BindRepeating(&MTPDeviceDelegateImplMacTest::OverlappedOnReadDir,
                          base::Unretained(this), &wait),
      base::BindOnce(&MTPDeviceDelegateImplMacTest::OverlappedOnError,
                     base::Unretained(this), &wait));

  // Signal the delegate that no files are coming.
  delegate_->NoMoreItems();

  task_environment_.RunUntilIdle();
  wait.Wait();

  EXPECT_EQ(base::File::FILE_OK, error_);
  EXPECT_EQ(1U, file_list_.size());
  EXPECT_EQ(base::File::FILE_OK, overlapped_error_);
  EXPECT_EQ(1U, overlapped_file_list_.size());
}

TEST_F(MTPDeviceDelegateImplMacTest, TestGetFileInfo) {
  base::Time time1 = base::Time::Now();
  base::File::Info info1;
  info1.size = 1;
  info1.is_directory = false;
  info1.is_symbolic_link = false;
  info1.last_modified = time1;
  info1.last_accessed = time1;
  info1.creation_time = time1;
  delegate_->ItemAdded("name1", info1);

  base::File::Info info;
  EXPECT_EQ(base::File::FILE_OK,
            GetFileInfo(base::FilePath("/ic:id/name1"), &info));
  EXPECT_EQ(info1.size, info.size);
  EXPECT_EQ(info1.is_directory, info.is_directory);
  EXPECT_EQ(info1.last_modified, info.last_modified);
  EXPECT_EQ(info1.last_accessed, info.last_accessed);
  EXPECT_EQ(info1.creation_time, info.creation_time);

  info1.size = 2;
  delegate_->ItemAdded("name2", info1);
  delegate_->NoMoreItems();

  EXPECT_EQ(base::File::FILE_OK,
            GetFileInfo(base::FilePath("/ic:id/name2"), &info));
  EXPECT_EQ(info1.size, info.size);

  EXPECT_EQ(base::File::FILE_OK, ReadDir(base::FilePath(kDevicePath)));

  ASSERT_EQ(2U, file_list_.size());
  EXPECT_EQ(filesystem::mojom::FsFileType::REGULAR_FILE, file_list_[0].type);
  EXPECT_EQ("name1", file_list_[0].name.value());

  EXPECT_EQ(filesystem::mojom::FsFileType::REGULAR_FILE, file_list_[1].type);
  EXPECT_EQ("name2", file_list_[1].name.value());
}

TEST_F(MTPDeviceDelegateImplMacTest, TestDirectoriesAndSorting) {
  base::Time time1 = base::Time::Now();
  base::File::Info info1;
  info1.size = 1;
  info1.is_directory = false;
  info1.is_symbolic_link = false;
  info1.last_modified = time1;
  info1.last_accessed = time1;
  info1.creation_time = time1;
  delegate_->ItemAdded("name2", info1);

  info1.is_directory = true;
  delegate_->ItemAdded("dir2", info1);
  delegate_->ItemAdded("dir1", info1);

  info1.is_directory = false;
  delegate_->ItemAdded("name1", info1);
  delegate_->NoMoreItems();

  EXPECT_EQ(base::File::FILE_OK, ReadDir(base::FilePath(kDevicePath)));

  ASSERT_EQ(4U, file_list_.size());
  EXPECT_EQ("dir1", file_list_[0].name.value());
  EXPECT_EQ("dir2", file_list_[1].name.value());
  EXPECT_EQ(filesystem::mojom::FsFileType::REGULAR_FILE, file_list_[2].type);
  EXPECT_EQ("name1", file_list_[2].name.value());

  EXPECT_EQ(filesystem::mojom::FsFileType::REGULAR_FILE, file_list_[3].type);
  EXPECT_EQ("name2", file_list_[3].name.value());
}

TEST_F(MTPDeviceDelegateImplMacTest, SubDirectories) {
  base::Time time1 = base::Time::Now();
  base::File::Info info1;
  info1.size = 0;
  info1.is_directory = true;
  info1.is_symbolic_link = false;
  info1.last_modified = time1;
  info1.last_accessed = time1;
  info1.creation_time = time1;
  delegate_->ItemAdded("dir1", info1);

  info1.size = 1;
  info1.is_directory = false;
  info1.is_symbolic_link = false;
  info1.last_modified = time1;
  info1.last_accessed = time1;
  info1.creation_time = time1;
  delegate_->ItemAdded("dir1/name1", info1);

  info1.is_directory = true;
  info1.size = 0;
  delegate_->ItemAdded("dir2", info1);

  info1.is_directory = false;
  info1.size = 1;
  delegate_->ItemAdded("dir2/name2", info1);

  info1.is_directory = true;
  info1.size = 0;
  delegate_->ItemAdded("dir2/subdir", info1);

  info1.is_directory = false;
  info1.size = 1;
  delegate_->ItemAdded("dir2/subdir/name3", info1);
  delegate_->ItemAdded("name4", info1);

  delegate_->NoMoreItems();

  EXPECT_EQ(base::File::FILE_OK, ReadDir(base::FilePath(kDevicePath)));
  ASSERT_EQ(3U, file_list_.size());
  EXPECT_EQ(filesystem::mojom::FsFileType::DIRECTORY, file_list_[0].type);
  EXPECT_EQ("dir1", file_list_[0].name.value());
  EXPECT_EQ(filesystem::mojom::FsFileType::DIRECTORY, file_list_[1].type);
  EXPECT_EQ("dir2", file_list_[1].name.value());
  EXPECT_EQ(filesystem::mojom::FsFileType::REGULAR_FILE, file_list_[2].type);
  EXPECT_EQ("name4", file_list_[2].name.value());

  EXPECT_EQ(base::File::FILE_OK,
            ReadDir(base::FilePath(kDevicePath).Append("dir1")));
  ASSERT_EQ(1U, file_list_.size());
  EXPECT_EQ(filesystem::mojom::FsFileType::REGULAR_FILE, file_list_[0].type);
  EXPECT_EQ("name1", file_list_[0].name.value());

  EXPECT_EQ(base::File::FILE_OK,
            ReadDir(base::FilePath(kDevicePath).Append("dir2")));
  ASSERT_EQ(2U, file_list_.size());
  EXPECT_EQ(filesystem::mojom::FsFileType::REGULAR_FILE, file_list_[0].type);
  EXPECT_EQ("name2", file_list_[0].name.value());
  EXPECT_EQ(filesystem::mojom::FsFileType::DIRECTORY, file_list_[1].type);
  EXPECT_EQ("subdir", file_list_[1].name.value());

  EXPECT_EQ(base::File::FILE_OK,
            ReadDir(base::FilePath(kDevicePath)
                    .Append("dir2").Append("subdir")));
  ASSERT_EQ(1U, file_list_.size());
  EXPECT_EQ(filesystem::mojom::FsFileType::REGULAR_FILE, file_list_[0].type);
  EXPECT_EQ("name3", file_list_[0].name.value());

  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            ReadDir(base::FilePath(kDevicePath)
                    .Append("dir2").Append("subdir").Append("subdir")));
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            ReadDir(base::FilePath(kDevicePath)
                    .Append("dir3").Append("subdir")));
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            ReadDir(base::FilePath(kDevicePath).Append("dir3")));
}

TEST_F(MTPDeviceDelegateImplMacTest, TestDownload) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::Time t1 = base::Time::Now();
  base::File::Info info;
  info.size = 4;
  info.is_directory = false;
  info.is_symbolic_link = false;
  info.last_modified = t1;
  info.last_accessed = t1;
  info.creation_time = t1;
  std::string kTestFileName("filename");
  MockMTPICCameraFile* picture1 =
      [[MockMTPICCameraFile alloc] init:base::SysUTF8ToNSString(kTestFileName)];
  [camera_ addMediaFile:picture1];
  delegate_->ItemAdded(kTestFileName, info);
  delegate_->NoMoreItems();

  EXPECT_EQ(base::File::FILE_OK, ReadDir(base::FilePath(kDevicePath)));
  ASSERT_EQ(1U, file_list_.size());
  ASSERT_EQ("filename", file_list_[0].name.value());

  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            DownloadFile(base::FilePath("/ic:id/nonexist"),
                         temp_dir_.GetPath().Append("target")));

  EXPECT_EQ(base::File::FILE_OK,
            DownloadFile(base::FilePath("/ic:id/filename"),
                         temp_dir_.GetPath().Append("target")));
  std::string contents;
  EXPECT_TRUE(
      base::ReadFileToString(temp_dir_.GetPath().Append("target"), &contents));
  EXPECT_EQ(kTestFileContents, contents);
}
