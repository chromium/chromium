// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.services;

interface ICrashReceiverService {
    void transmitCrashes(in ParcelFileDescriptor[] fileDescriptors, in List crashInfo);
}
