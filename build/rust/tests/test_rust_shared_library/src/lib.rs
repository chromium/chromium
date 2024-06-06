// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::alloc::{alloc, dealloc, Layout};

#[cxx::bridge]
mod ffi {
    pub struct SomeStruct {
        a: i32,
    }
    extern "Rust" {
        fn say_hello();
        fn alloc_aligned();
        fn add_two_ints_via_rust(x: i32, y: i32) -> i32;
        fn allocate_via_rust() -> Box<SomeStruct>;
        fn allocate_huge_via_rust(size: usize, align: usize) -> bool;
    }
}

pub fn say_hello() {
    println!(
        "Hello, world - from a Rust library. Calculations suggest that 3+4={}",
        add_two_ints_via_rust(3, 4)
    );
}

pub fn alloc_aligned() {
    let layout = unsafe { Layout::from_size_align_unchecked(1024, 512) };
    let ptr = unsafe { alloc(layout) };
    println!("Alloc aligned ptr: {:p}", ptr);
    unsafe { dealloc(ptr, layout) };
}

#[test]
fn test_hello() {
    assert_eq!(7, add_two_ints_via_rust(3, 4));
}

pub fn add_two_ints_via_rust(x: i32, y: i32) -> i32 {
    x + y
}

// Used from the RustComponentUsesPartitionAlloc unit tests.
pub fn allocate_via_rust() -> Box<ffi::SomeStruct> {
    Box::new(ffi::SomeStruct { a: 43 })
}

// Used from the RustLargeAllocationFailure unit tests.
pub fn allocate_huge_via_rust(size: usize, align: usize) -> bool {
    let layout = std::alloc::Layout::from_size_align(size, align).unwrap();
    let p = unsafe { std::alloc::alloc(layout) };
    // Rust can optimize out allocations. By printing the pointer value we ensure
    // the allocation actually happens (and can thus fail).
    dbg!(p);
    if !p.is_null() {
        unsafe { std::alloc::dealloc(p, layout) };
    }
    !p.is_null()
}
