// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_APPLE_OWNED_OBJC_H_
#define BASE_APPLE_OWNED_OBJC_H_

#include <memory>

#include "base/base_export.h"
#include "build/build_config.h"

// This file defines wrappers to allow C++ code to own Objective-C objects
// without being Objective-C++ code themselves. These should not be used for
// pure Objective-C++ code, in which the underlying Objective-C types should be
// used, nor should they be used for the case where the pimpl idiom would work
// (https://chromium.googlesource.com/chromium/src/+/main/docs/mac/mixing_cpp_and_objc.md).

#if __OBJC__

#define GENERATE_OWNED_OBJC_TYPE(name) @class name;
#define GENERATE_OWNED_OBJC_PROTOCOL(name) @protocol name;
#include "base/apple/owned_objc_types.h"
#undef GENERATE_OWNED_OBJC_TYPE
#undef GENERATE_OWNED_OBJC_PROTOCOL

#endif  // __OBJC__

// Define this class two ways: the full-fledged way that allows Objective-C code
// to fully construct and access the inner Objective-C object, and a
// C++-compatible way that does not expose any Objective-C code and only allows
// default construction and validity checking.
#if __OBJC__
#define OWNED_TYPE_DECL_OBJC_ADDITIONS(name, objctype) \
  explicit Owned##name(objctype obj);                  \
  objctype Get() const;
#else
#define OWNED_TYPE_DECL_OBJC_ADDITIONS(name, objctype)
#endif  // __OBJC__

#define OWNED_OBJC_DECL(name, objctype)                       \
  namespace base::apple {                                     \
  class BASE_EXPORT Owned##name {                             \
   public:                                                    \
    /* Default-construct in a null state. */                  \
    Owned##name();                                            \
    ~Owned##name();                                           \
    Owned##name(const Owned##name&);                          \
    Owned##name& operator=(const Owned##name&);               \
    /* Returns whether the object contains a valid object. */ \
    bool IsValid() const;                                     \
    /* Comparisons. */                                        \
    bool operator==(const Owned##name& other) const;          \
    bool operator!=(const Owned##name& other) const;          \
    /* Objective-C-only constructor and getter. */            \
    OWNED_TYPE_DECL_OBJC_ADDITIONS(name, objctype)            \
                                                              \
   private:                                                   \
    struct ObjCStorage;                                       \
    std::unique_ptr<ObjCStorage> objc_storage_;               \
  };                                                          \
  }  // namespace base::apple

#define GENERATE_OWNED_OBJC_TYPE(name) OWNED_OBJC_DECL(name, name*)
#define GENERATE_OWNED_OBJC_PROTOCOL(name) OWNED_OBJC_DECL(name, id<name>)

#include "base/apple/owned_objc_types.h"

#undef GENERATE_OWNED_OBJC_TYPE
#undef GENERATE_OWNED_OBJC_PROTOCOL
#undef OWNED_OBJC_DECL
#undef OWNED_TYPE_DECL_OBJC_ADDITIONS

#endif  // BASE_APPLE_OWNED_OBJC_H_
