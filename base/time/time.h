// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// `Time` represents an absolute point in coordinated universal time (UTC),
// internally represented as microseconds (s/1,000,000) since the Windows epoch
// (1601-01-01 00:00:00 UTC). System-dependent clock interface routines are
// defined in time_PLATFORM.cc. Note that values for `Time` may skew and jump
// around as the operating system makes adjustments to synchronize (e.g., with
// NTP servers). Thus, client code that uses the `Time` class must account for
// this.
//
// `TimeDelta` represents a duration of time, internally represented in
// microseconds.
//
// `TimeTicks` and `ThreadTicks` represent an abstract time that is most of the
// time incrementing, for use in measuring time durations. Internally, they are
// represented in microseconds. They cannot be converted to a human-readable
// time, but are guaranteed not to decrease (unlike the `Time` class). Note
// that `TimeTicks` may "stand still" (e.g., if the computer is suspended), and
// `ThreadTicks` will "stand still" whenever the thread has been de-scheduled
// by the operating system.
//
// All time classes are copyable, assignable, and occupy 64 bits per instance.
// Prefer to pass them by value, e.g.:
//
//   void MyFunction(TimeDelta arg);
//
// All time classes support `operator<<` with logging streams, e.g. `LOG(INFO)`.
// For human-readable formatting, use //base/i18n/time_formatting.h.
//
// Example use cases for different time classes:
//
//   Time:        Interpreting the wall-clock time provided by a remote system.
//                Detecting whether cached resources have expired. Providing the
//                user with a display of the current date and time. Determining
//                the amount of time between events across re-boots of the
//                machine.
//
//   TimeTicks:   Tracking the amount of time a task runs. Executing delayed
//                tasks at the right time. Computing presentation timestamps.
//                Synchronizing audio and video using TimeTicks as a common
//                reference clock (lip-sync). Measuring network round-trip
//                latency.
//
//   ThreadTicks: Benchmarking how long the current thread has been doing actual
//                work.
//
// Serialization:
//
// Use the helpers in //base/json/values_util.h when serializing `Time`
// or `TimeDelta` to/from `base::Value`.
//
// Otherwise:
//
// - Time: use `FromDeltaSinceWindowsEpoch()`/`ToDeltaSinceWindowsEpoch()`.
// - TimeDelta: use `FromMicroseconds()`/`InMicroseconds()`.
//
// `TimeTicks` and `ThreadTicks` do not have a stable origin; serialization for
// the purpose of persistence is not supported.

#ifndef BASE_TIME_TIME_H_
#define BASE_TIME_TIME_H_

#include <stdint.h>
#include <time.h>

#include <iosfwd>
#include <limits>

#include "base/base_export.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/numerics/clamped_math.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if defined(OS_FUCHSIA)
#include <zircon/types.h>
#endif

#if defined(OS_APPLE)
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_time.h>
// Avoid Mac system header macro leak.
#undef TYPE_BOOL
#endif

#if defined(OS_ANDROID)
#include <jni.h>
#endif

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <unistd.h>
#include <sys/time.h>
#endif

#if defined(OS_WIN)
#include "base/gtest_prod_util.h"
#include "base/win/windows_types.h"

namespace ABI {
namespace Windows {
namespace Foundation {
struct DateTime;
}  // namespace Foundation
}  // namespace Windows
}  // namespace ABI
#endif

namespace base {

class PlatformThreadHandle;

// TimeDelta ------------------------------------------------------------------

class BASE_EXPORT TimeDelta {
 public:
  constexpr TimeDelta() = default;

  // Converts units of time to TimeDeltas.
  // These conversions treat minimum argument values as min type values or -inf,
  // and maximum ones as max type values or +inf; and their results will produce
  // an is_min() or is_max() TimeDelta. WARNING: Floating point arithmetic is
  // such that FromXXXD(t.InXXXF()) may not precisely equal |t|. Hence, floating
  // point values should not be used for storage.
  static constexpr TimeDelta FromDays(int days);
  static constexpr TimeDelta FromHours(int hours);
  static constexpr TimeDelta FromMinutes(int minutes);
  static constexpr TimeDelta FromSecondsD(double secs);
  static constexpr TimeDelta FromSeconds(int64_t secs);
  static constexpr TimeDelta FromMillisecondsD(double ms);
  static constexpr TimeDelta FromMilliseconds(int64_t ms);
  static constexpr TimeDelta FromMicrosecondsD(double us);
  static constexpr TimeDelta FromMicroseconds(int64_t us);
  static constexpr TimeDelta FromNanosecondsD(double ns);
  static constexpr TimeDelta FromNanoseconds(int64_t ns);

#if defined(OS_WIN)
  static TimeDelta FromQPCValue(LONGLONG qpc_value);
  // TODO(crbug.com/989694): Avoid base::TimeDelta factory functions
  // based on absolute time
  static TimeDelta FromFileTime(FILETIME ft);
  static TimeDelta FromWinrtDateTime(ABI::Windows::Foundation::DateTime dt);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  static TimeDelta FromTimeSpec(const timespec& ts);
#endif
#if defined(OS_FUCHSIA)
  static TimeDelta FromZxDuration(zx_duration_t nanos);
#endif
#if defined(OS_MAC)
  static TimeDelta FromMachTime(uint64_t mach_time);
#endif  // defined(OS_MAC)

  // Converts a frequency in Hertz (cycles per second) into a period.
  static constexpr TimeDelta FromHz(double frequency);

  // Converts an integer value representing TimeDelta to a class. This is used
  // when deserializing a |TimeDelta| structure, using a value known to be
  // compatible. It is not provided as a constructor because the integer type
  // may be unclear from the perspective of a caller.
  //
  // DEPRECATED - Do not use in new code. http://crbug.com/634507
  static constexpr TimeDelta FromInternalValue(int64_t delta) {
    return TimeDelta(delta);
  }

