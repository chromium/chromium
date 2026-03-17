// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_INDEX_POINTER_H_
#define BASE_MEMORY_INDEX_POINTER_H_

namespace base::subtle {

// An IndexPointer stores a pointer in the form of an index from a base object.
// This is intended only for use in generated code for now.
//
// See StringSlice for a similar type that is specifically for strings
// represented as slices (which needs a few more bytes than a C-style string but
// could be faster to access in some contexts).
//
// This should only be used for pointers which are:
//
//  - constant
//  - stored in statically-allocated objects
//  - accessed rarely
//
// Storing pointers in this form avoids relocations, which saves disk space and
// allows storing data structures containing them in .rodata (which is truly a
// straight read-only mapping of clean file pages that the OS can discard
// anytime) instead of in .data.rel.ro (which has copy-on-write contents because
// the runtime linker writes into it).
//
// This is inspired by how the Linux kernel stores its kallsyms symbol table to
// avoid processing lots of relocations on boot and bloating the kernel image,
// though Linux implements it with a code generator that emits a data-only
// assembly file:
// https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/scripts/kallsyms.c
// (See in particular write_src(), which emits some values as subtractions
// between two labels.)
template <typename T, const T base[]>
class IndexPointer {
 public:
  explicit constexpr IndexPointer(unsigned int offset) : offset_(offset) {}
  // NOLINTNEXTLINE(google-explicit-constructor)
  inline operator const T*() const {
    // SAFETY: offset_ is always an in-bounds compile-time constant.
    return UNSAFE_BUFFERS(&base[offset_]);
  }

 private:
  unsigned int offset_;
};

}  // namespace base::subtle

#endif  // BASE_MEMORY_INDEX_POINTER_H_
