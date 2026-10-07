// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base/logging:log_severity_bindgen" as log_severity_bindgen;
}

use log_severity_bindgen::root::logging::{
    LOGGING_ERROR, LOGGING_FATAL, LOGGING_INFO, LOGGING_WARNING,
};
use std::ffi::CStr;
use std::pin::Pin;

/// Rust API for logging a message using `//base/logging.h`.
///
/// The control flow jumps between `rust_logger/print_rust_log.rs` and
/// `rust_logger/print_rust_log_ffi.cc` as follows:
///
/// * `.rs`: `print_rust_log` calls:
/// * `.cc`: `logging::internal::print_rust_log` which calls:
/// * `.rs`: `RustFmtArguments::format` which calls:
/// * `.rs`: `std::fmt::write` which calls:
/// * `.rs`: `impl std::fmt::Write for Pin<&mut ffi::LogMessageRustWrapper>`
///   which calls:
/// * `.cc`: `logging::internal::LogMessageRustWrapper::write_str` which calls:
/// * `//base`: `logging::LogMessage::operator<<(stream, ...)`
pub(crate) fn print_rust_log(
    args: &core::fmt::Arguments,
    filename: &CStr,
    line: Option<u32>,
    severity: LogSeverity,
) {
    let wrapped_args = RustFmtArguments(*args);

    // SAFETY: Safety requirements of the C++ function are met as follows:
    //
    // * `filename.to_bytes_with_nul()`: `CStr` promises to return a non-empty
    //   slice representing a NUL-terminated string.
    unsafe {
        ffi::print_rust_log(
            &wrapped_args,
            filename.to_bytes_with_nul(),
            line.unwrap_or(0) as i32,
            severity.as_log_severity(),
        )
    }
}

/// Strongly-typed Rust equivalent of `base::LogSeverity`.
///
/// (`bindgen`-generated `LogSeverity` is just a type alias for `u32`.)
#[allow(dead_code)] // Needed because `Verbose` values are ignored in release builds.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum LogSeverity {
    Fatal,
    Error,
    Warning,
    Info,
    /// Verbose logging level (e.g. `1` for `VLOG(1)`, `2` for `VLOG(2)`).
    /// In Chromium logging, verbosities are represented as negative integers
    /// (`-(verbose_level)`).
    Verbose(i32),
}

impl LogSeverity {
    pub fn as_log_severity(self) -> i32 {
        match self {
            LogSeverity::Fatal => LOGGING_FATAL,
            LogSeverity::Error => LOGGING_ERROR,
            LogSeverity::Warning => LOGGING_WARNING,
            LogSeverity::Info => LOGGING_INFO,
            LogSeverity::Verbose(level) => {
                debug_assert!(level > 0);
                -level
            }
        }
    }
}

/// Wrap a `std::fmt::Arguments` to pass to C++ code.
#[repr(transparent)]
struct RustFmtArguments<'a>(std::fmt::Arguments<'a>);

impl<'a> RustFmtArguments<'a> {
    /// Called from C++ to format `self` into the C++-provided `wrapper`.
    fn format(&self, mut wrapper: Pin<&mut ffi::LogMessageRustWrapper>) {
        // `unwrap` can't panic because our `Write` impl below is infallible.
        std::fmt::write(&mut wrapper, self.0).unwrap();
    }
}

/// `impl` used by `std::fmt::write` when called from `RustFmtArguments::format`
impl std::fmt::Write for Pin<&mut ffi::LogMessageRustWrapper> {
    fn write_str(&mut self, s: &str) -> Result<(), std::fmt::Error> {
        self.as_mut().write_str(s);
        Ok(())
    }
}

#[cxx::bridge(namespace = "logging::internal")]
mod ffi {
    extern "Rust" {
        type RustFmtArguments<'a>;
        fn format(&self, wrapper: Pin<&mut LogMessageRustWrapper>);
    }

    unsafe extern "C++" {
        include!("base/logging/rust_logger/print_rust_log_ffi.h");

        /// Wraps a C++ LogMessage object so we can write to its ostream.
        type LogMessageRustWrapper;

        /// Writes `s` to the wrapped `logging::LogMessage`.
        fn write_str(self: Pin<&mut LogMessageRustWrapper>, s: &str);

        /// Emit a log message to the C++-managed logger. `msg` is passed back
        /// to `format_to_wrapped_message` to be stringified. Positive values
        /// of `severity` map to logging level, negative values map to verbosity
        /// at the specified level (e.g. -1 is VLOG(1)).
        ///
        /// # Safety
        ///
        /// `file` must be a non-empty slice representing a NUL-terminated C
        /// string.
        unsafe fn print_rust_log(msg: &RustFmtArguments, file: &[u8], line: i32, severity: i32);
    }
}
