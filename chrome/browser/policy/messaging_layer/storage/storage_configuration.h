// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_CONFIGURATION_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_CONFIGURATION_H_

namespace reporting {

// Storage options class allowing to set parameters individually, e.g.:
// Storage::Create(Options()
//                     .set_directory("/var/cache/reporting")
//                     .set_max_record_size(4 * 1024)
//                     .set_max_total_files_size(64 * 1024 * 1024)
//                     .set_max_total_memory_size(256 * 1024),
//                 callback);
class StorageOptions {
 public:
  StorageOptions() = default;
  StorageOptions(const StorageOptions& options) = default;
  StorageOptions& operator=(const StorageOptions& options) = default;
  StorageOptions& set_directory(const base::FilePath& directory) {
    directory_ = directory;
    return *this;
  }
  StorageOptions& set_max_record_size(size_t max_record_size) {
    max_record_size_ = max_record_size;
    return *this;
  }
  StorageOptions& set_max_total_files_size(int64_t max_total_files_size) {
    max_total_files_size_ = max_total_files_size;
    return *this;
  }
  StorageOptions& set_max_total_memory_size(int64_t max_total_memory_size) {
    max_total_memory_size_ = max_total_memory_size;
    return *this;
  }
  StorageOptions& set_single_file_size(int64_t single_file_size) {
    single_file_size_ = single_file_size;
    return *this;
  }
  const base::FilePath& directory() const { return directory_; }
  size_t max_record_size() const { return max_record_size_; }
  int64_t max_total_files_size() const { return max_total_files_size_; }
  int64_t max_total_memory_size() const { return max_total_memory_size_; }
  int64_t single_file_size() const { return single_file_size_; }

 private:
  // Subdirectory of the location assigned for this Storage.
  base::FilePath directory_;

  // Maximum record size.
  int64_t max_record_size_ = 1 * 1024LL * 1024LL;  // 1 MiB

  // Maximum total size of all files in all queues.
  int64_t max_total_files_size_ = 64 * 1024LL * 1024LL;  // 64 MiB

  // Maximum memory usage (reading buffers).
  int64_t max_total_memory_size_ = 4 * 1024LL * 1024LL;  // 4 MiB

  // Cut-off size of an individual file in all queues.
  // When file exceeds this size, the new file is created
  // for further records. Note that each file must have at least
  // one record before it is closed, regardless of that record size.
  int64_t single_file_size_ = 1 * 1024LL * 1024LL;  // 1 MiB
};

// Single queue options class allowing to set parameters individually, e.g.:
// StorageQueue::Create(QueueOptions(storage_options)
//                  .set_subdirectory("reporting")
//                  .set_file_prefix(FILE_PATH_LITERAL("p00000001")),
//                 callback);
// storage_options must outlive QueueOptions.
class QueueOptions {
 public:
  explicit QueueOptions(const StorageOptions& storage_options)
      : storage_options_(storage_options) {}
  QueueOptions(const QueueOptions& options) = default;
  //   QueueOptions& operator=(const QueueOptions& options) = default;
  QueueOptions& set_subdirectory(
      const base::FilePath::StringType& subdirectory) {
    directory_ = storage_options_.directory().Append(subdirectory);
    return *this;
  }
  QueueOptions& set_file_prefix(const base::FilePath::StringType& file_prefix) {
    file_prefix_ = file_prefix;
    return *this;
  }
  QueueOptions& set_upload_period(base::TimeDelta upload_period) {
    upload_period_ = upload_period;
    return *this;
  }
  const base::FilePath& directory() const { return directory_; }
  const base::FilePath::StringType& file_prefix() const { return file_prefix_; }
  size_t max_record_size() const { return storage_options_.max_record_size(); }
  size_t max_total_files_size() const {
    return storage_options_.max_total_files_size();
  }
  size_t max_total_memory_size() const {
    return storage_options_.max_total_memory_size();
  }
  int64_t single_file_size() const {
    return storage_options_.single_file_size();
  }
  base::TimeDelta upload_period() const { return upload_period_; }

 private:
  // Whole storage options, which this queue options are based on.
  const StorageOptions& storage_options_;

  // Subdirectory of the Storage location assigned for this StorageQueue.
  base::FilePath directory_;
  // Prefix of data files assigned for this StorageQueue.
  base::FilePath::StringType file_prefix_;
  // Time period the data is uploaded with.
  // If 0, uploaded immediately after a new record is stored
  // (this setting is intended for the immediate priority).
  // Can be set to infinity - in that case Flush() is expected to be
  // called from time to time.
  base::TimeDelta upload_period_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_CONFIGURATION_H_
