// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.tab.flatbuffer.CriticalPersistedTabDataFlatBuffer;

import java.nio.ByteBuffer;

/**
 * Contains serialized {@link CriticalPersistedTabData}
 * TODO(crbug.com/1294620) rename to CriticalPersistedTabDataResult
 */
public class SerializedCriticalPersistedTabData implements PersistedTabDataResult {
    private final CriticalPersistedTabDataFlatBuffer mCriticalPersistedTabDataFlatBuffer;

    /**
     * @param byteBuffer the {@link ByteBuffer} containing serialized {@link
     *         CriticalPersistedTabData}
     */
    protected SerializedCriticalPersistedTabData(
            CriticalPersistedTabDataFlatBuffer criticalPersistedTabDataFlatBuffer) {
        mCriticalPersistedTabDataFlatBuffer = criticalPersistedTabDataFlatBuffer;
    }

    /**
     * @return true if the {@link SerializedCriticalPersistedTabData} is empty.
     */
    public boolean isEmpty() {
        return mCriticalPersistedTabDataFlatBuffer == null;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    protected CriticalPersistedTabDataFlatBuffer getFlatBuffer() {
        return mCriticalPersistedTabDataFlatBuffer;
    }
}
