// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/message_formatter.h"

#include <string_view>

#include "base/check.h"
#include "base/i18n/unicodestring.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/fmtable.h"
#include "third_party/icu/source/i18n/unicode/msgfmt.h"

using icu::UnicodeString;

namespace base {
namespace i18n {
namespace {
UnicodeString UnicodeStringFromStringView(std::string_view str) {
  return UnicodeString::fromUTF8(
      std::string_view(str.data(), base::checked_cast<int32_t>(str.size())));
}
}  // anonymous namespace

namespace internal {
MessageArg::MessageArg() : formattable(nullptr) {}

MessageArg::MessageArg(const char* s)
    : formattable(new icu::Formattable(UnicodeStringFromStringView(s))) {}

MessageArg::MessageArg(std::string_view s)
    : formattable(new icu::Formattable(UnicodeStringFromStringView(s))) {}

MessageArg::MessageArg(const std::string& s)
    : formattable(new icu::Formattable(UnicodeString::fromUTF8(s))) {}

MessageArg::MessageArg(const std::u16string& s)
    : formattable(new icu::Formattable(UnicodeString(s.data(), s.size()))) {}

MessageArg::MessageArg(int i) : formattable(new icu::Formattable(i)) {}

MessageArg::MessageArg(int64_t i) : formattable(new icu::Formattable(i)) {}

MessageArg::MessageArg(double d) : formattable(new icu::Formattable(d)) {}

MessageArg::MessageArg(const Time& t)
    : formattable(new icu::Formattable(
          static_cast<UDate>(t.InMillisecondsFSinceUnixEpoch()))) {}

MessageArg::~MessageArg() = default;

// Tests if this argument has a value, and if so increments *count.
bool MessageArg::has_value(int *count) const {
  if (formattable == nullptr)
    return false;

  ++*count;
  return true;
}

}  // namespace internal

std::u16string MessageFormatter::FormatWithNumberedArgs(
    std::u16string_view msg,
    const internal::MessageArg& arg0,
    const internal::MessageArg& arg1,
    const internal::MessageArg& arg2,
    const internal::MessageArg& arg3,
    const internal::MessageArg& arg4,
    const internal::MessageArg& arg5,
    const internal::MessageArg& arg6) {
  int32_t args_count = 0;
  icu::Formattable args[] = {
      arg0.has_value(&args_count) ? *arg0.formattable : icu::Formattable(),
      arg1.has_value(&args_count) ? *arg1.formattable : icu::Formattable(),
      arg2.has_value(&args_count) ? *arg2.formattable : icu::Formattable(),
      arg3.has_value(&args_count) ? *arg3.formattable : icu::Formattable(),
      arg4.has_value(&args_count) ? *arg4.formattable : icu::Formattable(),
      arg5.has_value(&args_count) ? *arg5.formattable : icu::Formattable(),
      arg6.has_value(&args_count) ? *arg6.formattable : icu::Formattable(),
  };

  UnicodeString msg_string(msg.data(), msg.size());
  UErrorCode error = U_ZERO_ERROR;
  icu::MessageFormat format(msg_string,  error);
  icu::UnicodeString formatted;
  icu::FieldPosition ignore(icu::FieldPosition::DONT_CARE);
  format.format(args, args_count, formatted, ignore, error);
  if (U_FAILURE(error)) {
    LOG(ERROR) << "MessageFormat(" << msg << ") failed with "
               << u_errorName(error);
    return std::u16string();
  }
  return i18n::UnicodeStringToString16(formatted);
}

std::u16string MessageFormatter::FormatWithNamedArgs(
    std::u16string_view msg,
    std::string_view name0,
    const internal::MessageArg& arg0,
    std::string_view name1,
    const internal::MessageArg& arg1,
    std::string_view name2,
    const internal::MessageArg& arg2,
    std::string_view name3,
    const internal::MessageArg& arg3,
    std::string_view name4,
    const internal::MessageArg& arg4,
    std::string_view name5,
    const internal::MessageArg& arg5,
    std::string_view name6,
    const internal::MessageArg& arg6) {
  icu::UnicodeString names[] = {
      UnicodeStringFromStringView(name0), UnicodeStringFromStringView(name1),
      UnicodeStringFromStringView(name2), UnicodeStringFromStringView(name3),
      UnicodeStringFromStringView(name4), UnicodeStringFromStringView(name5),
      UnicodeStringFromStringView(name6),
  };
  int32_t args_count = 0;
  icu::Formattable args[] = {
      arg0.has_value(&args_count) ? *arg0.formattable : icu::Formattable(),
      arg1.has_value(&args_count) ? *arg1.formattable : icu::Formattable(),
      arg2.has_value(&args_count) ? *arg2.formattable : icu::Formattable(),
      arg3.has_value(&args_count) ? *arg3.formattable : icu::Formattable(),
      arg4.has_value(&args_count) ? *arg4.formattable : icu::Formattable(),
      arg5.has_value(&args_count) ? *arg5.formattable : icu::Formattable(),
      arg6.has_value(&args_count) ? *arg6.formattable : icu::Formattable(),
  };

  UnicodeString msg_string(msg.data(), msg.size());
  UErrorCode error = U_ZERO_ERROR;
  icu::MessageFormat format(msg_string, error);

  icu::UnicodeString formatted;
  format.format(names, args, args_count, formatted, error);
  if (U_FAILURE(error)) {
    LOG(ERROR) << "MessageFormat(" << msg << ") failed with "
               << u_errorName(error);
    return std::u16string();
  }
  return i18n::UnicodeStringToString16(formatted);
}

}  // namespace i18n
}  // namespace base
