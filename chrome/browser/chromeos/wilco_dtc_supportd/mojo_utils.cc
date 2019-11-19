// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wilco_dtc_supportd/mojo_utils.h"

#include <cstring>

#include "base/files/file.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/unguessable_token.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace chromeos {

base::StringPiece GetStringPieceFromMojoHandle(
    mojo::ScopedHandle handle,
    base::ReadOnlySharedMemoryMapping* shared_memory) {
  DCHECK(shared_memory);

  base::PlatformFile platform_file;
  auto result = mojo::UnwrapPlatformFile(std::move(handle), &platform_file);
  if (result != MOJO_RESULT_OK)
    return base::StringPiece();

  base::File file(platform_file);
  const size_t file_size = file.GetLength();
  if (file_size <= 0)
    return base::StringPiece();

  base::subtle::PlatformSharedMemoryRegion platform_region =
      base::subtle::PlatformSharedMemoryRegion::Take(
          base::ScopedFD(file.TakePlatformFile()),
          base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly, file_size,
          base::UnguessableToken::Create());

  base::ReadOnlySharedMemoryRegion shm =
      base::ReadOnlySharedMemoryRegion::Deserialize(std::move(platform_region));
  *shared_memory = shm.Map();
  if (!shared_memory->IsValid())
    return base::StringPiece();

  return base::StringPiece(static_cast<const char*>(shared_memory->memory()),
                           shared_memory->size());
}

mojo::ScopedHandle CreateReadOnlySharedMemoryMojoHandle(
    const std::string& content) {
  if (content.empty())
    return mojo::ScopedHandle();

  base::MappedReadOnlyRegion shm =
      base::ReadOnlySharedMemoryRegion::Create(content.size());
  if (!shm.IsValid())
    return mojo::ScopedHandle();
  memcpy(shm.mapping.memory(), content.data(), content.length());

  base::subtle::PlatformSharedMemoryRegion platform_region =
      base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
          std::move(shm.region));
  return mojo::WrapPlatformFile(
      platform_region.PassPlatformHandle().fd.release());
}

}  // namespace chromeos
