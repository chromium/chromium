// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! A `Location` identifies a call site in the source code, so that it can be
//! passed to C++ APIs that take a `base::Location`.
//!
//! ```
//! chromium::import! {
//!     "//base:location";
//! }
//!
//! use location::{from_here, Location};
//!
//! let here: Location = from_here!();
//!
//! #[track_caller]
//! fn do_something() {
//!     let caller = Location::current();
//! }
//! ```
//!
//! A `Location` can be passed to C++ as a `base::RustLocation`, which C++ shims
//! convert to a `base::Location` with `base::Location::CreateForRust()`. In a
//! `cxx::bridge`, declare it with:
//!
//! ```
//! #[namespace = "base"]
//! type RustLocation = location::Location;
//! ```

#![deny(unsafe_code)]

use std::ffi::{c_char, CStr};
use std::fmt;
use std::hash::{Hash, Hasher};

/// A call site in the source code. Rust equivalent of `base::Location` in
/// `base/location.h`.
///
/// Create one with [`from_here!()`] or [`Location::current()`].
///
/// As in C++, two `Location`s are equal if they have the same program counter.
/// Unlike in C++, there is no function name, since Rust has no equivalent of
/// `__builtin_FUNCTION()`.
///
/// The layout matches `base::RustLocation` in `base/location.h`, so that it can
/// be passed to C++ as is.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Location {
    file_name: *const c_char,
    line_number: u32,
    program_counter: usize,
}

// Compile-time checks that the layout matches `base::RustLocation`, which
// `base/location.cc` checks in the same way.
const _: () = {
    use std::mem::{align_of, offset_of, size_of};
    assert!(size_of::<Location>() == 3 * size_of::<usize>());
    assert!(align_of::<Location>() == align_of::<usize>());
    assert!(offset_of!(Location, file_name) == 0);
    assert!(offset_of!(Location, line_number) == size_of::<usize>());
    assert!(offset_of!(Location, program_counter) == 2 * size_of::<usize>());
};

// SAFETY: `file_name` points to immutable `'static` data, so a `Location` can
// be sent to other threads.
#[allow(unsafe_code)]
unsafe impl Send for Location {}

// SAFETY: `file_name` points to immutable `'static` data, so a `Location` can
// be shared with other threads.
#[allow(unsafe_code)]
unsafe impl Sync for Location {}

// SAFETY: `Location` has the same layout as `base::RustLocation` (see the
// compile-time checks above and in `base/location.cc`), which is trivially
// copyable and trivially destructible in C++.
#[allow(unsafe_code)]
unsafe impl cxx::ExternType for Location {
    type Id = cxx::type_id!("base::RustLocation");
    type Kind = cxx::kind::Trivial;
}

#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("base/location.h");

        #[namespace = "base"]
        type RustLocation = super::Location;
    }
}

impl Location {
    /// Returns the location of the caller of the enclosing `#[track_caller]`
    /// function (or of this call, if there is none).
    ///
    /// The program counter uniquely identifies the call site, so it is
    /// suitable for identity, but it is not a code address and can't be
    /// symbolized. Use [`from_here!()`] where that matters.
    #[track_caller]
    #[inline]
    pub fn current() -> Self {
        let caller = std::panic::Location::caller();
        Self {
            file_name: strip_build_dir_prefix(caller.file_as_c_str()).as_ptr(),
            line_number: caller.line(),
            // Each call site has its own `'static` `std::panic::Location`, so
            // its address uniquely identifies the call site.
            program_counter: std::ptr::from_ref(caller) as usize,
        }
    }

    /// Implementation detail of [`from_here!()`]. Do not use.
    #[doc(hidden)]
    pub const fn __from_here(
        file_name: &'static CStr,
        line_number: u32,
        program_counter: usize,
    ) -> Self {
        Self { file_name: strip_build_dir_prefix(file_name).as_ptr(), line_number, program_counter }
    }

