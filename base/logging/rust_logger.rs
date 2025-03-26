// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base:logging_log_severity_bindgen" as log_severity;
}

use log::Level::{Debug, Error, Info, Trace, Warn};
use log::{LevelFilter, Metadata, Record};
use log_severity::logging::{LOGGING_ERROR, LOGGING_INFO, LOGGING_WARNING};
use std::ffi::CString;
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
        let wrapped_args = RustFmtArguments(*record.args());
        unsafe {
            ffi::print_rust_log(
                &wrapped_args,
                file.as_ptr(),
                record.line().unwrap() as i32,
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
    }
    fn flush(&self) {}
}

static RUST_LOGGER: RustLogger = RustLogger;

/// Wrap a `std::fmt::Arguments` to pass to C++ code.
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

#[cxx::bridge(namespace = "logging::internal")]
mod ffi {
    extern "Rust" {
        type RustFmtArguments<'a>;

        fn format(&self, wrapper: Pin<&mut LogMessageRustWrapper>);

        fn init_rust_log_crate();
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

pub fn init_rust_log_crate() {
    // An error may occur if set_logger has already been called, which can happen
    // during unit tests. In that case, return from the method without executing the
    // subsequent code.
    if let Err(_) = log::set_logger(&RUST_LOGGER) {
        return;
    };
    log::set_max_level(LevelFilter::Trace);
}
