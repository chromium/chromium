// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;
use std::path::Path;

#[gtest(FilePathRustTest, BasicConversions)]
fn test_basic_conversions() {
    let test_path = Path::new("/foo/bar/baz.txt");
    let file_path = base_file_path::ffi::FilePath::from_path(test_path);

    let roundtrip_path = file_path.to_path_buf();
    expect_eq!(test_path, roundtrip_path);
}

#[gtest(FilePathRustTest, NonUtf8Unix)]
#[cfg(unix)]
fn test_non_utf8_unix() {
    use std::os::unix::ffi::OsStrExt;

    // Create a path with invalid UTF-8 bytes (e.g. 0xff)
    let non_utf8_bytes = b"/foo/bar/\xff\xfe.txt";
    let os_str = std::ffi::OsStr::from_bytes(non_utf8_bytes);
    let test_path = Path::new(os_str);

    let file_path = base_file_path::ffi::FilePath::from_path(test_path);
    let roundtrip_path = file_path.to_path_buf();

    expect_eq!(test_path, roundtrip_path);
    expect_eq!(roundtrip_path.as_os_str().as_bytes(), non_utf8_bytes);
}

#[gtest(FilePathRustTest, ZeroCopyUnix)]
#[cfg(unix)]
fn test_zero_copy_unix() {
    use std::os::unix::ffi::OsStrExt;

    let non_utf8_bytes = b"/another/path/\xf0\xf1\xf2.bin";
    let os_str = std::ffi::OsStr::from_bytes(non_utf8_bytes);
    let test_path = Path::new(os_str);

    let file_path = base_file_path::ffi::FilePath::from_path(test_path);

    // Verify zero-copy path borrows match
    expect_eq!(file_path.as_os_str(), os_str);
    expect_eq!(file_path.as_path(), test_path);

    // Ensure it is exactly the same byte representation
    expect_eq!(file_path.as_path().as_os_str().as_bytes(), non_utf8_bytes);
}

#[gtest(FilePathRustTest, WideWindows)]
#[cfg(windows)]
fn test_wide_windows() {
    use std::os::windows::ffi::OsStrExt;

    // Create a Windows path with non-ASCII (wide) characters
    let test_path = Path::new(r"C:\foo\bar\测试_emoji_🚀.txt");

    let file_path = base_file_path::ffi::FilePath::from_path(test_path);
    let roundtrip_path = file_path.to_path_buf();

    expect_eq!(test_path, roundtrip_path);

    // Also verify the UTF-16 wide encoding matches exactly
    let original_wide: Vec<u16> = test_path.as_os_str().encode_wide().collect();
    let roundtrip_wide: Vec<u16> = roundtrip_path.as_os_str().encode_wide().collect();
    expect_eq!(original_wide, roundtrip_wide);
}
