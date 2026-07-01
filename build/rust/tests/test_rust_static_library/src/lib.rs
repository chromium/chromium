// Copyright 2021 The Chromium Authors
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
        fn allocate_zeroed_huge_via_rust(size: usize, align: usize) -> bool;
        unsafe fn reallocate_huge_via_rust(size: usize, align: usize) -> bool;
    }
}

pub fn say_hello() {
    println!(
        "Hello, world - from a Rust library. Calculations suggest that 3+4={}",
        add_two_ints_via_rust(3, 4)
    );
}

pub fn alloc_aligned() {
    // SAFETY: 512 is a power of two, and 1024 is already a multiple of 512
    let layout = unsafe { Layout::from_size_align_unchecked(1024, 512) };
    // SAFETY: `layout` is nonzero
    let ptr = unsafe { alloc(layout) };
    println!("Alloc aligned ptr: {:p}", ptr);
    // SAFETY: `ptr` was just allocated in this allocator with `layout`
    unsafe { dealloc(ptr, layout) };
}

#[test]
fn test_hello() {
    assert_eq!(7, add_two_ints_via_rust(3, 4));
}

pub fn add_two_ints_via_rust(x: i32, y: i32) -> i32 {
    x + y
}

// The next function is used from the
// AllocatorTest.RustComponentUsesPartitionAlloc unit test.
pub fn allocate_via_rust() -> Box<ffi::SomeStruct> {
    Box::new(ffi::SomeStruct { a: 43 })
}

mod tests {
    #[test]
    fn test_in_mod() {
        // Always passes; just to see if tests in modules are handled correctly.
    }
}

// Used from the RustLargeAllocationFailure unit tests.
pub fn allocate_huge_via_rust(size: usize, align: usize) -> bool {
    let layout = std::alloc::Layout::from_size_align(size, align).unwrap();
    // SAFETY: `from_size_align` ensures `layout` is non-zero.
    let p = unsafe { std::alloc::alloc(layout) };
    // Rust can optimize out allocations. By printing the pointer value we ensure
    // the allocation actually happens (and can thus fail).
    dbg!(p);
    if !p.is_null() {
        // SAFETY: `p` was just allocated in this allocator with `layout`
        unsafe { std::alloc::dealloc(p, layout) };
    }
    !p.is_null()
}

// Used from the RustLargeAllocationFailure unit tests.
pub fn allocate_zeroed_huge_via_rust(size: usize, align: usize) -> bool {
    let layout = std::alloc::Layout::from_size_align(size, align).unwrap();
    // SAFETY: `from_size_align` ensures `layout` is non-zero.
    let p = unsafe { std::alloc::alloc_zeroed(layout) };
    // Rust can optimize out allocations. By printing the pointer value we ensure
    // the allocation actually happens (and can thus fail).
    dbg!(p);
    if !p.is_null() {
        // SAFETY: `p` was just allocated in this allocator with `layout`
        unsafe { std::alloc::dealloc(p, layout) };
    }
    !p.is_null()
}

/// Used from the RustLargeAllocationFailure unit tests.
///
/// # Safety
///
/// - `size` is nonzero.
/// - `size`, when rounded up to the nearest multiple of `align`, does not
///   overflow `isize`.
pub unsafe fn reallocate_huge_via_rust(size: usize, align: usize) -> bool {
    let layout = std::alloc::Layout::from_size_align(align, align).unwrap();
    // SAFETY: `from_size_align` ensures `layout` is non-zero.
    let p = unsafe { std::alloc::alloc(layout) };
    assert!(!p.is_null());
    // SAFETY: `p` was allocated with this allocator with `layout`;
    // `size` is valid per this function's requirements.
    let p = unsafe { std::alloc::realloc(p, layout, size) };
    let layout = std::alloc::Layout::from_size_align(size, align).unwrap();
    // Rust can optimize out allocations. By printing the pointer value we ensure
    // the allocation actually happens (and can thus fail).
    dbg!(p);
    if !p.is_null() {
        // SAFETY: If `p` is non-null then `realloc` succeeded, so it matches the new
        // `layout`.
        unsafe { std::alloc::dealloc(p, layout) };
    }
    !p.is_null()
}
