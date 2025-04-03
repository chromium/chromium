// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Define the allocator that Rust code in Chrome should use.
//!
//! Any final artifact that depends on this crate, even transitively, will use
//! the allocator defined here. Currently this is a thin wrapper around
//! allocator_impls.cc's functions; see the documentation there.

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

#[cxx::bridge(namespace = "rust_allocator_internal")]
mod ffi {
    extern "C++" {
        include!("build/rust/std/allocator_impls.h");

        unsafe fn alloc(size: usize, align: usize) -> *mut u8;
        unsafe fn dealloc(p: *mut u8, size: usize, align: usize);
        unsafe fn realloc(p: *mut u8, old_size: usize, align: usize, new_size: usize) -> *mut u8;
        unsafe fn alloc_zeroed(size: usize, align: usize) -> *mut u8;
    }
}
