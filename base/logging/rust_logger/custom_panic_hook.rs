// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::print_rust_log;
use std::panic::PanicHookInfo;

/// Routes `panic!` and similar macros to `LOG(FATAL)`.
pub(crate) fn init() {
    // We don't call `std::panic::take_hook` to store and use the old/default hook.
    // Among other things this means that `RUST_BACKTRACE=1` has no effect on
    // Chromium binaries/tests.  This seems okay, because `LOG(FATAL)` should
    // print the callstack (and task trace, crash keys, etc.).
    std::panic::set_hook(Box::new(panic_hook));
}

fn panic_hook(info: &PanicHookInfo<'_>) {
    print_rust_log::print_rust_log(
        &format_args!("{info}"),
        info.location().map(|loc| loc.file_as_c_str()).unwrap_or(c"<unknown file>"),
        info.location().map(|loc| loc.line()),
        print_rust_log::LogSeverity::Fatal,
    )
}
