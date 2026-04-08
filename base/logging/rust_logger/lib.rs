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
/// * `panic!` => `LOG(FATAL)`
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
