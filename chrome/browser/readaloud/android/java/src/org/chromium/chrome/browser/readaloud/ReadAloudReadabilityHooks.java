// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import com.google.common.util.concurrent.ListenableFuture;

/** Interface providing access to ReadAloud page readability checking. */
public interface ReadAloudReadabilityHooks {
    /** Returns true if ReadAloud feature is available. */
    boolean isEnabled();

    /**
     * Checks whether a given page is readable.
     * @param checkSupportedRequest Serialized CheckSupportedRequest proto message.
     * @return Future that returns a serialized CheckSupportedResult proto message, or null if the
     *         check is unavailable.
     */
    ListenableFuture<byte[]> isPageReadable(byte[] checkSupportedRequest);
}
