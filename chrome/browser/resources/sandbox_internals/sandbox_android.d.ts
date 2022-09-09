// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.getAndroidSandboxStatus */

declare namespace chrome {
  interface AndroidSandboxStatus {
    androidBuildId: string;
    pid: string;
    procStatus: string;
    seccompStatus: number;
    secontext: string;
    uid: string;
  }

  type GetAndroidStatusCallback = (status: AndroidSandboxStatus) => void;

  // This function is only exposed to the Android chrome://sandbox webui.
  function getAndroidSandboxStatus(callback: GetAndroidStatusCallback): void;
}
