// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/timezone.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base::i18n {

namespace {

icu::TimeZone::EDisplayType ToIcuDisplayType(TimeZone::DisplayType style) {
  switch (style) {
    case TimeZone::kShort:
      return icu::TimeZone::SHORT;
    case TimeZone::kLong:
      return icu::TimeZone::LONG;
    case TimeZone::kShortGeneric:
      return icu::TimeZone::SHORT_GENERIC;
    case TimeZone::kLongGeneric:
      return icu::TimeZone::LONG_GENERIC;
    case TimeZone::kGenericLocation:
      return icu::TimeZone::GENERIC_LOCATION;
    case TimeZone::kShortGMT:
      return icu::TimeZone::SHORT_GMT;
    case TimeZone::kLongGMT:
      return icu::TimeZone::LONG_GMT;
    case TimeZone::kShortCommonlyUsed:
      return icu::TimeZone::SHORT_COMMONLY_USED;
  }
  NOTREACHED();
}

std::string GetIdFromIcuTimeZone(const icu::TimeZone& zone) {
  icu::UnicodeString id;
  std::string res;
  zone.getID(id).toUTF8String(res);
  return res;
}

}  // namespace

struct TimeZone::Impl {
  explicit Impl(std::unique_ptr<icu::TimeZone> zone)
      : icu_timezone(std::move(zone)) {
    DCHECK(icu_timezone);
    id = GetIdFromIcuTimeZone(*icu_timezone);
  }

  std::string id;
  std::unique_ptr<icu::TimeZone> icu_timezone;
};

TimeZone::TimeZone()
    : impl_(std::make_unique<Impl>(
          base::WrapUnique(icu::TimeZone::createDefault()))) {}

TimeZone::TimeZone(const TimeZone& other)
    : impl_(std::make_unique<Impl>(
          base::WrapUnique(other.impl_->icu_timezone->clone()))) {}

TimeZone& TimeZone::operator=(const TimeZone& other) {
  if (this != &other) {
    impl_ = std::make_unique<Impl>(
        base::WrapUnique(other.impl_->icu_timezone->clone()));
  }
  return *this;
}

TimeZone::TimeZone(TimeZone&& other) noexcept = default;

TimeZone& TimeZone::operator=(TimeZone&& other) noexcept = default;

TimeZone::~TimeZone() = default;

// static
TimeZone TimeZone::Default() {
  return TimeZone();
}

// static
TimeZone TimeZone::FromString(std::string_view id) {
  return TimeZone(std::make_unique<Impl>(base::WrapUnique(
      icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8(id)))));
}

// static
TimeZone TimeZone::GMT() {
  return FromString("GMT");
}

// static
TimeZone TimeZone::Unknown() {
  return FromString("Etc/Unknown");
}

// static
TimeZone TimeZone::DetectHostTimeZone() {
  std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::detectHostTimeZone());
  return TimeZone(std::make_unique<Impl>(std::move(zone)));
}

std::string_view TimeZone::GetID() const {
  return impl_->id;
}

std::string TimeZone::GetRegion() const {
  char region_code[4];
  UErrorCode status = U_ZERO_ERROR;
  int length = icu::TimeZone::getRegion(icu::UnicodeString::fromUTF8(impl_->id),
                                        region_code, 4, status);
  // Return an empty string if region_code is a 3-digit numeric code such
  // as 001 (World).
  if (U_SUCCESS(status) && length == 2) {
    return std::string(region_code, static_cast<size_t>(length));
  }
  return std::string();
}

std::u16string TimeZone::GetDisplayName(const base::LanguageCode& language_code,
                                        DisplayType style) const {
  icu::UnicodeString name;
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale locale = icu::Locale::forLanguageTag(
      std::string(language_code.ToString()).c_str(), status);
  DCHECK(U_SUCCESS(status));
  impl_->icu_timezone->getDisplayName(false, ToIcuDisplayType(style), locale,
                                      name);
  return UnicodeStringToString16(name);
}

std::u16string TimeZone::GetDisplayName(DisplayType style) const {
  icu::UnicodeString name;
  impl_->icu_timezone->getDisplayName(false, ToIcuDisplayType(style),
                                      icu::Locale::getDefault(), name);
  return UnicodeStringToString16(name);
}

base::TimeDelta TimeZone::GetRawOffset() const {
  return base::Milliseconds(impl_->icu_timezone->getRawOffset());
}

void TimeZone::GetOffset(base::Time time,
                         bool is_local,
                         base::TimeDelta& raw_offset,
                         base::TimeDelta& dst_offset) const {
  int32_t raw_ms = 0;
  int32_t dst_ms = 0;
  UErrorCode status = U_ZERO_ERROR;
  impl_->icu_timezone->getOffset(time.InMillisecondsFSinceUnixEpoch(), is_local,
                                 raw_ms, dst_ms, status);
  DCHECK(U_SUCCESS(status));
  raw_offset = base::Milliseconds(raw_ms);
  dst_offset = base::Milliseconds(dst_ms);
}

bool TimeZone::UseDaylightTime() const {
  return impl_->icu_timezone->useDaylightTime();
}

bool TimeZone::InDaylightTime(base::Time time) const {
  UErrorCode status = U_ZERO_ERROR;
  bool in_daylight = impl_->icu_timezone->inDaylightTime(
      time.InMillisecondsFSinceUnixEpoch(), status);
  DCHECK(U_SUCCESS(status));
  return in_daylight;
}

bool TimeZone::operator==(const TimeZone& other) const {
  if (impl_->id == other.impl_->id) {
    return true;
  }
  return *impl_->icu_timezone == *other.impl_->icu_timezone;
}

bool TimeZone::operator!=(const TimeZone& other) const {
  return !(*this == other);
}

TimeZone::TimeZone(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {
  DCHECK(impl_);
}

std::string CountryCodeForCurrentTimezone() {
  TimeZone zone = TimeZone::Default();
  // ICU returns '001' (world) for Etc/GMT. Preserve the old behavior
  // only for Etc/GMT while returning an empty string for Etc/UTC and
  // Etc/UCT because they're less likely to be chosen by mistake in UK in
  // place of Europe/London (Briitish Time).
  if (zone.GetID() == "Etc/GMT") {
    return "GB";
  }
  return zone.GetRegion();
}

}  // namespace base::i18n
