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
