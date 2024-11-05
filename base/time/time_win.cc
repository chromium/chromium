// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Windows Timer Primer
//
// A good article:  http://www.ddj.com/windows/184416651
// A good mozilla bug:  http://bugzilla.mozilla.org/show_bug.cgi?id=363258
//
// The default windows timer, GetSystemTimePreciseAsFileTime is quite precise.
// However it is not always fast on some hardware and is slower than the
// performance counters.
//
// QueryPerformanceCounter is the logical choice for a high-precision timer.
// However, it is known to be buggy on some hardware.  Specifically, it can
// sometimes "jump".  On laptops, QPC can also be very expensive to call.
// It's 3-4x slower than timeGetTime() on desktops, but can be 10x slower
// on laptops.  A unittest exists which will show the relative cost of various
// timers on any system.
//
// The next logical choice is timeGetTime().  timeGetTime has a precision of
// 1ms, but only if you call APIs (timeBeginPeriod()) which affect all other
// applications on the system.  By default, precision is only 15.5ms.
// Unfortunately, we don't want to call timeBeginPeriod because we don't
// want to affect other applications.  Further, on mobile platforms, use of
// faster multimedia timers can hurt battery life.  See the intel
// article about this here:
// http://softwarecommunity.intel.com/articles/eng/1086.htm
//
// To work around all this, we're going to generally use timeGetTime().  We
// will only increase the system-wide timer if we're not running on battery
// power.

#include "base/time/time.h"

#include <windows.h>

#include <mmsystem.h>
#include <stdint.h>
#include <windows.foundation.h>

#include <atomic>
#include <ostream>

#include "base/bit_cast.h"
#include "base/check_op.h"
#include "base/cpu.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/time/time_override.h"
#include "build/build_config.h"

