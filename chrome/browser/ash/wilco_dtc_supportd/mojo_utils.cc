// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wilco_dtc_supportd/mojo_utils.h"

#include <cstring>

#include "base/files/file.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/threading/thread_restrictions.h"
#include "base/unguessable_token.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace ash {

// static
base::StringPiece MojoUtils::GetStringPieceFromMojoHandle(
    mojo::ScopedHandle handle,
    base::ReadOnlySharedMemoryMapping* shared_memory) {
  DCHECK(shared_memory);

  base::ScopedPlatformFile platform_file;
  auto result = mojo::UnwrapPlatformFile(std::move(handle), &platform_file);
  if (result != MOJO_RESULT_OK)
    return base::StringPiece();

  base::File file(std::move(platform_file));
  size_t file_size = 0;
  {
    // TODO(b/146119375): Remove blocking operation from production code.
    base::ScopedAllowBlocking allow_blocking;
    file_size = file.GetLength();
  }
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

// static
mojo::ScopedHandle MojoUtils::CreateReadOnlySharedMemoryMojoHandle(
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
  return mojo::WrapPlatformFile(platform_region.PassPlatformHandle().fd);
}

}  // namespace ash
