// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

import java.util.Locale;

/**
 * {@link PersistedTabData} for Store websites with opening/closing hours
 */
public class StorePersistedTabData extends PersistedTabData {
    // TODO(crbug.com/1196065) Make endpoint finch configurable
    private static final String ENDPOINT =
            "https://task-management-chrome.sandbox.google.com/tabs/representations?url=%s&locale=en:US";
    private static final String[] SCOPES =
            new String[] {"https://www.googleapis.com/auth/userinfo.email",
                    "https://www.googleapis.com/auth/userinfo.profile"};
    private static final long TIMEOUT_MS = 1000L;
    private static final String HTTPS_METHOD = "GET";
    private static final String CONTENT_TYPE = "application/json; charset=UTF-8";
    private static final String EMPTY_POST_DATA = "";
    private static final String PERSISTED_TAB_DATA_ID = "STPTD";

    private StoreHours mStoreHours;

    /**
     * Constructor for {@link StorePersistedTabData}
     * @param tab {@link Tab} StorePersistedTabData is created for
     * @param data serialized {@link StorePersistedTabData} (this is for when the data is acquired
     *         from storage because it was cached or the app was closed).
     * @param storage {@link PersistedTabDataStorage}
     * @param persistedTabDataId id for {@link StorePersistedTabData}
     */
    protected StorePersistedTabData(
            Tab tab, byte[] data, PersistedTabDataStorage storage, String persistedTabDataId) {
        super(tab, storage, persistedTabDataId);
    }

    /**
     * Acquire {@link StorePersistedTabData} for a {@link Tab}
     * @param tab {@link Tab} StorePersistedTabData is created for
     * @param storage {@link PersistedTabDataStorage}
     * @param persistedTabDataId id for {@link StorePersistedTabData}
     */
    StorePersistedTabData(
            Tab tab, PersistedTabDataStorage persistedTabDataStorage, String persistedTabDataId) {
        super(tab, persistedTabDataStorage, persistedTabDataId);
    }

    /**
     * Acquire {@link StorePersistedTabData} for a {@link Tab}
     * @param tab {@link Tab} StorePersistedTabData is created for
     * @param storeHours {@link StoreHours} Store hours for the website open in this tab
     */
    StorePersistedTabData(Tab tab, StoreHours storeHours) {
        // TODO(crbug.com/1192808) Implement reading of storage implementation and unique identifier
        this(tab, new LevelDBPersistedTabDataStorage(Profile.getLastUsedRegularProfile()),
                PERSISTED_TAB_DATA_ID);
        mStoreHours = storeHours;
    }

    /**
     * StoreHours data type for {@link StorePersistedTabData}
     */
    public static class StoreHours {
        private final int mOpeningTime;
        private final int mClosingTime;

        /**
         * @param openingTime opening time in military minutes 0000 = 12:00 A.M
         * @param closingTime closing time in military minutes 0000 = 12:00 A.M
         */
        public StoreHours(int openingTime, int closingTime) {
            mOpeningTime = openingTime;
            mClosingTime = closingTime;
        }

        @Override
        public String toString() {
            // TODO(crbug.com/1192807) Implement converting of openingTime and closingTime to a
            // String
            return "9:00 AM - 5:00 P.M";
        }
    }

    public StoreHours getStoreHours() {
        assert mStoreHours != null;
        return mStoreHours;
    }

    @Override
    Supplier<byte[]> getSerializeSupplier() {
        return () -> {
            // TODO(crbug.com/1192809) Implement
            return new byte[] {(byte) 4};
        };
    }

    @Override
    boolean deserialize(@Nullable byte[] bytes) {
        // TODO(crbug.com/1192810) Implement
        return true;
    }

    @Override
    public String getUmaTag() {
        return "Store";
    }

    /**
     * Acquire {@link StorePersistedTabData} for a {@link Tab}
     * @param tab {@link Tab} {@link StorePersistedTabData} is acquired for
     * @param callback {@link Callback} {@link StorePersistedTabData is passed back in}
     */
    public static void from(Tab tab, Callback<StorePersistedTabData> callback) {
        PersistedTabData.from(tab,
                (data, storage, id)
                        -> { return new StorePersistedTabData(tab, data, storage, id); },
                (supplierCallback)
                        -> {
                    // TODO(crbug.com/1192835) Integrate with endpoint fetcher and parse response
                    EndpointFetcher.fetchUsingOAuth(
                            (endpointResponse)
                                    -> {
                                supplierCallback.onResult(
                                        new StorePersistedTabData(tab, new StoreHours(1000, 2400)));
                            },
                            Profile.getLastUsedRegularProfile(), PERSISTED_TAB_DATA_ID,
                            String.format(Locale.US, ENDPOINT, tab.getUrlString()), HTTPS_METHOD,
                            CONTENT_TYPE, SCOPES, EMPTY_POST_DATA, TIMEOUT_MS);
                },
                StorePersistedTabData.class, callback);
    }
}