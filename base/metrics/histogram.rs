// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! **Rust API for recording UMA Histograms.**
//!
//! For readability's sake, callers should qualify functions by module name
//! rather than importing them directly, e.g.
//!
//! ```
//! chromium::import! {
//!     "//base:histogram";
//! }
//!
//! // It's clear we're recording a bool for a histogram, specifically.
//! histogram::record_bool("My.Boolean", true);
//! ```

#[cxx::bridge(namespace = "base::rust")]
mod ffi {
    unsafe extern "C++" {
        include!("base/metrics/histogram_rust_shim.h");

        /// Records the boolean value `sample` for boolean histogram `name`.
        fn record_bool(name: &str, sample: bool);

        /// Records the integer value `sample` (up to `exclusive_max`) for
        /// linear histogram `name`.
        fn record_exact_linear(name: &str, sample: i32, exclusive_max: i32);

        /// Records the value `percent` (which denotes a percentage) to
        /// histogram `name`. `percent` should be a value between 1 and
        /// 100, inclusive; if `percent` is outside that range it is
        /// "clamped" to the nearest valid value.
        fn record_percentage(name: &str, percent: i32);

        /// Records the integer `sample` to sparse histogram `name`.
        fn record_sparse(name: &str, sample: i32);

        /// Records the integer `sample` to count histogram `name` using custom
        /// bucket parameters. `min` should be 1 or greater.
        fn record_custom_counts(
            name: &str,
            sample: i32,
            min: i32,
            exclusive_max: i32,
            buckets: usize,
        );

        /// Records the integer `sample` to count histogram `name` with a
        /// maximum value of 100 (50 buckets).
        fn record_counts_100(name: &str, sample: i32);

        /// Records the integer `sample` to count histogram `name` with a
        /// maximum value of 1,000 (50 buckets).
        fn record_counts_1000(name: &str, sample: i32);

        /// Records the integer `sample` to count histogram `name` with a
        /// maximum value of 10,000 (50 buckets).
        fn record_counts_10000(name: &str, sample: i32);

        /// Records the integer `sample` to count histogram `name` with a
        /// maximum value of 100,000 (50 buckets).
        fn record_counts_100000(name: &str, sample: i32);

        /// Records the integer `sample` to count histogram `name` with a
        /// maximum value of 1,000,000 (50 buckets).
        fn record_counts_1m(name: &str, sample: i32);

        /// Records the integer `sample` to count histogram `name` with a
        /// maximum value of 10,000,000 (50 buckets).
        fn record_counts_10m(name: &str, sample: i32);

        /// Records the memory size `sample_kb` (in kilobytes) to memory
        /// histogram `name`. The valid range is 1,000kb to 500mb (50
        /// buckets).
        fn record_memory_kb(name: &str, sample_kb: i32);

        /// Records the memory size `sample_mb` (in megabytes) to memory
        /// histogram `name`. The valid range is 1mb to 1,000mb (50 buckets).
        fn record_memory_mb(name: &str, sample_mb: i32);

        /// Records the memory size `sample_mb` (in megabytes) to memory
        /// histogram `name`. The valid range is 1mb to 64,000MB (100 buckets).
        fn record_memory_large_mb(name: &str, sample_mb: i32);

        fn record_custom_times(
            name: &str,
            sample_us: i64,
            min_us: i64,
            max_us: i64,
            buckets: usize,
        );
        fn record_short_times(name: &str, sample_us: i64);
        fn record_medium_times(name: &str, sample_us: i64);
        fn record_long_times(name: &str, sample_us: i64);
        fn record_long_times_100(name: &str, sample_us: i64);
        fn record_custom_microseconds_times(
            name: &str,
            sample_us: i64,
            min_us: i64,
            max_us: i64,
            buckets: usize,
        );
        fn record_microseconds_times(name: &str, sample_us: i64);
        fn time_ticks_now_microseconds() -> i64;
    }
}

