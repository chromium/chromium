// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Java bridge to provide information for the auxiliary search. */
@NullMarked
public class AuxiliarySearchBridge {
    private final long mNativeBridge;

    /**
     * Constructs a bridge for the auxiliary search provider.
     *
     * @param profile The Profile to retrieve the corresponding information.
     */
    public AuxiliarySearchBridge(Profile profile) {
        if (profile.isOffTheRecord()) {
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
    public void getNonSensitiveTabs(List<Tab> tabs, Callback<@Nullable List<Tab>> callback) {
        if (mNativeBridge == 0) {
            PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> callback.onResult(null));
            return;
        }

        AuxiliarySearchBridgeJni.get()
                .getNonSensitiveTabs(
                        mNativeBridge,
                        tabs,
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
    public void getNonSensitiveHistoryData(
            Callback<@Nullable List<AuxiliarySearchDataEntry>> callback) {
        if (mNativeBridge == 0) {
            PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> callback.onResult(null));
            return;
        }

        AuxiliarySearchBridgeJni.get().getNonSensitiveHistoryData(mNativeBridge, callback);
    }

    /**
     * This method will return a list of Custom Tabs URLs.
     *
     * @param url The current URL of the Custom Tab.
     * @param callback {@link Callback} to pass back the list of {@link AuxiliarySearchDataEntry}s.
     */
    public void getCustomTabs(
            GURL url, long timestamp, Callback<@Nullable List<AuxiliarySearchDataEntry>> callback) {
        if (mNativeBridge == 0) {
            PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> callback.onResult(null));
            return;
        }

        AuxiliarySearchBridgeJni.get().getCustomTabs(mNativeBridge, url, timestamp, callback);
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

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        long getForProfile(@JniType("Profile*") Profile profile);

        void getNonSensitiveTabs(
                long nativeAuxiliarySearchProvider,
                @JniType("std::vector<TabAndroid*>") List<Tab> tabs,
                Callback<Object[]> callback);

        void getNonSensitiveHistoryData(
                long nativeAuxiliarySearchProvider,
                Callback<@Nullable List<AuxiliarySearchDataEntry>> callback);

        void getCustomTabs(
                long nativeAuxiliarySearchProvider,
                GURL url,
                long timestamp,
                Callback<@Nullable List<AuxiliarySearchDataEntry>> callback);
    }
}
