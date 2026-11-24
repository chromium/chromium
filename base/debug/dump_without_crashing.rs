// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! **Rust API for `base::debug::DumpWithoutCrashing()`.**
//!
//! ```
//! chromium::import! {
//!     "//base:dump_without_crashing";
//! }
//!
//! use dump_without_crashing::dump_without_crashing;
//!
//! if unexpected_condition {
//!     dump_without_crashing!();
//! }
//! ```

chromium::import! {
    "//base:location";
}

use location::Location;

#[cxx::bridge(namespace = "base::debug")]
mod ffi {
    unsafe extern "C++" {
        include!("base/debug/dump_without_crashing_rust_shim.h");

        #[namespace = "base"]
        type RustLocation = location::Location;

        fn DumpWithoutCrashingShim(location: &RustLocation) -> bool;
    }
}

/// Silently generates a crash report for the current process without crashing,
/// so that unexpected-but-recoverable conditions can be investigated. Rust
/// equivalent of `base::debug::DumpWithoutCrashing()` in
/// `base/debug/dump_without_crashing.h`; see that file for details.
///
/// Like in C++, reports are throttled per call site (by default to one per
/// day). Evaluates to `true` if a report was generated, and `false` if it was
/// throttled or if no dump function has been set (e.g. in tests).
///
/// This is a macro rather than a function so that the call can't be a tail
/// call, which would omit the caller from the stack in the crash report. This
/// is the equivalent of `NOT_TAIL_CALLED` on `DumpWithoutCrashing()` in C++,
/// which Rust has no stable equivalent of. (Even without it, the caller's file
/// and line are recorded in the `DumpWithoutCrashing-file` and
/// `DumpWithoutCrashing-line` crash keys.)
#[macro_export]
macro_rules! dump_without_crashing {
    () => {
        // Using the result after the call returns means that the call can't be
        // a tail call.
        ::std::hint::black_box($crate::__dump_without_crashing())
    };
}

/// Implementation detail of [`dump_without_crashing!()`]. Do not use.
///
/// This is `#[track_caller]`, so that `Location::current()` identifies the
/// caller (i.e. the invocation of `dump_without_crashing!()`), and is never
/// inlined, so that it is always in the stack in crash reports, which therefore
/// have a consistent signature.
#[doc(hidden)]
#[track_caller]
#[inline(never)]
pub fn __dump_without_crashing() -> bool {
    // The program counter of `Location::current()` uniquely identifies the
    // call site, as required for throttling. Using the result after the call
    // returns means that this function stays in the stack (rather than being
    // replaced by the shim via a tail call).
    std::hint::black_box(ffi::DumpWithoutCrashingShim(&Location::current()))
}
