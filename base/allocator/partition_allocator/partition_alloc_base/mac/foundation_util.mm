// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc_base/mac/foundation_util.h"

#include "base/allocator/partition_allocator/partition_alloc_check.h"

namespace partition_alloc::internal::base::mac {

#define PA_CF_CAST_DEFN(TypeCF)                                    \
  template <>                                                      \
  TypeCF##Ref CFCast<TypeCF##Ref>(const CFTypeRef& cf_val) {       \
    if (cf_val == NULL) {                                          \
      return NULL;                                                 \
    }                                                              \
    if (CFGetTypeID(cf_val) == TypeCF##GetTypeID()) {              \
      return (TypeCF##Ref)(cf_val);                                \
    }                                                              \
    return NULL;                                                   \
  }                                                                \
                                                                   \
  template <>                                                      \
  TypeCF##Ref CFCastStrict<TypeCF##Ref>(const CFTypeRef& cf_val) { \
    TypeCF##Ref rv = CFCast<TypeCF##Ref>(cf_val);                  \
    PA_DCHECK(cf_val == NULL || rv);                               \
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

}  // namespace partition_alloc::internal::base::mac
