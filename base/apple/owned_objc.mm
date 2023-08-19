// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/owned_objc.h"

#include <MacTypes.h>  // For nil, to avoid having to bring in frameworks.

#include "build/build_config.h"

#define OWNED_OBJC_IMPL(classname, objctype, ownership)                      \
  namespace base::apple {                                                    \
  struct classname::ObjCStorage {                                            \
    objctype ownership obj;                                                  \
  };                                                                         \
  classname::classname() : objc_storage_(std::make_unique<ObjCStorage>()) {} \
  classname::~classname() = default;                                         \
  classname::classname(objctype obj) : classname() {                         \
    objc_storage_->obj = obj;                                                \
  }                                                                          \
  classname::classname(const classname& other) : classname() {               \
    objc_storage_->obj = other.objc_storage_->obj;                           \
  }                                                                          \
  classname& classname::operator=(const classname& other) {                  \
    objc_storage_->obj = other.objc_storage_->obj;                           \
    return *this;                                                            \
  }                                                                          \
  classname::operator bool() const {                                         \
    return objc_storage_->obj != nil;                                        \
  }                                                                          \
  bool classname::operator==(const classname& other) const {                 \
    return objc_storage_->obj == other.objc_storage_->obj;                   \
  }                                                                          \
  bool classname::operator!=(const classname& other) const {                 \
    return !this->operator==(other);                                         \
  }                                                                          \
  objctype classname::Get() const {                                          \
    return objc_storage_->obj;                                               \
  }                                                                          \
  }  // namespace base::apple

#define GENERATE_STRONG_OBJC_TYPE(name) \
  OWNED_OBJC_IMPL(Owned##name, name*, __strong)
#define GENERATE_STRONG_OBJC_PROTOCOL(name) \
  OWNED_OBJC_IMPL(Owned##name, id<name>, __strong)
#define GENERATE_WEAK_OBJC_TYPE(name) OWNED_OBJC_IMPL(Weak##name, name*, __weak)
#define GENERATE_WEAK_OBJC_PROTOCOL(name) \
  OWNED_OBJC_IMPL(Weak##name, id<name>, __weak)

#include "base/apple/owned_objc_types.h"

#undef GENERATE_STRONG_OBJC_TYPE
#undef GENERATE_STRONG_OBJC_PROTOCOL
#undef GENERATE_WEAK_OBJC_TYPE
#undef GENERATE_WEAK_OBJC_PROTOCOL
#undef OWNED_OBJC_IMPL
