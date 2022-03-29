// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use autocxx::include_cpp;
use cxx::{CxxString, UniquePtr};

include_cpp! {
    #include "base/cpu.h"
    #include "url/origin.h"
    safety!(unsafe_ffi) // see https://google.github.io/autocxx/safety.html
    generate!("base::CPU")
    generate!("url::Origin")
}

use ffi::*;

pub fn serialize_url(scheme: &str, host: &str, port: u16) -> String {
    let o: UniquePtr<url::Origin> = url::Origin::CreateFromNormalizedTuple(scheme, host, port);
    let serialized: UniquePtr<CxxString> = o.Serialize();
    serialized.to_str().unwrap().to_string()
}

pub fn get_cpu_vendor() -> String {
    let cpu: UniquePtr<base::CPU> = base::CPU::make_unique();
    cpu.vendor_name().to_string_lossy().to_string()
}

#[test]
fn test_get_cpu_vendor() {
    assert!(!get_cpu_vendor().is_empty())
}
