// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/i18n/icu_mergeable_data_file.h"

#include <sys/mman.h>

#include "base/check.h"
#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/hash/hash.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/chromeos_buildflags.h"

namespace base::i18n {

// Enable merging of `icudtl.dat` in Lacros.
BASE_FEATURE(kLacrosMergeIcuDataFile,
             "LacrosMergeIcuDataFile",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
// Path of Ash's ICU data file.
constexpr char kIcuDataFileAshPath[] = "/opt/google/chrome/icudtl.dat";
#endif  // BUILDFLAG(IS_CHROMEOS_DEVICE)

// Expected size of a system page.
constexpr int64_t kPageSize = 0x1000;

// Size of a page hash. Changing this will break compatibility
// with existing `icudtl.dat.hash` files, so be careful.
constexpr size_t kHashBytes = 8;
static_assert(sizeof(IcuMergeableDataFile::HashType) == kHashBytes);

inline IcuMergeableDataFile::HashType HashPage(const uint8_t* page) {
  return FastHash(base::make_span(page, static_cast<size_t>(kPageSize)));
}

IcuMergeableDataFile::HashType ReadHash(const uint8_t* data, size_t offset) {
  CHECK_EQ(0ul, offset % kHashBytes);
  IcuMergeableDataFile::HashType hash = 0;
  for (size_t i = 0; i < kHashBytes; i++) {
    IcuMergeableDataFile::HashType byte = data[offset + i];
    hash |= byte << (i * 8);
  }
  return hash;
}

constexpr size_t NPages(size_t length) {
  return (length + kPageSize - 1) / kPageSize;
}

}  // namespace

class AshMemoryMappedFile {
 public:
  bool Initialize(File ash_file) {
    fd_ = ash_file.GetPlatformFile();
    return memory_mapped_file_.Initialize(std::move(ash_file));
  }

  PlatformFile fd() const { return fd_; }
  const uint8_t* data() const { return memory_mapped_file_.data(); }
  size_t length() const { return memory_mapped_file_.length(); }

 private:
  PlatformFile fd_;
  MemoryMappedFile memory_mapped_file_;
};

std::unique_ptr<AshMemoryMappedFile> MmapAshFile(
    const FilePath& ash_file_path) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  // Open Ash's data file.
  File ash_file(FilePath(ash_file_path), File::FLAG_OPEN | File::FLAG_READ);

  // Mmap Ash's data file.
  auto ash_mapped_file = std::make_unique<AshMemoryMappedFile>();
  bool map_successful = ash_mapped_file->Initialize(std::move(ash_file));
  if (!map_successful) {
    PLOG(DFATAL) << "Failed to mmap Ash's icudtl.dat";
    return nullptr;
  }

  return ash_mapped_file;
}

// Class wrapping the memory-merging logic for `icudtl.dat`.
IcuMergeableDataFile::IcuMergeableDataFile() = default;

IcuMergeableDataFile::~IcuMergeableDataFile() {
  if (lacros_data_) {
    ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
    munmap(lacros_data_, lacros_length_);
  }
}

IcuMergeableDataFile::Hashes::Hashes() = default;
IcuMergeableDataFile::Hashes::Hashes(HashToOffsetMap ash,
                                     std::vector<HashType> lacros)
    : ash(std::move(ash)), lacros(std::move(lacros)) {}
IcuMergeableDataFile::Hashes::Hashes(Hashes&& other) = default;
IcuMergeableDataFile::Hashes& IcuMergeableDataFile::Hashes::operator=(
    Hashes&& other) = default;
IcuMergeableDataFile::Hashes::~Hashes() = default;

