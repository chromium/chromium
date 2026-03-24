// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge(namespace = "base::test")]
pub mod ffi {
    unsafe extern "C++" {
        include!("base/test/task_environment_rust_shim.h");

        #[namespace = "base::test"]
        type SingleThreadTaskEnvironment;

        fn CreateTaskEnvironment() -> UniquePtr<SingleThreadTaskEnvironment>;
    }
}
