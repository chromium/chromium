// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WILCO_DTC_SUPPORTD_MOJO_UTILS_H_
#define CHROME_BROWSER_ASH_WILCO_DTC_SUPPORTD_MOJO_UTILS_H_

#include <memory>
#include <string>

#include "base/memory/shared_memory_mapping.h"
#include "base/strings/string_piece.h"
#include "mojo/public/cpp/system/handle.h"

namespace ash {

// This class is created to enable its functions (i.e.
// GetStringPieceFromMojoHandle) to use base::ScopedAllowBlocking's private
// constructor. That is achieved by making this class friend of
// base::ScopedAllowBlocking.
// Non-goal: This class is not created to group static member functions (as
// discouraged by chromium style guide).
class MojoUtils final {
 public:
  // Disallow all implicit constructors.
  MojoUtils() = delete;
  MojoUtils(const MojoUtils& mojo_utils) = delete;
  MojoUtils& operator=(const MojoUtils& mojo_utils) = delete;

  // Allows to get access to the buffer in read only shared memory. It converts
  // mojo::Handle to base::ReadOnlySharedMemoryMapping and returns a string
  // content.
  //
  // |handle| must be a valid mojo handle of the non-empty buffer in the shared
  // memory.
  //
  // Returns an empty string and an invalid |shared_memory| if error.
  //
  // TODO(crbug.com/989503): Use mojo::ScopedSharedBufferHandle or
  // base::ReadOnlySharedMemoryRegion instead of mojo::ScopedHandle
  // once ChromeOS updates to the required version of mojo library.
  static base::StringPiece GetStringPieceFromMojoHandle(
      mojo::ScopedHandle handle,
      base::ReadOnlySharedMemoryMapping* shared_memory);

  // Allocates buffer in shared memory, copies |content| to the buffer and
  // converts shared buffer handle into |mojo::ScopedHandle|.
  //
  // Allocated shared memory is read only for another process.
  //
  // Returns invalid |mojo::ScopedHandle| if |content| is empty or error
  // happened.
  //
  // TODO(crbug.com/989503): Remove mojo::ScopedHandle wrapping once
  // ChromeOS updates to the required version of mojo library.
  static mojo::ScopedHandle CreateReadOnlySharedMemoryMojoHandle(
      const std::string& content);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WILCO_DTC_SUPPORTD_MOJO_UTILS_H_
