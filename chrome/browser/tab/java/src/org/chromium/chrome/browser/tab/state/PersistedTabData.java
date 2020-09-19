// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * PersistedTabData is Tab data persisted across restarts
 * A constructor of taking a Tab and a byte[] (serialized
 * {@link PersistedTabData}, PersistedTabDataStorage and
 * PersistedTabDataID (identifier for {@link PersistedTabData}
 * in storage) is required as reflection is used to build
 * the object after acquiring the serialized object from storage.
 */
public abstract class PersistedTabData implements UserData {
    private static final String TAG = "PTD";
    private static final Map<String, List<Callback>> sCachedCallbacks = new HashMap<>();
    protected final Tab mTab;
    private final PersistedTabDataStorage mPersistedTabDataStorage;
    private final String mPersistedTabDataId;

    /**
     * @param tab {@link Tab} {@link PersistedTabData} is being stored for
     * @param data serialized {@link Tab} metadata
     * @param persistedTabDataStorage storage for {@link PersistedTabData}
     * @param persistedTabDataId identifier for {@link PersistedTabData} in storage
     */
    PersistedTabData(Tab tab, byte[] data, PersistedTabDataStorage persistedTabDataStorage,
            String persistedTabDataId) {
        this(tab, persistedTabDataStorage, persistedTabDataId);
        deserializeAndLog(data);
    }

    /**
     * @param tab {@link Tab} {@link PersistedTabData} is being stored for
     * @param persistedTabDataStorage storage for {@link PersistedTabData}
     * @param persistedTabDataId identifier for {@link PersistedTabData} in storage
     */
    PersistedTabData(
            Tab tab, PersistedTabDataStorage persistedTabDataStorage, String persistedTabDataId) {
        mTab = tab;
        mPersistedTabDataStorage = persistedTabDataStorage;
        mPersistedTabDataId = persistedTabDataId;
    }

    /**
     * Build {@link PersistedTabData} from serialized form
     * @param tab associated with {@link PersistedTabData}
     * @param factory method for creating {@link PersistedTabData}
     * @param data serialized {@link PersistedTabData}
     * @param clazz {@link PersistedTabData} class
     * @return deserialized {@link PersistedTabData}
     */
    protected static <T extends PersistedTabData> T build(
            Tab tab, PersistedTabDataFactory<T> factory, byte[] data, Class<T> clazz) {
        PersistedTabDataConfiguration config =
                PersistedTabDataConfiguration.get(clazz, tab.isIncognito());
        T persistedTabData = factory.create(data, config.storage, config.id);
        setUserData(tab, clazz, persistedTabData);
        return persistedTabData;
    }

    /**
     * Asynchronously acquire a {@link PersistedTabData}
     * for a {@link Tab}
     * @param tab {@link Tab} {@link PersistedTabData} is being acquired for.
     * At a minimum, a frozen tab with an identifier and isIncognito fields set
     * is required.
     * @param factory {@link PersistedTabDataFactory} which will create {@link PersistedTabData}
     * @param supplier for constructing a {@link PersistedTabData} from a
     * {@link Tab}. This will be used as a fallback in the event that the {@link PersistedTabData}
     * cannot be found in storage.
     * @param clazz class of the {@link PersistedTabData}
     * @param callback callback to pass the {@link PersistedTabData} in
     * @return {@link PersistedTabData} from storage
     */
    protected static <T extends PersistedTabData> void from(Tab tab,
            PersistedTabDataFactory<T> factory, Supplier<T> supplier, Class<T> clazz,
            Callback<T> callback) {
        ThreadUtils.assertOnUiThread();
        // TODO(crbug.com/1059602) cache callbacks
        T persistedTabDataFromTab = getUserData(tab, clazz);
        if (persistedTabDataFromTab != null) {
            PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                    () -> { callback.onResult(persistedTabDataFromTab); });
            return;
        }
        String key = String.format(Locale.ENGLISH, "%d-%s", tab.getId(), clazz.toString());
        addCallback(key, callback);
        // Only load data for the same key once
        if (sCachedCallbacks.get(key).size() > 1) return;
        PersistedTabDataConfiguration config =
                PersistedTabDataConfiguration.get(clazz, tab.isIncognito());
        config.storage.restore(tab.getId(), config.id, (data) -> {
            T persistedTabData;
            if (data == null) {
                persistedTabData = supplier.get();
            } else {
                persistedTabData = factory.create(data, config.storage, config.id);
            }
            if (persistedTabData != null) {
                setUserData(tab, clazz, persistedTabData);
            }
            for (Callback cachedCallback : sCachedCallbacks.get(key)) {
                cachedCallback.onResult(persistedTabData);
            }
            sCachedCallbacks.remove(key);
        });
    }

    /**
     * Acquire {@link PersistedTabData} from a {@link Tab} or create and
     * associate using provided {@link Supplier}
     * @param corresponding {@link Tab} for which {@link PersistedTabData} is
     * desired
     * @param  userDataKey derived {@link PersistedTabData} class corresponding
     * to desired {@link PersistedTabData}
     * @param  supplier means of building {@link PersistedTabData} if it doesn't
     * exist on the {@link Tab}
     */
    protected static <T extends PersistedTabData> T from(
            Tab tab, Class<T> userDataKey, Supplier<T> supplier) {
        UserDataHost host = tab.getUserDataHost();
        T persistedTabData = host.getUserData(userDataKey);
        if (persistedTabData == null) {
            persistedTabData = host.setUserData(userDataKey, supplier.get());
        }
        return persistedTabData;
    }

    private static <T extends PersistedTabData> void addCallback(String key, Callback<T> callback) {
        if (!sCachedCallbacks.containsKey(key)) {
            sCachedCallbacks.put(key, new LinkedList<>());
        }
        sCachedCallbacks.get(key).add(callback);
    }

    /**
     * Acquire {@link PersistedTabData} stored in {@link UserData} on a {@link Tab}
     * @param tab the {@link Tab}
     * @param clazz {@link PersistedTabData} class
     */
    private static <T extends PersistedTabData> T getUserData(Tab tab, Class<T> clazz) {
        return clazz.cast(tab.getUserDataHost().getUserData(clazz));
    }

    /**
     * Set {@link PersistedTabData} on a {@link Tab} object using {@link UserDataHost}
     * @param tab the {@link Tab}
     * @param clazz {@link PersistedTabData} class - they key for {@link UserDataHost}
     * @param persistedTabData {@link PersistedTabData} stored on the {@link UserDataHost}
     * associated with the {@link Tab}
     */
    private static <T extends PersistedTabData> T setUserData(
            Tab tab, Class<T> clazz, T persistedTabData) {
        return tab.getUserDataHost().setUserData(clazz, persistedTabData);
    }

    /**
     * Save {@link PersistedTabData} to storage
     * @param callback callback indicating success/failure
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    protected void save() {
        mPersistedTabDataStorage.save(mTab.getId(), mPersistedTabDataId, serializeAndLog());
    }

    /**
     * @return {@link PersistedTabData} in serialized form.
     */
    abstract byte[] serialize();

    private byte[] serializeAndLog() {
        byte[] res;
        try (TraceEvent e = TraceEvent.scoped("PersistedTabData.Serialize")) {
            res = serialize();
        }
        RecordHistogram.recordBooleanHistogram(
                "Tabs.PersistedTabData.Serialize." + getUmaTag(), res != null);
        return res;
    }

    /**
     * Deserialize serialized {@link PersistedTabData} and
     * assign to fields in {@link PersistedTabData}
     * @param bytes serialized PersistedTabData
     */
    abstract boolean deserialize(@Nullable byte[] bytes);

    private void deserializeAndLog(@Nullable byte[] bytes) {
        boolean success;
        try (TraceEvent e = TraceEvent.scoped("PersistedTabData.Deserialize")) {
            success = deserialize(bytes);
        }
        RecordHistogram.recordBooleanHistogram(
                "Tabs.PersistedTabData.Deserialize." + getUmaTag(), success);
    }

    /**
     * Delete {@link PersistedTabData} for a tab id
     * @param tabId tab identifier
     * @param profile profile
     */
    protected void delete() {
        mPersistedTabDataStorage.delete(mTab.getId(), mPersistedTabDataId);
    }

    /**
     * Destroy the object. This will clean up any {@link PersistedTabData}
     * in memory. It will not delete the stored data on a file or database.
     */
    @Override
    public abstract void destroy();

    /**
     * @return unique tag for logging in Uma
     */
    public abstract String getUmaTag();
}
