// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;

import java.nio.ByteBuffer;

/**
 * MockPersistedTabData object used for testing
 */
public class MockPersistedTabData extends PersistedTabData {
    private int mField;

    /**
     * @param tab   tab associated with {@link MockPersistedTabData}
     * @param field field stored in {@link MockPersistedTabData}
     */
    public MockPersistedTabData(Tab tab, int field) {
        super(tab,
                PersistedTabDataConfiguration.get(MockPersistedTabData.class, tab.isIncognito())
                        .getStorage(),
                PersistedTabDataConfiguration.get(MockPersistedTabData.class, tab.isIncognito())
                        .getId());
        mField = field;
    }

    private MockPersistedTabData(Tab tab, byte[] data, PersistedTabDataStorage storage, String id) {
        super(tab, storage, id);
        deserializeAndLog(data);
    }

    /**
     * Acquire {@link MockPersistedTabData} from storage or create it and
     * associate with {@link Tab}
     * @param tab      {@link Tab} {@link MockPersistedTabData} will be associated with
     * @param callback callback {@link MockPersistedTabData} will be passed back in
     */
    public static void from(Tab tab, Callback<MockPersistedTabData> callback) {
        PersistedTabData.from(tab, (data, storage, id) -> {
            return new MockPersistedTabData(tab, data, storage, id);
        }, null, MockPersistedTabData.class, callback);
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
    public Supplier<byte[]> getSerializeSupplier() {
        ByteBuffer byteBuffer = ByteBuffer.allocate(4).putInt(mField);
        return () -> {
            return byteBuffer.array();
        };
    }

    @Override
    public boolean deserialize(byte[] data) {
        mField = ByteBuffer.wrap(data).getInt();
        return true;
    }

    @Override
    public String getUmaTag() {
        return "MockCritical";
    }
}
