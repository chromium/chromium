// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1298039): A buildflag macro should be used instead.
#[cfg(buildflag__build_rust_json_parser)]
mod json;
// TODO(crbug.com/1298039): A buildflag macro should be used instead.
#[cfg(buildflag__build_rust_json_parser)]
mod rs_glue;
// TODO(crbug.com/1298039): A buildflag macro should be used instead.
#[cfg(buildflag__build_rust_json_parser)]
mod values;
// TODO(crbug.com/1298039): A buildflag macro should be used instead.
#[cfg(buildflag__build_rust_json_parser)]
mod values_deserialization;

// TODO(crbug.com/1298039): A buildflag macro should be used instead.
#[cfg(buildflag__build_rust_json_parser)]
pub use json::json_parser::{decode_json, JsonOptions};
// TODO(crbug.com/1298039): A buildflag macro should be used instead.
#[cfg(buildflag__build_rust_json_parser)]
pub use values::ValueSlotRef;

// TODO(crbug.com/1298039): A buildflag macro should be used instead.
#[cfg(buildflag__build_rust_json_parser)]
pub use rs_glue::ffi::NewValueSlotForTesting;