namespace base {

namespace {

// From MSDN, FILETIME "Contains a 64-bit value representing the number of
// 100-nanosecond intervals since January 1, 1601 (UTC)."
int64_t FileTimeToMicroseconds(const FILETIME& ft) {
  // Need to bit_cast to fix alignment, then divide by 10 to convert
  // 100-nanoseconds to microseconds. This only works on little-endian
  // machines.
  return bit_cast<int64_t, FILETIME>(ft) / 10;
}

bool CanConvertToFileTime(int64_t us) {
  return us >= 0 && us <= (std::numeric_limits<int64_t>::max() / 10);
}

FILETIME MicrosecondsToFileTime(int64_t us) {
  DCHECK(CanConvertToFileTime(us)) << "Out-of-range: Cannot convert " << us
                                   << " microseconds to FILETIME units.";

  // Multiply by 10 to convert microseconds to 100-nanoseconds. Bit_cast will
  // handle alignment problems. This only works on little-endian machines.
  return bit_cast<FILETIME, int64_t>(us * 10);
}

int64_t CurrentWallclockMicroseconds() {
  FILETIME ft;
  ::GetSystemTimePreciseAsFileTime(&ft);
  return FileTimeToMicroseconds(ft);
}

// Time between resampling the un-granular clock for this API.
constexpr TimeDelta kMaxTimeToAvoidDrift = Seconds(60);

int64_t g_initial_time = 0;
TimeTicks g_initial_ticks;

void InitializeClock() {
  g_initial_ticks = subtle::TimeTicksNowIgnoringOverride();
  g_initial_time = CurrentWallclockMicroseconds();
}

// Track the last value passed to timeBeginPeriod so that we can cancel that
// call by calling timeEndPeriod with the same value. A value of zero means that
// the timer frequency is not currently raised.
UINT g_last_interval_requested_ms = 0;
// Track if kMinTimerIntervalHighResMs or kMinTimerIntervalLowResMs is active.
// For most purposes this could also be named g_is_on_ac_power.
bool g_high_res_timer_enabled = false;
// How many times the high resolution timer has been called.
uint32_t g_high_res_timer_count = 0;
// Start time of the high resolution timer usage monitoring. This is needed
// to calculate the usage as percentage of the total elapsed time.
TimeTicks g_high_res_timer_usage_start;
// The cumulative time the high resolution timer has been in use since
// |g_high_res_timer_usage_start| moment.
TimeDelta g_high_res_timer_usage;
// Timestamp of the last activation change of the high resolution timer. This
// is used to calculate the cumulative usage.
TimeTicks g_high_res_timer_last_activation;
// The lock to control access to the above set of variables.
Lock* GetHighResLock() {
  static auto* lock = new Lock();
  return lock;
}

// The two values that ActivateHighResolutionTimer uses to set the systemwide
// timer interrupt frequency on Windows. These control how precise timers are
// but also have a big impact on battery life.

// Used when a faster timer has been requested (g_high_res_timer_count > 0) and
// the computer is running on AC power (plugged in) so that it's okay to go to
// the highest frequency.
constexpr UINT kMinTimerIntervalHighResMs = 1;

// Used when a faster timer has been requested (g_high_res_timer_count > 0) and
// the computer is running on DC power (battery) so that we don't want to raise
// the timer frequency as much.
constexpr UINT kMinTimerIntervalLowResMs = 8;

// Calculate the desired timer interrupt interval. Note that zero means that the
// system default should be used.
UINT GetIntervalMs() {
  if (!g_high_res_timer_count)
    return 0;  // Use the default, typically 15.625
  if (g_high_res_timer_enabled)
    return kMinTimerIntervalHighResMs;
  return kMinTimerIntervalLowResMs;
}

// Compare the currently requested timer interrupt interval to the last interval
// requested and update if necessary (by cancelling the old request and making a
// new request). If there is no change then do nothing.
void UpdateTimerIntervalLocked() {
  UINT new_interval = GetIntervalMs();
  if (new_interval == g_last_interval_requested_ms)
    return;
  if (g_last_interval_requested_ms) {
    // Record how long the timer interrupt frequency was raised.
    g_high_res_timer_usage += subtle::TimeTicksNowIgnoringOverride() -
                              g_high_res_timer_last_activation;
    // Reset the timer interrupt back to the default.
    timeEndPeriod(g_last_interval_requested_ms);
  }
  g_last_interval_requested_ms = new_interval;
  if (g_last_interval_requested_ms) {
    // Record when the timer interrupt was raised.
    g_high_res_timer_last_activation = subtle::TimeTicksNowIgnoringOverride();
    timeBeginPeriod(g_last_interval_requested_ms);
  }
}

// Returns the current value of the performance counter.
int64_t QPCNowRaw() {
  LARGE_INTEGER perf_counter_now = {};
  // According to the MSDN documentation for QueryPerformanceCounter(), this
  // will never fail on systems that run XP or later.
  // https://msdn.microsoft.com/library/windows/desktop/ms644904.aspx
  ::QueryPerformanceCounter(&perf_counter_now);
  return perf_counter_now.QuadPart;
}

bool SafeConvertToWord(int in, WORD* out) {
  CheckedNumeric<WORD> result = in;
  *out = result.ValueOrDefault(std::numeric_limits<WORD>::max());
  return result.IsValid();
}

}  // namespace

// Time -----------------------------------------------------------------------

namespace subtle {
Time TimeNowIgnoringOverride() {
  if (g_initial_time == 0)
    InitializeClock();

  // We implement time using the high-resolution timers so that we can get
  // timeouts which likely are smaller than those if we just used
  // CurrentWallclockMicroseconds().
  //
  // To make this work, we initialize the clock (g_initial_time) and the
  // counter (initial_ctr).  To compute the initial time, we can check
  // the number of ticks that have elapsed, and compute the delta.
  //
  // To avoid any drift, we periodically resync the counters to the system
  // clock.
  while (true) {
    TimeTicks ticks = TimeTicksNowIgnoringOverride();

    // Calculate the time elapsed since we started our timer
    TimeDelta elapsed = ticks - g_initial_ticks;

    // Check if enough time has elapsed that we need to resync the clock.
    if (elapsed > kMaxTimeToAvoidDrift) {
      InitializeClock();
      continue;
    }

    return Time() + elapsed + Microseconds(g_initial_time);
  }
}

Time TimeNowFromSystemTimeIgnoringOverride() {
  // Force resync.
  InitializeClock();
  return Time() + Microseconds(g_initial_time);
}
}  // namespace subtle

// static
Time Time::FromFileTime(FILETIME ft) {
  if (bit_cast<int64_t, FILETIME>(ft) == 0)
    return Time();
  if (ft.dwHighDateTime == std::numeric_limits<DWORD>::max() &&
      ft.dwLowDateTime == std::numeric_limits<DWORD>::max())
    return Max();
  return Time(FileTimeToMicroseconds(ft));
}

FILETIME Time::ToFileTime() const {
  if (is_null())
    return bit_cast<FILETIME, int64_t>(0);
  if (is_max()) {
    FILETIME result;
    result.dwHighDateTime = std::numeric_limits<DWORD>::max();
    result.dwLowDateTime = std::numeric_limits<DWORD>::max();
    return result;
  }
  return MicrosecondsToFileTime(us_);
}

// static
// Enable raising of the system-global timer interrupt frequency to 1 kHz (when
// enable is true, which happens when on AC power) or some lower frequency when
// on battery power (when enable is false). If the g_high_res_timer_enabled
// setting hasn't actually changed or if if there are no outstanding requests
// (if g_high_res_timer_count is zero) then do nothing.
// TL;DR - call this when going from AC to DC power or vice-versa.
void Time::EnableHighResolutionTimer(bool enable) {
  AutoLock lock(*GetHighResLock());
  g_high_res_timer_enabled = enable;
  UpdateTimerIntervalLocked();
}

// static
// Request that the system-global Windows timer interrupt frequency be raised.
// How high the frequency is raised depends on the system's power state and
// possibly other options.
// TL;DR - call this at the beginning and end of a time period where you want
// higher frequency timer interrupts. Each call with activating=true must be
// paired with a subsequent activating=false call.
bool Time::ActivateHighResolutionTimer(bool activating) {
  // We only do work on the transition from zero to one or one to zero so we
  // can easily undo the effect (if necessary) when EnableHighResolutionTimer is
  // called.
  const uint32_t max = std::numeric_limits<uint32_t>::max();

  AutoLock lock(*GetHighResLock());
  if (activating) {
    DCHECK_NE(g_high_res_timer_count, max);
    ++g_high_res_timer_count;
  } else {
    DCHECK_NE(g_high_res_timer_count, 0u);
    --g_high_res_timer_count;
  }
  UpdateTimerIntervalLocked();
  return true;
}

// static
// See if the timer interrupt interval has been set to the lowest value.
bool Time::IsHighResolutionTimerInUse() {
  AutoLock lock(*GetHighResLock());
  return g_last_interval_requested_ms == kMinTimerIntervalHighResMs;
}

// static
void Time::ResetHighResolutionTimerUsage() {
  AutoLock lock(*GetHighResLock());
  g_high_res_timer_usage = TimeDelta();
  g_high_res_timer_usage_start = subtle::TimeTicksNowIgnoringOverride();
  if (g_high_res_timer_count > 0)
    g_high_res_timer_last_activation = g_high_res_timer_usage_start;
}

// static
double Time::GetHighResolutionTimerUsage() {
  AutoLock lock(*GetHighResLock());
  TimeTicks now = subtle::TimeTicksNowIgnoringOverride();
  TimeDelta elapsed_time = now - g_high_res_timer_usage_start;
  if (elapsed_time.is_zero()) {
    // This is unexpected but possible if TimeTicks resolution is low and
    // GetHighResolutionTimerUsage() is called promptly after
    // ResetHighResolutionTimerUsage().
    return 0.0;
  }
  TimeDelta used_time = g_high_res_timer_usage;
  if (g_high_res_timer_count > 0) {
    // If currently activated add the remainder of time since the last
    // activation.
    used_time += now - g_high_res_timer_last_activation;
  }
  return used_time / elapsed_time * 100;
}

// static
bool Time::FromExploded(bool is_local, const Exploded& exploded, Time* time) {
  // Create the system struct representing our exploded time. It will either be
  // in local time or UTC.If casting from int to WORD results in overflow,
  // fail and return Time(0).
  SYSTEMTIME st;
  if (!SafeConvertToWord(exploded.year, &st.wYear) ||
      !SafeConvertToWord(exploded.month, &st.wMonth) ||
      !SafeConvertToWord(exploded.day_of_week, &st.wDayOfWeek) ||
      !SafeConvertToWord(exploded.day_of_month, &st.wDay) ||
      !SafeConvertToWord(exploded.hour, &st.wHour) ||
      !SafeConvertToWord(exploded.minute, &st.wMinute) ||
      !SafeConvertToWord(exploded.second, &st.wSecond) ||
      !SafeConvertToWord(exploded.millisecond, &st.wMilliseconds)) {
    *time = Time(0);
    return false;
  }

  FILETIME ft;
  bool success = true;
  // Ensure that it's in UTC.
  if (is_local) {
    SYSTEMTIME utc_st;
    success = TzSpecificLocalTimeToSystemTime(nullptr, &st, &utc_st) &&
              SystemTimeToFileTime(&utc_st, &ft);
  } else {
    success = !!SystemTimeToFileTime(&st, &ft);
  }

  *time = Time(success ? FileTimeToMicroseconds(ft) : 0);
  return success;
}

void Time::Explode(bool is_local, Exploded* exploded) const {
  if (!CanConvertToFileTime(us_)) {
    // We are not able to convert it to FILETIME.
    ZeroMemory(exploded, sizeof(*exploded));
    return;
  }

  const FILETIME utc_ft = MicrosecondsToFileTime(us_);

  // FILETIME in local time if necessary.
  bool success = true;
  // FILETIME in SYSTEMTIME (exploded).
  SYSTEMTIME st = {0};
  if (is_local) {
    SYSTEMTIME utc_st;
    // We don't use FileTimeToLocalFileTime here, since it uses the current
    // settings for the time zone and daylight saving time. Therefore, if it is
    // daylight saving time, it will take daylight saving time into account,
    // even if the time you are converting is in standard time.
    success = FileTimeToSystemTime(&utc_ft, &utc_st) &&
              SystemTimeToTzSpecificLocalTime(nullptr, &utc_st, &st);
  } else {
    success = !!FileTimeToSystemTime(&utc_ft, &st);
  }

  if (!success) {
    ZeroMemory(exploded, sizeof(*exploded));
    return;
  }

  exploded->year = st.wYear;
  exploded->month = st.wMonth;
  exploded->day_of_week = st.wDayOfWeek;
  exploded->day_of_month = st.wDay;
  exploded->hour = st.wHour;
  exploded->minute = st.wMinute;
  exploded->second = st.wSecond;
  exploded->millisecond = st.wMilliseconds;
}

// TimeTicks ------------------------------------------------------------------

namespace {

// We define a wrapper to adapt between the __stdcall and __cdecl call of the
// mock function, and to avoid a static constructor.  Assigning an import to a
// function pointer directly would require setup code to fetch from the IAT.
DWORD timeGetTimeWrapper() {
  return timeGetTime();
}

DWORD (*g_tick_function)(void) = &timeGetTimeWrapper;

// A structure holding the most significant bits of "last seen" and a
// "rollover" counter.
union LastTimeAndRolloversState {
  // The state as a single 32-bit opaque value.
  std::atomic<int32_t> as_opaque_32{0};

