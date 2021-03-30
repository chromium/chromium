// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplierImpl;
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
    private static final long NEEDS_UPDATE_DISABLED = Long.MAX_VALUE;
    private static final long LAST_UPDATE_UNKNOWN = 0;
    protected final Tab mTab;
    private final PersistedTabDataStorage mPersistedTabDataStorage;
    private final String mPersistedTabDataId;
    private long mLastUpdatedMs = LAST_UPDATE_UNKNOWN;
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public ObservableSupplierImpl<Boolean> mIsTabSaveEnabledSupplier;
    private Callback<Boolean> mTabSaveEnabledToggleCallback;
    private boolean mFirstSaveDone;

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
        T persistedTabData = factory.create(data, config.getStorage(), config.getId());
        if (persistedTabData != null) {
            setUserData(tab, clazz, persistedTabData);
        }
        return persistedTabData;
    }

    /**
     * Asynchronously acquire a {@link PersistedTabData}
     * for a {@link Tab}
     * @param tab {@link Tab} {@link PersistedTabData} is being acquired for.
     * At a minimum, a frozen tab with an identifier and isIncognito fields set
     * is required.
     * @param factory {@link PersistedTabDataFactory} which will create {@link PersistedTabData}
     * @param tabDataCreator for constructing a {@link PersistedTabData} corresponding to the passed
     * in tab. This will be used as a fallback in the event that the {@link PersistedTabData} cannot
     * be found in storage or needs an update.
     * @param clazz class of the {@link PersistedTabData}
     * @param callback callback to pass the {@link PersistedTabData} in
     * @return {@link PersistedTabData} from storage
     */
    protected static <T extends PersistedTabData> void from(Tab tab,
            PersistedTabDataFactory<T> factory, Callback<Callback<T>> tabDataCreator,
            Class<T> clazz, Callback<T> callback) {
        ThreadUtils.assertOnUiThread();
        // TODO(crbug.com/1059602) cache callbacks
        T persistedTabDataFromTab = getUserData(tab, clazz);
        if (persistedTabDataFromTab != null) {
            if (persistedTabDataFromTab.needsUpdate()) {
                tabDataCreator.onResult((tabData) -> {
                    updateLastUpdatedMs(tabData);
                    if (tabData != null) {
                        setUserData(tab, clazz, tabData);
                    }
                    PostTask.runOrPostTask(
                            UiThreadTaskTraits.DEFAULT, () -> { callback.onResult(tabData); });
                });
            } else {
                PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                        () -> { callback.onResult(persistedTabDataFromTab); });
            }
            return;
        }
        String key = String.format(Locale.ENGLISH, "%d-%s", tab.getId(), clazz.toString());
        addCallback(key, callback);
        // Only load data for the same key once
        if (sCachedCallbacks.get(key).size() > 1) return;
        PersistedTabDataConfiguration config =
                PersistedTabDataConfiguration.get(clazz, tab.isIncognito());
        config.getStorage().restore(tab.getId(), config.getId(), (data) -> {
            if (data == null) {
                tabDataCreator.onResult((tabData) -> {
                    updateLastUpdatedMs(tabData);
                    onPersistedTabDataResult(tabData, tab, clazz, key);
                });
            } else {
                T persistedTabDataFromStorage =
                        factory.create(data, config.getStorage(), config.getId());
                if (persistedTabDataFromStorage.needsUpdate()) {
                    tabDataCreator.onResult((tabData) -> {
                        updateLastUpdatedMs(tabData);
                        onPersistedTabDataResult(tabData, tab, clazz, key);
                    });
                } else {
                    onPersistedTabDataResult(persistedTabDataFromStorage, tab, clazz, key);
                }
            }
        });
    }

    private static void updateLastUpdatedMs(PersistedTabData persistedTabData) {
        if (persistedTabData != null) {
            persistedTabData.setLastUpdatedMs(System.currentTimeMillis());
        }
    }

    /**
     * @return if the {@link PersistedTabData} should be refetched.
     */
    protected boolean needsUpdate() {
        if (getTimeToLiveMs() == NEEDS_UPDATE_DISABLED) {
            return false;
        }
        if (mLastUpdatedMs == LAST_UPDATE_UNKNOWN) {
            return true;
        }
        return mLastUpdatedMs + getTimeToLiveMs() < System.currentTimeMillis();
    }

    private static <T extends PersistedTabData> void onPersistedTabDataResult(
            T persistedTabData, Tab tab, Class<T> clazz, String key) {
        if (persistedTabData != null) {
            setUserData(tab, clazz, persistedTabData);
        }
        for (Callback cachedCallback : sCachedCallbacks.get(key)) {
            cachedCallback.onResult(persistedTabData);
        }
        sCachedCallbacks.remove(key);
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
        T persistedTabData = from(tab, userDataKey);
        if (persistedTabData == null) {
            persistedTabData = tab.getUserDataHost().setUserData(userDataKey, supplier.get());
        }
        return persistedTabData;
    }

    /**
     * Acquire {@link PersistedTabData} from a {@link Tab} using a {@link UserData} key
     * @param tab the {@link PersistedTabData} will be acquired from
     * @param userDataKey the {@link UserData} object to be acquired from the {@link Tab}
     */
    protected static <T extends PersistedTabData> T from(Tab tab, Class<T> userDataKey) {
        return tab.getUserDataHost().getUserData(userDataKey);
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
        if (mIsTabSaveEnabledSupplier != null && mIsTabSaveEnabledSupplier.get()) {
            mPersistedTabDataStorage.save(mTab.getId(), mPersistedTabDataId,
                    getOomAndMetricsWrapper(getSerializeSupplier()));
        }
    }

    /**
     * @return {@link Supplier} for {@link PersistedTabData} in serialized form.
     */
    abstract Supplier<byte[]> getSerializeSupplier();

    @VisibleForTesting
    protected Supplier<byte[]> getOomAndMetricsWrapper(Supplier<byte[]> serializeSupplier) {
        return () -> {
            byte[] res;
            try (TraceEvent e = TraceEvent.scoped("PersistedTabData.Serialize")) {
                res = serializeSupplier.get();
            } catch (OutOfMemoryError oe) {
                Log.e(TAG, "Out of memory error when attempting to save PersistedTabData");
                res = null;
            }
            // TODO(crbug.com/1162293) convert to enum histogram and differentiate null/not null/out
            // of memory
            RecordHistogram.recordBooleanHistogram(
                    "Tabs.PersistedTabData.Serialize." + getUmaTag(), res != null);
            return res;
        };
    }

    /**
     * Deserialize serialized {@link PersistedTabData} and
     * assign to fields in {@link PersistedTabData}
     * @param bytes serialized PersistedTabData
     */
    abstract boolean deserialize(@Nullable byte[] bytes);

    protected void deserializeAndLog(@Nullable byte[] bytes) {
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
    public void destroy() {
        if (mIsTabSaveEnabledSupplier != null) {
            mIsTabSaveEnabledSupplier.removeObserver(mTabSaveEnabledToggleCallback);
            mTabSaveEnabledToggleCallback = null;
        }
    }

    /**
     * @return unique tag for logging in Uma
     */
    public abstract String getUmaTag();

    /**
     * @return length of time before data should be refetched from endpoint
     * The default value is NEEDS_UPDATE_DISABLED (Long.MAX_VALUE) indicating
     * the PersistedTabData will never be refetched. Subclasses can override
     * this value if they need to make use of the time to live functionality.
     */
    public long getTimeToLiveMs() {
        return NEEDS_UPDATE_DISABLED;
    }

    /**
     * Set last time the {@link PersistedTabData} was updated
     * @param lastUpdatedMs time last updated in milliseconds
     */
    protected void setLastUpdatedMs(long lastUpdatedMs) {
        mLastUpdatedMs = lastUpdatedMs;
    }

    /**
     * @return time the {@link PersistedTabDAta} was last updated in milliseconds
     */
    protected long getLastUpdatedMs() {
        return mLastUpdatedMs;
    }

    /**
     * @param isTabSaveEnabledSupplier {@link ObservableSupplierImpl} which provides
     * access to the flag indicating if the {@link Tab} metadata will be saved and
     * forward changes to the flag's value.
     */
    public void registerIsTabSaveEnabledSupplier(
            ObservableSupplierImpl<Boolean> isTabSaveEnabledSupplier) {
        mIsTabSaveEnabledSupplier = isTabSaveEnabledSupplier;
        mTabSaveEnabledToggleCallback = (isTabSaveEnabled) -> {
            if (isTabSaveEnabled) {
                save();
                mFirstSaveDone = true;
            } else if (mFirstSaveDone) {
                delete();
            }
        };
        mIsTabSaveEnabledSupplier.addObserver(mTabSaveEnabledToggleCallback);
    }

    /**
     * Delete all {@link PersistedTabData} when a {@link Tab} is closed.
     */
    public static void onTabClose(Tab tab) {
        tab.setIsTabSaveEnabled(false);
        if (ShoppingPersistedTabData.from(tab) != null) {
            ShoppingPersistedTabData.from(tab).disableSaving();
        }
    }
}
