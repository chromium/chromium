// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Define the allocator that Rust code in Chrome should use.
//!
//! Any final artifact that depends on this crate, even transitively, will use
//! the allocator defined here. Currently this is a thin wrapper around
//! allocator_impls.cc's functions; see the documentation there.

// Required to apply weak linkage to symbols.
#![feature(linkage)]
// Required to apply `#[rustc_std_internal_symbol]` to our alloc error handler
// so the name is correctly mangled as rustc expects.
#![cfg_attr(mangle_alloc_error_handler, allow(internal_features))]
#![cfg_attr(mangle_alloc_error_handler, feature(rustc_attrs))]

#[cfg(use_cpp_allocator_impls)]
use std::alloc::{GlobalAlloc, Layout};

#[cfg(use_cpp_allocator_impls)]
struct Allocator;

#[cfg(use_cpp_allocator_impls)]
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

#[cfg(use_cpp_allocator_impls)]
#[global_allocator]
static GLOBAL: Allocator = Allocator;

#[cfg(not(use_cpp_allocator_impls))]
#[global_allocator]
static GLOBAL: std::alloc::System = std::alloc::System;

// As part of rustc's contract for using `#[global_allocator]` without
// rustc-generated shims we must define this symbol, since we are opting in to
// unstable functionality. See https://github.com/rust-lang/rust/issues/123015
#[no_mangle]
#[linkage = "weak"]
static __rust_no_alloc_shim_is_unstable: u8 = 0;

// Mangle the symbol name as rustc expects.
#[cfg_attr(mangle_alloc_error_handler, rustc_std_internal_symbol)]
#[cfg_attr(not(mangle_alloc_error_handler), no_mangle)]
#[allow(non_upper_case_globals)]
#[linkage = "weak"]
static __rust_alloc_error_handler_should_panic: u8 = 0;

// Mangle the symbol name as rustc expects.
#[cfg_attr(mangle_alloc_error_handler, rustc_std_internal_symbol)]
#[cfg_attr(not(mangle_alloc_error_handler), no_mangle)]
#[allow(non_upper_case_globals)]
#[linkage = "weak"]
fn __rust_alloc_error_handler(_size: usize, _align: usize) {
    unsafe { ffi::crash_immediately() }
}

// TODO(crbug.com/408221149): conditionally include the FFI glue based on
// `use_cpp_allocator_impls`
#[allow(dead_code)]
#[cxx::bridge(namespace = "rust_allocator_internal")]
mod ffi {
    extern "C++" {
        include!("build/rust/allocator/allocator_impls.h");

        unsafe fn alloc(size: usize, align: usize) -> *mut u8;
        unsafe fn dealloc(p: *mut u8, size: usize, align: usize);
        unsafe fn realloc(p: *mut u8, old_size: usize, align: usize, new_size: usize) -> *mut u8;
        unsafe fn alloc_zeroed(size: usize, align: usize) -> *mut u8;
        unsafe fn crash_immediately();
    }
}
