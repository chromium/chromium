// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RS_GLUE_VALUES_GLUE_H_
#define BASE_RS_GLUE_VALUES_GLUE_H_

#include <stddef.h>
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
void ValueSetNoneKey(base::Value& v, rust::Str key);
void ValueSetBoolKey(base::Value& v, rust::Str key, bool value);
void ValueSetIntegerKey(base::Value& v, rust::Str key, int value);
void ValueSetDoubleKey(base::Value& v, rust::Str key, double value);
void ValueSetStringKey(base::Value& v, rust::Str key, rust::Str value);
base::Value& ValueSetDictKey(base::Value& v, rust::Str key);
base::Value& ValueSetListKey(base::Value& v, rust::Str key);
void ValueAppendNone(base::Value& v);
void ValueAppendString(base::Value& v, rust::Str value);
base::Value& ValueAppendDict(base::Value& v);
base::Value& ValueAppendList(base::Value& v);
void ValueReserveSize(base::Value& v, size_t len);
std::unique_ptr<ValueSlot> NewValueSlot();
rust::String DumpValueSlot(const ValueSlot& v);
void ConstructNoneValue(ValueSlot& v);
void ConstructBoolValue(ValueSlot& v, bool value);
void ConstructIntegerValue(ValueSlot& v, int value);
void ConstructDoubleValue(ValueSlot& v, double value);
void ConstructStringValue(ValueSlot& v, rust::Str value);
base::Value& ConstructDictValue(ValueSlot& v);
base::Value& ConstructListValue(ValueSlot& v);

}  // namespace rs_glue
}  // namespace base

#endif  // BASE_RS_GLUE_VALUES_GLUE_H_
