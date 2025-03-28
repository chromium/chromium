// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchProvider.Observer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Java bridge to provide information for the auxiliary search. */
public class AuxiliarySearchBridge {
    private long mNativeBridge;
    private AuxiliarySearchProvider.Observer mObserver;

    /**
     * Constructs a bridge for the auxiliary search provider.
     *
     * @param profile The Profile to retrieve the corresponding information.
     */
    public AuxiliarySearchBridge(@NonNull Profile profile) {
        if ((!ChromeFeatureList.sAndroidAppIntegration.isEnabled()
                        && !ChromeFeatureList.sAndroidAppIntegrationV2.isEnabled())
                || profile.isOffTheRecord()) {
            mNativeBridge = 0;
        } else {
            mNativeBridge = AuxiliarySearchBridgeJni.get().getForProfile(profile);
        }
    }

    /**
     * This method will return non-sensitive url tabs, and the scheme is http or https.
     *
     * @param tabs A list of {@link Tab}s to check if they are sensitive.
     * @param callback {@link Callback} to pass back the list of non-sensitive {@link Tab}s.
     */
    public void getNonSensitiveTabs(List<Tab> tabs, Callback<List<Tab>> callback) {
        if (mNativeBridge == 0) {
            PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> callback.onResult(null));
            return;
        }

        AuxiliarySearchBridgeJni.get()
                .getNonSensitiveTabs(
                        mNativeBridge,
                        tabs.toArray(new Tab[0]),
                        new Callback<Object[]>() {
                            @Override
                            public void onResult(Object[] tabs) {
                                ArrayList<Tab> tabList = new ArrayList<>();
                                for (Object o : tabs) {
                                    assert (o instanceof Tab);

                                    tabList.add((Tab) o);
                                }

                                PostTask.runOrPostTask(
                                        TaskTraits.UI_DEFAULT, callback.bind(tabList));
                            }
                        });
    }

    /**
     * This method will return non-sensitive URLs of supported data types.
     *
     * @param callback {@link Callback} to pass back the list of non-sensitive {@link
     *     AuxiliarySearchDataEntry}s.
     */
    public void getNonSensitiveHistoryData(Callback<List<AuxiliarySearchDataEntry>> callback) {
        if (mNativeBridge == 0) {
            PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> callback.onResult(null));
            return;
        }

        AuxiliarySearchBridgeJni.get().getNonSensitiveHistoryData(mNativeBridge, callback);
    }

    /**
     * Assigns {@link #mObserver}, possibly to null. If non-null {@param observer} is passed,
     * requires {@link #mObserver} initially null, then fetches the current most visited site
     * suggestions.
     *
     * @param observer The observer to receive suggestions when they are ready.
     */
    public void setObserver(Observer observer) {
        if (observer == null) {
            mObserver = null;
            return;
        }

        assert mObserver == null;
        mObserver = observer;
        AuxiliarySearchBridgeJni.get().setObserverAndTrigger(mNativeBridge, this);
    }

    /** Starts a fetch of the current most visited sites suggestions. */
    public void getMostVisitedSites() {
        if (mNativeBridge == 0) {
            mObserver.onSiteSuggestionsAvailable(null);
            return;
        }

        AuxiliarySearchBridgeJni.get().getMostVisitedSites(mNativeBridge);
    }

    /**
     * Helper to call previously injected callback to pass suggestion results.
     *
     * @param entries The list of fetched entries.
     * @param callback The callback to notify once the fetching is completed.
     */
    @CalledByNative
    @VisibleForTesting
    static void onDataReady(
            @JniType("std::vector") List<AuxiliarySearchDataEntry> entries,
            Callback<List<AuxiliarySearchDataEntry>> callback) {
        callback.onResult(entries);
    }

    @CalledByNative
    @VisibleForTesting
    static AuxiliarySearchDataEntry addDataEntry(
            @AuxiliarySearchEntryType int type,
            GURL url,
            String title,
            long lastActiveTime,
            int tabId,
            @Nullable String appId,
            int visitId,
            int score) {
        return new AuxiliarySearchDataEntry(
                type, url, title, lastActiveTime, tabId, appId, visitId, score);
    }

    @CalledByNative
    @VisibleForTesting
    void onMostVisitedSitesURLsAvailable(
            @JniType("std::vector") List<AuxiliarySearchDataEntry> entries) {
        mObserver.onSiteSuggestionsAvailable(entries);
    }

    @CalledByNative
    void onIconMadeAvailable(@JniType("GURL") GURL siteUrl) {
        mObserver.onIconMadeAvailable(siteUrl);
    }

    AuxiliarySearchProvider.Observer getObserverForTesting() {
        return mObserver;
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        long getForProfile(@JniType("Profile*") Profile profile);

        void getNonSensitiveTabs(
                long nativeAuxiliarySearchProvider, Tab[] tabs, Callback<Object[]> callback);

        void getNonSensitiveHistoryData(
                long nativeAuxiliarySearchProvider,
                Callback<List<AuxiliarySearchDataEntry>> callback);

        void setObserverAndTrigger(long nativeAuxiliarySearchProvider, AuxiliarySearchBridge self);

        void getMostVisitedSites(long nativeAuxiliarySearchProvider);
    }
}
