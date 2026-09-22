// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_HISTOGRAM_MACROS_INTERNAL_H_
#define BASE_METRICS_HISTOGRAM_MACROS_INTERNAL_H_

#include <stdint.h>

#include <atomic>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

#include "base/dcheck_is_on.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/sparse_histogram.h"
#include "base/no_destructor.h"
#include "base/time/time.h"

// This is for macros and helpers internal to base/metrics. They should not be
// used outside of this directory. For writing to UMA histograms, see
// histogram_macros.h.

namespace base::internal {

// Helper trait for deducing the boundary value for enums.
template <typename Enum>
struct EnumSizeTraits {
  static constexpr uintmax_t Count() {
    if constexpr (base::metrics_internal::HasKMaxValue<Enum>) {
      // Since the UMA histogram macros expect a value one larger than the max
      // defined enumerator value, add one.
      return static_cast<uintmax_t>(std::to_underlying(Enum::kMaxValue)) + 1;
    } else if constexpr (std::is_enum_v<Enum>) {
      static_assert(
          sizeof(Enum) == 0,
          "`Enum` must define a `kMaxValue` enumerator to use this macro");
    }
    // Return a placeholder size here to avoid cascading errors in the caller.
    return 1;
  }
};

}  // namespace base::internal

// TODO(rkaplow): Improve commenting of these methods.
//------------------------------------------------------------------------------
// Histograms are often put in areas where they are called many many times, and
// performance is critical.  As a result, they are designed to have a very low
// recurring cost of executing (adding additional samples). Toward that end,
// the macros declare a static pointer to the histogram in question, and only
// take a "slow path" to construct (or find) the histogram on the first run
// through the macro. We leak the histograms at shutdown time so that we don't
// have to validate using the pointers at any time during the running of the
// process.

// In some cases (integration into 3rd party code), it's useful to separate the
// definition of `atomic_histogram_pointer` from its use. To achieve this we
// define HISTOGRAM_POINTER_USE, which uses an `atomic_histogram_pointer`, and
// STATIC_HISTOGRAM_POINTER_BLOCK, which defines an `atomic_histogram_pointer`
// and forwards to HISTOGRAM_POINTER_USE.
#define HISTOGRAM_POINTER_USE(                                           \
    atomic_histogram_pointer, constant_histogram_name,                   \
    histogram_add_method_invocation, histogram_factory_get_invocation)   \
  do {                                                                   \
    base::HistogramBase* histogram_pointer(                              \
        reinterpret_cast<base::HistogramBase*>(                          \
            atomic_histogram_pointer->load(std::memory_order_acquire))); \
    if (!histogram_pointer) {                                            \
      /*                                                                 \
       * This is the slow path, which will construct OR find the         \
       * matching histogram. `histogram_factory_get_invocation` includes \
       * locks on a global histogram name map and is completely thread   \
       * safe.                                                           \
       */                                                                \
      histogram_pointer = histogram_factory_get_invocation;              \
                                                                         \
      /*                                                                 \
       * We could do this without any barrier, since FactoryGet()        \
       * entered and exited a lock after construction, but this barrier  \
       * makes things clear.                                             \
       */                                                                \
      atomic_histogram_pointer->store(                                   \
          reinterpret_cast<uintptr_t>(histogram_pointer),                \
          std::memory_order_release);                                    \
    }                                                                    \
    if (DCHECK_IS_ON())                                                  \
      histogram_pointer->CheckName(constant_histogram_name);             \
    histogram_pointer->histogram_add_method_invocation;                  \
  } while (0)

// This is a helper macro used by other macros and shouldn't be used directly.
// Defines the static `atomic_histogram_pointer` and forwards to
// HISTOGRAM_POINTER_USE.
#define STATIC_HISTOGRAM_POINTER_BLOCK(constant_histogram_name,               \
                                       histogram_add_method_invocation,       \
                                       histogram_factory_get_invocation)      \
  do {                                                                        \
    /*                                                                        \
     * The pointer's presence indicates that the initialization is complete.  \
     * Initialization is idempotent, so it can safely be atomically repeated. \
     */                                                                       \
    static std::atomic_uintptr_t atomic_histogram_pointer;                    \
    HISTOGRAM_POINTER_USE(                                                    \
        std::addressof(atomic_histogram_pointer), constant_histogram_name,    \
        histogram_add_method_invocation, histogram_factory_get_invocation);   \
  } while (0)

