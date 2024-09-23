// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::ffi::CString;
use windows_sys::Win32::Storage::FileSystem::{OpenFile, OFSTRUCT};

fn main() {
    let filename = CString::new("hi").unwrap();
    let mut out = OFSTRUCT {
        cBytes: 0,
        fFixedDisk: 0,
        nErrCode: 0,
        Reserved1: 0,
        Reserved2: 0,
        szPathName: [0; 128],
    };
    let ustyle: u32 = 0;
    unsafe { OpenFile(filename.as_bytes().as_ptr(), &mut out as *mut OFSTRUCT, ustyle) };
}
