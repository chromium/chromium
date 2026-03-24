// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base:scoped_refptr";
    "//base:sequenced_task_runner";
}

#[cxx::bridge(namespace = "base::task::test")]
pub mod ffi {
    unsafe extern "C++" {
        include!("base/task/sequenced_task_runner_test_util.h");
        pub type TestRefCounted;

        pub fn CreateTestRefCounted(b: &mut bool) -> *mut TestRefCounted;
        pub fn HasOneRef(&self) -> bool;
        pub fn HasAtLeastOneRef(&self) -> bool;

        fn AddRef(&self);

        // TODO(crbug.com/472552387): Tweak `cxx` to make this `allow` obsolete.
        #[allow(clippy::missing_safety_doc)]
        /// # Safety
        /// Same requirements as in base::memory::scoped_refptr::CxxRefCounted.
        unsafe fn Release(&self);
    }
}

// SAFETY:
// The C++ implementation guarantees that ref-counting is the only mechanism
// managing the lifetime of a `SequencedTaskRunner`.
unsafe impl scoped_refptr::CxxRefCounted for ffi::TestRefCounted {
    fn add_ref(&self) {
        self.AddRef();
    }

    // SAFETY: The trait imposes the same requirements as `Release`.
    unsafe fn release(&self) {
        unsafe {
            self.Release();
        }
    }
}
