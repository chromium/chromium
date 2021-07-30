// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_SHARE_INFO_FILE_HANDLER_H_
#define CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_SHARE_INFO_FILE_HANDLER_H_

#include <cstdint>
#include <list>
#include <memory>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/nearby_share/share_info_file_stream_adapter.h"
#include "components/arc/mojom/nearby_share.mojom.h"
#include "content/public/browser/browser_thread.h"

class GURL;
class Profile;

namespace storage {
class FileSystemContext;
class FileSystemURL;
}  // namespace storage

namespace content {
class BrowserContext;
}  // namespace content

namespace file_manager {
namespace util {
struct FileSystemURLAndHandle;
}  // namespace util
}  // namespace file_manager

namespace arc {

// ShareInfoFileHandler will process share intent from Android and only handle
// share info and respective FileInfo objects for files. The class will process
// Android ContentProvider URIs and convert them FileSystemURLs used by Chrome
// OS virtual filesystem backends.  It contains a list of file stream adapters
// and dispatches them in a separate task from the main pool to obtain the raw
// bytes of the virtual files. All file IO operations are performed on a
// separate task.  This class is created on the UI thread but deleted on the
// IO thread.  FileShareConfig is a convenience subclass for containing related
// files information needed to create a Chrome share intent.
class ShareInfoFileHandler
    : public base::RefCountedThreadSafe<ShareInfoFileHandler> {
 public:
  // |result| signifies state of shared files after streaming has completed.
  using CompletedCallback =
      base::OnceCallback<void(absl::optional<base::File::Error> result)>;

  // |value| is a percentage from 0 to 1 in double format (e.g. 0.50 for 50%).
  using ProgressBarUpdateCallback = base::RepeatingCallback<void(double value)>;

  // |profile| is the current user profile.
  // |share_info| represents the data being shared.
  // |directory| is the top level share directory.
  // |task_runner| is used for any cleanup which requires disk IO.
  ShareInfoFileHandler(Profile* profile,
                       mojom::ShareIntentInfo* share_info,
                       base::FilePath directory,
                       scoped_refptr<base::SequencedTaskRunner> task_runner);

  ShareInfoFileHandler(const ShareInfoFileHandler&) = delete;
  ShareInfoFileHandler& operator=(const ShareInfoFileHandler&) = delete;

  // Returns FileSystemURL for a given |context| and |url|.
  static file_manager::util::FileSystemURLAndHandle GetFileSystemContext(
      content::BrowserContext* context,
      const GURL& url);

  const std::vector<base::FilePath>& GetFilePaths() const;
  const std::vector<std::string>& GetMimeTypes() const;
  uint64_t GetTotalSizeOfFiles() const { return file_config_.total_size; }
  size_t GetNumberOfFiles() const { return file_config_.num_files; }
  const base::FilePath& GetShareDirectory() const {
    return file_config_.directory;
  }

  // Start streaming virtual files to destination file descriptors in
  // preparation for Nearby Share.  Callbacks are run on the UI thread.
  // |completed_callback| is called when file streaming is completed with
  // either error or success.
  // |update_callback| is for updating a progress bar view value if needed
  // (e.g. views::ProgressBar::SetValue(double)).
  void StartPreparingFiles(CompletedCallback completed_callback,
                           ProgressBarUpdateCallback update_callback);

 private:
  friend class base::RefCountedThreadSafe<ShareInfoFileHandler>;

  ~ShareInfoFileHandler();

  // Start streaming virtual files into local path in scoped temp directory.
  bool CreateDirectoryAndStreamFiles();

  // Create file with create and write flags and return scoped fd.
  base::ScopedFD CreateFileForWrite(const base::FilePath& file_path);

  // Called when temp directory for Nearby Share cached files is created and
  // started streaming files.
  void OnCreatedDirectoryAndStreamingFiles(bool result);

  // Called when the raw bytes of files have completed streaming from ARC VFS
  // to Chrome local path.
  void OnFileStreamReadCompleted(
      const std::string& url_str,
      std::list<scoped_refptr<ShareInfoFileStreamAdapter>>::iterator it,
      const int64_t bytes_read,
      bool result);

  // Called back once file streaming time exceeds the maximum duration.
  void OnFileStreamingTimeout(const std::string& timeout_message);

  // Notify caller that sharing is completed via |completed_callback_| with
  // result which could be FILE_OK or some file error.
  void NotifyFileSharingCompleted(base::File::Error result);

  struct FileShareConfig {
    FileShareConfig();
    FileShareConfig(FileShareConfig&&) = delete;
    FileShareConfig& operator=(FileShareConfig&&) = delete;
    FileShareConfig(const FileShareConfig&) = delete;
    FileShareConfig& operator=(const FileShareConfig&) = delete;
    ~FileShareConfig();

    std::vector<base::FilePath> paths;
    std::vector<GURL> external_urls;
    std::vector<std::string> mime_types;
    std::vector<std::string> names;
    std::vector<int64_t> sizes;

    // Top level directory for all files.
    base::FilePath directory;

    // Total size in bytes for all files.
    uint64_t total_size = 0;

    // Number of files.
    size_t num_files = 0;
  };

  std::list<scoped_refptr<ShareInfoFileStreamAdapter>> file_stream_adapters_;
  std::list<scoped_refptr<storage::FileSystemContext>> contexts_;
  std::list<base::ScopedTempDir> scoped_temp_dirs_;
  FileShareConfig file_config_;
  CompletedCallback completed_callback_;
  ProgressBarUpdateCallback update_callback_;

  // Updated by multiple stream adapters on UI thread.
  uint64_t num_bytes_read_ = 0;
  size_t num_files_streamed_ = 0;

  // Track whether a file sharing flow has started .
  bool file_sharing_started_ = false;

  // Unowned pointer to profile.
  Profile* const profile_;

  // Runner for tasks that may require disk IO.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Timeout timer for asynchronous file streaming tasks.
  base::OneShotTimer file_streaming_timer_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShareInfoFileHandler> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_SHARE_INFO_FILE_HANDLER_H_
