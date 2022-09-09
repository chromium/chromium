// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Script that imports all necessary scripts to run a test.
importScripts(
  'testharness.js',
  // Needed for using Mojo bindings.
  'mojo_bindings_lite.js',
  'system_extensions_test_runner.test-mojom-lite.js',
  // Registers callback that notify the browser of test harness events, e.g.
  // when tests finish running.
  'testharnessreport.js');