// This is a helper macro used by other macros and shouldn't be used directly.
#define INTERNAL_HISTOGRAM_CUSTOM_COUNTS_WITH_FLAG(name, sample, min, max, \
                                                   bucket_count, flag)     \
  STATIC_HISTOGRAM_POINTER_BLOCK(                                          \
      name, Add(sample),                                                   \
      base::Histogram::FactoryGet(name, min, max, bucket_count, flag))

// This is a helper macro used by other macros and shouldn't be used directly.
// The bucketing scheme is linear with a bucket size of 1. For N items,
// recording values in the range [0, N - 1] creates a linear histogram with N +
// 1 buckets:
//   [0, 1), [1, 2), ..., [N - 1, N)
// and an overflow bucket [N, infinity).
//
// Code should never emit to the overflow bucket; only to the other N buckets.
// This allows future versions of Chrome to safely increase the boundary size.
// Otherwise, the histogram would have [N - 1, infinity) as its overflow bucket,
// and so the maximal value (N - 1) would be emitted to this overflow bucket.
// But, if an additional value were later added, the bucket label for
// the value (N - 1) would change to [N - 1, N), which would result in different
// versions of Chrome using different bucket labels for identical data.
#define INTERNAL_HISTOGRAM_EXACT_LINEAR_WITH_FLAG(name, sample, boundary,  \
                                                  flag)                    \
  do {                                                                     \
    static_assert(!std::is_enum_v<std::decay_t<decltype(sample)>>,         \
                  "|sample| should not be an enum type!");                 \
    static_assert(!std::is_enum_v<std::decay_t<decltype(boundary)>>,       \
                  "|boundary| should not be an enum type!");               \
    STATIC_HISTOGRAM_POINTER_BLOCK(                                        \
        name, Add(sample),                                                 \
        base::LinearHistogram::FactoryGet(name, 1, boundary, boundary + 1, \
                                          flag));                          \
  } while (0)

// While this behaves the same as the above macro, the wrapping of a linear
// histogram with another object to do the scaling means the POINTER_BLOCK
// macro can't be used as it is tied to HistogramBase
#define INTERNAL_HISTOGRAM_SCALED_EXACT_LINEAR_WITH_FLAG(                      \
    name, sample, count, boundary, scale, flag)                                \
  do {                                                                         \
    static_assert(!std::is_enum_v<std::decay_t<decltype(sample)>>,             \
                  "|sample| should not be an enum type!");                     \
    static_assert(!std::is_enum_v<std::decay_t<decltype(boundary)>>,           \
                  "|boundary| should not be an enum type!");                   \
    class ScaledLinearHistogramInstance : public base::ScaledLinearHistogram { \
     public:                                                                   \
      ScaledLinearHistogramInstance()                                          \
          : ScaledLinearHistogram(name,                                        \
                                  1,                                           \
                                  boundary,                                    \
                                  boundary + 1,                                \
                                  scale,                                       \
                                  flag) {}                                     \
    };                                                                         \
    static base::NoDestructor<ScaledLinearHistogramInstance> scaled_leaky;     \
    scaled_leaky->AddScaledCount(sample, count);                               \
  } while (0)

// Helper for 'overloading' UMA_HISTOGRAM_ENUMERATION with a variable number of
// arguments.
#define INTERNAL_UMA_HISTOGRAM_ENUMERATION_GET_MACRO(_1, _2, NAME, ...) NAME

#define INTERNAL_UMA_HISTOGRAM_ENUMERATION_DEDUCE_BOUNDARY(name, sample,   \
                                                           flags)          \
  do {                                                                     \
    using InternalHistogramDecayedSample = std::decay_t<decltype(sample)>; \
    static_assert(std::is_enum_v<InternalHistogramDecayedSample>,          \
                  "`sample` is not an enum.");                             \
    INTERNAL_HISTOGRAM_ENUMERATION_WITH_FLAG(                              \
        name, sample,                                                      \
        base::internal::EnumSizeTraits<                                    \
            InternalHistogramDecayedSample>::Count(),                      \
        flags);                                                            \
  } while (0)

