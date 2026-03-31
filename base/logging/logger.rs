// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base/logging:log_severity_bindgen" as log_severity;
}

use log::Level::{Debug, Error, Info, Trace, Warn};
use log::{LevelFilter, Metadata, Record};
use log_severity::root::logging::{
    LogSeverity, LOGGING_ERROR, LOGGING_FATAL, LOGGING_INFO, LOGGING_WARNING,
};
use std::ffi::{CStr, CString};
use std::panic::PanicHookInfo;
use std::pin::Pin;

struct RustLogger;

impl log::Log for RustLogger {
    fn enabled(&self, _metadata: &Metadata) -> bool {
        // Always enabled, as it's controlled by command line flags managed by the C++
        // implementation.
        true
    }

    fn log(&self, record: &Record) {
        // TODO(thiruak1024@gmail.com): Rather than using heap allocation to pass |msg|
        // and |file|, we should return a pointer and size object to leverage the
        // string_view object in C++. https://crbug.com/371112531
        let file = CString::new(record.file().unwrap())
            .expect("CString::new failed to create the log file name!");

        print_rust_log(
            record.args(),
            &file,
            record.line(),
            match record.metadata().level() {
                Error => LOGGING_ERROR,
                Warn => LOGGING_WARNING,
                Info => LOGGING_INFO,
                // Note that Debug and Trace level logs are dropped at
                // compile time at the macro call-site when DCHECK_IS_ON()
                // is false. This is done through a Cargo feature.
                Debug | Trace => LOGGING_INFO,
            },
            record.metadata().level() == Trace,
        )
    }
    fn flush(&self) {}
}

/// Wrap a `std::fmt::Arguments` to pass to C++ code.
#[repr(transparent)]
struct RustFmtArguments<'a>(std::fmt::Arguments<'a>);

impl<'a> RustFmtArguments<'a> {
    /// Format `msg` to the C++-provided stream in `wrapper`.
    fn format(&self, mut wrapper: Pin<&mut ffi::LogMessageRustWrapper>) {
        // No error expected because our `Write` impl below is infallible.
        std::fmt::write(&mut wrapper, self.0).unwrap();
    }
}

// Glue impl to use std::fmt tools with `ffi::LogMessageRustWrapper`.
impl std::fmt::Write for Pin<&mut ffi::LogMessageRustWrapper> {
    fn write_str(&mut self, s: &str) -> Result<(), std::fmt::Error> {
        self.as_mut().write_to_stream(s);
        Ok(())
    }
}

/// Rust API for logging a message using `//base/logging.h`.
fn print_rust_log(
    args: &core::fmt::Arguments,
    filename: &CStr,
    line: Option<u32>,
    severity: LogSeverity,
    verbose: bool,
) {
    let wrapped_args = RustFmtArguments(*args);

    // SAFETY: Safety requirements of the C++ function are met as follows:
    //
    // * `&wrapped_args`: Taken from a Rust reference, so must be valid
    // * `filename.as_ptr()`: `CStr` promises to return a pointer to a
    //   NUL-terminated string
    unsafe {
        ffi::print_rust_log(
            &wrapped_args,
            filename.as_ptr(),
            line.unwrap_or(0) as i32,
            severity,
            verbose,
        )
    }
}

#[cxx::bridge(namespace = "logging::internal")]
mod ffi {
    extern "Rust" {
        type RustFmtArguments<'a>;

        fn format(&self, wrapper: Pin<&mut LogMessageRustWrapper>);

        fn init_rust_logging();
    }

    unsafe extern "C++" {
        include!("base/logging/rust_log_integration.h");

        /// Wraps a C++ LogMessage object so we can write to its ostream.
        type LogMessageRustWrapper;

        /// Write a block of characters to the stream.
        fn write_to_stream(self: Pin<&mut LogMessageRustWrapper>, s: &str);

        /// Emit a log message to the C++-managed logger. `msg` is passed back
        /// to `format_to_wrapped_message` to be stringified.
        unsafe fn print_rust_log(
            msg: &RustFmtArguments,
            file: *const c_char,
            line: i32,
            severity: i32,
            verbose: bool,
        );
    }
}

fn panic_hook(info: &PanicHookInfo<'_>) {
    print_rust_log(
        &format_args!("{info}"),
        info.location().map(|loc| loc.file_as_c_str()).unwrap_or(c"<unknown file>"),
        info.location().map(|loc| loc.line()),
        LOGGING_FATAL,
        false,
    )
}

/// Initializes the integration between Rust and `base/logging.h`.
///
/// It is safe to call `init_rust_logging` multiple times.
///
/// The integration covers the following translation:
///
/// * `log::error!` => `LOG(ERROR)`
/// * `log::warn!` => `LOG(WARNING)`
/// * `log::info!` => `LOG(INFO)`
/// * `panic!` => `LOG(FATAL)`
pub fn init_rust_logging() {
    // Gracefully handle being called more than once - e.g. when
    // `//base/logging_unittest.cc` uses `logging::ScopedLoggingSettings` which
    // calls `InitLogging` to re-initialize logging.
    static ONCE: std::sync::Once = std::sync::Once::new();
    ONCE.call_once(|| {
        // We don't call `std::panic::take_hook` to store and use the old/default hook.
        // Among other things this means that `RUST_BACKTRACE=1` has no effect on
        // Chromium binaries/tests.  This seems okay, because `LOG(FATAL)` should
        // print the callstack (and task trace, crash keys, etc.).
        std::panic::set_hook(Box::new(panic_hook));

        static RUST_LOGGER: RustLogger = RustLogger;
        if log::set_logger(&RUST_LOGGER).is_err() {
            panic!("A custom logger has already been set in the `log` crate.")
        }
        log::set_max_level(LevelFilter::Trace);
    });
}
