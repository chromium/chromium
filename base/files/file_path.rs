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

        #[rust_name = "strip_trailing_separators"]
        fn StripTrailingSeparators(path: &FilePath) -> UniquePtr<FilePath>;
    }
}

// SAFETY: `base::FilePath` is an immutable value-like type from Rust's
// perspective (no mutation methods are exposed to Rust). It is a C++
// `base::FilePath` which is thread-safe for concurrent reads. Therefore, it is
// safe to share references across threads (`Sync`) and transfer ownership
// (`Send`).
unsafe impl Send for ffi::FilePath {}
// SAFETY: As above
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

    pub fn strip_trailing_separators(&self) -> UniquePtr<Self> {
        ffi::strip_trailing_separators(self)
    }

    /// Borrows `FilePath::value()` directly from C++ as a byte slice (no copy).
    #[cfg(unix)]
    fn comparison_key(&self) -> &[u8] {
        ffi::FilePathToBytes(self)
    }

    /// Returns a zero-copy canonical key `(normalized_first_char, rest)` for
    /// equality and ordering comparisons.
    ///
    /// On Windows (`FILE_PATH_USES_DRIVE_LETTERS`),
    /// `base::FilePath::operator==` and `base::FilePath::operator<=>` compare
    /// a leading `[A-Za-z]:` drive letter case-insensitively while comparing
    /// the remainder of the path case-sensitively (see `FindDriveLetter`,
    /// `CompareDriveLetterCaseInsensitive`, and
    /// `EqualDriveLetterCaseInsensitive` in `base/files/file_path.cc`).
    ///
    /// Splitting the borrowed UTF-16 slice into `(Option<u16>, &[u16])` lets us
    /// lowercase a leading drive letter by value without mutating or copying
    /// the underlying C++ `std::wstring` buffer: empty paths return
    /// `(None, &[])` (which sorts before any `(Some(_), _)` because
    /// `None < Some(_)` in Rust), while non-empty paths compare the
    /// (potentially lowercased) first code unit first and, if equal, compare
    /// the borrowed `rest` sub-slice (`&slice[1..]`).
    #[cfg(windows)]
    fn comparison_key(&self) -> (Option<u16>, &[u16]) {
        let slice = ffi::FilePathToWide(self);
        let Some((&first, rest)) = slice.split_first() else {
            return (None, &[]);
        };
        // `u16` does not have ASCII helper methods in Rust's stdlib, so narrow
        // `first` to `u8` (`0..=255`); any code unit > 255 returns `Err` and
        // cannot be an ASCII drive letter.
        let normalized_first = match u8::try_from(first) {
            Ok(first_byte)
                if rest.first() == Some(&u16::from(b':')) && first_byte.is_ascii_alphabetic() =>
            {
                u16::from(first_byte.to_ascii_lowercase())
            }
            _ => first,
        };
        (Some(normalized_first), rest)
    }
}

// Equality and ordering compare `FilePath::value()` via `comparison_key()`,
// which on Windows case-folds a leading `[A-Za-z]:` drive letter to match
// C++ `base::FilePath::operator==` and `base::FilePath::operator<=>`.
impl PartialEq for ffi::FilePath {
    fn eq(&self, other: &Self) -> bool {
        self.comparison_key() == other.comparison_key()
    }
}
impl Eq for ffi::FilePath {}

impl PartialOrd for ffi::FilePath {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for ffi::FilePath {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        Ord::cmp(&self.comparison_key(), &other.comparison_key())
    }
}

impl std::fmt::Debug for ffi::FilePath {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_tuple("FilePath").field(&self.to_path_buf()).finish()
    }
}
