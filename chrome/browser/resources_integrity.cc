// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resources_integrity.h"

#include <array>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/memory/page_size.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool.h"
#include "chrome/common/chrome_paths.h"
#include "crypto/secure_hash.h"

#if BUILDFLAG(ENABLE_PAK_FILE_INTEGRITY_CHECKS)
#include "chrome/app/packed_resources_integrity.h"
#endif

namespace {

bool CheckResourceIntegrityInternal(
    const base::FilePath& path,
    const base::span<const uint8_t, crypto::kSHA256Length> expected_signature) {
  // Open the file for reading; allowing other consumers to also open it for
  // reading and deleting. Do not allow others to write to it.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_EXCLUSIVE_WRITE |
                            base::File::FLAG_SHARE_DELETE);
  if (!file.IsValid())
    return false;

  auto hash = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  std::vector<char> buffer(base::GetPageSize());

  int bytes_read = 0;
  do {
    bytes_read = file.ReadAtCurrentPos(buffer.data(), buffer.size());
    if (bytes_read == -1)
      return false;
    hash->Update(buffer.data(), bytes_read);
  } while (bytes_read > 0);

  std::array<uint8_t, crypto::kSHA256Length> digest;
  hash->Finish(digest.data(), digest.size());

  return base::ranges::equal(digest, expected_signature);
}

#if BUILDFLAG(ENABLE_PAK_FILE_INTEGRITY_CHECKS)
void ReportPakIntegrity(const std::string& histogram_name, bool hash_matches) {
  base::UmaHistogramBoolean(histogram_name, hash_matches);
}
#endif

}  // namespace

void CheckResourceIntegrity(
    const base::FilePath& path,
    const base::span<const uint8_t, crypto::kSHA256Length> expected_signature,
    base::OnceCallback<void(bool)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CheckResourceIntegrityInternal, path, expected_signature),
      std::move(callback));
}

#if BUILDFLAG(ENABLE_PAK_FILE_INTEGRITY_CHECKS)
void CheckPakFileIntegrity() {
  base::FilePath resources_pack_path;
  base::PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);

  CheckResourceIntegrity(resources_pack_path, kSha256_resources_pak,
                         base::BindOnce(&ReportPakIntegrity,
                                        "SafeBrowsing.PakIntegrity.Resources"));
  CheckResourceIntegrity(
      resources_pack_path.DirName().AppendASCII("chrome_100_percent.pak"),
      kSha256_chrome_100_percent_pak,
      base::BindOnce(&ReportPakIntegrity,
                     "SafeBrowsing.PakIntegrity.Chrome100"));
  CheckResourceIntegrity(
      resources_pack_path.DirName().AppendASCII("chrome_200_percent.pak"),
      kSha256_chrome_200_percent_pak,
      base::BindOnce(&ReportPakIntegrity,
                     "SafeBrowsing.PakIntegrity.Chrome200"));
}
#endif
