// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_PROTO_FILE_MANAGER_H_
#define CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_PROTO_FILE_MANAGER_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace apps {
// Writes a file to a given `path` on disk. If the write operation fails,
// the existing data on disk will be unaffected. Returns true if data is
// written to disk successfully and false otherwise.
bool WriteFileToDisk(const base::FilePath& path, const std::string& data);

// The ProtoFileManager is used to store protos on disk
// and read the stored data from disk.
template <typename M>
class ProtoFileManager {
 public:
  explicit ProtoFileManager(base::FilePath& path)
      : path_(path),
        task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {}

  ~ProtoFileManager() = default;
  ProtoFileManager(const ProtoFileManager&) = delete;
  ProtoFileManager& operator=(const ProtoFileManager&) = delete;

  // Writes proto data to a file on disk using another thread.
  void WriteProtoToFile(M proto, base::OnceCallback<void(bool)> callback) {
    // Write is a blocking function, so the task is posted to another thread.
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            [](const base::FilePath& path, const M& proto) {
              return WriteFileToDisk(path, proto.SerializeAsString());
            },
            path_, std::move(proto)),
        std::move(callback));
  }

  // Reads proto data from the file path on disk using another thread.
  void ReadProtoFromFile(base::OnceCallback<void(std::optional<M>)> callback) {
    // Read is a blocking function, so the task is posted to another thread.
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            [](const base::FilePath& path) -> std::optional<M> {
              std::string serialized_data;
              if (!base::ReadFileToString(path, &serialized_data)) {
                LOG(ERROR) << "Reading file from disk failed: " << path.value();
                return std::nullopt;
              }

              M proto;
              if (!proto.ParseFromString(serialized_data)) {
                LOG(ERROR) << "Parsing proto failed: " << path.value();
                return std::nullopt;
              }

              return proto;
            },
            path_),
        std::move(callback));
  }

 private:
  // Absolute path to the file where data is stored.
  base::FilePath path_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};
}  // namespace apps

#endif  // CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_PROTO_FILE_MANAGER_H_
