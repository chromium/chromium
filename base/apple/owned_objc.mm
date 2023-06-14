// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/owned_objc.h"

#include <MacTypes.h>  // For nil, to avoid having to bring in frameworks.

#include "build/build_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#define OWNED_OBJC_IMPL(name, objctype)                                \
  namespace base::apple {                                              \
  struct Owned##name::ObjCStorage {                                    \
    objctype __strong obj;                                             \
  };                                                                   \
  Owned##name::Owned##name()                                           \
      : objc_storage_(std::make_unique<ObjCStorage>()) {}              \
  Owned##name::~Owned##name() = default;                               \
  Owned##name::Owned##name(objctype obj) : Owned##name() {             \
    objc_storage_->obj = obj;                                          \
  }                                                                    \
  Owned##name::Owned##name(const Owned##name& other) : Owned##name() { \
    objc_storage_->obj = other.objc_storage_->obj;                     \
  }                                                                    \
  Owned##name& Owned##name::operator=(const Owned##name& other) {      \
    objc_storage_->obj = other.objc_storage_->obj;                     \
    return *this;                                                      \
  }                                                                    \
  bool Owned##name::IsValid() const {                                  \
    return objc_storage_->obj != nil;                                  \
  }                                                                    \
  bool Owned##name::operator==(const Owned##name& other) const {       \
    return objc_storage_->obj == other.objc_storage_->obj;             \
  }                                                                    \
  bool Owned##name::operator!=(const Owned##name& other) const {       \
    return !this->operator==(other);                                   \
  }                                                                    \
  objctype Owned##name::Get() const {                                  \
    return objc_storage_->obj;                                         \
  }                                                                    \
  }  // namespace base::apple

#define GENERATE_OWNED_OBJC_TYPE(name) OWNED_OBJC_IMPL(name, name*)
#define GENERATE_OWNED_OBJC_PROTOCOL(name) OWNED_OBJC_IMPL(name, id<name>)

#include "base/apple/owned_objc_types.h"

#undef GENERATE_OWNED_OBJC_TYPE
#undef GENERATE_OWNED_OBJC_PROTOCOL
#undef OWNED_OBJC_IMPL