pub use ffi::{
    record_bool, record_counts_100, record_counts_1000, record_counts_10000, record_counts_100000,
    record_counts_10m, record_counts_1m, record_custom_counts, record_exact_linear,
    record_memory_kb, record_memory_large_mb, record_memory_mb, record_percentage, record_sparse,
};

chromium::import! {
    "//base:histogram_enum_macro";
}

pub use histogram_enum_macro::UmaEnum;

/// Trait for an enum that represents the categories for an enumerated
/// histogram.
///
/// Use `#[derive(UmaEnum)]` to automatically implement this for
/// a fieldless enum.
///
/// # Example
/// ```
/// chromium::import! {
///     "//base:histogram";
/// }
/// use histogram::UmaEnum;
///
/// #[derive(Copy, Clone, UmaEnum)]
/// #[repr(i32)]
/// enum MyEnum {
///     First = 0,
///     Second = 1,
/// }
///
/// histogram::record_enum("My.Enumeration", MyEnum::Second);
/// ```
pub trait UmaEnum: Copy {
    /// The maximum valid value of the enum (corresponds to `kMaxValue` in C++).
    const MAX_VALUE: i32;

    /// Converts the enum variant to its `i32` sample representation.
    fn to_sample(self) -> i32;
}

/// Records the enum `sample` to enumerated histogram `name`.
///
/// (`sample` must be an enum type implementing [`UmaEnum`]. Use
/// `#[derive(UmaEnum)]` to implement this for your enum.)
pub fn record_enum<T: UmaEnum>(name: &str, sample: T) {
    let sample_val = sample.to_sample();
    debug_assert!(sample_val >= 0, "Histogram sample {} must be non-negative", sample_val);
    debug_assert!(
        sample_val <= T::MAX_VALUE,
        "Histogram sample {} exceeds MAX_VALUE {}",
        sample_val,
        T::MAX_VALUE
    );
    // Enumerated histograms are exact linear histograms "under the hood".
    ffi::record_exact_linear(name, sample_val, T::MAX_VALUE + 1);
}

fn duration_to_us(duration: core::time::Duration) -> i64 {
    i64::try_from(duration.as_micros()).unwrap_or(i64::MAX)
}

/// Records the elapsed time `sample` to time histogram `name` using custom
/// bucket parameters (millisecond resolution).
pub fn record_custom_times(
    name: &str,
    sample: core::time::Duration,
    min: core::time::Duration,
    max: core::time::Duration,
    buckets: usize,
) {
    ffi::record_custom_times(
        name,
        duration_to_us(sample),
        duration_to_us(min),
        duration_to_us(max),
        buckets,
    );
}

/// Records the elapsed time `sample` to time histogram `name` for short
/// timings up to 10 seconds (50 buckets, millisecond resolution).
pub fn record_short_times(name: &str, sample: core::time::Duration) {
    ffi::record_short_times(name, duration_to_us(sample));
}

/// Records the elapsed time `sample` to time histogram `name` for medium
/// timings up to 3 minutes (50 buckets, millisecond resolution).
pub fn record_medium_times(name: &str, sample: core::time::Duration) {
    ffi::record_medium_times(name, duration_to_us(sample));
}

/// Records the elapsed time `sample` to time histogram `name` for long
/// timings up to 1 hour (50 buckets, millisecond resolution).
pub fn record_long_times(name: &str, sample: core::time::Duration) {
    ffi::record_long_times(name, duration_to_us(sample));
}

/// Records the elapsed time `sample` to time histogram `name` for long
/// timings up to 1 hour (100 buckets, millisecond resolution).
pub fn record_long_times_100(name: &str, sample: core::time::Duration) {
    ffi::record_long_times_100(name, duration_to_us(sample));
}

