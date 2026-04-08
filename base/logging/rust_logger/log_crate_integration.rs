// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::print_rust_log;
use std::ffi::CString;

/// Routes `log::error!` and similar macros to `LOG(ERROR)`, etc.
pub(crate) fn init() {
    static RUST_LOGGER: RustLogger = RustLogger;
    if log::set_logger(&RUST_LOGGER).is_err() {
        panic!("A custom logger has already been set in the `log` crate.")
    }
    log::set_max_level(log::LevelFilter::Trace);
}

struct RustLogger;

impl log::Log for RustLogger {
    fn enabled(&self, _metadata: &log::Metadata) -> bool {
        // Always enabled, as it's controlled by command line flags managed by the C++
        // implementation.
        true
    }

    fn log(&self, record: &log::Record) {
        // TODO(thiruak1024@gmail.com): Rather than using heap allocation to pass |msg|
        // and |file|, we should return a pointer and size object to leverage the
        // string_view object in C++. https://crbug.com/371112531
        let file = CString::new(record.file().unwrap())
            .expect("CString::new failed to create the log file name!");

        // Note that Debug and Trace level logs are dropped at
        // compile time at the macro call-site when DCHECK_IS_ON()
        // is false. This is done through a Cargo feature.
        //
        // TODO(danakj, lukasza): Consider mapping some of these levels to `VLOG(INFO)`
        // instead of `LOG(INFO)`.
        let severity = match record.metadata().level() {
            log::Level::Error => print_rust_log::LogSeverity::Error,
            log::Level::Warn => print_rust_log::LogSeverity::Warning,
            log::Level::Info | log::Level::Debug | log::Level::Trace => {
                print_rust_log::LogSeverity::Info
            }
        };

        print_rust_log::print_rust_log(record.args(), &file, record.line(), severity)
    }

    fn flush(&self) {}
}