// Note: The value in `sample` must be strictly less than `enum_size`.
//
// TODO(dcheng): The assertions below only require that `sample` and `enum_size`
// are consistent; integral arguments are still accepted for legacy callers.
// Both should eventually be required to be enums.
#define INTERNAL_UMA_HISTOGRAM_ENUMERATION_SPECIFY_BOUNDARY(name, sample,     \
                                                            enum_size, flags) \
  do {                                                                        \
    using InternalHistogramDecayedSample = std::decay_t<decltype(sample)>;    \
    using InternalHistogramDecayedBoundary =                                  \
        std::decay_t<decltype(enum_size)>;                                    \
    static_assert(                                                            \
        !base::metrics_internal::HasKMaxValue<                                \
            InternalHistogramDecayedSample> &&                                \
            !base::metrics_internal::HasKMaxValue<                            \
                InternalHistogramDecayedBoundary>,                            \
        "Do not explicitly specify the boundary value for an enum that "      \
        "defines `kMaxValue`; omit the boundary argument so it is deduced "   \
        "from `kMaxValue`.");                                                 \
    static_assert(!std::is_enum_v<InternalHistogramDecayedBoundary> ||        \
                      std::is_enum_v<InternalHistogramDecayedSample>,         \
                  "`boundary` is enum, but `sample` is not.");                \
    static_assert(!std::is_enum_v<InternalHistogramDecayedSample> ||          \
                      !std::is_enum_v<InternalHistogramDecayedBoundary> ||    \
                      std::is_same_v<InternalHistogramDecayedSample,          \
                                     InternalHistogramDecayedBoundary>,       \
                  "`sample` and `boundary` shouldn't be of different enums"); \
    INTERNAL_HISTOGRAM_ENUMERATION_WITH_FLAG(name, sample, enum_size, flags); \
  } while (0)

// Similar to `INTERNAL_HISTOGRAM_EXACT_LINEAR_WITH_FLAG`, but intended for
// enumerations. This delegates to `INTERNAL_HISTOGRAM_EXACT_LINEAR_WITH_FLAG`,
// but supports scoped enumerations as well by forcing an explicit cast to the
// `HistogramBase::Sample32` integral type.
//
// Note that the declared enum size is checked for two separate issues:
// - that it is > 0, i.e. the histogram has at least one valid bucket
// - that it isn't out of range of `HistogramBase::Sample32`
#define INTERNAL_HISTOGRAM_ENUMERATION_WITH_FLAG(name, sample, boundary, flag) \
  do {                                                                         \
    constexpr uintmax_t kInternalHistogramBoundary =                           \
        static_cast<uintmax_t>(boundary);                                      \
    static_assert(                                                             \
        kInternalHistogramBoundary > 0,                                        \
        "`boundary` must be greater than 0: an enumerated histogram "          \
        "needs at least one valid bucket");                                    \
    static_assert(                                                             \
        kInternalHistogramBoundary <                                           \
            std::numeric_limits<base::HistogramBase::Sample32>::max(),         \
        "`boundary` is out of range of HistogramBase::Sample32");              \
    INTERNAL_HISTOGRAM_EXACT_LINEAR_WITH_FLAG(                                 \
        name, static_cast<base::HistogramBase::Sample32>(sample),              \
        static_cast<base::HistogramBase::Sample32>(                            \
            kInternalHistogramBoundary),                                       \
        flag);                                                                 \
  } while (0)

