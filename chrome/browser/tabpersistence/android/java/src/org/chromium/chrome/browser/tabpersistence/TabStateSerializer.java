// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabpersistence;

import org.chromium.chrome.browser.tab.TabState;

import java.nio.ByteBuffer;

/** Interface for serializing and deserializing {@link TabState} */
public interface TabStateSerializer {

    /**
     * @param tabState {@link TabState} to be serializsed
     * @return serialized {@link TabState} in the form of a ByteBuffer
     */
    ByteBuffer serialize(TabState tabState);

    /**
     * @param byteBuffer serialized {@link TabState}
     * @return deserialized {@link TabState}
     */
    TabState deserialize(ByteBuffer byteBuffer);
}
