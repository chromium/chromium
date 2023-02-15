// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_ICU_MERGEABLE_DATA_FILE_H_
#define BASE_I18N_ICU_MERGEABLE_DATA_FILE_H_

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/memory_mapped_file.h"
#include "base/i18n/base_i18n_export.h"
#include "base/memory/raw_ptr.h"

namespace base::i18n {

// Enable merging of icudtl.dat in Lacros.
BASE_I18N_EXPORT BASE_DECLARE_FEATURE(kLacrosMergeIcuDataFile);

// Class wrapping the memory-mapped instance of Ash's icudtl.dat.
// Needed to keep track of its file descriptor.
class AshMemoryMappedFile;

// Class wrapping the memory-merging logic for icudtl.dat.
class BASE_I18N_EXPORT IcuMergeableDataFile {
 public:
  using HashType = uint64_t;

  // Extension for ICU data's associated files containing page hashes.
  static constexpr char kIcuDataFileHashExtension[] = "hash";

  IcuMergeableDataFile();
  ~IcuMergeableDataFile();

  // The following APIs are designed to be consistent with MemoryMappedFile.
  bool Initialize(File lacros_file, MemoryMappedFile::Region region);
  const uint8_t* data() const;

  // Attempt merging with Ash's icudtl.dat.
  // Return `true` if successful or in case of non-critical failure.
  // Return `false` in case of critical failure (mmap will need to be called
  // again).
  bool MergeWithAshVersion(const FilePath& icudtl_ash_path);

  // True if page hashes were read from cache files, false otherwise.
  bool used_cached_hashes() const { return used_cached_hashes_; }

 private:
  using HashOffset = std::pair<HashType, size_t>;
  using HashToOffsetMap = base::flat_map<HashType, size_t>;

  struct Hashes {
    Hashes();
    Hashes(HashToOffsetMap ash, std::vector<HashType> lacros);
    Hashes(Hashes&& other);
    Hashes& operator=(Hashes&& other);
    ~Hashes();
    // Map from page hashes to offsets for Ash's icudtl.dat.
    HashToOffsetMap ash;
    // Vector of page hashes for Lacros's icudtl.dat. Indexed by page index.
    std::vector<HashType> lacros;
  };

  struct Slice {
    size_t offset;
    size_t length;
  };

  bool MmapLacrosFile(bool remap);

  Slice FindOverlap(const AshMemoryMappedFile& ash_file,
                    const Hashes& hashes,
                    size_t lacros_offset) const;

  bool MergeArea(const AshMemoryMappedFile& ash_file,
                 const Slice& ash_overlap,
                 size_t lacros_offset);

  // Count the number of equal pages (if any), starting at the given Ash and
  // Lacros offsets. `ash_page` and `lacros_page` are pages with the same
  // hash, so they likely represent the beginning of an overlapping area
  // in their respective `icudtl.dat` file.
  size_t CountEqualPages(const AshMemoryMappedFile& ash_file,
                         const uint8_t* ash_page,
                         const uint8_t* lacros_page) const;

  Hashes CalculateHashes(const AshMemoryMappedFile& ash_file,
                         const FilePath& ash_file_path);

  // Try loading pre-computed hashes from `icudtl.dat.hash` files.
  // Return `true` if successful, `false` otherwise.
  // `hashes` will contain the pre-computed hashes if successful,
  // will be left untouched otherwise.
  bool MaybeLoadCachedHashes(const AshMemoryMappedFile& ash_file,
                             const FilePath& ash_file_path,
                             Hashes& hashes);

  // Get Lacros's `icudtl.dat` path from its file descriptor.
  // Necessary because `File` objects don't keep track of the file path.
  FilePath GetLacrosFilePath();

  File lacros_file_;
  size_t lacros_length_ = 0;
  raw_ptr<uint8_t, AllowPtrArithmetic> lacros_data_ = nullptr;
  bool used_cached_hashes_ = false;
};

}  // namespace base::i18n

#endif  // BASE_I18N_MERGEABLE_ICU_DATA_FILE_H_