#define INTERNAL_HISTOGRAM_SCALED_ENUMERATION_WITH_FLAG(name, sample, count, \
                                                        scale, flag)         \
  do {                                                                       \
    using InternalHistogramDecayedSample = std::decay_t<decltype(sample)>;   \
    static_assert(std::is_enum_v<InternalHistogramDecayedSample>,            \
                  "`sample` is not an enum.");                               \
    constexpr auto kInternalHistogramBoundary =                              \
        base::internal::EnumSizeTraits<                                      \
            InternalHistogramDecayedSample>::Count();                        \
    static_assert(                                                           \
        kInternalHistogramBoundary > 0,                                      \
        "`boundary` must be greater than 0: an enumerated histogram "        \
        "needs at least one valid bucket");                                  \
    static_assert(                                                           \
        kInternalHistogramBoundary <                                         \
            std::numeric_limits<base::HistogramBase::Sample32>::max(),       \
        "`boundary` is out of range of HistogramBase::Sample32");            \
    INTERNAL_HISTOGRAM_SCALED_EXACT_LINEAR_WITH_FLAG(                        \
        name, static_cast<base::HistogramBase::Sample32>(sample), count,     \
        static_cast<base::HistogramBase::Sample32>(                          \
            kInternalHistogramBoundary),                                     \
        scale, flag);                                                        \
  } while (0)

// This is a helper macro used by other macros and shouldn't be used directly.
// This is necessary to expand __COUNTER__ to an actual value.
#define INTERNAL_SCOPED_UMA_HISTOGRAM_TIMER_EXPANDER(name, timing, key) \
  INTERNAL_SCOPED_UMA_HISTOGRAM_TIMER_UNIQUE(name, timing, key)

// This is a helper macro used by other macros and shouldn't be used directly.
#define INTERNAL_SCOPED_UMA_HISTOGRAM_TIMER_UNIQUE(name, timing, key)  \
  INTERNAL_SCOPED_UMA_HISTOGRAM_TIMER_UNIQUE_DEFINE(name, timing, key) \
  scoped_histogram_timer_##key

// This is a helper macro used by other macros and shouldn't be used directly.
#define INTERNAL_SCOPED_UMA_HISTOGRAM_TIMER_UNIQUE_DEFINE(name, timing, key) \
  class ScopedHistogramTimer##key {                                          \
   public:                                                                   \
    ScopedHistogramTimer##key() : constructed_(base::TimeTicks::Now()) {}    \
    ~ScopedHistogramTimer##key() {                                           \
      base::TimeDelta elapsed = base::TimeTicks::Now() - constructed_;       \
      switch (timing) {                                                      \
        case ScopedHistogramTiming::kMicrosecondTimes:                       \
          UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(                           \
              name, elapsed, base::Microseconds(1), base::Seconds(1), 50);   \
          break;                                                             \
        case ScopedHistogramTiming::kMediumTimes:                            \
          UMA_HISTOGRAM_TIMES(name, elapsed);                                \
          break;                                                             \
        case ScopedHistogramTiming::kLongTimes:                              \
          UMA_HISTOGRAM_LONG_TIMES_100(name, elapsed);                       \
          break;                                                             \
      }                                                                      \
    }                                                                        \
                                                                             \
   private:                                                                  \
    base::TimeTicks constructed_;                                            \
  }

// This is a helper macro used by other macros and shouldn't be used directly.
// This is necessary to expand __COUNTER__ to an actual value.
#define INTERNAL_SCOPED_UMA_HISTOGRAM_TIMER_SUBSAMPLED_EXPANDER(             \
    name, should_sample, timing, key)                                        \
  INTERNAL_SCOPED_UMA_HISTOGRAM_TIMER_SUBSAMPLED_UNIQUE(name, should_sample, \
                                                        timing, key)

// This is a helper macro used by other macros and shouldn't be used directly.
#define INTERNAL_SCOPED_UMA_HISTOGRAM_TIMER_SUBSAMPLED_UNIQUE(           \
    name, should_sample, timing, key)                                    \
  INTERNAL_SCOPED_UMA_HISTOGRAM_TIMER_UNIQUE_DEFINE(name, timing, key);  \
  std::optional<ScopedHistogramTimer##key> scoped_histogram_timer_##key; \
  if (should_sample) {                                                   \
    scoped_histogram_timer_##key.emplace();                              \
  }

#endif  // BASE_METRICS_HISTOGRAM_MACROS_INTERNAL_H_