  // Returns the maximum time delta, which should be greater than any reasonable
  // time delta we might compare it to. If converted to double with ToDouble()
  // it becomes an IEEE double infinity. Use FiniteMax() if you want a very
  // large number that doesn't do this. TimeDelta math saturates at the end
  // points so adding to TimeDelta::Max() leaves the value unchanged.
  // Subtracting should leave the value unchanged but currently changes it
  // TODO(https://crbug.com/869387).
  static constexpr TimeDelta Max();

  // Returns the minimum time delta, which should be less than than any
  // reasonable time delta we might compare it to. For more details see the
  // comments for Max().
  static constexpr TimeDelta Min();

  // Returns the maximum time delta which is not equivalent to infinity. Only
  // subtracting a finite time delta from this time delta has a defined result.
  static constexpr TimeDelta FiniteMax();

  // Returns the minimum time delta which is not equivalent to -infinity. Only
  // adding a finite time delta to this time delta has a defined result.
  static constexpr TimeDelta FiniteMin();

  // Returns the internal numeric value of the TimeDelta object. Please don't
  // use this and do arithmetic on it, as it is more error prone than using the
  // provided operators.
  // For serializing, use FromInternalValue to reconstitute.
  //
  // DEPRECATED - Do not use in new code. http://crbug.com/634507
  constexpr int64_t ToInternalValue() const { return delta_; }

  // Returns the magnitude (absolute value) of this TimeDelta.
  constexpr TimeDelta magnitude() const {
    // The code below will not work correctly in this corner case.
    if (is_min())
      return Max();

    // std::abs() is not currently constexpr.  The following is a simple
    // branchless implementation:
    const int64_t mask = delta_ >> (sizeof(delta_) * 8 - 1);
    return TimeDelta((delta_ + mask) ^ mask);
  }

  // Returns true if the time delta is zero.
  constexpr bool is_zero() const { return delta_ == 0; }

  // Returns true if the time delta is the maximum/minimum time delta.
  constexpr bool is_max() const { return *this == Max(); }
  constexpr bool is_min() const { return *this == Min(); }
  constexpr bool is_inf() const { return is_min() || is_max(); }

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  struct timespec ToTimeSpec() const;
#endif
#if defined(OS_FUCHSIA)
  zx_duration_t ToZxDuration() const;
#endif
#if defined(OS_WIN)
  ABI::Windows::Foundation::DateTime ToWinrtDateTime() const;
#endif

  // Returns the frequency in Hertz (cycles per second) that has a period of
  // *this.
  constexpr double ToHz() const { return FromSeconds(1) / *this; }

  // Returns the time delta in some unit. Minimum argument values return as
  // -inf for doubles and min type values otherwise. Maximum ones are treated as
  // +inf for doubles and max type values otherwise. Their results will produce
  // an is_min() or is_max() TimeDelta. The InXYZF versions return a floating
  // point value. The InXYZ versions return a truncated value (aka rounded
  // towards zero, std::trunc() behavior). The InXYZFloored() versions round to
  // lesser integers (std::floor() behavior). The XYZRoundedUp() versions round
  // up to greater integers (std::ceil() behavior). WARNING: Floating point
  // arithmetic is such that FromXXXD(t.InXXXF()) may not precisely equal |t|.
  // Hence, floating point values should not be used for storage.
  int InDays() const;
  int InDaysFloored() const;
  constexpr int InHours() const;
  constexpr int InMinutes() const;
  constexpr double InSecondsF() const;
  constexpr int64_t InSeconds() const;
  double InMillisecondsF() const;
  int64_t InMilliseconds() const;
  int64_t InMillisecondsRoundedUp() const;
  constexpr int64_t InMicroseconds() const { return delta_; }
  double InMicrosecondsF() const;
  constexpr int64_t InNanoseconds() const;

  // Computations with other deltas.
  constexpr TimeDelta operator+(TimeDelta other) const;
  constexpr TimeDelta operator-(TimeDelta other) const;

  constexpr TimeDelta& operator+=(TimeDelta other) {
    return *this = (*this + other);
  }
  constexpr TimeDelta& operator-=(TimeDelta other) {
    return *this = (*this - other);
  }
  constexpr TimeDelta operator-() const {
    if (!is_inf())
      return TimeDelta(-delta_);
    return (delta_ < 0) ? Max() : Min();
  }

  // Computations with numeric types.
  template <typename T>
  constexpr TimeDelta operator*(T a) const {
    CheckedNumeric<int64_t> rv(delta_);
    rv *= a;
    if (rv.IsValid())
      return TimeDelta(rv.ValueOrDie());
    return ((delta_ < 0) == (a < 0)) ? Max() : Min();
  }
  template <typename T>
  constexpr TimeDelta operator/(T a) const {
    CheckedNumeric<int64_t> rv(delta_);
    rv /= a;
    if (rv.IsValid())
      return TimeDelta(rv.ValueOrDie());
    return ((delta_ < 0) == (a < 0)) ? Max() : Min();
  }
  template <typename T>
  constexpr TimeDelta& operator*=(T a) {
    return *this = (*this * a);
  }
  template <typename T>
  constexpr TimeDelta& operator/=(T a) {
    return *this = (*this / a);
  }

  // This does floating-point division. For an integer result, either call
  // IntDiv(), or (possibly clearer) use this operator with
  // base::Clamp{Ceil,Floor,Round}() or base::saturated_cast() (for truncation).
  // Note that converting to double here drops precision to 53 bits.
  constexpr double operator/(TimeDelta a) const {
    // 0/0 and inf/inf (any combination of positive and negative) are invalid
    // (they are almost certainly not intentional, and result in NaN, which
    // turns into 0 if clamped to an integer; this makes introducing subtle bugs
    // too easy).
    CHECK(!is_zero() || !a.is_zero());
    CHECK(!is_inf() || !a.is_inf());

    return ToDouble() / a.ToDouble();
  }
  constexpr int64_t IntDiv(TimeDelta a) const {
    if (!is_inf() && !a.is_zero())
      return delta_ / a.delta_;

    // For consistency, use the same edge case CHECKs and behavior as the code
    // above.
    CHECK(!is_zero() || !a.is_zero());
    CHECK(!is_inf() || !a.is_inf());
    return ((delta_ < 0) == (a.delta_ < 0))
               ? std::numeric_limits<int64_t>::max()
               : std::numeric_limits<int64_t>::min();
  }

