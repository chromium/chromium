// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/number_formatting.h"

#include <stddef.h>

#include <memory>

#include "base/check.h"
#include "base/format_macros.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_local.h"
#include "third_party/icu/source/common/unicode/ustring.h"
#include "third_party/icu/source/i18n/unicode/numfmt.h"

namespace base {

namespace {

// ICU formatters are not thread-safe for mutation. Using thread-local storage
// ensures that each thread has its own instance, preventing data races and
// potential Use-After-Free when multiple threads concurrently call
// FormatDouble with different digit requirements. See crbug.com/506477192.
base::ThreadLocalOwnedPointer<icu::NumberFormat>& GetIntFormatterStorage() {
  static base::NoDestructor<base::ThreadLocalOwnedPointer<icu::NumberFormat>>
      instance;
  return *instance;
}

base::ThreadLocalOwnedPointer<icu::NumberFormat>& GetFloatFormatterStorage() {
  static base::NoDestructor<base::ThreadLocalOwnedPointer<icu::NumberFormat>>
      instance;
  return *instance;
}

icu::NumberFormat* GetFormatter(
    base::ThreadLocalOwnedPointer<icu::NumberFormat>& storage) {
  icu::NumberFormat* formatter = storage.Get();
  if (formatter) {
    return formatter;
  }
  // There's no ICU call to destroy a NumberFormat object other than
  // operator delete, so use the default Delete, which calls operator delete.
  // This can cause problems if a different allocator is used by this file
  // than by ICU.
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::NumberFormat> instance(
      icu::NumberFormat::createInstance(status));
  DCHECK(U_SUCCESS(status));
  formatter = instance.get();
  storage.Set(std::move(instance));
  return formatter;
}

}  // namespace

std::u16string FormatNumber(int64_t number) {
  icu::NumberFormat* number_format = GetFormatter(GetIntFormatterStorage());

  if (!number_format) {
    // As a fallback, just return the raw number in a string.
    return ASCIIToUTF16(StringPrintf("%" PRId64, number));
  }
  icu::UnicodeString ustr;
  number_format->format(number, ustr);

  return i18n::UnicodeStringToString16(ustr);
}

std::u16string FormatDouble(double number, int fractional_digits) {
  return FormatDouble(number, fractional_digits, fractional_digits);
}

std::u16string FormatDouble(double number,
                            int min_fractional_digits,
                            int max_fractional_digits) {
  icu::NumberFormat* number_format = GetFormatter(GetFloatFormatterStorage());

  if (!number_format) {
    // As a fallback, just return the raw number in a string.
    return ASCIIToUTF16(StringPrintf("%f", number));
  }
  number_format->setMaximumFractionDigits(max_fractional_digits);
  number_format->setMinimumFractionDigits(min_fractional_digits);
  icu::UnicodeString ustr;
  number_format->format(number, ustr);

  return i18n::UnicodeStringToString16(ustr);
}

std::u16string FormatPercent(int number) {
  return i18n::MessageFormatter::FormatWithNumberedArgs(
      u"{0,number,percent}", static_cast<double>(number) / 100.0);
}

void ResetFormattersForTesting() {
  GetIntFormatterStorage().Set(nullptr);
  GetFloatFormatterStorage().Set(nullptr);
}

}  // namespace base
