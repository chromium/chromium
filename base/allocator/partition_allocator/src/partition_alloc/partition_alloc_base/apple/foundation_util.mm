// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/apple/foundation_util.h"

#include "partition_alloc/partition_alloc_base/check.h"

namespace partition_alloc::internal::base::apple {

#define PA_CF_CAST_DEFN(TypeCF)                                    \
  template <>                                                      \
  TypeCF##Ref CFCast<TypeCF##Ref>(const CFTypeRef& cf_val) {       \
    if (cf_val == nullptr) {                                       \
      return nullptr;                                              \
    }                                                              \
    if (CFGetTypeID(cf_val) == TypeCF##GetTypeID()) {              \
      return (TypeCF##Ref)(cf_val);                                \
    }                                                              \
    return nullptr;                                                \
  }                                                                \
                                                                   \
  template <>                                                      \
  TypeCF##Ref CFCastStrict<TypeCF##Ref>(const CFTypeRef& cf_val) { \
    TypeCF##Ref rv = CFCast<TypeCF##Ref>(cf_val);                  \
    PA_BASE_CHECK(cf_val == nullptr || rv);                        \
    return rv;                                                     \
  }

PA_CF_CAST_DEFN(CFArray)
PA_CF_CAST_DEFN(CFBag)
PA_CF_CAST_DEFN(CFBoolean)
PA_CF_CAST_DEFN(CFData)
PA_CF_CAST_DEFN(CFDate)
PA_CF_CAST_DEFN(CFDictionary)
PA_CF_CAST_DEFN(CFNull)
PA_CF_CAST_DEFN(CFNumber)
PA_CF_CAST_DEFN(CFSet)
PA_CF_CAST_DEFN(CFString)
PA_CF_CAST_DEFN(CFURL)
PA_CF_CAST_DEFN(CFUUID)

#undef PA_CF_CAST_DEFN

}  // namespace partition_alloc::internal::base::apple
