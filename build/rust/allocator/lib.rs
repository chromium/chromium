// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Define the allocator that Rust code in Chrome should use.
//!
//! Any final artifact that depends on this crate, even transitively, will use
//! the allocator defined here.
//!
//! List of known issues:
//!
//! 1. We'd like to use PartitionAlloc on Windows, but the stdlib uses Windows
//!    heap functions directly that PartitionAlloc can not intercept.
//! 2. We'd like `Vec::try_reserve` to fail at runtime on Linux instead of
//!    crashing in malloc() where PartitionAlloc replaces that function.

// Required to apply weak linkage to symbols.
//
// TODO(https://crbug.com/410596442): Stop using unstable features here.
// https://github.com/rust-lang/rust/issues/29603 tracks stabilization of the `linkage` feature.
#![feature(linkage)]
// Required to apply `#[rustc_std_internal_symbol]` to our alloc error handler
// so the name is correctly mangled as rustc expects.
//
// TODO(https://crbug.com/410596442): Stop using internal features here.
#![allow(internal_features)]
#![feature(rustc_attrs)]

/// Module that provides `#[global_allocator]` / `GlobalAlloc` interface for
/// using an allocator from C++.
#[cfg(rust_allocator_uses_allocator_impls_h)]
mod cpp_allocator {
    use allocator_impls_ffi::rust_allocator_internal as ffi;
    use std::alloc::{GlobalAlloc, Layout};

    struct Allocator;

    unsafe impl GlobalAlloc for Allocator {
        unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
            unsafe { ffi::alloc(layout.size(), layout.align()) }
        }

        unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
            unsafe {
                ffi::dealloc(ptr, layout.size(), layout.align());
            }
        }

        unsafe fn alloc_zeroed(&self, layout: Layout) -> *mut u8 {
            unsafe { ffi::alloc_zeroed(layout.size(), layout.align()) }
        }

        unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, new_size: usize) -> *mut u8 {
            unsafe { ffi::realloc(ptr, layout.size(), layout.align(), new_size) }
        }
    }

    #[global_allocator]
    static GLOBAL: Allocator = Allocator;
}

/// Module that provides `#[global_allocator]` / `GlobalAlloc` interface for
/// using the default Rust allocator.
#[cfg(not(rust_allocator_uses_allocator_impls_h))]
mod rust_allocator {
    #[global_allocator]
    static GLOBAL: std::alloc::System = std::alloc::System;
}

/// Module that provides global symbols that are needed both by `cpp_allocator`
/// and `rust_allocator`.
///
/// When `rustc` drives linking, then it will define the symbols below.  But
/// Chromium only uses `rustc` to link Rust-only executables (e.g. `build.rs`
/// scripts) and otherwise uses a non-Rust linker.  This is why we have to
/// manually define a few symbols below.  We define those symbols
/// as "weak" symbols, so that Rust-provided symbols "win" in case where Rust
/// actually does drive the linking.  This hack works (not only for Chromium,
/// but also for google3 and other projects), but isn't officially supported by
/// `rustc`.
///
/// TODO(https://crbug.com/410596442): Stop using internal features here.
mod both_allocators {
    use alloc_error_handler_impl_ffi::rust_allocator_internal as ffi;

    /// As part of rustc's contract for using `#[global_allocator]` without
    /// rustc-generated shims we must define this symbol, since we are opting in
    /// to unstable functionality. See https://github.com/rust-lang/rust/issues/123015
    #[no_mangle]
    #[linkage = "weak"]
    static __rust_no_alloc_shim_is_unstable: u8 = 0;

    // Mangle the symbol name as rustc expects.
    #[rustc_std_internal_symbol]
    #[allow(non_upper_case_globals)]
    #[linkage = "weak"]
    static __rust_alloc_error_handler_should_panic: u8 = 0;

    // Mangle the symbol name as rustc expects.
    #[rustc_std_internal_symbol]
    #[allow(non_upper_case_globals)]
    #[linkage = "weak"]
    fn __rust_alloc_error_handler(_size: usize, _align: usize) {
        // TODO(lukasza): Investigate if we can just call `std::process::abort()` here.
        // (Not really _needed_, but it could simplify code a little bit.)
        unsafe { ffi::alloc_error_handler_impl() }
    }
}
