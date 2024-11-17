// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base:logging_log_severity_bindgen" as log_severity;
    "//base:logging_rust_log_integration_bindgen" as rust_log_integration;
}

use log::Level::{Debug, Error, Info, Trace, Warn};
use log::{LevelFilter, Metadata, Record};
use log_severity::logging::{LOGGING_ERROR, LOGGING_INFO, LOGGING_WARNING};
use rust_log_integration::logging::internal::print_rust_log;
use std::ffi::CString;

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
        let msg = match record.args().as_str() {
            Some(s) => CString::new(s),
            None => CString::new(&*record.args().to_string()),
        }
        .expect("CString::new failed to create the log message!");
        let file = CString::new(record.file().unwrap())
            .expect("CString::new failed to create the log file name!");
        unsafe {
            print_rust_log(
                msg.as_ptr(),
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

#[cxx::bridge(namespace = "logging::internal")]
mod ffi {
    extern "Rust" {
        fn init_rust_log_crate();
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
