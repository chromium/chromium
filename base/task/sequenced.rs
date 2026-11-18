// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Types for managing shared state that should only be accessed on a single
//! sequence.

mod thread_lock;

// Public so that we can test these independently, but external code shouldn't
// use them directly.
#[doc(hidden)]
pub mod for_testing {
    pub use crate::thread_lock::{Conflict, ThreadLock, ThreadLockGuard};
}
