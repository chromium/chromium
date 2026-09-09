// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabpersistence;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.TabState;

import java.nio.ByteBuffer;

/** Interface for serializing and deserializing {@link TabState} */
@NullMarked
public interface TabStateSerializer {

    /**
     * @param tabState {@link TabState} to be serialized
     * @return serialized {@link TabState} in the form of a ByteBuffer
     */
    ByteBuffer serialize(TabState tabState);

    /**
     * @param byteBuffer serialized {@link TabState}
     * @return deserialized {@link TabState} or null if it failed.
     */
    @Nullable TabState deserialize(ByteBuffer byteBuffer);
}
