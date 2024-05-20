// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabpersistence;

import org.chromium.chrome.browser.tab.TabState;

import java.nio.ByteBuffer;

/** Interface for serializing and deserializing {@link TabState} */
public interface TabStateSerializer {

    /**
     * @param tabState {@link TabState} to be serialized
     * @param contentsStateBytes copy of the {@link
     *     org.chromium.chrome.browser.tab.WebContentsState} bytes. WebContentsState should not be
     *     written to the file directly because it could be memory mapped from the same file.
     * @return serialized {@link TabState} in the form of a ByteBuffer
     */
    ByteBuffer serialize(TabState tabState, byte[] contentsStateBytes);

    /**
     * @param byteBuffer serialized {@link TabState}
     * @return deserialized {@link TabState}
     */
    TabState deserialize(ByteBuffer byteBuffer);
}
