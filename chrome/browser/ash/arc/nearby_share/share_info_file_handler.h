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
class ShareInfoFileHandler : public base::RefCountedThreadSafe<
                                 ShareInfoFileStreamAdapter,
                                 content::BrowserThread::DeleteOnIOThread> {
 public:
  // |result| signifies whether all requested files were streamed successfully.
  using CompletedCallback = base::OnceCallback<void(bool result)>;

  ShareInfoFileHandler(Profile* profile,
                       mojom::ShareIntentInfo* share_info,
                       base::FilePath directory);

  ShareInfoFileHandler(const ShareInfoFileHandler&) = delete;
  ShareInfoFileHandler& operator=(const ShareInfoFileHandler&) = delete;

  // Returns FileSystemURL for a given |context| and |url|.
  static file_manager::util::FileSystemURLAndHandle GetFileSystemContext(
      content::BrowserContext* context,
      const GURL& url);

  const std::vector<base::FilePath>& GetFilePaths() const;
  const std::vector<std::string>& GetMimeTypes() const;
  uint64_t GetTotalSizeOfFiles() const;

  // Start streaming virtual files to destination file descriptors in
  // preparation for Nearby Share.
  void StartPreparingFiles(CompletedCallback callback);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;
  friend class base::DeleteHelper<ShareInfoFileStreamAdapter>;

  ~ShareInfoFileHandler();

  // Start streaming virtual files into local path in scoped temp directory.
  bool CreateDirectoryAndStreamFiles();

  // Create file with create and write flags and return scoped fd.
  base::ScopedFD CreateFileForWrite(const base::FilePath& file_path);

  // Called when temp directory for Nearby Share cached files is created and
  // raw bytes of files are streamed from ARC VFS to Chrome local path.
  void OnCreatedDirectoryAndStreamedFiles(CompletedCallback callback,
                                          bool result);

  void OnFileStreamReadCompleted(
      const std::string& url_str,
      std::list<scoped_refptr<ShareInfoFileStreamAdapter>>::iterator it,
      const int64_t bytes_read,
      bool result);

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
    uint64_t total_size;
  };

  std::list<scoped_refptr<ShareInfoFileStreamAdapter>> file_stream_adapters_;
  std::list<scoped_refptr<storage::FileSystemContext>> contexts_;
  std::list<base::ScopedTempDir> scoped_temp_dirs_;
  FileShareConfig file_config_;

  // Updated by multiple stream adapters on UI thread.
  uint64_t num_bytes_read_;

  // Unowned pointer to profile.
  Profile* const profile_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShareInfoFileHandler> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_SHARE_INFO_FILE_HANDLER_H_
