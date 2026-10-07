// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

mod custom_panic_hook;
mod log_crate_integration;
mod print_rust_log;

#[cxx::bridge(namespace = "logging::internal")]
mod ffi {
    extern "Rust" {
        fn init_rust_logging();
        fn update_rust_log_level(min_log_level: i32);
    }
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
/// * `log::debug!` => `DVLOG(1)`
/// * `log::trace!` => `DVLOG(2)`
/// * `panic!` => `LOG(FATAL)`
///
/// Note that in release builds (when `DCHECK_IS_ON()` is false), `log::debug!`
/// and `log::trace!` are compiled out at the call-site via the
/// `release_max_level_info` Cargo feature on the `log` crate, behaving like
/// `DVLOG(1)` and `DVLOG(2)`.
fn init_rust_logging() {
    // Gracefully handle being called more than once - e.g. when
    // `//base/logging_unittest.cc` uses `logging::ScopedLoggingSettings` which
    // calls `InitLogging` to re-initialize logging.
    static ONCE: std::sync::Once = std::sync::Once::new();
    ONCE.call_once(|| {
        // Set up the panic hook first, so that it is ready in case
        // any subsequent Rust code panics.
        custom_panic_hook::init();
        log_crate_integration::init();
    });
}

fn update_rust_log_level(min_log_level: i32) {
    log_crate_integration::update_min_log_level(min_log_level);
}
