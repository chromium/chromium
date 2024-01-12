// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::env;
use std::io::Write;
use std::path::Path;
use std::process::Command;
use std::str::{self, FromStr};

fn main() {
    println!("cargo:rustc-cfg=build_script_ran");
    let minor = match rustc_minor_version() {
        Some(minor) => minor,
        None => return,
    };

    let target = env::var("TARGET").unwrap();

    if minor >= 34 {
        println!("cargo:rustc-cfg=is_new_rustc");
    } else {
        println!("cargo:rustc-cfg=is_old_rustc");
    }

    if target.contains("android") {
        println!("cargo:rustc-cfg=is_android");
    }
    if target.contains("darwin") {
        println!("cargo:rustc-cfg=is_mac");
    }

    let feature_a_enabled = env::var_os("CARGO_FEATURE_MY_FEATURE_A").is_some();
    if feature_a_enabled {
        println!("cargo:rustc-cfg=has_feature_a");
    }
    let feature_b_enabled = env::var_os("CARGO_FEATURE_MY_FEATURE_B").is_some();
    if feature_b_enabled {
        println!("cargo:rustc-cfg=has_feature_b");
    }

    // Some tests as to whether we're properly emulating various cargo features.
    let cargo_manifest_dir = &env::var_os("CARGO_MANIFEST_DIR").unwrap();
    let manifest_dir_path = Path::new(cargo_manifest_dir);
    assert!(
        !manifest_dir_path.is_absolute(),
        "CARGO_MANIFEST_DIR={} should be relative path for build cache sharing.",
        manifest_dir_path.display()
    );
    assert!(manifest_dir_path.join("build.rs").exists());
    assert!(Path::new("build.rs").exists());
    assert!(Path::new(&env::var_os("OUT_DIR").unwrap()).exists());
    // Confirm the following env var is set, but do not attempt to validate content
    // since the whole point is that it will differ on different platforms.
    env::var_os("CARGO_CFG_TARGET_ARCH").unwrap();

    generate_some_code().unwrap();
}

fn generate_some_code() -> std::io::Result<()> {
    let output_dir = Path::new(&env::var_os("OUT_DIR").unwrap()).join("generated");
    let _ = std::fs::create_dir_all(&output_dir);
    // Test that environment variables from .gn files are passed to build scripts
    let preferred_number = env::var("ENV_VAR_FOR_BUILD_SCRIPT").unwrap();
    let mut file = std::fs::File::create(output_dir.join("generated.rs"))?;
    write!(file, "fn run_some_generated_code() -> u32 {{ {} }}", preferred_number)?;
    Ok(())
}

fn rustc_minor_version() -> Option<u32> {
    let rustc = match env::var_os("RUSTC") {
        Some(rustc) => rustc,
        None => return None,
    };

    let output = match Command::new(rustc).arg("--version").output() {
        Ok(output) => output,
        Err(_) => return None,
    };

    let version = match str::from_utf8(&output.stdout) {
        Ok(version) => version,
        Err(_) => return None,
    };

    let mut pieces = version.split('.');
    if pieces.next() != Some("rustc 1") {
        return None;
    }

    let next = match pieces.next() {
        Some(next) => next,
        None => return None,
    };

    u32::from_str(next).ok()
}
