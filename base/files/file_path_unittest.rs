// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base:file_path";
}

use rust_gtest_interop::prelude::*;
use std::path::Path;

#[gtest(FilePathRustTest, BasicConversions)]
fn test_basic_conversions() {
    let test_path = Path::new("/foo/bar/baz.txt");
    let file_path = file_path::ffi::FilePath::from_path(test_path);

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

    let file_path = file_path::ffi::FilePath::from_path(test_path);
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

    let file_path = file_path::ffi::FilePath::from_path(test_path);

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

    let file_path = file_path::ffi::FilePath::from_path(test_path);
    let roundtrip_path = file_path.to_path_buf();

    expect_eq!(test_path, roundtrip_path);

    // Also verify the UTF-16 wide encoding matches exactly
    let original_wide: Vec<u16> = test_path.as_os_str().encode_wide().collect();
    let roundtrip_wide: Vec<u16> = roundtrip_path.as_os_str().encode_wide().collect();
    expect_eq!(original_wide, roundtrip_wide);
}

#[gtest(FilePathRustTest, EqualityAndOrdering)]
fn test_equality_and_ordering() {
    use file_path::ffi::FilePath;
    use std::collections::BTreeSet;

    let path_a1 = FilePath::from_path(Path::new("/foo/bar"));
    let path_a2 = FilePath::from_path(Path::new("/foo/bar"));
    let path_with_slash = FilePath::from_path(Path::new("/foo/bar/"));
    let path_b = FilePath::from_path(Path::new("/foo/baz"));

    expect_eq!(format!("{path_a1:?}"), format!("FilePath({:?})", Path::new("/foo/bar")));

    expect_eq!(*path_a1, *path_a2);
    expect_eq!(path_a1.cmp(&path_a2), std::cmp::Ordering::Equal);
    // Raw FilePath comparison compares the underlying string directly (unlike
    // std::path::Path, which ignores trailing separators).
    expect_ne!(*path_a1, *path_with_slash);
    expect_lt!(*path_a1, *path_with_slash);
    expect_lt!(*path_a1, *path_b);

    let mut set = BTreeSet::new();
    set.insert(path_a1);
    expect_true!(set.contains(&path_a2));
    expect_false!(set.contains(&path_with_slash));
    expect_false!(set.contains(&path_b));

    #[cfg(windows)]
    {
        let upper_drive = FilePath::from_path(Path::new(r"C:\foo"));
        let lower_drive = FilePath::from_path(Path::new(r"c:\foo"));
        let diff_case_rest = FilePath::from_path(Path::new(r"c:\Foo"));
        let d_drive = FilePath::from_path(Path::new(r"D:\foo"));

        // Leading drive letters are compared case-insensitively in both Eq and
        // Ord, while the remainder of the path is compared case-sensitively.
        expect_eq!(*upper_drive, *lower_drive);
        expect_eq!(upper_drive.cmp(&lower_drive), std::cmp::Ordering::Equal);
        expect_ne!(*lower_drive, *diff_case_rest);
        expect_lt!(*upper_drive, *d_drive);
        expect_lt!(*lower_drive, *d_drive);
        expect_lt!(*diff_case_rest, *upper_drive);

        let mut win_set = BTreeSet::new();
        win_set.insert(upper_drive);
        expect_true!(win_set.contains(&lower_drive));
        expect_false!(win_set.contains(&diff_case_rest));
    }
}

#[gtest(FilePathRustTest, StripTrailingSeparators)]
fn test_strip_trailing_separators() {
    use file_path::ffi::FilePath;

    let path_no_slash = FilePath::from_path(Path::new("/foo/bar"));
    let path_with_slash = FilePath::from_path(Path::new("/foo/bar/"));
    let path_with_multi_slash = FilePath::from_path(Path::new("/foo/bar///"));

    expect_eq!(*path_no_slash, *path_no_slash.strip_trailing_separators());
    expect_eq!(*path_no_slash, *path_with_slash.strip_trailing_separators());
    expect_eq!(*path_no_slash, *path_with_multi_slash.strip_trailing_separators());

    // Stripping root paths preserves a single separator.
    let root = FilePath::from_path(Path::new("/"));
    let multi_root = FilePath::from_path(Path::new("////"));
    expect_eq!(*root, *multi_root.strip_trailing_separators());
}
