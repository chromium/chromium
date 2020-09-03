// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TRACE_CONVERSION_HELPER_H_
#define BASE_TRACE_EVENT_TRACE_CONVERSION_HELPER_H_

#include <sstream>
#include <string>

#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/traced_value.h"

namespace base {

namespace trace_event {

// Helpers for base::trace_event::ValueToString.
namespace internal {

// Return std::string representation given by |value|'s ostream operator<<.
template <typename ValueType>
std::string OstreamValueToString(const ValueType& value) {
  std::stringstream ss;
  ss << value;
  return ss.str();
}

// Helper class to mark call priority when trying different options in SFINAE.
// Lower number has precedence.
//
// Example: Suppose that we are looking if there is a method |T::foo| or
// |T::bar| and if there are both we should use |T::foo|.
//
//   // decltype will return the type of the last expression (in this case int)
//   // if the first expression compiles. If the first part does not compile
//   // other versions of |getValueHelper| will be considered, as "substitution
//   // failure is not an error".
//   decltype(std::declval<const T&>().foo(), int())
//   getValueHelper(SFINAEPriority<0>, const T& obj) {
//     return obj.foo();
//   }
//
//   // If both versions are defined (there are both |T::foo| and |T::bar|) then
//   // this version is not used because of the type of the first argument
//   // (SFINAEPriority<0> takes precedence during overload resolution because
//   // it is the more specific (child) class).
//   decltype(std::declval<const T&>().bar(), int())
//   getValueHelper(SFINAEPriority<1>, const T& obj) {
//     return obj.bar();
//   }
template <int N>
class SFINAEPriority : public SFINAEPriority<N + 1> {};
template <>
// The number must be at least the largest used.
// Functions that use this class:
//   ValueToStringHelper
//   SetTracedValueArgHelper
class SFINAEPriority<6> {};

// Use SFINAE to decide how to extract a string from the given parameter.

// Check if |value| can be used as a parameter of |base::NumberToString|. If
// std::string is not constructible from the returned value of
// |base::NumberToString| cause compilation error.
//
// |base::NumberToString| does not do locale specific formatting and should be
// faster than using |std::ostream::operator<<|.
template <typename ValueType>
decltype(base::NumberToString(std::declval<const ValueType>()), std::string())
ValueToStringHelper(SFINAEPriority<0>,
                    const ValueType& value,
                    std::string /* unused */) {
  return base::NumberToString(value);
}

// If there is |ValueType::ToString| whose return value can be used to construct
// |std::string|, use this. Else use other methods.
template <typename ValueType>
decltype(std::string(std::declval<const ValueType>().ToString()))
ValueToStringHelper(SFINAEPriority<1>,
                    const ValueType& value,
                    std::string /* unused */) {
  return value.ToString();
}

// If |std::ostream::operator<<| can be used, use it. Useful for |void*|.
template <typename ValueType>
decltype(
    std::declval<std::ostream>().operator<<(std::declval<const ValueType>()),
    std::string())
ValueToStringHelper(SFINAEPriority<2>,
                    const ValueType& value,
                    std::string /* unused */) {
  return OstreamValueToString(value);
}

// Use |ValueType::operator<<| if applicable.
template <typename ValueType>
decltype(operator<<(std::declval<std::ostream&>(),
                    std::declval<const ValueType&>()),
         std::string())
ValueToStringHelper(SFINAEPriority<3>,
                    const ValueType& value,
                    std::string /* unused */) {
  return OstreamValueToString(value);
}

// If there is |ValueType::data| whose return value can be used to construct
// |std::string|, use it.
template <typename ValueType>
decltype(std::string(std::declval<const ValueType>().data()))
ValueToStringHelper(SFINAEPriority<4>,
                    const ValueType& value,
                    std::string /* unused */) {
  return value.data();
}

// Fallback returns the |fallback_value|. Needs to have |ValueToStringPriority|
// with the highest number (to be called last).
template <typename ValueType>
std::string ValueToStringHelper(SFINAEPriority<5>,
                                const ValueType& /* unused */,
                                std::string fallback_value) {
  return fallback_value;
}

/*********************************************
********* SetTracedValueArg methods. *********
*********************************************/

// SFINAEPriority parameter is there to define ordering in which the following
// methods will be considered. Note that for instance |bool| type is also
// |std::is_integral|, so we need to test |bool| before testing for integral.
template <typename T>
typename std::enable_if<std::is_same<T, bool>::value>::type
SetTracedValueArgHelper(SFINAEPriority<0>,
                        TracedValue* traced_value,
                        const char* name,
                        const T& value) {
  traced_value->SetBoolean(name, value);
}

// std::is_integral<bool>::value == true
// This needs to be considered only when T is not bool (has higher
// SFINAEPriority).
template <typename T>
typename std::enable_if<std::is_integral<T>::value>::type
SetTracedValueArgHelper(SFINAEPriority<1>,
                        TracedValue* traced_value,
                        const char* name,
                        const T& value) {
  // Avoid loss of precision.
  if (sizeof(int) < sizeof(value)) {
    // TODO(crbug.com/1111787): Add 64-bit support to TracedValue.
    traced_value->SetString(name, base::NumberToString(value));
  } else {
    traced_value->SetInteger(name, value);
  }
}

// Any floating point type is converted to double.
template <typename T>
typename std::enable_if<std::is_floating_point<T>::value>::type
SetTracedValueArgHelper(SFINAEPriority<2>,
                        TracedValue* traced_value,
                        const char* name,
                        const T& value) {
  traced_value->SetDouble(name, static_cast<double>(value));
}

// |void*| is traced natively.
template <typename T>
typename std::enable_if<std::is_same<T, void*>::value>::type
SetTracedValueArgHelper(SFINAEPriority<3>,
                        TracedValue* traced_value,
                        const char* name,
                        const T& value) {
  traced_value->SetPointer(name, value);
}

// |const char*| is traced natively.
template <typename T>
typename std::enable_if<std::is_same<T, const char*>::value>::type
SetTracedValueArgHelper(SFINAEPriority<4>,
                        TracedValue* traced_value,
                        const char* name,
                        const T& value) {
  traced_value->SetString(name, value);
}

// If an instance of |base::StringPiece| can be constructed from an instance of
// |T| trace |value| as a string.
template <typename T>
decltype(base::StringPiece(std::declval<const T>()), void())
SetTracedValueArgHelper(SFINAEPriority<5>,
                        TracedValue* traced_value,
                        const char* name,
                        const T& value) {
  traced_value->SetString(name, value);
}

// Fallback.
template <typename T>
void SetTracedValueArgHelper(SFINAEPriority<6>,
                             TracedValue* traced_value,
                             const char* name,
                             const T& /* unused */) {
  // TODO(crbug.com/1111787): Add fallback to |ValueToString|. Crashes on
  // operator<< have been seen with it.
  traced_value->SetString(name, "<value>");
}

}  // namespace internal

// The function to be used.
template <typename ValueType>
std::string ValueToString(const ValueType& value,
                          std::string fallback_value = "<value>") {
  return internal::ValueToStringHelper(internal::SFINAEPriority<0>(), value,
                                       std::move(fallback_value));
}

// ToTracedValue helpers simplify using |AsValueInto| method to capture by
// eliminating the need to create TracedValue manually. Also supports passing
// pointers, including null ones.
template <typename T>
std::unique_ptr<TracedValue> ToTracedValue(T& value) {
  std::unique_ptr<TracedValue> result = std::make_unique<TracedValue>();
  // AsValueInto might not be const-only, so do not use const references.
  value.AsValueInto(result.get());
  return result;
}

template <typename T>
std::unique_ptr<TracedValue> ToTracedValue(T* value) {
  if (!value)
    return TracedValue::Build({{"this", "nullptr"}});
  return ToTracedValue(*value);
}

// Method to trace |value| into the given |traced_value|. Support types where
// there is |TracedValue::SetT| natively.
//
// TODO(crbug.com/1111787): Add support for:
//   base::Optional
//   AsValueInto (T& and T*)
//   array and map types
//   fallback to ValueToString
template <typename ValueType>
void SetTracedValueArg(TracedValue* traced_value,
                       const char* name,
                       const ValueType& value) {
  internal::SetTracedValueArgHelper(internal::SFINAEPriority<0>(), traced_value,
                                    name, value);
}

// Parameter pack support: do nothing for an empty parameter pack.
//
// Inline this to avoid linker duplicate symbol error.
inline void SetTracedValueArg(TracedValue* traced_value, const char* name) {}

// Parameter pack support. All of the packed parameters are traced under the
// same name. Serves to trace a parameter pack, all parameters having the same
// name (of the parameter pack) is desired.
//
// Example use when |args| is a parameter pack:
//   SetTracedValueArg(traced_value, name, args...);
template <typename ValueType, typename... ValueTypes>
void SetTracedValueArg(TracedValue* traced_value,
                       const char* name,
                       const ValueType& value,
                       const ValueTypes&... args) {
  SetTracedValueArg(traced_value, name, value);
  // Trace the rest from the parameter pack.
  SetTracedValueArg(traced_value, name, args...);
}

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_TRACE_CONVERSION_HELPER_H_
