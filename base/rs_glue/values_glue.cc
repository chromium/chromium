// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rs_glue/values_glue.h"
#include <stddef.h>
#include <sstream>

namespace base {
namespace rs_glue {

// This file has functions which are called from Rust code to populate
// bits of a base::Value. The functions exist because Rust C++ FFI
// is not yet quite good enough to operate on a base::Value directly
// without these intermediate layer. With future inprovements in interop,
// they may disappear.

std::unique_ptr<ValueSlot> NewValueSlotForTesting() {
  return std::make_unique<ValueSlot>();
}

void ValueSetNoneKey(base::Value& v, rust::Str key) {
  v.GetDict().Set(base::RustStrToStringPiece(key), base::Value());
}

void ValueSetBoolKey(base::Value& v, rust::Str key, bool value) {
  v.GetDict().Set(base::RustStrToStringPiece(key), value);
}

void ValueSetIntegerKey(base::Value& v, rust::Str key, int value) {
  v.GetDict().Set(base::RustStrToStringPiece(key), value);
}

void ValueSetDoubleKey(base::Value& v, rust::Str key, double value) {
  v.GetDict().Set(base::RustStrToStringPiece(key), value);
}

void ValueSetStringKey(base::Value& v, rust::Str key, rust::Str value) {
  v.GetDict().Set(base::RustStrToStringPiece(key),
                  base::RustStrToStringPiece(value));
}

base::Value& ValueSetDictKey(base::Value& v, rust::Str key) {
  return *v.GetDict().Set(base::RustStrToStringPiece(key), base::Value::Dict());
}

base::Value& ValueSetListKey(base::Value& v, rust::Str key) {
  return *v.GetDict().Set(base::RustStrToStringPiece(key), base::Value::List());
}

void ValueAppendNone(base::Value& v) {
  v.GetList().Append(base::Value());
}

void ValueAppendString(base::Value& v, rust::Str value) {
  v.GetList().Append(base::RustStrToStringPiece(value));
}

base::Value& ValueAppendDict(base::Value& v) {
  v.GetList().Append(base::Value::Dict());
  return v.GetList().back();
}

base::Value& ValueAppendList(base::Value& v) {
  v.GetList().Append(base::Value::List());
  return v.GetList().back();
}

void ValueReserveSize(base::Value& v, size_t len) {
  v.GetList().reserve(len);
}

rust::String DumpValueSlot(const ValueSlot& v) {
  std::ostringstream os;
  if (v.has_value()) {
    os << *v;
  } else {
    os << "(empty)";
  }
  return rust::String(os.str());
}

void ConstructNoneValue(ValueSlot& v) {
  v.emplace(base::Value::Type::NONE);
}

void ConstructBoolValue(ValueSlot& v, bool value) {
  v.emplace(value);
}

void ConstructIntegerValue(ValueSlot& v, int value) {
  v.emplace(value);
}

void ConstructDoubleValue(ValueSlot& v, double value) {
  v.emplace(value);
}

void ConstructStringValue(ValueSlot& v, rust::Str value) {
  v.emplace(base::RustStrToStringPiece(value));
}

base::Value& ConstructDictValue(ValueSlot& v) {
  return v.emplace(base::Value::Type::DICTIONARY);
}

base::Value& ConstructListValue(ValueSlot& v) {
  return v.emplace(base::Value::Type::LIST);
}

}  // namespace rs_glue
}  // namespace base
