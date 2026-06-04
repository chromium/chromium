// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use cxx::UniquePtr;
use std::path::{Path, PathBuf};

#[cxx::bridge]
pub mod ffi {
    #[namespace = "base::rust::file_path"]
    unsafe extern "C++" {
        include!("base/files/file_path.h");
        include!("base/files/file_path_rust_shim.h");

        #[namespace = "base"]
        pub type FilePath;

        #[cfg(target_family = "unix")]
        fn CreateFilePathFromBytes(bytes: &[u8]) -> UniquePtr<FilePath>;
        #[cfg(target_family = "unix")]
        fn FilePathToBytes(path: &FilePath) -> &[u8];

        #[cfg(target_family = "windows")]
        fn CreateFilePathFromWide(wide: &[u16]) -> UniquePtr<FilePath>;
        #[cfg(target_family = "windows")]
        fn FilePathToWide(path: &FilePath) -> &[u16];
    }
}

// SAFETY: `base::FilePath` is an immutable value-like type from Rust's
// perspective (no mutation methods are exposed to Rust). It is a C++
// `base::FilePath` which is thread-safe for concurrent reads. Therefore, it is
// safe to share references across threads (`Sync`) and transfer ownership
// (`Send`).
unsafe impl Send for ffi::FilePath {}
unsafe impl Sync for ffi::FilePath {}

impl ffi::FilePath {
    pub fn from_path(path: &Path) -> UniquePtr<Self> {
        #[cfg(unix)]
        {
            use std::os::unix::ffi::OsStrExt;
            ffi::CreateFilePathFromBytes(path.as_os_str().as_bytes())
        }
        #[cfg(windows)]
        {
            use std::os::windows::ffi::OsStrExt;
            let wide: Vec<u16> = path.as_os_str().encode_wide().collect();
            ffi::CreateFilePathFromWide(&wide)
        }
        #[cfg(not(any(unix, windows)))]
        compile_error!("Unsupported platform for base::FilePath FFI");
    }

    pub fn to_path_buf(&self) -> PathBuf {
        #[cfg(unix)]
        {
            use std::os::unix::ffi::OsStringExt;
            let bytes = ffi::FilePathToBytes(self);
            PathBuf::from(std::ffi::OsString::from_vec(bytes.to_vec()))
        }
        #[cfg(windows)]
        {
            use std::os::windows::ffi::OsStringExt;
            let wide = ffi::FilePathToWide(self);
            PathBuf::from(std::ffi::OsString::from_wide(wide))
        }
        #[cfg(not(any(unix, windows)))]
        compile_error!("Unsupported platform for base::FilePath FFI");
    }

    /// Note: `as_os_str` and `as_path` are Unix-only because Unix platforms
    /// store paths as bytes, allowing zero-copy references. Windows uses
    /// UTF-16 internally, while Rust's `OsStr` uses WTF-8, so any
    /// conversion on Windows requires allocation and is not zero-copy.
    #[cfg(unix)]
    pub fn as_os_str(&self) -> &std::ffi::OsStr {
        use std::os::unix::ffi::OsStrExt;
        std::ffi::OsStr::from_bytes(ffi::FilePathToBytes(self))
    }

    #[cfg(unix)]
    pub fn as_path(&self) -> &Path {
        Path::new(self.as_os_str())
    }
}