  constexpr TimeDelta operator%(TimeDelta a) const {
    return TimeDelta(
        (is_inf() || a.is_zero() || a.is_inf()) ? delta_ : (delta_ % a.delta_));
  }
  constexpr TimeDelta& operator%=(TimeDelta other) {
    return *this = (*this % other);
  }

  // Comparison operators.
  constexpr bool operator==(TimeDelta other) const {
    return delta_ == other.delta_;
  }
  constexpr bool operator!=(TimeDelta other) const {
    return delta_ != other.delta_;
  }
  constexpr bool operator<(TimeDelta other) const {
    return delta_ < other.delta_;
  }
  constexpr bool operator<=(TimeDelta other) const {
    return delta_ <= other.delta_;
  }
  constexpr bool operator>(TimeDelta other) const {
    return delta_ > other.delta_;
  }
  constexpr bool operator>=(TimeDelta other) const {
    return delta_ >= other.delta_;
  }

  // Returns this delta, ceiled/floored/rounded-away-from-zero to the nearest
  // multiple of |interval|.
  TimeDelta CeilToMultiple(TimeDelta interval) const;
  TimeDelta FloorToMultiple(TimeDelta interval) const;
  TimeDelta RoundToMultiple(TimeDelta interval) const;

 private:
  // Constructs a delta given the duration in microseconds. This is private
  // to avoid confusion by callers with an integer constructor. Use
  // FromSeconds, FromMilliseconds, etc. instead.
  constexpr explicit TimeDelta(int64_t delta_us) : delta_(delta_us) {}

  // Returns a double representation of this TimeDelta's tick count.  In
  // particular, Max()/Min() are converted to +/-infinity.
  constexpr double ToDouble() const {
    if (!is_inf())
      return static_cast<double>(delta_);
    return (delta_ < 0) ? -std::numeric_limits<double>::infinity()
                        : std::numeric_limits<double>::infinity();
  }

  // Delta in microseconds.
  int64_t delta_ = 0;
};

constexpr TimeDelta TimeDelta::operator+(TimeDelta other) const {
  if (!other.is_inf())
    return TimeDelta(int64_t{base::ClampAdd(delta_, other.delta_)});

  // Additions involving two infinities are only valid if signs match.
  CHECK(!is_inf() || (delta_ == other.delta_));
  return other;
}

constexpr TimeDelta TimeDelta::operator-(TimeDelta other) const {
  if (!other.is_inf())
    return TimeDelta(int64_t{base::ClampSub(delta_, other.delta_)});

  // Subtractions involving two infinities are only valid if signs differ.
  CHECK_NE(delta_, other.delta_);
  return (other.delta_ < 0) ? Max() : Min();
}

template <typename T>
constexpr TimeDelta operator*(T a, TimeDelta td) {
  return td * a;
}

// For logging use only.
BASE_EXPORT std::ostream& operator<<(std::ostream& os, TimeDelta time_delta);

// TimeBase--------------------------------------------------------------------

// Do not reference the time_internal::TimeBase template class directly.  Please
// use one of the time subclasses instead, and only reference the public
// TimeBase members via those classes.
namespace time_internal {

// Provides value storage and comparison/math operations common to all time
// classes. Each subclass provides for strong type-checking to ensure
// semantically meaningful comparison/math of time values from the same clock
// source or timeline.
template<class TimeClass>
class TimeBase {
 public:
  static constexpr int64_t kHoursPerDay = 24;
  static constexpr int64_t kSecondsPerMinute = 60;
  static constexpr int64_t kMinutesPerHour = 60;
  static constexpr int64_t kSecondsPerHour =
      kSecondsPerMinute * kMinutesPerHour;
  static constexpr int64_t kMillisecondsPerSecond = 1000;
  static constexpr int64_t kMillisecondsPerDay =
      kMillisecondsPerSecond * kSecondsPerHour * kHoursPerDay;
  static constexpr int64_t kMicrosecondsPerMillisecond = 1000;
  static constexpr int64_t kMicrosecondsPerSecond =
      kMicrosecondsPerMillisecond * kMillisecondsPerSecond;
  static constexpr int64_t kMicrosecondsPerMinute =
      kMicrosecondsPerSecond * kSecondsPerMinute;
  static constexpr int64_t kMicrosecondsPerHour =
      kMicrosecondsPerMinute * kMinutesPerHour;
  static constexpr int64_t kMicrosecondsPerDay =
      kMicrosecondsPerHour * kHoursPerDay;
  static constexpr int64_t kMicrosecondsPerWeek = kMicrosecondsPerDay * 7;
  static constexpr int64_t kNanosecondsPerMicrosecond = 1000;
  static constexpr int64_t kNanosecondsPerSecond =
      kNanosecondsPerMicrosecond * kMicrosecondsPerSecond;

  // Returns true if this object has not been initialized.
  //
  // Warning: Be careful when writing code that performs math on time values,
  // since it's possible to produce a valid "zero" result that should not be
  // interpreted as a "null" value.
  constexpr bool is_null() const { return us_ == 0; }

  // Returns true if this object represents the maximum/minimum time.
  constexpr bool is_max() const { return *this == Max(); }
  constexpr bool is_min() const { return *this == Min(); }
  constexpr bool is_inf() const { return is_min() || is_max(); }

  // Returns the maximum/minimum times, which should be greater/less than than
  // any reasonable time with which we might compare it.
  static constexpr TimeClass Max() {
    return TimeClass(std::numeric_limits<int64_t>::max());
  }

  static constexpr TimeClass Min() {
    return TimeClass(std::numeric_limits<int64_t>::min());
  }

  // For legacy serialization only. When serializing to `base::Value`, prefer
  // the helpers from //base/json/values_util.h instead. Otherwise, use
  // `Time::ToDeltaSinceWindowsEpoch()` for `Time` and
  // `TimeDelta::InMiseconds()` for `TimeDelta`. See http://crbug.com/634507.
  constexpr int64_t ToInternalValue() const { return us_; }

