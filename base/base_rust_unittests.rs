// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Crate root for all base Rust unit tests.

#[path = "task/sequenced_task_runner_unittest.rs"]
mod sequenced_task_runner_unittest;

#[path = "files/file_path_unittest.rs"]
mod file_path_unittest;
