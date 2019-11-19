// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.services;

oneway interface IVariationsSeedServer {
    // Apps request variations seeds from WebView's service by calling this interface. They should
    // pass the "date" field of their current seed as oldSeedDate (in milliseconds since epoch), or
    // Long.MIN_VALUE to indicate they have no seed. If the service's latest seed is newer than
    // oldSeedDate, the service will write it to newSeedFile.
    void getSeed(in ParcelFileDescriptor newSeedFile, in long oldSeedDate);
}
