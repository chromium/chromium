// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.services;

import org.chromium.android_webview.common.services.IVariationsSeedServerCallback;

oneway interface IVariationsSeedServer {
    // Apps request variations seeds from WebView's service by calling this interface. They should
    // pass the "date" field of their current seed as oldSeedDate (in milliseconds since epoch), or
    // Long.MIN_VALUE to indicate they have no seed. If the service's latest seed is newer than
    // oldSeedDate, the service will write it to newSeedFile. callback may be used to pass
    // information back to the embedding app from the service.
    void getSeed(in ParcelFileDescriptor newSeedFile, in long oldSeedDate, in IVariationsSeedServerCallback callback);
}