/// Records the elapsed time `sample` to time histogram `name` using custom
/// bucket parameters (microsecond resolution).
pub fn record_custom_microseconds_times(
    name: &str,
    sample: core::time::Duration,
    min: core::time::Duration,
    max: core::time::Duration,
    buckets: usize,
) {
    ffi::record_custom_microseconds_times(
        name,
        duration_to_us(sample),
        duration_to_us(min),
        duration_to_us(max),
        buckets,
    );
}

/// Records the elapsed time `sample` to high-resolution time histogram `name`
/// from 1 microsecond up to 10 seconds (50 buckets, microsecond resolution).
pub fn record_microseconds_times(name: &str, sample: core::time::Duration) {
    ffi::record_microseconds_times(name, duration_to_us(sample));
}

/// Scoped RAII timer that records the elapsed time of a scope to a
/// histogram upon being dropped.
///
/// Defaults to using [`record_short_times`] (1ms – 10s, 50 buckets), but a
/// custom reporting function or closure can be supplied via
/// [`ScopedHistogramTimer::new_with_reporter`].
///
/// For any given timer, values recorded *outside* the given range get clamped
/// to the underflow/overflow buckets respectively.
///
/// # Examples
/// ```
/// chromium::import! {
///     "//base:histogram";
/// }
///
/// fn do_work() {
///     let _timer = histogram::ScopedHistogramTimer::new("My.FunctionTime");
///     // ... do stuff ...
/// } // Elapsed time is automatically recorded with `record_short_times` when `_timer` drops.
///
/// fn do_microsecond_work() {
///     let _timer = histogram::ScopedHistogramTimer::new_with_reporter(
///         "My.MicrosecondTime",
///         histogram::record_microseconds_times,
///     );
///     // ... do stuff ...
/// }
/// ```
#[must_use = "ScopedHistogramTimer records time when dropped; assign it to a variable or it will drop immediately"]
pub struct ScopedHistogramTimer<'a, F = fn(&str, core::time::Duration)>
where
    F: FnOnce(&str, core::time::Duration),
{
    name: &'a str,
    reporter: Option<F>,
    start_ticks_microseconds: i64,
}

impl<'a> ScopedHistogramTimer<'a, fn(&str, core::time::Duration)> {
    /// Constructs a scoped timer that records elapsed time using
    /// [`record_short_times`].
    pub fn new(name: &'a str) -> Self {
        Self::new_with_reporter(name, record_short_times)
    }
}

impl<'a, F> ScopedHistogramTimer<'a, F>
where
    F: FnOnce(&str, core::time::Duration),
{
    /// Constructs a scoped timer with a custom recording function or closure.
    pub fn new_with_reporter(name: &'a str, reporter: F) -> Self {
        debug_assert!(!name.is_empty(), "Histogram name must not be empty");
        Self {
            name,
            reporter: Some(reporter),
            start_ticks_microseconds: ffi::time_ticks_now_microseconds(),
        }
    }

    /// Returns the elapsed duration since the timer was created.
    pub fn elapsed(&self) -> core::time::Duration {
        let elapsed_microseconds =
            ffi::time_ticks_now_microseconds().saturating_sub(self.start_ticks_microseconds);
        core::time::Duration::from_micros(elapsed_microseconds as u64)
    }

    /// Cancels the timer so that no sample is recorded when it is dropped.
    ///
    /// (This is equivalent to calling mem::forget; doing so is both safe and
    /// will not result in memory leaks.)
    pub fn cancel(mut self) {
        self.reporter = None;
    }
}

impl<'a, F> Drop for ScopedHistogramTimer<'a, F>
where
    F: FnOnce(&str, core::time::Duration),
{
    fn drop(&mut self) {
        if let Some(reporter) = self.reporter.take() {
            let elapsed_microseconds =
                ffi::time_ticks_now_microseconds().saturating_sub(self.start_ticks_microseconds);
            let duration = core::time::Duration::from_micros(elapsed_microseconds as u64);
            reporter(self.name, duration);
        }
    }
}
