// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Crate root for all base Rust unit tests.

#[path = "task/sequenced_task_runner_unittest.rs"]
mod sequenced_task_runner_unittest;

#[path = "task/thread_lock_unittest.rs"]
mod thread_lock_unittest;

#[path = "files/file_path_unittest.rs"]
mod file_path_unittest;

#[path = "command_line_unittest.rs"]
mod command_line_unittest;

#[path = "debug/dump_without_crashing_unittest.rs"]
mod dump_without_crashing_unittest;

#[path = "location_unittest.rs"]
mod location_unittest;

#[path = "metrics/histogram_unittest.rs"]
mod histogram_unittest;
