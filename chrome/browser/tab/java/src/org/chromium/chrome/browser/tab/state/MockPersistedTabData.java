// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Callback;
import org.chromium.build.annotations.DoNotClassMerge;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.NullUnmarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;

import java.nio.ByteBuffer;

/**
 * MockPersistedTabData object used for testing
 *
 * This class should not be merged because it is being used as a key in a Map
 * in PersistedTabDataConfiguration.java.
 */
@DoNotClassMerge
@NullMarked
public class MockPersistedTabData extends PersistedTabData {
    private int mField;

    /**
     * @param tab   tab associated with {@link MockPersistedTabData}
     * @param field field stored in {@link MockPersistedTabData}
     */
    public MockPersistedTabData(Tab tab, int field) {
        super(
                tab,
                PersistedTabDataConfiguration.get(MockPersistedTabData.class, tab.isIncognito())
                        .getStorage(),
                PersistedTabDataConfiguration.get(MockPersistedTabData.class, tab.isIncognito())
                        .getId());
        mField = field;
    }

    private MockPersistedTabData(
            Tab tab, ByteBuffer data, PersistedTabDataStorage storage, String id) {
        super(tab, storage, id);
        deserializeAndLog(data);
    }

    /**
     * Acquire {@link MockPersistedTabData} from storage or create it and
     * associate with {@link Tab}
     * @param tab      {@link Tab} {@link MockPersistedTabData} will be associated with
     * @param callback callback {@link MockPersistedTabData} will be passed back in
     */
    @NullUnmarked
    public static void from(Tab tab, Callback<@Nullable MockPersistedTabData> callback) {
        PersistedTabData.<@Nullable MockPersistedTabData>from(
                tab,
                (data, storage, id, factoryCallback) -> {
                    factoryCallback.onResult(new MockPersistedTabData(tab, data, storage, id));
                },
                null,
                (Class<@Nullable MockPersistedTabData>) MockPersistedTabData.class,
                callback);
    }

    /**
     * @return field stored in {@link MockPersistedTabData}
     */
    public int getField() {
        return mField;
    }

    /**
     * Sets field
     * @param field new value of field
     */
    public void setField(int field) {
        mField = field;
        save();
    }

    @Override
    public Serializer<ByteBuffer> getSerializer() {
        ByteBuffer byteBuffer = ByteBuffer.allocate(4).putInt(mField);
        byteBuffer.rewind();
        return () -> {
            return byteBuffer;
        };
    }

    @Override
    public boolean deserialize(@Nullable ByteBuffer data) {
        mField = assumeNonNull(data).getInt();
        return true;
    }

    @Override
    public String getUmaTag() {
        return "MockCritical";
    }
}
