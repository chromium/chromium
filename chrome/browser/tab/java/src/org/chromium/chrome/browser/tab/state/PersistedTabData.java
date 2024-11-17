// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.NativeMethods;

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
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.tab.Tab;

import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * PersistedTabData is Tab data persisted across restarts
 * A constructor of taking a Tab, a PersistedTabDataStorage and
 * PersistedTabDataID (identifier for {@link PersistedTabData}
 * in storage) is required as reflection is used to build
 * the object after acquiring the serialized object from storage.
 */
public abstract class PersistedTabData implements UserData {
    private static final String TAG = "PTD";
    private static final Map<String, List<Callback>> sCachedCallbacks = new HashMap<>();
    private static final long NEEDS_UPDATE_DISABLED = Long.MAX_VALUE;
    private static final long LAST_UPDATE_UNKNOWN = 0;
    private static Set<Class<? extends PersistedTabData>> sSupportedMaintenanceClasses =
            new HashSet<>();
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
     * @param callback {@link Callback} the {@link PersistedTabData} is passed back in
     */
    protected static <T extends PersistedTabData> void build(
            Tab tab,
            PersistedTabDataFactory<T> factory,
            ByteBuffer data,
            Class<T> clazz,
            Callback<T> callback) {
        PersistedTabDataConfiguration config =
                PersistedTabDataConfiguration.get(clazz, tab.isIncognito());
        factory.create(
                data,
                config.getStorage(),
                config.getId(),
                (persistedTabData) -> {
                    if (persistedTabData != null) {
                        setUserData(tab, clazz, persistedTabData);
                    }
                    callback.onResult(persistedTabData);
                });
    }

    /**
     * Asynchronously acquire a {@link PersistedTabData} for a {@link Tab}
     *
     * @param tab {@link Tab} {@link PersistedTabData} is being acquired for. At a minimum, a frozen
     *     tab with an identifier and isIncognito fields set is required.
     * @param factory {@link PersistedTabDataFactory} which will create {@link PersistedTabData}
     * @param tabDataCreator for constructing a {@link PersistedTabData} corresponding to the passed
     *     in tab. This will be used as a fallback in the event that the {@link PersistedTabData}
     *     cannot be found in storage or needs an update.
     * @param clazz class of the {@link PersistedTabData}
     * @param callback callback to pass the {@link PersistedTabData} in
     */
    protected static <T extends PersistedTabData> void from(
            Tab tab,
            PersistedTabDataFactory<T> factory,
            Callback<Callback<T>> tabDataCreator,
            Class<T> clazz,
            Callback<T> callback) {
        ThreadUtils.assertOnUiThread();
        // TODO(crbug.com/40121680) cache callbacks
        T persistedTabDataFromTab = getUserData(tab, clazz);
        if (persistedTabDataFromTab != null) {
            if (persistedTabDataFromTab.needsUpdate()) {
                tabDataCreator.onResult(
                        (tabData) -> {
                            if (tab.isDestroyed()) {
                                PostTask.postTask(
                                        TaskTraits.UI_DEFAULT,
                                        () -> {
                                            callback.onResult(null);
                                        });
                                return;
                            }
                            updateLastUpdatedMs(tabData);
                            if (tabData != null) {
                                setUserData(tab, clazz, tabData);
                            }
                            PostTask.postTask(
                                    TaskTraits.UI_DEFAULT,
                                    () -> {
                                        callback.onResult(tabData);
                                    });
                        });
            } else {
                PostTask.postTask(
                        TaskTraits.UI_DEFAULT,
                        () -> {
                            callback.onResult(persistedTabDataFromTab);
                        });
            }
            return;
        }
        String key = String.format(Locale.ENGLISH, "%d-%s", tab.getId(), clazz.toString());
        addCallback(key, callback);
        // Only load data for the same key once
        if (sCachedCallbacks.get(key).size() > 1) return;
        PersistedTabDataConfiguration config =
                PersistedTabDataConfiguration.get(clazz, tab.isIncognito());
        config.getStorage()
                .restore(
                        tab.getId(),
                        config.getId(),
                        (data) -> {
                            if (data == null) {
                                tabDataCreator.onResult(
                                        (tabData) -> {
                                            updateLastUpdatedMs(tabData);
                                            onPersistedTabDataResult(tabData, tab, clazz, key);
                                        });
                            } else {
                                onPersistedTabDataRetrieved(
                                        data, config, factory, tabDataCreator, tab, clazz, key);
                            }
                        });
    }