  // The amount of time since the origin (or "zero") point. This is a syntactic
  // convenience to aid in code readability, mainly for debugging/testing use
  // cases.
  //
  // Warning: While the Time subclass has a fixed origin point, the origin for
  // the other subclasses can vary each time the application is restarted.
  constexpr TimeDelta since_origin() const {
    return TimeDelta::FromMicroseconds(us_);
  }

  constexpr TimeClass& operator=(TimeClass other) {
    us_ = other.us_;
    return *(static_cast<TimeClass*>(this));
  }

  // Compute the difference between two times.
  constexpr TimeDelta operator-(TimeClass other) const {
    return TimeDelta::FromMicroseconds(us_ - other.us_);
  }

  // Return a new time modified by some delta.
  constexpr TimeClass operator+(TimeDelta delta) const {
    return TimeClass(
        (TimeDelta::FromMicroseconds(us_) + delta).InMicroseconds());
  }
  constexpr TimeClass operator-(TimeDelta delta) const {
    return TimeClass(
        (TimeDelta::FromMicroseconds(us_) - delta).InMicroseconds());
  }

  // Modify by some time delta.
  constexpr TimeClass& operator+=(TimeDelta delta) {
    return static_cast<TimeClass&>(*this = (*this + delta));
  }
  constexpr TimeClass& operator-=(TimeDelta delta) {
    return static_cast<TimeClass&>(*this = (*this - delta));
  }

  // Comparison operators
  constexpr bool operator==(TimeClass other) const { return us_ == other.us_; }
  constexpr bool operator!=(TimeClass other) const { return us_ != other.us_; }
  constexpr bool operator<(TimeClass other) const { return us_ < other.us_; }
  constexpr bool operator<=(TimeClass other) const { return us_ <= other.us_; }
  constexpr bool operator>(TimeClass other) const { return us_ > other.us_; }
  constexpr bool operator>=(TimeClass other) const { return us_ >= other.us_; }

 protected:
  constexpr explicit TimeBase(int64_t us) : us_(us) {}

  // Time value in a microsecond timebase.
  int64_t us_;
};

}  // namespace time_internal

template <class TimeClass>
inline constexpr TimeClass operator+(TimeDelta delta, TimeClass t) {
  return t + delta;
}

// Time -----------------------------------------------------------------------

// Represents a wall clock time in UTC. Values are not guaranteed to be
// monotonically non-decreasing and are subject to large amounts of skew.
// Time is stored internally as microseconds since the Windows epoch (1601).
class BASE_EXPORT Time : public time_internal::TimeBase<Time> {
 public:
  // Offset of UNIX epoch (1970-01-01 00:00:00 UTC) from Windows FILETIME epoch
  // (1601-01-01 00:00:00 UTC), in microseconds. This value is derived from the
  // following: ((1970-1601)*365+89)*24*60*60*1000*1000, where 89 is the number
  // of leap year days between 1601 and 1970: (1970-1601)/4 excluding 1700,
  // 1800, and 1900.
  static constexpr int64_t kTimeTToMicrosecondsOffset =
      INT64_C(11644473600000000);

#if defined(OS_WIN)
  // To avoid overflow in QPC to Microseconds calculations, since we multiply
  // by kMicrosecondsPerSecond, then the QPC value should not exceed
  // (2^63 - 1) / 1E6. If it exceeds that threshold, we divide then multiply.
  static constexpr int64_t kQPCOverflowThreshold = INT64_C(0x8637BD05AF7);
#endif

// kExplodedMinYear and kExplodedMaxYear define the platform-specific limits
// for values passed to FromUTCExploded() and FromLocalExploded(). Those
// functions will return false if passed values outside these limits. The limits
// are inclusive, meaning that the API should support all dates within a given
// limit year.
//
// WARNING: These are not the same limits for the inverse functionality,
// UTCExplode() and LocalExplode(). See method comments for further details.
#if defined(OS_WIN)
  static constexpr int kExplodedMinYear = 1601;
  static constexpr int kExplodedMaxYear = 30827;
#elif defined(OS_IOS) && !__LP64__
  static constexpr int kExplodedMinYear = std::numeric_limits<int>::min();
  static constexpr int kExplodedMaxYear = std::numeric_limits<int>::max();
#elif defined(OS_APPLE)
  static constexpr int kExplodedMinYear = 1902;
  static constexpr int kExplodedMaxYear = std::numeric_limits<int>::max();
#elif defined(OS_ANDROID)
  // Though we use 64-bit time APIs on both 32 and 64 bit Android, some OS
  // versions like KitKat (ARM but not x86 emulator) can't handle some early
  // dates (e.g. before 1170). So we set min conservatively here.
  static constexpr int kExplodedMinYear = 1902;
  static constexpr int kExplodedMaxYear = std::numeric_limits<int>::max();
#else
  static constexpr int kExplodedMinYear =
      (sizeof(time_t) == 4 ? 1902 : std::numeric_limits<int>::min());
  static constexpr int kExplodedMaxYear =
      (sizeof(time_t) == 4 ? 2037 : std::numeric_limits<int>::max());
#endif

  // Represents an exploded time that can be formatted nicely. This is kind of
  // like the Win32 SYSTEMTIME structure or the Unix "struct tm" with a few
  // additions and changes to prevent errors.
  // This structure always represents dates in the Gregorian calendar and always
  // encodes day_of_week as Sunday==0, Monday==1, .., Saturday==6. This means
  // that base::Time::LocalExplode and base::Time::FromLocalExploded only
  // respect the current local time zone in the conversion and do *not* use a
  // calendar or day-of-week encoding from the current locale.
  struct BASE_EXPORT Exploded {
    int year;          // Four digit year "2007"
    int month;         // 1-based month (values 1 = January, etc.)
    int day_of_week;   // 0-based day of week (0 = Sunday, etc.)
    int day_of_month;  // 1-based day of month (1-31)
    int hour;          // Hour within the current day (0-23)
    int minute;        // Minute within the current hour (0-59)
    int second;        // Second within the current minute (0-59 plus leap
                       //   seconds which may take it up to 60).
    int millisecond;   // Milliseconds within the current second (0-999)

