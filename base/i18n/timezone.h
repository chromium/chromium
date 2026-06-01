// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_TIMEZONE_H_
#define BASE_I18N_TIMEZONE_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/i18n/base_i18n_export.h"
#include "base/i18n/language_code.h"
#include "base/time/time.h"

namespace base {
namespace i18n {

// A value-type wrapper for icu::TimeZone.
// This class provides a more idiomatic Chromium API for time zone operations,
// using base::Time and base::TimeDelta, while handling the underlying ICU
// lifecycle.
//
// Internal implementation details are hidden via PIMPL to avoid leaking ICU
// headers.
class BASE_I18N_EXPORT TimeZone {
 public:
  // Styles for display names.
  enum DisplayType {
    kShort,              // e.g., "PST"
    kLong,               // e.g., "Pacific Standard Time"
    kShortGeneric,       // e.g., "PT"
    kLongGeneric,        // e.g., "Pacific Time"
    kGenericLocation,    // e.g., "Los Angeles"
    kShortGMT,           // e.g., "-0800"
    kLongGMT,            // e.g., "GMT-08:00"
    kShortCommonlyUsed,  // e.g., "PST"
  };

  // Creates a TimeZone representing the current ICU default time zone.
  TimeZone();

  // Copy and move support.
  TimeZone(const TimeZone& other);
  TimeZone& operator=(const TimeZone& other);
  TimeZone(TimeZone&& other) noexcept;
  TimeZone& operator=(TimeZone&& other) noexcept;

  ~TimeZone();

  // Static factory methods.

  // Returns a TimeZone representing the current ICU default time zone.
  static TimeZone Default();

  // Creates a TimeZone from a given ID (e.g., "America/Los_Angeles").
  // If the ID is invalid, it may represent the "unknown" zone.
  static TimeZone FromString(std::string_view id);

  // Returns a TimeZone representing GMT (UTC).
  static TimeZone GMT();

  // Returns a TimeZone representing the "unknown" time zone.
  static TimeZone Unknown();

  // Detects the host system time zone.
  static TimeZone DetectHostTimeZone();

  // Returns the time zone ID (e.g., "America/Los_Angeles").
  std::string_view GetID() const;

  // Returns the region (ISO 3166-1 alpha-2 country code) associated with this
  // time zone. Returns an empty string if the region is unknown or is a
  // 3-digit numeric code (like "001" for World).
  std::string GetRegion() const;

  // Returns a localized name for this time zone.
  std::u16string GetDisplayName(const base::LanguageCode& language_code,
                                DisplayType style = kLong) const;
  std::u16string GetDisplayName(DisplayType style = kLong) const;

  // Returns the raw GMT offset (without DST).
  base::TimeDelta GetRawOffset() const;

  // Returns the total offset (raw + DST) at a given time.
  // `is_local` determines if `time` is interpreted as local time or UTC.
  void GetOffset(base::Time time,
                 bool is_local,
                 base::TimeDelta& raw_offset,
                 base::TimeDelta& dst_offset) const;

  // Returns true if this time zone ever uses daylight saving time.
  bool UseDaylightTime() const;

  // Returns true if daylight saving time is in effect at the given time.
  bool InDaylightTime(base::Time time) const;

  // Equality operators.
  bool operator==(const TimeZone& other) const;
  bool operator!=(const TimeZone& other) const;

 private:
  struct Impl;
  explicit TimeZone(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

// Checks the system timezone and turns it into a two-character ISO 3166 country
// code. This may fail (for example, it used to always fail on Android), in
// which case it will return an empty string. It'll also return an empty string
// when the timezone is Etc/UTC or Etc/UCT, but will return 'GB" for Etc/GMT
// because people in the UK tends to select Etc/GMT by mistake instead of
// Europe/London (British Time).
BASE_I18N_EXPORT std::string CountryCodeForCurrentTimezone();

}  // namespace i18n

// For compatibility with existing call sites.
using i18n::CountryCodeForCurrentTimezone;

}  // namespace base

#endif  // BASE_I18N_TIMEZONE_H_
