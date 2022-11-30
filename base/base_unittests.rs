// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1298039): A buildflag macro should be used instead.
#[cfg(buildflag__build_rust_json_parser)]
#[path = "values_unittest.rs"]
mod values_unittest;

// TODO(crbug.com/1298039): A buildflag macro should be used instead.
#[cfg(buildflag__build_rust_json_parser)]
#[path = "json/json_parser_unittest.rs"]
mod json_parser_unittest;