    // A cursory test for whether the data members are within their
    // respective ranges. A 'true' return value does not guarantee the
    // Exploded value can be successfully converted to a Time value.
    bool HasValidValues() const;
  };

  // Contains the NULL time. Use Time::Now() to get the current time.
  constexpr Time() : TimeBase(0) {}

  // Returns the time for epoch in Unix-like system (Jan 1, 1970).
  static Time UnixEpoch();

  // Returns the current time. Watch out, the system might adjust its clock
  // in which case time will actually go backwards. We don't guarantee that
  // times are increasing, or that two calls to Now() won't be the same.
  static Time Now();

  // Returns the current time. Same as Now() except that this function always
  // uses system time so that there are no discrepancies between the returned
  // time and system time even on virtual environments including our test bot.
  // For timing sensitive unittests, this function should be used.
  static Time NowFromSystemTime();

  // Converts to/from TimeDeltas relative to the Windows epoch (1601-01-01
  // 00:00:00 UTC).
  //
  // For serialization, when handling `base::Value`, prefer the helpers in
  // //base/json/values_util.h instead. Otherwise, use these methods for
  // opaque serialization and deserialization, e.g.
  //
  //   // Serialization:
  //   base::Time last_updated = ...;
  //   SaveToDatabase(last_updated.ToDeltaSinceWindowsEpoch().InMicroseconds());
  //
  //   // Deserialization:
  //   base::Time last_updated = base::Time::FromDeltaSinceWindowsEpoch(
  //       base::TimeDelta::FromMicroseconds(LoadFromDatabase()));
  //
  // Do not use `FromInternalValue()` or `ToInternalValue()` for this purpose.
  static Time FromDeltaSinceWindowsEpoch(TimeDelta delta);
  TimeDelta ToDeltaSinceWindowsEpoch() const;

  // Converts to/from time_t in UTC and a Time class.
  static Time FromTimeT(time_t tt);
  time_t ToTimeT() const;

  // Converts time to/from a double which is the number of seconds since epoch
  // (Jan 1, 1970).  Webkit uses this format to represent time.
  // Because WebKit initializes double time value to 0 to indicate "not
  // initialized", we map it to empty Time object that also means "not
  // initialized".
  static Time FromDoubleT(double dt);
  double ToDoubleT() const;

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  // Converts the timespec structure to time. MacOS X 10.8.3 (and tentatively,
  // earlier versions) will have the |ts|'s tv_nsec component zeroed out,
  // having a 1 second resolution, which agrees with
  // https://developer.apple.com/legacy/library/#technotes/tn/tn1150.html#HFSPlusDates.
  static Time FromTimeSpec(const timespec& ts);
#endif

  // Converts to/from the Javascript convention for times, a number of
  // milliseconds since the epoch:
  // https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Date/getTime.
  //
  // Don't use ToJsTime() in new code, since it contains a subtle hack (only
  // exactly 1601-01-01 00:00 UTC is represented as 1970-01-01 00:00 UTC), and
  // that is not appropriate for general use. Try to use ToJsTimeIgnoringNull()
  // unless you have a very good reason to use ToJsTime().
  static Time FromJsTime(double ms_since_epoch);
  double ToJsTime() const;
  double ToJsTimeIgnoringNull() const;

  // Converts to/from Java convention for times, a number of milliseconds since
  // the epoch. Because the Java format has less resolution, converting to Java
  // time is a lossy operation.
  static Time FromJavaTime(int64_t ms_since_epoch);
  int64_t ToJavaTime() const;

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  static Time FromTimeVal(struct timeval t);
  struct timeval ToTimeVal() const;
#endif

#if defined(OS_FUCHSIA)
  static Time FromZxTime(zx_time_t time);
  zx_time_t ToZxTime() const;
#endif

#if defined(OS_APPLE)
  static Time FromCFAbsoluteTime(CFAbsoluteTime t);
  CFAbsoluteTime ToCFAbsoluteTime() const;
#if defined(__OBJC__)
  static Time FromNSDate(NSDate* date);
  NSDate* ToNSDate() const;
#endif
#endif

#if defined(OS_WIN)
  static Time FromFileTime(FILETIME ft);
  FILETIME ToFileTime() const;

  // The minimum time of a low resolution timer.  This is basically a windows
  // constant of ~15.6ms.  While it does vary on some older OS versions, we'll
  // treat it as static across all windows versions.
  static const int kMinLowResolutionThresholdMs = 16;

  // Enable or disable Windows high resolution timer.
  static void EnableHighResolutionTimer(bool enable);

  // Activates or deactivates the high resolution timer based on the |activate|
  // flag.  If the HighResolutionTimer is not Enabled (see
  // EnableHighResolutionTimer), this function will return false.  Otherwise
  // returns true.  Each successful activate call must be paired with a
  // subsequent deactivate call.
  // All callers to activate the high resolution timer must eventually call
  // this function to deactivate the high resolution timer.
  static bool ActivateHighResolutionTimer(bool activate);

  // Returns true if the high resolution timer is both enabled and activated.
  // This is provided for testing only, and is not tracked in a thread-safe
  // way.
  static bool IsHighResolutionTimerInUse();

  // The following two functions are used to report the fraction of elapsed time
  // that the high resolution timer is activated.
  // ResetHighResolutionTimerUsage() resets the cumulative usage and starts the
  // measurement interval and GetHighResolutionTimerUsage() returns the
  // percentage of time since the reset that the high resolution timer was
  // activated.
  // ResetHighResolutionTimerUsage() must be called at least once before calling
  // GetHighResolutionTimerUsage(); otherwise the usage result would be
  // undefined.
  static void ResetHighResolutionTimerUsage();
  static double GetHighResolutionTimerUsage();
#endif  // defined(OS_WIN)

