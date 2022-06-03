// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_SCOPED_BLOCK_H_
#define BASE_MAC_SCOPED_BLOCK_H_

#include <Block.h>

#include "base/mac/scoped_typeref.h"

#if defined(__has_feature) && __has_feature(objc_arc)
#error "Cannot include base/mac/scoped_block.h in file built with ARC."
#endif

namespace base {
namespace mac {

namespace internal {

template <typename B>
struct ScopedBlockTraits {
  static B InvalidValue() { return nullptr; }
  static B Retain(B block) { return Block_copy(block); }
  static void Release(B block) { Block_release(block); }
};

}  // namespace internal

// ScopedBlock<> is patterned after ScopedCFTypeRef<>, but uses Block_copy() and
// Block_release() instead of CFRetain() and CFRelease().
template <typename B>
using ScopedBlock = ScopedTypeRef<B, internal::ScopedBlockTraits<B>>;

}  // namespace mac
}  // namespace base

#endif  // BASE_MAC_SCOPED_BLOCK_H_
