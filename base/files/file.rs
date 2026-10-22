// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Rust wrapper around C++ `base::File`.
//!
//! NOTE: Try to avoid using this type for now, since it's early in
//! development and may be replaced with `std::fs::File` in the future.

use std::fmt;
#[cfg(target_family = "unix")]
use std::os::fd::{FromRawFd, IntoRawFd, OwnedFd as OwnedPlatformFile};
#[cfg(target_family = "windows")]
use std::os::windows::io::{
    FromRawHandle, IntoRawHandle, OwnedHandle as OwnedPlatformFile, RawHandle,
};

use cxx::UniquePtr;

#[cxx::bridge]
pub mod ffi {
    #[namespace = "base::rust::file"]
    unsafe extern "C++" {
        include!("base/files/file.h");
        include!("base/files/file_rust_shim.h");

        #[namespace = "base"]
        pub type File;

        /// # Safety
        /// `fd` must be an open file descriptor that the caller owns. Ownership
        /// is transferred to the returned `File`.
        #[cfg(target_family = "unix")]
        unsafe fn CreateFileFromRawFd(fd: i32) -> UniquePtr<File>;
        #[cfg(target_family = "unix")]
        fn TakeRawFd(file: Pin<&mut File>) -> i32;

        /// # Safety
        /// `handle` must be an open `HANDLE` that the caller owns. Ownership is
        /// transferred to the returned `File`.
        #[cfg(target_family = "windows")]
        unsafe fn CreateFileFromRawHandle(handle: usize) -> UniquePtr<File>;
        #[cfg(target_family = "windows")]
        fn TakeRawHandle(file: Pin<&mut File>) -> usize;

        #[rust_name = "is_valid"]
        fn IsValid(self: &File) -> bool;
        #[cxx_name = "async"]
        fn is_async(self: &File) -> bool;
        #[rust_name = "close"]
        fn Close(self: Pin<&mut File>);
    }
}

/// Thin wrapper around a synchronous C++ `base::File`.
pub struct File {
    // Invariant: this is never null and never async.
    file: UniquePtr<ffi::File>,
}

impl File {
    pub fn is_valid(&self) -> bool {
        self.inner().is_valid()
    }

    pub fn close(&mut self) {
        self.file.pin_mut().close();
    }

    fn inner(&self) -> &ffi::File {
        self.file.as_ref().expect("File should never be null")
    }
}

impl TryFrom<UniquePtr<ffi::File>> for File {
    type Error = anyhow::Error;

    fn try_from(ptr: UniquePtr<ffi::File>) -> anyhow::Result<Self> {
        if ptr.is_null() {
            anyhow::bail!("cannot construct base::File from a null UniquePtr");
        }
        assert!(!ptr.is_async(), "Async base::File is not supported in Rust");
        Ok(Self { file: ptr })
    }
}

impl From<File> for UniquePtr<ffi::File> {
    fn from(file: File) -> Self {
        file.file
    }
}

impl AsRef<ffi::File> for File {
    fn as_ref(&self) -> &ffi::File {
        self.inner()
    }
}

impl From<OwnedPlatformFile> for File {
    fn from(handle: OwnedPlatformFile) -> Self {
        // SAFETY: `handle` is an `OwnedPlatformFile`, so the raw handle is
        // open and owned, and `into_raw_*` relinquishes that ownership to C++.
        #[cfg(target_family = "unix")]
        let file = unsafe { ffi::CreateFileFromRawFd(handle.into_raw_fd()) };
        // SAFETY: As above.
        #[cfg(target_family = "windows")]
        let file = unsafe { ffi::CreateFileFromRawHandle(handle.into_raw_handle() as usize) };
        Self { file }
    }
}

impl TryFrom<File> for OwnedPlatformFile {
    type Error = anyhow::Error;

    fn try_from(mut file: File) -> anyhow::Result<Self> {
        if !file.is_valid() {
            anyhow::bail!("cannot extract OS handle from an invalid base::File");
        }
        assert!(!file.inner().is_async(), "Async base::File is not supported in Rust");
        // SAFETY: `file.is_valid()` was true, so `TakePlatformFile()` returned
        // a valid owned OS handle and relinquished C++ ownership.
        #[cfg(target_family = "unix")]
        let handle = unsafe { OwnedPlatformFile::from_raw_fd(ffi::TakeRawFd(file.file.pin_mut())) };
        // SAFETY: As above.
        #[cfg(target_family = "windows")]
        let handle = unsafe {
            OwnedPlatformFile::from_raw_handle(ffi::TakeRawHandle(file.file.pin_mut()) as RawHandle)
        };
        Ok(handle)
    }
}

impl fmt::Debug for File {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("File").field("is_valid", &self.is_valid()).finish()
    }
}

// SAFETY: `base::File` is just an OS handle, so it can be moved across threads
unsafe impl Send for File {}
// SAFETY: Shared references to `File` only expose const functions.
// Mutating functions like `close` take `&mut self`.
unsafe impl Sync for File {}