  // Converts an exploded structure representing either the local time or UTC
  // into a Time class. Returns false on a failure when, for example, a day of
  // month is set to 31 on a 28-30 day month. Returns Time(0) on overflow.
  // FromLocalExploded respects the current time zone but does not attempt to
  // use the calendar or day-of-week encoding from the current locale - see the
  // comments on base::Time::Exploded for more information.
  static bool FromUTCExploded(const Exploded& exploded,
                              Time* time) WARN_UNUSED_RESULT {
    return FromExploded(false, exploded, time);
  }
  static bool FromLocalExploded(const Exploded& exploded,
                                Time* time) WARN_UNUSED_RESULT {
    return FromExploded(true, exploded, time);
  }

  // Converts a string representation of time to a Time object.
  // An example of a time string which is converted is as below:-
  // "Tue, 15 Nov 1994 12:45:26 GMT". If the timezone is not specified
  // in the input string, FromString assumes local time and FromUTCString
  // assumes UTC. A timezone that cannot be parsed (e.g. "UTC" which is not
  // specified in RFC822) is treated as if the timezone is not specified.
  //
  // WARNING: the underlying converter is very permissive. For example: it is
  // not checked whether a given day of the week matches the date; Feb 29
  // silently becomes Mar 1 in non-leap years; under certain conditions, whole
  // English sentences may be parsed successfully and yield unexpected results.
  //
  // TODO(iyengar) Move the FromString/FromTimeT/ToTimeT/FromFileTime to
  // a new time converter class.
  static bool FromString(const char* time_string,
                         Time* parsed_time) WARN_UNUSED_RESULT {
    return FromStringInternal(time_string, true, parsed_time);
  }
  static bool FromUTCString(const char* time_string,
                            Time* parsed_time) WARN_UNUSED_RESULT {
    return FromStringInternal(time_string, false, parsed_time);
  }

  // Fills the given |exploded| structure with either the local time or UTC from
  // this Time instance. If the conversion cannot be made, the output will be
  // assigned invalid values. Use Exploded::HasValidValues() to confirm a
  // successful conversion.
  //
  // Y10K compliance: This method will successfully convert all Times that
  // represent dates on/after the start of the year 1601 and on/before the start
  // of the year 30828. Some platforms might convert over a wider input range.
  // LocalExplode respects the current time zone but does not attempt to use the
  // calendar or day-of-week encoding from the current locale - see the comments
  // on base::Time::Exploded for more information.
  void UTCExplode(Exploded* exploded) const { Explode(false, exploded); }
  void LocalExplode(Exploded* exploded) const { Explode(true, exploded); }

  // The following two functions round down the time to the nearest day in
  // either UTC or local time. It will represent midnight on that day.
  Time UTCMidnight() const { return Midnight(false); }
  Time LocalMidnight() const { return Midnight(true); }

  // For legacy deserialization only. Converts an integer value representing
  // Time to a class. This may be used when deserializing a |Time| structure,
  // using a value known to be compatible. It is not provided as a constructor
  // because the integer type may be unclear from the perspective of a caller.
  //
  // DEPRECATED - Do not use in new code. When deserializing from `base::Value`,
  // prefer the helpers from //base/json/values_util.h instead.
  // Otherwise, use `Time::FromDeltaSinceWindowsEpoch()` for `Time` and
  // `TimeDelta::FromMiseconds()` for `TimeDelta`. http://crbug.com/634507
  static constexpr Time FromInternalValue(int64_t us) { return Time(us); }

 private:
  friend class time_internal::TimeBase<Time>;

  constexpr explicit Time(int64_t microseconds_since_win_epoch)
      : TimeBase(microseconds_since_win_epoch) {}

  // Explodes the given time to either local time |is_local = true| or UTC
  // |is_local = false|.
  void Explode(bool is_local, Exploded* exploded) const;

  // Unexplodes a given time assuming the source is either local time
  // |is_local = true| or UTC |is_local = false|. Function returns false on
  // failure and sets |time| to Time(0). Otherwise returns true and sets |time|
  // to non-exploded time.
  static bool FromExploded(bool is_local,
                           const Exploded& exploded,
                           Time* time) WARN_UNUSED_RESULT;

  // Some platforms use the ICU library to provide To/FromExploded, when their
  // native library implementations are insufficient in some way.
  static void ExplodeUsingIcu(int64_t millis_since_unix_epoch,
                              bool is_local,
                              Exploded* exploded);
  static bool FromExplodedUsingIcu(bool is_local,
                                   const Exploded& exploded,
                                   int64_t* millis_since_unix_epoch)
      WARN_UNUSED_RESULT;

  // Rounds down the time to the nearest day in either local time
  // |is_local = true| or UTC |is_local = false|.
  Time Midnight(bool is_local) const;

  // Converts a string representation of time to a Time object.
  // An example of a time string which is converted is as below:-
  // "Tue, 15 Nov 1994 12:45:26 GMT". If the timezone is not specified
  // in the input string, local time |is_local = true| or
  // UTC |is_local = false| is assumed. A timezone that cannot be parsed
  // (e.g. "UTC" which is not specified in RFC822) is treated as if the
  // timezone is not specified.
  static bool FromStringInternal(const char* time_string,
                                 bool is_local,
                                 Time* parsed_time) WARN_UNUSED_RESULT;

  // Comparison does not consider |day_of_week| when doing the operation.
  static bool ExplodedMostlyEquals(const Exploded& lhs,
                                   const Exploded& rhs) WARN_UNUSED_RESULT;

  // Converts the provided time in milliseconds since the Unix epoch (1970) to a
  // Time object, avoiding overflows.
  static bool FromMillisecondsSinceUnixEpoch(int64_t unix_milliseconds,
                                             Time* time) WARN_UNUSED_RESULT;

