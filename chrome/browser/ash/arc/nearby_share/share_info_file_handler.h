// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_SHARE_INFO_FILE_HANDLER_H_
#define CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_SHARE_INFO_FILE_HANDLER_H_

#include <cstdint>
#include <list>
#include <memory>
#include <vector>

#include "ash/components/arc/mojom/nearby_share.mojom.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/nearby_share/share_info_file_stream_adapter.h"
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
  // Signifies whether all shared files have started streaming successfully.
  using StartedCallback = base::OnceCallback<void(void)>;

  // |result| signifies state of shared files after streaming has completed.
  using CompletedCallback =
      base::OnceCallback<void(std::optional<base::File::Error> result)>;

  // |value| is a percentage from 0 to 1 in double format (e.g. 0.50 for 50%).
  using ProgressBarUpdateCallback = base::RepeatingCallback<void(double value)>;

  // |profile| is the current user profile.
  // |share_info| represents the data being shared.
  // |directory| is the top level share directory. The owner of this class
  // object will be required to clean up |directory| including any
  // subdirectories and files within it.
  // |task_runner| is used for any cleanup which requires disk IO.
  ShareInfoFileHandler(Profile* profile,
                       mojom::ShareIntentInfo* share_info,
                       base::FilePath directory,
                       scoped_refptr<base::SequencedTaskRunner> task_runner);

  ShareInfoFileHandler(const ShareInfoFileHandler&) = delete;
  ShareInfoFileHandler& operator=(const ShareInfoFileHandler&) = delete;

  // Returns FileSystemURL for a given |context| and |url|.
  static file_manager::util::FileSystemURLAndHandle GetFileSystemURL(
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
  // |started_callback| is called when all files have successfully started
  // streaming.
  // |completed_callback| is called when file streaming is completed with
  // either error or success.
  // |update_callback| is for updating a progress bar view value if needed
  // (e.g. views::ProgressBar::SetValue(double)).
  void StartPreparingFiles(StartedCallback started_callback,
                           CompletedCallback completed_callback,
                           ProgressBarUpdateCallback update_callback);

 private:
  friend class base::RefCountedThreadSafe<ShareInfoFileHandler>;

  ~ShareInfoFileHandler();

  // Create local unique share directory for cache files.
  base::FilePath CreateShareDirectory();

  // Called when share directory path is created and can start streaming files.
  void OnShareDirectoryPathCreated(base::FilePath share_dir);

  // Create file with create and write flags and return scoped fd.
  base::ScopedFD CreateFileForWrite(const base::FilePath& file_path);

  // Called when destination file descriptor is created and ready for file
  // contents to be streamed into it.
  void OnFileDescriptorCreated(const GURL& url,
                               const base::FilePath& dest_file_path,
                               const int64_t file_size,
                               base::ScopedFD dest_fd);

  // Called when the raw bytes of files have completed streaming from ARC
  // virtual filesystem to Chrome local path.
  void OnFileStreamReadCompleted(
      const std::string& url_str,
      std::list<scoped_refptr<ShareInfoFileStreamAdapter>>::iterator it_adapter,
      const std::string& file_system_id,
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
  FileShareConfig file_config_;
  StartedCallback started_callback_;
  CompletedCallback completed_callback_;
  ProgressBarUpdateCallback update_callback_;

  // Updated by multiple stream adapters on UI thread.
  uint64_t num_bytes_read_ = 0;
  size_t num_files_streamed_ = 0;

  // Track whether a file sharing flow has started .
  bool file_sharing_started_ = false;

  // Unowned pointer to profile.
  const raw_ptr<Profile> profile_;

  // Runner for tasks that may require disk IO.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Timeout timer for asynchronous file streaming tasks.
  base::OneShotTimer file_streaming_timer_;

  // Time when the file streaming is started.
  base::TimeTicks file_streaming_started_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShareInfoFileHandler> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_SHARE_INFO_FILE_HANDLER_H_
