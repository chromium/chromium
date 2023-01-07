// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_SYSTEM_INDEXER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_SYSTEM_INDEXER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace base {
class FilePath;
class FileEnumerator;
class Time;
}

class DevToolsFileSystemIndexer
    : public base::RefCountedThreadSafe<DevToolsFileSystemIndexer> {
 public:

  typedef base::OnceCallback<void(int)> TotalWorkCallback;
  typedef base::RepeatingCallback<void(int)> WorkedCallback;
  typedef base::OnceCallback<void()> DoneCallback;
  typedef base::OnceCallback<void(const std::vector<std::string>&)> SearchCallback;

  class FileSystemIndexingJob
      : public base::RefCountedThreadSafe<FileSystemIndexingJob> {
   public:
    void Stop();

   private:
    friend class base::RefCountedThreadSafe<FileSystemIndexingJob>;
    friend class DevToolsFileSystemIndexer;
    FileSystemIndexingJob(const base::FilePath& file_system_path,
                          const std::vector<base::FilePath>& excluded_folders,
                          TotalWorkCallback total_work_callback,
                          const WorkedCallback& worked_callback,
                          DoneCallback done_callback);
    virtual ~FileSystemIndexingJob();

    void Start();
    void StopOnImplSequence();
    void CollectFilesToIndex();
    void IndexFiles();
    void ReadFromFile();
    void OnRead(base::File::Error error,
                const char* data,
                int bytes_read);
    void FinishFileIndexing(bool success);
    void CloseFile();
    void ReportWorked();

    base::FilePath file_system_path_;
    std::vector<base::FilePath> excluded_folders_;

    std::vector<base::FilePath> pending_folders_;
    TotalWorkCallback total_work_callback_;
    WorkedCallback worked_callback_;
    DoneCallback done_callback_;
    std::unique_ptr<base::FileEnumerator> file_enumerator_;
    typedef std::map<base::FilePath, base::Time> FilePathTimesMap;
    FilePathTimesMap file_path_times_;
    FilePathTimesMap::const_iterator indexing_it_;
    base::File current_file_;
    int64_t current_file_offset_;
    typedef int32_t Trigram;
    std::vector<Trigram> current_trigrams_;
    // The index in this vector is the trigram id.
    std::vector<bool> current_trigrams_set_;
    base::TimeTicks last_worked_notification_time_;
    int files_indexed_;
    bool stopped_;
  };

  DevToolsFileSystemIndexer();

  DevToolsFileSystemIndexer(const DevToolsFileSystemIndexer&) = delete;
  DevToolsFileSystemIndexer& operator=(const DevToolsFileSystemIndexer&) =
      delete;

  // Performs file system indexing for given |file_system_path| and sends
  // progress callbacks.
  scoped_refptr<FileSystemIndexingJob> IndexPath(
      const std::string& file_system_path,
      const std::vector<std::string>& excluded_folders,
      TotalWorkCallback total_work_callback,
      const WorkedCallback& worked_callback,
      DoneCallback done_callback);

  // Performs trigram search for given |query| in |file_system_path|.
  void SearchInPath(const std::string& file_system_path,
                    const std::string& query,
                    SearchCallback callback);

 private:
  friend class base::RefCountedThreadSafe<DevToolsFileSystemIndexer>;

  virtual ~DevToolsFileSystemIndexer();

  void SearchInPathOnImplSequence(const std::string& file_system_path,
                                  const std::string& query,
                                  SearchCallback callback);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_SYSTEM_INDEXER_H_
