// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::print_rust_log;
use std::ffi::{CStr, CString};
use std::sync::atomic::{AtomicI32, Ordering};

static MIN_LOG_LEVEL: AtomicI32 = AtomicI32::new(0);

/// Routes `log::error!` and similar macros to `LOG(ERROR)`, etc.
pub(crate) fn init() {
    static RUST_LOGGER: RustLogger = RustLogger;
    if log::set_logger(&RUST_LOGGER).is_err() {
        panic!("A custom logger has already been set in the `log` crate.")
    }
    update_min_log_level(MIN_LOG_LEVEL.load(Ordering::Relaxed));
}

pub(crate) fn update_min_log_level(min_log_level: i32) {
    MIN_LOG_LEVEL.store(min_log_level, Ordering::Relaxed);
    let level_filter = match min_log_level {
        ..=-2 => log::LevelFilter::Trace,
        -1 => log::LevelFilter::Debug,
        0 => log::LevelFilter::Info,
        1 => log::LevelFilter::Warn,
        2 => log::LevelFilter::Error,
        _ => log::LevelFilter::Off,
    };
    log::set_max_level(level_filter);
}

/// Calls `f` with `file` converted to a NUL-terminated C string, as expected
/// by `print_rust_log`. `None` (or a path containing an interior NUL) is
/// passed as an empty string.
fn with_nul_terminated_file<R>(file: Option<&str>, f: impl FnOnce(&CStr) -> R) -> R {
    match file {
        None => f(c""),
        Some(s) => {
            // Logging can be frequent, so avoid a heap allocation for the
            // common case of a short file path by building the C string in a
            // stack buffer. Longer paths fall back to an allocating `CString`.
            const STACK_BUF_LEN: usize = 256;
            let bytes = s.as_bytes();
            if bytes.len() < STACK_BUF_LEN {
                let mut buf = [0u8; STACK_BUF_LEN];
                buf[..bytes.len()].copy_from_slice(bytes);
                debug_assert_eq!(buf[bytes.len()], 0);
                let c_str = CStr::from_bytes_with_nul(&buf[..=bytes.len()]).unwrap_or(c"");
                f(c_str)
            } else {
                let c_string = CString::new(bytes).unwrap_or_default();
                f(&c_string)
            }
        }
    }
}

/// Maps a `log` crate level to the corresponding Chromium log severity.
///
/// Note that Debug and Trace level logs are dropped at compile time at the
/// macro call-site when debug assertions are off (which corresponds to
/// `DCHECK_IS_ON()` being false). This is done through the
/// `release_max_level_info` Cargo feature on the `log` crate. Therefore,
/// `log::debug!` and `log::trace!` effectively behave as `DVLOG(1)` and
/// `DVLOG(2)`.
fn to_log_severity(level: log::Level) -> print_rust_log::LogSeverity {
    match level {
        log::Level::Error => print_rust_log::LogSeverity::Error,
        log::Level::Warn => print_rust_log::LogSeverity::Warning,
        log::Level::Info => print_rust_log::LogSeverity::Info,
        log::Level::Debug => print_rust_log::LogSeverity::Verbose(1),
        log::Level::Trace => print_rust_log::LogSeverity::Verbose(2),
    }
}

struct RustLogger;

impl log::Log for RustLogger {
    fn enabled(&self, metadata: &log::Metadata) -> bool {
        let min_level = MIN_LOG_LEVEL.load(Ordering::Relaxed);
        to_log_severity(metadata.level()).as_log_severity() >= min_level
    }

    fn log(&self, record: &log::Record) {
        if !self.enabled(record.metadata()) {
            return;
        }

        let level = record.metadata().level();
        #[cfg(not(debug_assertions))]
        if level > log::Level::Info {
            panic!("`log::debug!` and `log::trace!` should be compiled out in release builds");
        }
        let severity = to_log_severity(level);

        with_nul_terminated_file(record.file(), |file| {
            print_rust_log::print_rust_log(record.args(), file, record.line(), severity);
        });
    }

    fn flush(&self) {}
}