    /// The source file name, relative to the source root (e.g.
    /// `base/location.rs`, or `base\location.rs` on Windows), as with
    /// `base::Location::file_name()`.
    pub fn file_name(&self) -> &'static CStr {
        // SAFETY: `file_name` always points to a `'static` NUL-terminated
        // string, since it is only ever set from a `&'static CStr`.
        #[allow(unsafe_code)]
        unsafe {
            CStr::from_ptr(self.file_name)
        }
    }

    pub fn line_number(&self) -> u32 {
        self.line_number
    }

    pub fn program_counter(&self) -> usize {
        self.program_counter
    }
}

impl PartialEq for Location {
    fn eq(&self, other: &Self) -> bool {
        self.program_counter == other.program_counter
    }
}

impl Eq for Location {}

impl Hash for Location {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.program_counter.hash(state);
    }
}

impl fmt::Debug for Location {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}:{} ({:#x})",
            self.file_name().to_string_lossy(),
            self.line_number,
            self.program_counter
        )
    }
}

impl fmt::Display for Location {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}:{}", self.file_name().to_string_lossy(), self.line_number)
    }
}

/// Returns the location of this macro invocation. Rust equivalent of
/// `FROM_HERE` in C++.
///
/// Unlike [`Location::current()`], the program counter is a code address at
/// the call site: the address of a function generated by the macro, which
/// symbolizes to `<enclosing function>::here`.
#[macro_export]
macro_rules! from_here {
    () => {{
        // A function that is unique to this call site, so that its address
        // uniquely identifies it. It refers to a static with interior
        // mutability, so that identical code folding can't merge it with
        // the functions generated by other invocations (as it would if they
        // all had the same code).
        //
        // (We have to use this trick because `return_address` is currently
        // unstable in Rust.)
        fn here() -> *const ::std::sync::atomic::AtomicU8 {
            static ANCHOR: ::std::sync::atomic::AtomicU8 = ::std::sync::atomic::AtomicU8::new(0);
            &ANCHOR
        }
        const FILE_NAME: &::std::ffi::CStr =
            match ::std::ffi::CStr::from_bytes_with_nul(concat!(file!(), "\0").as_bytes()) {
                Ok(file_name) => file_name,
                Err(_) => panic!("file name contains a NUL"),
            };
        $crate::Location::__from_here(
            FILE_NAME,
            line!(),
            here as fn() -> *const ::std::sync::atomic::AtomicU8 as usize,
        )
    }};
}

/// The path of this file relative to the source root.
/// Used to work out the length of the build-directory prefix on paths returned
/// from `file!()`. Similar to the trick //base/location.cc performs with
/// kThisFilePath.
const THIS_FILE_PATH: &str = "base/location.rs";

/// The length of the build-directory prefix of paths from `file!()`.
/// Uses THIS_FILE_PATH as an anchor to compute it, similar to how
/// //base/location.cc does it.
const BUILD_DIR_PREFIX_LENGTH: usize = {
    let file = file!().as_bytes();
    let this_file_path = THIS_FILE_PATH.as_bytes();
    assert!(file.len() >= this_file_path.len(), "unexpected path for location.rs");
    let prefix_length = file.len() - this_file_path.len();
    let mut i = 0;
    while i < this_file_path.len() {
        let c = file[prefix_length + i];
        // Allow either path separator, in case `file!()` uses backslashes on
        // Windows.
        let c = if c == b'\\' { b'/' } else { c };
        assert!(c == this_file_path[i], "unexpected path for location.rs");
        i += 1;
    }
    prefix_length
};

/// Strips the build directory prefix from `file_name`, if present.
const fn strip_build_dir_prefix(file_name: &'static CStr) -> &'static CStr {
    let bytes = file_name.to_bytes_with_nul();
    let prefix = file!().as_bytes();
    if bytes.len() <= BUILD_DIR_PREFIX_LENGTH {
        return file_name;
    }
    let mut i = 0;
    while i < BUILD_DIR_PREFIX_LENGTH {
        if bytes[i] != prefix[i] {
            return file_name;
        }
        i += 1;
    }
    let (_, rest) = bytes.split_at(BUILD_DIR_PREFIX_LENGTH);
    match CStr::from_bytes_with_nul(rest) {
        Ok(stripped) => stripped,
        // Unreachable, since `rest` is a suffix of a valid C string.
        Err(_) => file_name,
    }
}
