// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RS_GLUE_VALUES_GLUE_H_
#define BASE_RS_GLUE_VALUES_GLUE_H_

#include <stddef.h>

#include "base/base_export.h"
#include "base/strings/string_piece_rust.h"
#include "base/values.h"
#include "third_party/rust/cxx/v1/crate/include/cxx.h"

namespace base {
namespace rs_glue {

// This file has functions which are called from Rust code to populate
// bits of a base::Value. The functions exist because Rust C++ FFI
// is not yet always good enough to operate on a base::Value directly
// without these intermediate layer. With future inprovements in interop,
// they may disappear.

// Storage space into which a `base::Value` can be constructed.
using ValueSlot = absl::optional<base::Value>;

// Function purposes explained in mod.rs in the same directory.
BASE_EXPORT void ValueSetNoneKey(base::Value& v, rust::Str key);
BASE_EXPORT void ValueSetBoolKey(base::Value& v, rust::Str key, bool value);
BASE_EXPORT void ValueSetIntegerKey(base::Value& v, rust::Str key, int value);
BASE_EXPORT void ValueSetDoubleKey(base::Value& v, rust::Str key, double value);
BASE_EXPORT void ValueSetStringKey(base::Value& v,
                                   rust::Str key,
                                   rust::Str value);
BASE_EXPORT base::Value& ValueSetDictKey(base::Value& v, rust::Str key);
BASE_EXPORT base::Value& ValueSetListKey(base::Value& v, rust::Str key);
BASE_EXPORT void ValueAppendNone(base::Value& v);
BASE_EXPORT void ValueAppendString(base::Value& v, rust::Str value);
BASE_EXPORT base::Value& ValueAppendDict(base::Value& v);
BASE_EXPORT base::Value& ValueAppendList(base::Value& v);
BASE_EXPORT void ValueReserveSize(base::Value& v, size_t len);
BASE_EXPORT std::unique_ptr<ValueSlot> NewValueSlotForTesting();
BASE_EXPORT rust::String DumpValueSlot(const ValueSlot& v);
BASE_EXPORT void ConstructNoneValue(ValueSlot& v);
BASE_EXPORT void ConstructBoolValue(ValueSlot& v, bool value);
BASE_EXPORT void ConstructIntegerValue(ValueSlot& v, int value);
BASE_EXPORT void ConstructDoubleValue(ValueSlot& v, double value);
BASE_EXPORT void ConstructStringValue(ValueSlot& v, rust::Str value);
BASE_EXPORT base::Value& ConstructDictValue(ValueSlot& v);
BASE_EXPORT base::Value& ConstructListValue(ValueSlot& v);

}  // namespace rs_glue
}  // namespace base

#endif  // BASE_RS_GLUE_VALUES_GLUE_H_