bool IcuMergeableDataFile::Initialize(File lacros_file,
                                      MemoryMappedFile::Region region) {
  CHECK(region == MemoryMappedFile::Region::kWholeFile);
  CHECK(!lacros_file_.IsValid())
      << "ICUDataFile::Initialize called twice";

  lacros_file_ = std::move(lacros_file);
  int64_t lacros_length = lacros_file_.GetLength();
  if (lacros_length < 0) {
    return false;
  }
  // Narrow to size_t, since it's used for pointer arithmetic, mmap and other
  // APIs that accept size_t.
  lacros_length_ = base::checked_cast<size_t>(lacros_length);

  // Map Lacros's version of `icudtl.dat`, then attempt merging with Ash.
  bool map_successful = MmapLacrosFile(/*remap=*/false);

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  // If we're inside an actual ChromeOS system (i.e. not just in
  // linux-lacros-rel) then we can expect Ash Chrome (and its version of
  // `icudtl.dat`) to be present in the default directory.
  // In that case, we can attempt merging.
  if (map_successful && base::FeatureList::IsEnabled(kLacrosMergeIcuDataFile)) {
    bool merge_successful = MergeWithAshVersion(FilePath(kIcuDataFileAshPath));
    // If we hit a critical failure while merging, remap Lacros's version.
    if (!merge_successful) {
      PLOG(DFATAL) << "Attempt to merge Lacros's icudtl.dat with Ash's failed";
      map_successful = MmapLacrosFile(/*remap=*/true);
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_DEVICE)

  return map_successful;
}

const uint8_t* IcuMergeableDataFile::data() const {
  return static_cast<const uint8_t*>(lacros_data_);
}

bool IcuMergeableDataFile::MergeWithAshVersion(const FilePath& ash_file_path) {
  // Verify the assumption that page size is 4K.
  CHECK_EQ(sysconf(_SC_PAGESIZE), kPageSize);

  // Mmap Ash's data file.
  auto ash_file = MmapAshFile(ash_file_path);
  if (!ash_file)
    return true;  // Non-critical failure.

  // Calculate hashes for each page in Ash and Lacros's data files.
  Hashes hashes = CalculateHashes(*ash_file, ash_file_path);

  // Find Lacros's ICU pages that are duplicated in Ash.
  size_t lacros_offset = 0;
  while (lacros_offset < lacros_length_) {
    Slice ash_overlap = FindOverlap(*ash_file, hashes, lacros_offset);
    // If there's no overlap, move to the next page and keep scanning.
    if (ash_overlap.length == 0) {
      lacros_offset += kPageSize;
      continue;
    }

    // Found a sequence of equal pages, merge them with Ash.
    bool merge_successful = MergeArea(*ash_file, ash_overlap, lacros_offset);
    if (!merge_successful)
      return false;  // Critical failure.

    lacros_offset += ash_overlap.length;
  }

  return true;  // Success.
}

bool IcuMergeableDataFile::MmapLacrosFile(bool remap) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  if (remap) {
    // If `remap` == true, we add the MAP_FIXED option to unmap the
    // existing map and replace it with the new one in a single operation.
    CHECK_NE(lacros_data_, nullptr);
    lacros_data_ = static_cast<uint8_t*>(
        mmap(lacros_data_, lacros_length_, PROT_READ, MAP_FIXED | MAP_PRIVATE,
             lacros_file_.GetPlatformFile(), 0));
  } else {
    // Otherwise, simply map the file.
    lacros_data_ = static_cast<uint8_t*>(
        mmap(nullptr, lacros_length_, PROT_READ, MAP_PRIVATE,
             lacros_file_.GetPlatformFile(), 0));
  }

  if (lacros_data_ == MAP_FAILED) {
    lacros_data_ = nullptr;
    PLOG(DFATAL) << "Failed to mmap Lacros's icudtl.dat";
    return false;
  }

  return true;
}

IcuMergeableDataFile::Slice IcuMergeableDataFile::FindOverlap(
    const AshMemoryMappedFile& ash_file,
    const Hashes& hashes,
    size_t lacros_offset) const {
  // Search for equal pages by hash.
  HashType hash = hashes.lacros[lacros_offset / kPageSize];
  auto search = hashes.ash.find(hash);
  if (search == hashes.ash.end())
    return {0, 0};

  // Count how many pages (if any) have the same content.
  size_t ash_offset = search->second;
  size_t overlap_length =
      kPageSize * CountEqualPages(ash_file, ash_file.data() + ash_offset,
                                  lacros_data_ + lacros_offset);

  return {ash_offset, overlap_length};
}

bool IcuMergeableDataFile::MergeArea(const AshMemoryMappedFile& ash_file,
                                     const Slice& ash_overlap,
                                     size_t lacros_offset) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  // Unmap from Lacros's file and map from Ash's file instead.
  // NOTE: "[...] If the memory region specified by addr and length overlaps
  //        pages of any existing mapping(s), then the overlapped part of the
  //        existing mapping(s) will be discarded.  If the specified address
  //        cannot be used, mmap() will fail."
  // Reference: https://man7.org/linux/man-pages/man2/mmap.2.html
  uint8_t* map_result = static_cast<uint8_t*>(
      mmap(lacros_data_ + lacros_offset, ash_overlap.length, PROT_READ,
           MAP_FIXED | MAP_PRIVATE, ash_file.fd(), ash_overlap.offset));

  if (map_result == MAP_FAILED) {
    PLOG(DFATAL) << "Couldn't mmap Ash's icudtl.dat while merging";
    return false;
  }

  return true;
}

size_t IcuMergeableDataFile::CountEqualPages(
    const AshMemoryMappedFile& ash_file,
    const uint8_t* ash_page,
    const uint8_t* lacros_page) const {
  if (!ash_page || !lacros_page) {
    return 0;
  }

  size_t pages = 0;
  const uint8_t* ash_end = ash_file.data() + ash_file.length();
  const uint8_t* lacros_end = lacros_data_ + lacros_length_;

  while (ash_page < ash_end && lacros_page < lacros_end &&
         memcmp(ash_page, lacros_page, kPageSize) == 0) {
    ash_page += kPageSize;
    lacros_page += kPageSize;
    pages++;
  }

  return pages;
}

