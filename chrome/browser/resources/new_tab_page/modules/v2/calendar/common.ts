// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

// Microseconds between windows and unix epoch.
const kWindowsToUnixEpochOffset: bigint = 11644473600000000n;

export function toJsTimestamp(time: Time): number {
  return Number((time.internalValue - kWindowsToUnixEpochOffset) / 1000n);
}
