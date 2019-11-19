// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/experimental/supervised_user_blacklist.h"

#include <algorithm>
#include <cstring>
#include <fstream>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/task/post_task.h"
#include "url/gurl.h"

namespace {

std::unique_ptr<std::vector<SupervisedUserBlacklist::Hash>>
ReadFromBinaryFileOnFileThread(const base::FilePath& path) {
  std::unique_ptr<std::vector<SupervisedUserBlacklist::Hash>> host_hashes(
      new std::vector<SupervisedUserBlacklist::Hash>);

  base::MemoryMappedFile file;
  if (!file.Initialize(path))
    return host_hashes;

  size_t size = file.length();
  if (size <= 0 || size % base::kSHA1Length != 0)
    return host_hashes;

  size_t hash_count = size / base::kSHA1Length;
  host_hashes->resize(hash_count);

  for (size_t i = 0; i < hash_count; i++) {
    memcpy((*host_hashes.get())[i].data,
           file.data() + i * base::kSHA1Length,
           base::kSHA1Length);
  }

  std::sort(host_hashes->begin(), host_hashes->end());

  return host_hashes;
}

} // namespace

SupervisedUserBlacklist::Hash::Hash(const std::string& host) {
  const unsigned char* host_bytes =
      reinterpret_cast<const unsigned char*>(host.c_str());
  base::SHA1HashBytes(host_bytes, host.length(), data);
}

bool SupervisedUserBlacklist::Hash::operator<(const Hash& rhs) const {
  return memcmp(data, rhs.data, base::kSHA1Length) < 0;
}

SupervisedUserBlacklist::SupervisedUserBlacklist() {}

SupervisedUserBlacklist::~SupervisedUserBlacklist() {}

bool SupervisedUserBlacklist::HasURL(const GURL& url) const {
  Hash hash(url.host());
  return std::binary_search(host_hashes_.begin(), host_hashes_.end(), hash);
}

size_t SupervisedUserBlacklist::GetEntryCount() const {
  return host_hashes_.size();
}

void SupervisedUserBlacklist::ReadFromFile(const base::FilePath& path,
                                           const base::Closure& done_callback) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ReadFromBinaryFileOnFileThread, path),
      base::BindOnce(&SupervisedUserBlacklist::OnReadFromFileCompleted,
                     weak_ptr_factory_.GetWeakPtr(), done_callback));
}

void SupervisedUserBlacklist::OnReadFromFileCompleted(
    const base::Closure& done_callback,
    std::unique_ptr<std::vector<Hash>> host_hashes) {
  host_hashes_.swap(*host_hashes);
  LOG_IF(WARNING, host_hashes_.empty()) << "Got empty blacklist";

  if (!done_callback.is_null())
    done_callback.Run();
}
