// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import java.nio.ByteBuffer;

/**
 * @param <T> a {@link PersistedTabDataResult}
 * Maps a {@link ByteBuffer} to a {@link PersistedTabDataResult}
 */
public interface PersistedTabDataMapper<T extends PersistedTabDataResult> {
    /**
     * @param byteBuffer serialized {@link PersistedTabData} result
     * @return the mapped {@link PersistedTabDataResult}
     */
    T map(ByteBuffer byteBuffer);
}
