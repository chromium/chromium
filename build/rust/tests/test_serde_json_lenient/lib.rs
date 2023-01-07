// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Demo library to ensure that serde_json_lenient is working independently of
// its integration with Chromium.

use serde_json_lenient::{Result, Value};

#[cxx::bridge]
mod ffi {
    extern "Rust" {
        fn serde_works() -> bool;
    }
}

fn serde_works() -> bool {
    parses_ok().unwrap_or_default()
}

fn parses_ok() -> Result<bool> {
    let data = r#"
        {
            "name": "Slartibartfast",
            "planets": [ "Magrathea" ]
        }"#;
    let v: Value = serde_json_lenient::from_str(data)?;
    Ok(v["name"] == "Slartibartfast" && v["planets"][0] == "Magrathea")
}
