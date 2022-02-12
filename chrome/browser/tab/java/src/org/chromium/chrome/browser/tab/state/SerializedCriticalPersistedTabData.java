// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.VisibleForTesting;

import java.nio.ByteBuffer;

/**
 * Contains serialized {@link CriticalPersistedTabData}
 */
public class SerializedCriticalPersistedTabData {
    private final ByteBuffer mByteBuffer;

    /**
     * @param byteBuffer the {@link ByteBuffer} containing serialized {@link
     *         CriticalPersistedTabData}
     */
    protected SerializedCriticalPersistedTabData(ByteBuffer byteBuffer) {
        mByteBuffer = byteBuffer;
    }

    /**
     * @return true if the {@link SerializedCriticalPersistedTabData} is empty.
     */
    public boolean isEmpty() {
        return mByteBuffer == null || mByteBuffer.limit() == 0;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public ByteBuffer getByteBuffer() {
        return mByteBuffer;
    }
}
