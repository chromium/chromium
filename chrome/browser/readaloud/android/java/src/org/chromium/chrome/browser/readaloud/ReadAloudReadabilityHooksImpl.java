// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

/** Empty implementation of ReadAloudReadabilityHooks. */
public class ReadAloudReadabilityHooksImpl implements ReadAloudReadabilityHooks {
    @Override
    boolean isEnabled() {
        return false;
    }

    @Override
    ListenableFuture<byte[]> isPageReadable(byte[] checkSupportedRequest) {
        return Futures.immediateFuture(null);
    }
}