  // Returns the milliseconds since the Unix epoch (1970), rounding the
  // microseconds towards -infinity.
  int64_t ToRoundedDownMillisecondsSinceUnixEpoch() const;
};

// TimeDelta functions that must appear below the declarations of Time/TimeDelta

// static
constexpr TimeDelta TimeDelta::FromDays(int days) {
  return (days == std::numeric_limits<int>::max())
             ? Max()
             : TimeDelta(days * Time::kMicrosecondsPerDay);
}

// static
constexpr TimeDelta TimeDelta::FromHours(int hours) {
  return (hours == std::numeric_limits<int>::max())
             ? Max()
             : TimeDelta(hours * Time::kMicrosecondsPerHour);
}

// static
constexpr TimeDelta TimeDelta::FromMinutes(int minutes) {
  return (minutes == std::numeric_limits<int>::max())
             ? Max()
             : TimeDelta(minutes * Time::kMicrosecondsPerMinute);
}

// static
constexpr TimeDelta TimeDelta::FromSecondsD(double secs) {
  return TimeDelta(
      saturated_cast<int64_t>(secs * Time::kMicrosecondsPerSecond));
}

// static
constexpr TimeDelta TimeDelta::FromSeconds(int64_t secs) {
  return TimeDelta(int64_t{base::ClampMul(secs, Time::kMicrosecondsPerSecond)});
}

// static
constexpr TimeDelta TimeDelta::FromMillisecondsD(double ms) {
  return TimeDelta(
      saturated_cast<int64_t>(ms * Time::kMicrosecondsPerMillisecond));
}

// static
constexpr TimeDelta TimeDelta::FromMilliseconds(int64_t ms) {
  return TimeDelta(
      int64_t{base::ClampMul(ms, Time::kMicrosecondsPerMillisecond)});
}

// static
constexpr TimeDelta TimeDelta::FromMicrosecondsD(double us) {
  return TimeDelta(saturated_cast<int64_t>(us));
}

// static
constexpr TimeDelta TimeDelta::FromMicroseconds(int64_t us) {
  return TimeDelta(us);
}

// static
constexpr TimeDelta TimeDelta::FromNanosecondsD(double ns) {
  return TimeDelta(
      saturated_cast<int64_t>(ns / Time::kNanosecondsPerMicrosecond));
}

// static
constexpr TimeDelta TimeDelta::FromNanoseconds(int64_t ns) {
  return TimeDelta(ns / Time::kNanosecondsPerMicrosecond);
}

// static
constexpr TimeDelta TimeDelta::FromHz(double frequency) {
  return FromSeconds(1) / frequency;
}

constexpr int TimeDelta::InHours() const {
  // saturated_cast<> is necessary since very large (but still less than
  // min/max) deltas would result in overflow.
  return saturated_cast<int>(delta_ / Time::kMicrosecondsPerHour);
}

constexpr int TimeDelta::InMinutes() const {
  // saturated_cast<> is necessary since very large (but still less than
  // min/max) deltas would result in overflow.
  return saturated_cast<int>(delta_ / Time::kMicrosecondsPerMinute);
}

constexpr double TimeDelta::InSecondsF() const {
  if (!is_inf())
    return static_cast<double>(delta_) / Time::kMicrosecondsPerSecond;
  return (delta_ < 0) ? -std::numeric_limits<double>::infinity()
                      : std::numeric_limits<double>::infinity();
}

constexpr int64_t TimeDelta::InSeconds() const {
  return is_inf() ? delta_ : (delta_ / Time::kMicrosecondsPerSecond);
}

constexpr int64_t TimeDelta::InNanoseconds() const {
  return base::ClampMul(delta_, Time::kNanosecondsPerMicrosecond);
}

// static
constexpr TimeDelta TimeDelta::Max() {
  return TimeDelta(std::numeric_limits<int64_t>::max());
}

// static
constexpr TimeDelta TimeDelta::Min() {
  return TimeDelta(std::numeric_limits<int64_t>::min());
}

// static
constexpr TimeDelta TimeDelta::FiniteMax() {
  return TimeDelta(std::numeric_limits<int64_t>::max() - 1);
}

// static
constexpr TimeDelta TimeDelta::FiniteMin() {
  return TimeDelta(std::numeric_limits<int64_t>::min() + 1);
}

// For logging use only.
BASE_EXPORT std::ostream& operator<<(std::ostream& os, Time time);

// TimeTicks ------------------------------------------------------------------

// Represents monotonically non-decreasing clock time.
class BASE_EXPORT TimeTicks : public time_internal::TimeBase<TimeTicks> {
 public:
  // The underlying clock used to generate new TimeTicks.
  enum class Clock {
    FUCHSIA_ZX_CLOCK_MONOTONIC,
    LINUX_CLOCK_MONOTONIC,
    IOS_CF_ABSOLUTE_TIME_MINUS_KERN_BOOTTIME,
    MAC_MACH_ABSOLUTE_TIME,
    WIN_QPC,
    WIN_ROLLOVER_PROTECTED_TIME_GET_TIME
  };

  constexpr TimeTicks() : TimeBase(0) {}

  // Platform-dependent tick count representing "right now." When
  // IsHighResolution() returns false, the resolution of the clock could be
  // as coarse as ~15.6ms. Otherwise, the resolution should be no worse than one
  // microsecond.
  static TimeTicks Now();

  // Returns true if the high resolution clock is working on this system and
  // Now() will return high resolution values. Note that, on systems where the
  // high resolution clock works but is deemed inefficient, the low resolution
  // clock will be used instead.
  static bool IsHighResolution() WARN_UNUSED_RESULT;

  // Returns true if TimeTicks is consistent across processes, meaning that
  // timestamps taken on different processes can be safely compared with one
  // another. (Note that, even on platforms where this returns true, time values
  // from different threads that are within one tick of each other must be
  // considered to have an ambiguous ordering.)
  static bool IsConsistentAcrossProcesses() WARN_UNUSED_RESULT;

#if defined(OS_FUCHSIA)
  // Converts between TimeTicks and an ZX_CLOCK_MONOTONIC zx_time_t value.
  static TimeTicks FromZxTime(zx_time_t nanos_since_boot);
  zx_time_t ToZxTime() const;
#endif

#if defined(OS_WIN)
  // Translates an absolute QPC timestamp into a TimeTicks value. The returned
  // value has the same origin as Now(). Do NOT attempt to use this if
  // IsHighResolution() returns false.
  static TimeTicks FromQPCValue(LONGLONG qpc_value);
#endif

#if defined(OS_MAC)
  static TimeTicks FromMachAbsoluteTime(uint64_t mach_absolute_time);

