// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the user-visible interface for using a sequenced task
//! runner from Rust code. In the future it will likely be expanded to cover
//! other types of task runner.
//!
//! # Examples
//!
//! ```
//! let ptr: SequencedTaskRunnerHandle = SequencedTaskRunnerHandle::get_current_default().unwrap();
//! let do_something = || println!("I did something!");
//! ptr.post_task(do_something);
//! ```

chromium::import! {
    "//base:scoped_refptr";
    "//base:callback";
    "//base:run_loop";
}

#[cxx::bridge(namespace = "base")]
pub mod ffi {
    unsafe extern "C++" {
        include!("base/functional/callback.rs.h");
        include!("base/task/sequenced_task_runner_rust_shim.h");

        // Tell cxx that we already have bindings for this type
        type RustOnceClosure = callback::RustOnceClosure;

        type SequencedTaskRunner;

        fn PostTaskFromRust(
            runner: Pin<&mut SequencedTaskRunner>,
            task: Box<RustOnceClosure>,
        ) -> bool;

        fn AddRef(runner: &SequencedTaskRunner);

        // TODO(crbug.com/472552387): Tweak `cxx` to make this `allow` obsolete.
        #[allow(clippy::missing_safety_doc)]
        /// # Safety
        ///
        /// The caller must assert that it owns 1 ref-count of this
        /// object, and no code will ever dereference this instance of `self`
        /// afterwards unless they know the ref-count is at least 1 (e.g.
        /// because they have a different ref-counted pointer to the
        /// same object).
        unsafe fn Release(runner: &SequencedTaskRunner);

        // We need a shim here because the normal `GetCurrentDefault` function
        // returns a scoped_refptr, and we can't pass arbitrary generic/templated
        // types across the bridge.
        fn GetCurrentDefaultSequencedTaskRunnerForRust() -> *mut SequencedTaskRunner;
    }
}

use scoped_refptr::{CxxRefCounted, CxxRefCountedThreadSafe, ScopedRefPtr};

// SAFETY:
// The C++ implementation guarantees that ref-counting is the only mechanism
// managing the lifetime of a `SequencedTaskRunner`.
unsafe impl CxxRefCounted for ffi::SequencedTaskRunner {
    fn add_ref(&self) {
        ffi::AddRef(self);
    }

    // SAFETY: The trait imposes the same requirements as `Release`.
    unsafe fn release(&self) {
        unsafe {
            ffi::Release(self);
        }
    }
}

// SAFETY: The C++ implementation of this class is designed to be thread-safe.
unsafe impl CxxRefCountedThreadSafe for ffi::SequencedTaskRunner {}
unsafe impl Send for ffi::SequencedTaskRunner {}
unsafe impl Sync for ffi::SequencedTaskRunner {}

/// This type is the Rust representation of a C++
/// `scoped_refptr<base::SequencedTaskRunner>`. It can interoperate with
/// existing `scoped_refptr`s in C++ code, and keeps the task runner alive
/// at least until it goes out of scope.
#[derive(Clone)]
pub struct SequencedTaskRunnerHandle(ScopedRefPtr<ffi::SequencedTaskRunner>);

impl SequencedTaskRunnerHandle {
    /// Get the current default task runner. This function corresponds to
    /// `base::SequencedTaskRunner::GetCurrentDefault` in C++.
    pub fn get_current_default() -> Option<Self> {
        let default_ptr = ffi::GetCurrentDefaultSequencedTaskRunnerForRust();
        // SAFETY: The ffi function above returns a pointer that owns one ref-count
        unsafe { ScopedRefPtr::wrap_ref_counted(default_ptr) }.map(SequencedTaskRunnerHandle)
    }

    /// Post a task to the sequenced task runner. This function corresponds to
    /// `base::SequencedTaskRunner::PostTask`, but without the location
    /// argument (crbug.com/469133195).
    pub fn post_task<F: FnOnce() + Send + 'static>(&self, task: F) -> bool {
        ffi::PostTaskFromRust(self.0.as_pin(), Box::new(task.into()))
    }

    /// Retrieve the contained ScopedRefPtr. This should generally only be used
    /// for ffi purposes.
    pub fn as_scoped_refptr(&self) -> &ScopedRefPtr<ffi::SequencedTaskRunner> {
        &self.0
    }

    /// Run all tasks which have been queued up so far
    pub fn run_all_current_tasks_for_testing(&self) {
        let run_loop = run_loop::RunLoop::new();
        self.post_task(run_loop.get_quit_closure());
        run_loop.run();
    }

    pub fn run_all_current_tasks_on_default_runner_for_testing() {
        Self::get_current_default()
            .expect("Must be called in a context with a default task runner")
            .run_all_current_tasks_for_testing();
    }
}