  // The state as usable values.
  struct {
    // The top 8-bits of the "last" time. This is enough to check for rollovers
    // and the small bit-size means fewer CompareAndSwap operations to store
    // changes in state, which in turn makes for fewer retries.
    uint8_t last_8;
    // A count of the number of detected rollovers. Using this as bits 47-32
    // of the upper half of a 64-bit value results in a 48-bit tick counter.
    // This extends the total rollover period from about 49 days to about 8800
    // years while still allowing it to be stored with last_8 in a single
    // 32-bit value.
    uint16_t rollovers;
  } as_values;
};
std::atomic<int32_t> g_last_time_and_rollovers = 0;
static_assert(
    sizeof(LastTimeAndRolloversState) <= sizeof(g_last_time_and_rollovers),
    "LastTimeAndRolloversState does not fit in a single atomic word");

// We use timeGetTime() to implement TimeTicks::Now().  This can be problematic
// because it returns the number of milliseconds since Windows has started,
// which will roll over the 32-bit value every ~49 days.  We try to track
// rollover ourselves, which works if TimeTicks::Now() is called at least every
// 48.8 days (not 49 days because only changes in the top 8 bits get noticed).
TimeTicks RolloverProtectedNow() {
  LastTimeAndRolloversState state;
  DWORD now;  // DWORD is always unsigned 32 bits.

  while (true) {
    // Fetch the "now" and "last" tick values, updating "last" with "now" and
    // incrementing the "rollovers" counter if the tick-value has wrapped back
    // around. Atomic operations ensure that both "last" and "rollovers" are
    // always updated together.
    int32_t original =
        g_last_time_and_rollovers.load(std::memory_order_acquire);
    state.as_opaque_32 = original;
    now = g_tick_function();
    uint8_t now_8 = static_cast<uint8_t>(now >> 24);
    if (now_8 < state.as_values.last_8)
      ++state.as_values.rollovers;
    state.as_values.last_8 = now_8;

    // If the state hasn't changed, exit the loop.
    if (state.as_opaque_32 == original)
      break;

    // Save the changed state. If the existing value is unchanged from the
    // original so that the operation is successful. Exit the loop.
    bool success = g_last_time_and_rollovers.compare_exchange_strong(
        original, state.as_opaque_32, std::memory_order_release);
    if (success)
      break;

    // Another thread has done something in between so retry from the top.
  }

  return TimeTicks() +
         Milliseconds(now +
                      (static_cast<uint64_t>(state.as_values.rollovers) << 32));
}

// Discussion of tick counter options on Windows:
//
// (1) CPU cycle counter. (Retrieved via RDTSC)
// The CPU counter provides the highest resolution time stamp and is the least
// expensive to retrieve. However, on older CPUs, two issues can affect its
// reliability: First it is maintained per processor and not synchronized
// between processors. Also, the counters will change frequency due to thermal
// and power changes, and stop in some states.
//
// (2) QueryPerformanceCounter (QPC). The QPC counter provides a high-
// resolution (<1 microsecond) time stamp. On most hardware running today, it
// auto-detects and uses the constant-rate RDTSC counter to provide extremely
// efficient and reliable time stamps.
//
// On older CPUs where RDTSC is unreliable, it falls back to using more
// expensive (20X to 40X more costly) alternate clocks, such as HPET or the ACPI
// PM timer, and can involve system calls; and all this is up to the HAL (with
// some help from ACPI). According to
// http://blogs.msdn.com/oldnewthing/archive/2005/09/02/459952.aspx, in the
// worst case, it gets the counter from the rollover interrupt on the
// programmable interrupt timer. In best cases, the HAL may conclude that the
// RDTSC counter runs at a constant frequency, then it uses that instead. On
// multiprocessor machines, it will try to verify the values returned from
// RDTSC on each processor are consistent with each other, and apply a handful
// of workarounds for known buggy hardware. In other words, QPC is supposed to
// give consistent results on a multiprocessor computer, but for older CPUs it
// can be unreliable due bugs in BIOS or HAL.
//
// (3) System time. The system time provides a low-resolution (from ~1 to ~15.6
// milliseconds) time stamp but is comparatively less expensive to retrieve and
// more reliable. Time::EnableHighResolutionTimer() and
// Time::ActivateHighResolutionTimer() can be called to alter the resolution of
// this timer; and also other Windows applications can alter it, affecting this
// one.

TimeTicks InitialNowFunction();

// See "threading notes" in InitializeNowFunctionPointer() for details on how
// concurrent reads/writes to these globals has been made safe.
std::atomic<TimeTicksNowFunction> g_time_ticks_now_ignoring_override_function{
    &InitialNowFunction};
int64_t g_qpc_ticks_per_second = 0;

TimeDelta QPCValueToTimeDelta(LONGLONG qpc_value) {
  // Ensure that the assignment to |g_qpc_ticks_per_second|, made in
  // InitializeNowFunctionPointer(), has happened by this point.
  std::atomic_thread_fence(std::memory_order_acquire);

  DCHECK_GT(g_qpc_ticks_per_second, 0);

  // If the QPC Value is below the overflow threshold, we proceed with
  // simple multiply and divide.
  if (qpc_value < Time::kQPCOverflowThreshold) {
    return Microseconds(qpc_value * Time::kMicrosecondsPerSecond /
                        g_qpc_ticks_per_second);
  }
  // Otherwise, calculate microseconds in a round about manner to avoid
  // overflow and precision issues.
  int64_t whole_seconds = qpc_value / g_qpc_ticks_per_second;
  int64_t leftover_ticks = qpc_value - (whole_seconds * g_qpc_ticks_per_second);
  return Microseconds((whole_seconds * Time::kMicrosecondsPerSecond) +
                      ((leftover_ticks * Time::kMicrosecondsPerSecond) /
                       g_qpc_ticks_per_second));
}

TimeTicks QPCNow() {
  return TimeTicks() + QPCValueToTimeDelta(QPCNowRaw());
}

void InitializeNowFunctionPointer() {
  LARGE_INTEGER ticks_per_sec = {};
  if (!QueryPerformanceFrequency(&ticks_per_sec))
    ticks_per_sec.QuadPart = 0;

  // If Windows cannot provide a QPC implementation, TimeTicks::Now() must use
  // the low-resolution clock.
  //
  // If the QPC implementation is expensive and/or unreliable, TimeTicks::Now()
  // will still use the low-resolution clock. A CPU lacking a non-stop time
  // counter will cause Windows to provide an alternate QPC implementation that
  // works, but is expensive to use.
  //
  // Otherwise, Now uses the high-resolution QPC clock. As of 9 September 2024,
  // ~97% of users fall within this category.
  CPU cpu;
  const TimeTicksNowFunction now_function =
      (ticks_per_sec.QuadPart <= 0 || !cpu.has_non_stop_time_stamp_counter())
          ? &RolloverProtectedNow
          : &QPCNow;

  // Threading note 1: In an unlikely race condition, it's possible for two or
  // more threads to enter InitializeNowFunctionPointer() in parallel. This is
  // not a problem since all threads end up writing out the same values
  // to the global variables, and those variable being atomic are safe to read
  // from other threads.
  //
  // Threading note 2: A release fence is placed here to ensure, from the
  // perspective of other threads using the function pointers, that the
  // assignment to |g_qpc_ticks_per_second| happens before the function pointers
  // are changed.
  g_qpc_ticks_per_second = ticks_per_sec.QuadPart;
  std::atomic_thread_fence(std::memory_order_release);
  // Also set g_time_ticks_now_function to avoid the additional indirection via
  // TimeTicksNowIgnoringOverride() for future calls to TimeTicks::Now(), only
  // if it wasn't already overridden to a different value. memory_order_relaxed
  // is sufficient since an explicit fence was inserted above.
  base::TimeTicksNowFunction initial_time_ticks_now_function =
      &subtle::TimeTicksNowIgnoringOverride;
  internal::g_time_ticks_now_function.compare_exchange_strong(
      initial_time_ticks_now_function, now_function, std::memory_order_relaxed);
  g_time_ticks_now_ignoring_override_function.store(now_function,
                                                    std::memory_order_relaxed);
}

TimeTicks InitialNowFunction() {
  InitializeNowFunctionPointer();
  return g_time_ticks_now_ignoring_override_function.load(
      std::memory_order_relaxed)();
}

}  // namespace

// static
TimeTicks::TickFunctionType TimeTicks::SetMockTickFunction(
    TickFunctionType ticker) {
  TickFunctionType old = g_tick_function;
  g_tick_function = ticker;
  g_last_time_and_rollovers.store(0, std::memory_order_relaxed);
  return old;
}

namespace subtle {
TimeTicks TimeTicksNowIgnoringOverride() {
  return g_time_ticks_now_ignoring_override_function.load(
      std::memory_order_relaxed)();
}
}  // namespace subtle

// static
bool TimeTicks::IsHighResolution() {
  if (g_time_ticks_now_ignoring_override_function == &InitialNowFunction)
    InitializeNowFunctionPointer();
  return g_time_ticks_now_ignoring_override_function == &QPCNow;
}

// static
bool TimeTicks::IsConsistentAcrossProcesses() {
  // According to Windows documentation [1] QPC is consistent post-Windows
  // Vista. So if we are using QPC then we are consistent which is the same as
  // being high resolution.
  //
  // [1] https://msdn.microsoft.com/en-us/library/windows/desktop/dn553408(v=vs.85).aspx
  //
  // "In general, the performance counter results are consistent across all
  // processors in multi-core and multi-processor systems, even when measured on
  // different threads or processes. Here are some exceptions to this rule:
  // - Pre-Windows Vista operating systems that run on certain processors might
  // violate this consistency because of one of these reasons:
  //     1. The hardware processors have a non-invariant TSC and the BIOS
  //     doesn't indicate this condition correctly.
  //     2. The TSC synchronization algorithm that was used wasn't suitable for
  //     systems with large numbers of processors."
  return IsHighResolution();
}

// static
TimeTicks::Clock TimeTicks::GetClock() {
  return IsHighResolution() ? Clock::WIN_QPC
                            : Clock::WIN_ROLLOVER_PROTECTED_TIME_GET_TIME;
}

// LiveTicks ------------------------------------------------------------------

namespace subtle {
LiveTicks LiveTicksNowIgnoringOverride() {
  ULONGLONG unbiased_interrupt_time;
  QueryUnbiasedInterruptTimePrecise(&unbiased_interrupt_time);
  // QueryUnbiasedInterruptTimePrecise gets the interrupt time in system time
  // units of 100 nanoseconds.
  return LiveTicks() + Nanoseconds(unbiased_interrupt_time * 100);
}
}  // namespace subtle

// ThreadTicks ----------------------------------------------------------------

namespace subtle {
ThreadTicks ThreadTicksNowIgnoringOverride() {
  return ThreadTicks::GetForThread(PlatformThread::CurrentHandle());
}
}  // namespace subtle

// static
ThreadTicks ThreadTicks::GetForThread(
    const PlatformThreadHandle& thread_handle) {
  DCHECK(IsSupported());

#if defined(ARCH_CPU_ARM64)
  // QueryThreadCycleTime versus TSCTicksPerSecond doesn't have much relation to
  // actual elapsed time on Windows on Arm, because QueryThreadCycleTime is
  // backed by the actual number of CPU cycles executed, rather than a
  // constant-rate timer like Intel. To work around this, use GetThreadTimes
  // (which isn't as accurate but is meaningful as a measure of elapsed
  // per-thread time).
  FILETIME creation_time, exit_time, kernel_time, user_time;
  ::GetThreadTimes(thread_handle.platform_handle(), &creation_time, &exit_time,
                   &kernel_time, &user_time);

  const int64_t us = FileTimeToMicroseconds(user_time);
#else
  // Get the number of TSC ticks used by the current thread.
  ULONG64 thread_cycle_time = 0;
  ::QueryThreadCycleTime(thread_handle.platform_handle(), &thread_cycle_time);

  // Get the frequency of the TSC.
  const double tsc_ticks_per_second = time_internal::TSCTicksPerSecond();
  if (tsc_ticks_per_second == 0)
    return ThreadTicks();

  // Return the CPU time of the current thread.
  const double thread_time_seconds = thread_cycle_time / tsc_ticks_per_second;
  const int64_t us =
      static_cast<int64_t>(thread_time_seconds * Time::kMicrosecondsPerSecond);
#endif

  return ThreadTicks(us);
}

// static
bool ThreadTicks::IsSupportedWin() {
#if defined(ARCH_CPU_ARM64)
  // The Arm implementation does not use QueryThreadCycleTime and therefore does
  // not care about the time stamp counter.
  return true;
#else
  return time_internal::HasConstantRateTSC();
#endif
}

// static
void ThreadTicks::WaitUntilInitializedWin() {
#if !defined(ARCH_CPU_ARM64)
  while (time_internal::TSCTicksPerSecond() == 0)
    ::Sleep(10);
#endif
}

// static
TimeTicks TimeTicks::FromQPCValue(LONGLONG qpc_value) {
  return TimeTicks() + QPCValueToTimeDelta(qpc_value);
}

// TimeDelta ------------------------------------------------------------------

// static
TimeDelta TimeDelta::FromQPCValue(LONGLONG qpc_value) {
  return QPCValueToTimeDelta(qpc_value);
}

// static
TimeDelta TimeDelta::FromFileTime(FILETIME ft) {
  return Microseconds(FileTimeToMicroseconds(ft));
}

// static
TimeDelta TimeDelta::FromWinrtDateTime(ABI::Windows::Foundation::DateTime dt) {
  // UniversalTime is 100 ns intervals since January 1, 1601 (UTC)
  return Microseconds(dt.UniversalTime / 10);
}

ABI::Windows::Foundation::DateTime TimeDelta::ToWinrtDateTime() const {
  ABI::Windows::Foundation::DateTime date_time;
  date_time.UniversalTime = InMicroseconds() * 10;
  return date_time;
}

// static
TimeDelta TimeDelta::FromWinrtTimeSpan(ABI::Windows::Foundation::TimeSpan ts) {
  // Duration is 100 ns intervals
  return Microseconds(ts.Duration / 10);
}

ABI::Windows::Foundation::TimeSpan TimeDelta::ToWinrtTimeSpan() const {
  ABI::Windows::Foundation::TimeSpan time_span;
  time_span.Duration = InMicroseconds() * 10;
  return time_span;
}

#if !defined(ARCH_CPU_ARM64)
namespace time_internal {

bool HasConstantRateTSC() {
  static bool is_supported = CPU().has_non_stop_time_stamp_counter();
  return is_supported;
}

double TSCTicksPerSecond() {
  DCHECK(HasConstantRateTSC());
  // The value returned by QueryPerformanceFrequency() cannot be used as the TSC
  // frequency, because there is no guarantee that the TSC frequency is equal to
  // the performance counter frequency.
  // The TSC frequency is cached in a static variable because it takes some time
  // to compute it.
  static double tsc_ticks_per_second = 0;
  if (tsc_ticks_per_second != 0)
    return tsc_ticks_per_second;

  // Increase the thread priority to reduces the chances of having a context
  // switch during a reading of the TSC and the performance counter.
  const int previous_priority = ::GetThreadPriority(::GetCurrentThread());
  ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

  // The first time that this function is called, make an initial reading of the
  // TSC and the performance counter.

  static const uint64_t tsc_initial = __rdtsc();
  static const int64_t perf_counter_initial = QPCNowRaw();

  // Make a another reading of the TSC and the performance counter every time
  // that this function is called.
  const uint64_t tsc_now = __rdtsc();
  const int64_t perf_counter_now = QPCNowRaw();

  // Reset the thread priority.
  ::SetThreadPriority(::GetCurrentThread(), previous_priority);

  // Make sure that at least 50 ms elapsed between the 2 readings. The first
  // time that this function is called, we don't expect this to be the case.
  // Note: The longer the elapsed time between the 2 readings is, the more
  //   accurate the computed TSC frequency will be. The 50 ms value was
  //   chosen because local benchmarks show that it allows us to get a
  //   stddev of less than 1 tick/us between multiple runs.
  // Note: According to the MSDN documentation for QueryPerformanceFrequency(),
  //   this will never fail on systems that run XP or later.
  //   https://msdn.microsoft.com/library/windows/desktop/ms644905.aspx
  LARGE_INTEGER perf_counter_frequency = {};
  ::QueryPerformanceFrequency(&perf_counter_frequency);
  DCHECK_GE(perf_counter_now, perf_counter_initial);
  const int64_t perf_counter_ticks = perf_counter_now - perf_counter_initial;
  const double elapsed_time_seconds =
      perf_counter_ticks / static_cast<double>(perf_counter_frequency.QuadPart);

  constexpr double kMinimumEvaluationPeriodSeconds = 0.05;
  if (elapsed_time_seconds < kMinimumEvaluationPeriodSeconds)
    return 0;

  // Compute the frequency of the TSC.
  DCHECK_GE(tsc_now, tsc_initial);
  const uint64_t tsc_ticks = tsc_now - tsc_initial;
  tsc_ticks_per_second = tsc_ticks / elapsed_time_seconds;

  return tsc_ticks_per_second;
}

}  // namespace time_internal
#endif  // defined(ARCH_CPU_ARM64)

}  // namespace base
