// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/resources_integrity.h"

#include <array>

#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/memory/page_size.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool.h"
#include "chrome/common/chrome_paths.h"
#include "crypto/secure_hash.h"
#include "ui/base/buildflags.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/app/chrome_exe_main_win.h"
#else
#include "chrome/app/packed_resources_integrity.h"  // nogncheck
#endif

namespace {

bool CheckResourceIntegrityInternal(
    const base::FilePath& path,
    const base::span<const uint8_t, crypto::kSHA256Length> expected_signature) {
  // Open the file for reading; allowing other consumers to also open it for
  // reading and deleting. Do not allow others to write to it.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                            base::File::FLAG_WIN_SHARE_DELETE);
  if (!file.IsValid())
    return false;

  auto hash = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  std::vector<uint8_t> buffer(base::GetPageSize());

  std::optional<size_t> bytes_read = 0;
  do {
    bytes_read = file.ReadAtCurrentPos(buffer);
    if (!bytes_read.has_value()) {
      return false;
    }
    hash->Update(buffer.data(), *bytes_read);
  } while (bytes_read.value_or(0) > 0);

  std::array<uint8_t, crypto::kSHA256Length> digest;
  hash->Finish(digest);

  return base::ranges::equal(digest, expected_signature);
}

void ReportPakIntegrity(const std::string& histogram_name, bool hash_matches) {
  base::UmaHistogramBoolean(histogram_name, hash_matches);
}

}  // namespace

void CheckResourceIntegrity(
    const base::FilePath& path,
    const base::span<const uint8_t, crypto::kSHA256Length> expected_signature,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceCallback<void(bool)> callback) {
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CheckResourceIntegrityInternal, path, expected_signature),
      std::move(callback));
}

void CheckPakFileIntegrity() {
  base::FilePath resources_pack_path;
  base::PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);

  // On Windows, the hashes cannot be embedded in the chrome.dll target that
  // this file is a part of, because it creates a cyclic build dependency
  // with the Grit resource allow-list generation. Instead, the hashes are
  // embedded in chrome.exe, which provides an exported function to
  // access them.
#if BUILDFLAG(IS_WIN)
  auto get_pak_file_hashes = reinterpret_cast<decltype(&GetPakFileHashes)>(
      ::GetProcAddress(::GetModuleHandle(nullptr), "GetPakFileHashes"));
  if (!get_pak_file_hashes) {
    // This is only exported by chrome.exe and unit_tests.exe, so in
    // other tests, like browser_tests.exe, this export will not be available.
    return;
  }

  const uint8_t *resources_hash_raw = nullptr, *chrome_100_hash_raw = nullptr,
                *chrome_200_hash_raw = nullptr;
  get_pak_file_hashes(&resources_hash_raw, &chrome_100_hash_raw,
                      &chrome_200_hash_raw);

  base::span<const uint8_t, crypto::kSHA256Length> resources_hash(
      resources_hash_raw, crypto::kSHA256Length);
  base::span<const uint8_t, crypto::kSHA256Length> chrome_100_hash(
      chrome_100_hash_raw, crypto::kSHA256Length);
  base::span<const uint8_t, crypto::kSHA256Length> chrome_200_hash(
      chrome_200_hash_raw, crypto::kSHA256Length);
#else
  base::span<const uint8_t, crypto::kSHA256Length> resources_hash =
      kSha256_resources_pak;
  base::span<const uint8_t, crypto::kSHA256Length> chrome_100_hash =
      kSha256_chrome_100_percent_pak;
#if BUILDFLAG(ENABLE_HIDPI)
  base::span<const uint8_t, crypto::kSHA256Length> chrome_200_hash =
      kSha256_chrome_200_percent_pak;
#endif
#endif  // BUILDFLAG(IS_WIN)

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  CheckResourceIntegrity(resources_pack_path, resources_hash, task_runner,
                         base::BindOnce(&ReportPakIntegrity,
                                        "SafeBrowsing.PakIntegrity.Resources"));
  CheckResourceIntegrity(
      resources_pack_path.DirName().AppendASCII("chrome_100_percent.pak"),
      chrome_100_hash, task_runner,
      base::BindOnce(&ReportPakIntegrity,
                     "SafeBrowsing.PakIntegrity.Chrome100"));
#if BUILDFLAG(ENABLE_HIDPI)
  CheckResourceIntegrity(
      resources_pack_path.DirName().AppendASCII("chrome_200_percent.pak"),
      chrome_200_hash, task_runner,
      base::BindOnce(&ReportPakIntegrity,
                     "SafeBrowsing.PakIntegrity.Chrome200"));
#endif
}
