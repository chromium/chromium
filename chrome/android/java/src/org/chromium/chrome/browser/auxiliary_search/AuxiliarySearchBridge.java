// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchBookmarkGroup;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

import java.util.ArrayList;
import java.util.List;

/** Java bridge to provide information for the auxiliary search. */
public class AuxiliarySearchBridge {
    private long mNativeBridge;

    /**
     * Constructs a bridge for the auxiliary search provider.
     *
     * @param profile The Profile to retrieve the corresponding information.
     */
    public AuxiliarySearchBridge(@NonNull Profile profile) {
        if (!ChromeFeatureList.sAndroidAppIntegration.isEnabled() || profile.isOffTheRecord()) {
            mNativeBridge = 0;
        } else {
            mNativeBridge = AuxiliarySearchBridgeJni.get().getForProfile(profile);
        }
    }

    /**
     * @return AuxiliarySearchGroup for bookmarks, which is necessary for the auxiliary search.
     */
    public @Nullable AuxiliarySearchBookmarkGroup getBookmarksSearchableData() {
        if (mNativeBridge != 0) {
            try {
                return AuxiliarySearchBookmarkGroup.parseFrom(
                        AuxiliarySearchBridgeJni.get().getBookmarksSearchableData(mNativeBridge));

            } catch (InvalidProtocolBufferException e) {
            }
        }

        return null;
    }

    /**
     * This method will return non sensitive url tabs, and the scheme is http or https.
     *
     * @param tabs A list of {@link Tab}s to check if they are sensitive.
     * @param callback {@link Callback} to pass back the list of non sensitive {@link Tab}s.
     */
    public void getNonSensitiveTabs(List<Tab> tabs, Callback<List<Tab>> callback) {
        if (mNativeBridge == 0) {
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        callback.onResult(null);
                    });
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
                                        TaskTraits.UI_DEFAULT,
                                        () -> {
                                            callback.onResult(tabList);
                                        });
                            }
                        });
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        long getForProfile(@JniType("Profile*") Profile profile);

        byte[] getBookmarksSearchableData(long nativeAuxiliarySearchProvider);

        void getNonSensitiveTabs(
                long nativeAuxiliarySearchProvider, Tab[] tabs, Callback<Object[]> callback);
    }
}