    /**
     * Simpler implementation of |from| where data is client side only (i.e. no service call needed)
     * and data doesn't go stale (i.e. no re-fetch needed if the time to lie expires).
     *
     * @param tab {@link Tab} corresponding to {@link PersistedTabData}
     * @param supplier to provide newly instantiated {@link PersistedTabData} object
     * @param clazz class of {@link PersistedTabData} client
     * @param callback to pass back restored {@link PersistedTabData} in or null if none was found
     * @param <T> {@link PersistedTabData} client
     */
    protected static <T extends PersistedTabData> void from(
            Tab tab, Supplier<T> supplier, Class<T> clazz, Callback<T> callback) {
        ThreadUtils.assertOnUiThread();
        if (!tab.isInitialized() || tab.isDestroyed() || tab.isCustomTab()) {
            onInvalidTab(callback);
            return;
        }
        T userData = getUserData(tab, clazz);
        // {@link PersistedTabData} already attached to {@link Tab}
        if (userData != null) {
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        callback.onResult(userData);
                    });
            return;
        }
        String key = String.format(Locale.ENGLISH, "%d-%s", tab.getId(), clazz);
        addCallback(key, callback);
        // Only load data for the same key once
        if (sCachedCallbacks.get(key).size() > 1) return;
        PersistedTabDataConfiguration config =
                PersistedTabDataConfiguration.get(clazz, tab.isIncognito());
        T persistedTabData = supplier.get();
        config.getStorage()
                .restore(
                        tab.getId(),
                        config.getId(),
                        (data) -> {
                            if (tab.isDestroyed()) {
                                onInvalidTab(callback);
                                return;
                            }
                            // No stored {@link PersistedTabData} found, return null.
                            if (data == null || data.limit() == 0) {
                                PostTask.postTask(
                                        TaskTraits.UI_DEFAULT,
                                        () -> {
                                            onPersistedTabDataResult(
                                                    persistedTabData, tab, clazz, key);
                                        });
                            } else {
                                // stored {@link PersistedTabData} found
                                // deserialize on background thread to reduce risk
                                // of jank.
                                PostTask.postTask(
                                        TaskTraits.USER_BLOCKING_MAY_BLOCK,
                                        () -> {
                                            if (tab.isDestroyed()) {
                                                onInvalidTab(callback);
                                                return;
                                            }
                                            persistedTabData.deserializeAndLog(data);
                                            // Post result back to UI thread.
                                            PostTask.postTask(
                                                    TaskTraits.UI_DEFAULT,
                                                    () -> {
                                                        onPersistedTabDataResult(
                                                                persistedTabData, tab, clazz, key);
                                                    });
                                        });
                            }
                        });
    }

    private static <T extends PersistedTabData> void onInvalidTab(Callback<T> callback) {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    callback.onResult(null);
                });
    }

    private static <T extends PersistedTabData> void onPersistedTabDataRetrieved(
            ByteBuffer data,
            PersistedTabDataConfiguration config,
            PersistedTabDataFactory<T> factory,
            Callback<Callback<T>> tabDataCreator,
            Tab tab,
            Class<T> clazz,
            String key) {
        factory.create(
                data,
                config.getStorage(),
                config.getId(),
                (persistedTabDataFromStorage) -> {
                    if (persistedTabDataFromStorage != null
                            && persistedTabDataFromStorage.needsUpdate()) {
                        tabDataCreator.onResult(
                                (tabData) -> {
                                    updateLastUpdatedMs(tabData);
                                    onPersistedTabDataResult(tabData, tab, clazz, key);
                                });
                    } else {
                        onPersistedTabDataResult(persistedTabDataFromStorage, tab, clazz, key);
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
            T pPersistedTabData, Tab tab, Class<T> clazz, String key) {
        final T persistedTabData = tab.isDestroyed() ? null : pPersistedTabData;
        if (persistedTabData != null) {
            setUserData(tab, clazz, persistedTabData);
        }
        for (Callback cachedCallback : sCachedCallbacks.get(key)) {
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT, () -> cachedCallback.onResult(persistedTabData));
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

    /** Save {@link PersistedTabData} to storage */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void save() {
        if (mIsTabSaveEnabledSupplier != null && mIsTabSaveEnabledSupplier.get()) {
            mPersistedTabDataStorage.save(
                    mTab.getId(), mPersistedTabDataId, getOomAndMetricsWrapper());
        }
    }

    /**
     * Save {@link PersistedTabData} to storage
     * @param callback called after save is completed
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void save(Callback<Integer> onComplete) {
        if (mIsTabSaveEnabledSupplier != null && mIsTabSaveEnabledSupplier.get()) {
            mPersistedTabDataStorage.save(
                    mTab.getId(), mPersistedTabDataId, getOomAndMetricsWrapper(), onComplete);
        }
    }

    /**
     * @return {@link Supplier} for {@link PersistedTabData} in serialized form.
     */
    abstract Serializer<ByteBuffer> getSerializer();

    @VisibleForTesting
    protected Serializer<ByteBuffer> getOomAndMetricsWrapper() {
        final Serializer<ByteBuffer> serializer = getSerializerWithOomSoftFallback();
        return new Serializer<ByteBuffer>() {
            @Override
            public ByteBuffer get() {
                if (serializer == null) return null;
                ByteBuffer res;
                try (TraceEvent e = TraceEvent.scoped("PersistedTabData.Serialize")) {
                    res = serializer.get();
                } catch (OutOfMemoryError oe) {
                    Log.e(
                            TAG,
                            "Out of memory error when attempting to save PersistedTabData."
                                    + " Details: "
                                    + oe.getMessage());
                    res = null;
                }
                // TODO(crbug.com/40162721) convert to enum histogram and differentiate null/not
                // null/out of memory
                RecordHistogram.recordBooleanHistogram(
                        "Tabs.PersistedTabData.Serialize." + getUmaTag(), res != null);
                return res;
            }

            @Override
            public void preSerialize() {
                serializer.preSerialize();
            }
        };
    }

    private Serializer<ByteBuffer> getSerializerWithOomSoftFallback() {
        try {
            return getSerializer();
        } catch (OutOfMemoryError oe) {
            Log.e(
                    TAG,
                    "Out of memory error when attempting to save PersistedTabData "
                            + oe.getMessage());
        }
        return null;
    }

    /**
     * Deserialize serialized {@link PersistedTabData} and
     * assign to fields in {@link PersistedTabData}
     * @param bytes serialized PersistedTabData
     */
    abstract boolean deserialize(@Nullable ByteBuffer bytes);

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void deserializeAndLog(@Nullable ByteBuffer bytes) {
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
        mTabSaveEnabledToggleCallback =
                (isTabSaveEnabled) -> {
                    if (isTabSaveEnabled) {
                        save();
                        mFirstSaveDone = true;
                    } else if (mFirstSaveDone) {
                        delete();
                    }
                };
        mIsTabSaveEnabledSupplier.addObserver(mTabSaveEnabledToggleCallback);
    }

    /** Delete all {@link PersistedTabData} when a {@link Tab} is closed. */
    public static void onTabClose(Tab tab) {
        // TODO(crbug.com/40187854) ensure we cleanup ShoppingPersistedTabData on startup
        ShoppingPersistedTabData shoppingPersistedTabData =
                tab.getUserDataHost().getUserData(ShoppingPersistedTabData.class);
        if (shoppingPersistedTabData != null) {
            shoppingPersistedTabData.disableSaving();
        }
        PersistedTabDataJni.get().onTabClose(tab);
    }

    /**
     * Add {@link PersistedTabData} class which is supported for maintenance.
     * @param clazz the class which is supported for maintenance.
     */
    protected static void addSupportedMaintenanceClass(Class<? extends PersistedTabData> clazz) {
        sSupportedMaintenanceClasses.add(clazz);
    }

    /**
     * Delete any stored {@link PersistedTabData} not matching any current live regular Tab
     * identifiers. This method is not supported for all {@link PersistedTabData} - call
     * addSupportedMaintenanceClass to gain support. This method is also not supported for incognito
     * Tabs. Must be called from UI Thread.
     * @param liveTabIds {@link Tab} identifiers which are currently live - no {@link
     *         PersistedTabData} will be deleted for these Tabs.
     */
    public static void performStorageMaintenance(List<Integer> liveTabIds) {
        ThreadUtils.assertOnUiThread();
        for (Class<? extends PersistedTabData> clazz : sSupportedMaintenanceClasses) {
            // Maintenance is supported only for regular Tabs.
            boolean isEncrypted = false;
            PersistedTabDataConfiguration config =
                    PersistedTabDataConfiguration.get(clazz, isEncrypted);
            PersistedTabDataStorage storage = config.getStorage();
            storage.performMaintenance(liveTabIds, config.getId());
        }
    }

    @VisibleForTesting
    protected static Set<Class<? extends PersistedTabData>>
            getSupportedMaintenanceClassesForTesting() {
        return sSupportedMaintenanceClasses;
    }

    /** Signal to {@link PersistedTabData} that deferred startup is complete. */
    public static void onDeferredStartup() {
        PersistedTabDataJni.get().onDeferredStartup();
    }

    @VisibleForTesting
    public void existsInStorage(Callback<Boolean> callback) {
        mPersistedTabDataStorage.restore(
                mTab.getId(),
                mPersistedTabDataId,
                (res) -> {
                    callback.onResult(res != null && res.limit() > 0);
                });
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        void onTabClose(Tab tab);

        void onDeferredStartup();
    }
}