IcuMergeableDataFile::Hashes IcuMergeableDataFile::CalculateHashes(
    const AshMemoryMappedFile& ash_file,
    const FilePath& ash_file_path) {
  // Try loading hashes from the pre-computed files first.
  Hashes hashes;
  used_cached_hashes_ = MaybeLoadCachedHashes(ash_file, ash_file_path, hashes);

  if (!used_cached_hashes_) {
    // Calculate hashes for each page in Ash's data file.
    std::vector<HashOffset> ash_hashes;
    ash_hashes.reserve(NPages(ash_file.length()));
    for (size_t offset = 0; offset < ash_file.length(); offset += kPageSize) {
      // NOTE: "POSIX specifies that the system shall always zero fill any
      //        partial page at the end of the object [...]".
      // Reference: https://man7.org/linux/man-pages/man2/mmap.2.html
      //
      // Therefore this code works even if the size of Ash's `icudtl.dat` is not
      // a multiple of the page size.
      HashType hash = HashPage(ash_file.data() + offset);
      ash_hashes.emplace_back(hash, offset);
    }

    // Calculate hashes for each page in Lacros's data file.
    hashes.lacros.reserve(NPages(lacros_length_));
    for (size_t offset = 0; offset < lacros_length_; offset += kPageSize) {
      HashType hash = HashPage(lacros_data_ + offset);
      hashes.lacros.emplace_back(hash);
    }

    hashes.ash = HashToOffsetMap(std::move(ash_hashes));
  }

  return hashes;
}

bool IcuMergeableDataFile::MaybeLoadCachedHashes(
    const AshMemoryMappedFile& ash_file,
    const FilePath& ash_file_path,
    Hashes& hashes) {
  FilePath ash_hash_path =
      ash_file_path.AddExtensionASCII(kIcuDataFileHashExtension);
  FilePath lacros_hash_path =
      GetLacrosFilePath().AddExtensionASCII(kIcuDataFileHashExtension);

  // Memory map Ash's `icudtl.dat.hash`. Ensure its size is valid and consistent
  // with the current version of `icudtl.dat`.
  MemoryMappedFile ash_hash_file;
  size_t ash_pages = NPages(ash_file.length());
  bool result = ash_hash_file.Initialize(ash_hash_path);
  if (!result || (ash_hash_file.length() % kHashBytes) ||
      ((ash_hash_file.length() / kHashBytes) != ash_pages)) {
    return false;
  }

  // Same for Lacros's `icudtl.dat.hash`.
  MemoryMappedFile lacros_hash_file;
  size_t lacros_pages = NPages(lacros_length_);
  result = lacros_hash_file.Initialize(lacros_hash_path);
  if (!result || (lacros_hash_file.length() % kHashBytes) ||
      ((lacros_hash_file.length() / kHashBytes) != lacros_pages)) {
    return false;
  }

  // Load Ash's hashes.
  std::vector<HashOffset> ash_hashes;
  ash_hashes.reserve(ash_pages);
  for (size_t i = 0; i < ash_hash_file.length(); i += kHashBytes) {
    HashType hash = ReadHash(ash_hash_file.data(), i);
    size_t offset = (i / kHashBytes) * kPageSize;
    ash_hashes.emplace_back(hash, offset);
  }

  // Load Lacros's hashes.
  hashes.lacros.reserve(lacros_pages);
  for (size_t i = 0; i < lacros_hash_file.length(); i += kHashBytes) {
    HashType hash = ReadHash(lacros_hash_file.data(), i);
    hashes.lacros.emplace_back(hash);
  }

  hashes.ash = HashToOffsetMap(std::move(ash_hashes));
  return true;
}

FilePath IcuMergeableDataFile::GetLacrosFilePath() {
  // /proc/self/fd/<fd>
  //   This is a subdirectory containing one entry for each file
  //   which the process has open, named by its file descriptor,
  //   and which is a symbolic link to the actual file.
  // Reference: proc(5) - Linux manual page.
  char path[PATH_MAX];
  FilePath proc_path =
      FilePath("/proc/self/fd/")
          .AppendASCII(base::NumberToString(lacros_file_.GetPlatformFile()));

  // We read the content of the symbolic link to find the path of the
  // file associated with the file descriptor.
  int64_t path_len = readlink(proc_path.value().c_str(), path, sizeof(path));
  CHECK_NE(path_len, -1);
  CHECK_LT(path_len, PATH_MAX);

  return FilePath(std::string(path, 0, path_len));
}

}  // namespace base::i18n
