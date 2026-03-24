// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the user-visible interface for using a base::RunLoop
//! object from Rust.

#[cxx::bridge(namespace = "base")]
mod ffi {
    unsafe extern "C++" {
        include!("base/run_loop_rust_shim.h");

        #[namespace = "base"]
        type RunLoop;

        // We need a shim because cxx won't let us allocate on the stack.
        fn CreateRunLoop() -> UniquePtr<RunLoop>;

        // Call run_loop.Run(). We need a shim because cxx doesn't support
        // functions with default arguments, and for the same reason as
        // `QuitRunLoop` below.
        fn RunRunLoop(run_loop: &UniquePtr<RunLoop>);

        // Quit the given `RunLoop`. We need a shim because the function takes
        // a mutable reference, but since it's thread safe we need to be able to
        // call it with a shared reference.
        fn QuitRunLoop(run_loop: &UniquePtr<RunLoop>);
    }
}

use cxx::UniquePtr;
use std::sync::Arc;

pub struct RunLoop {
    run_loop: Arc<UniquePtr<ffi::RunLoop>>,
}

impl Default for RunLoop {
    fn default() -> Self {
        Self::new()
    }
}

impl RunLoop {
    // Create a new RunLoop object.
    pub fn new() -> Self {
        RunLoop { run_loop: Arc::new(ffi::CreateRunLoop()) }
    }

    // Run the loop until this loop's `quit_closure()` is called. If it has already
    // been called for this loop, return immediately instead.
    //
    // Each loop can only be run once (further runs would just return immediately),
    // so this function takes the `RunLoop` by value to reflect that.
    pub fn run(self) {
        ffi::RunRunLoop(&self.run_loop);
    }

    // Return a closure that will quit `self` when executed.
    pub fn get_quit_closure(&self) -> impl Fn() + Send + 'static {
        let self_weak = Arc::downgrade(&self.run_loop);
        move || {
            if let Some(run_loop) = self_weak.upgrade() {
                ffi::QuitRunLoop(&run_loop)
            }
        }
    }
}

// SAFETY: we only expose the thread-safe subset of RunLoop's functionality
unsafe impl Send for ffi::RunLoop {}
unsafe impl Sync for ffi::RunLoop {}