  static mach_timebase_info_data_t* MachTimebaseInfo();
#endif  // defined(OS_MAC)

#if defined(OS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
  // Converts to TimeTicks the value obtained from SystemClock.uptimeMillis().
  // Note: this convertion may be non-monotonic in relation to previously
  // obtained TimeTicks::Now() values because of the truncation (to
  // milliseconds) performed by uptimeMillis().
  static TimeTicks FromUptimeMillis(int64_t uptime_millis_value);
#endif

  // Get an estimate of the TimeTick value at the time of the UnixEpoch. Because
  // Time and TimeTicks respond differently to user-set time and NTP
  // adjustments, this number is only an estimate. Nevertheless, this can be
  // useful when you need to relate the value of TimeTicks to a real time and
  // date. Note: Upon first invocation, this function takes a snapshot of the
  // realtime clock to establish a reference point.  This function will return
  // the same value for the duration of the application, but will be different
  // in future application runs.
  static TimeTicks UnixEpoch();

  // Returns |this| snapped to the next tick, given a |tick_phase| and
  // repeating |tick_interval| in both directions. |this| may be before,
  // after, or equal to the |tick_phase|.
  TimeTicks SnappedToNextTick(TimeTicks tick_phase,
                              TimeDelta tick_interval) const;

  // Returns an enum indicating the underlying clock being used to generate
  // TimeTicks timestamps. This function should only be used for debugging and
  // logging purposes.
  static Clock GetClock();

  // Converts an integer value representing TimeTicks to a class. This may be
  // used when deserializing a |TimeTicks| structure, using a value known to be
  // compatible. It is not provided as a constructor because the integer type
  // may be unclear from the perspective of a caller.
  //
  // DEPRECATED - Do not use in new code. For deserializing TimeTicks values,
  // prefer TimeTicks + TimeDelta(); however, be aware that the origin is not
  // fixed and may vary. Serializing for persistence is strongly discouraged.
  // http://crbug.com/634507
  static constexpr TimeTicks FromInternalValue(int64_t us) {
    return TimeTicks(us);
  }

 protected:
#if defined(OS_WIN)
  typedef DWORD (*TickFunctionType)(void);
  static TickFunctionType SetMockTickFunction(TickFunctionType ticker);
#endif

 private:
  friend class time_internal::TimeBase<TimeTicks>;

  // Please use Now() to create a new object. This is for internal use
  // and testing.
  constexpr explicit TimeTicks(int64_t us) : TimeBase(us) {}
};

// For logging use only.
BASE_EXPORT std::ostream& operator<<(std::ostream& os, TimeTicks time_ticks);

// ThreadTicks ----------------------------------------------------------------

// Represents a clock, specific to a particular thread, than runs only while the
// thread is running.
class BASE_EXPORT ThreadTicks : public time_internal::TimeBase<ThreadTicks> {
 public:
  constexpr ThreadTicks() : TimeBase(0) {}

  // Returns true if ThreadTicks::Now() is supported on this system.
  static bool IsSupported() WARN_UNUSED_RESULT {
#if (defined(_POSIX_THREAD_CPUTIME) && (_POSIX_THREAD_CPUTIME >= 0)) || \
    defined(OS_MAC) || defined(OS_ANDROID) || defined(OS_FUCHSIA)
    return true;
#elif defined(OS_WIN)
    return IsSupportedWin();
#else
    return false;
#endif
  }

  // Waits until the initialization is completed. Needs to be guarded with a
  // call to IsSupported().
  static void WaitUntilInitialized() {
#if defined(OS_WIN)
    WaitUntilInitializedWin();
#endif
  }

  // Returns thread-specific CPU-time on systems that support this feature.
  // Needs to be guarded with a call to IsSupported(). Use this timer
  // to (approximately) measure how much time the calling thread spent doing
  // actual work vs. being de-scheduled. May return bogus results if the thread
  // migrates to another CPU between two calls. Returns an empty ThreadTicks
  // object until the initialization is completed. If a clock reading is
  // absolutely needed, call WaitUntilInitialized() before this method.
  static ThreadTicks Now();

#if defined(OS_WIN)
  // Similar to Now() above except this returns thread-specific CPU time for an
  // arbitrary thread. All comments for Now() method above apply apply to this
  // method as well.
  static ThreadTicks GetForThread(const PlatformThreadHandle& thread_handle);
#endif

  // Converts an integer value representing ThreadTicks to a class. This may be
  // used when deserializing a |ThreadTicks| structure, using a value known to
  // be compatible. It is not provided as a constructor because the integer type
  // may be unclear from the perspective of a caller.
  //
  // DEPRECATED - Do not use in new code. For deserializing ThreadTicks values,
  // prefer ThreadTicks + TimeDelta(); however, be aware that the origin is not
  // fixed and may vary. Serializing for persistence is strongly
  // discouraged. http://crbug.com/634507
  static constexpr ThreadTicks FromInternalValue(int64_t us) {
    return ThreadTicks(us);
  }

 private:
  friend class time_internal::TimeBase<ThreadTicks>;

  // Please use Now() or GetForThread() to create a new object. This is for
  // internal use and testing.
  constexpr explicit ThreadTicks(int64_t us) : TimeBase(us) {}

#if defined(OS_WIN)
  FRIEND_TEST_ALL_PREFIXES(TimeTicks, TSCTicksPerSecond);

#if defined(ARCH_CPU_ARM64)
  // TSCTicksPerSecond is not supported on Windows on Arm systems because the
  // cycle-counting methods use the actual CPU cycle count, and not a consistent
  // incrementing counter.
#else
  // Returns the frequency of the TSC in ticks per second, or 0 if it hasn't
  // been measured yet. Needs to be guarded with a call to IsSupported().
  // This method is declared here rather than in the anonymous namespace to
  // allow testing.
  static double TSCTicksPerSecond();
#endif

  static bool IsSupportedWin() WARN_UNUSED_RESULT;
  static void WaitUntilInitializedWin();
#endif
};

// For logging use only.
BASE_EXPORT std::ostream& operator<<(std::ostream& os, ThreadTicks time_ticks);

}  // namespace base

#endif  // BASE_TIME_TIME_H_
