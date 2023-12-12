// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Additional chrome types used by the media app that are not yet
 * available in the TypeScript toolchain.
 */

interface LaunchParams {
  readonly files: readonly FileSystemHandle[];
}

interface LaunchQueue {
  setConsumer(consumer: (params: LaunchParams) => void): void;
}

interface Window {
  readonly launchQueue: LaunchQueue;
}
