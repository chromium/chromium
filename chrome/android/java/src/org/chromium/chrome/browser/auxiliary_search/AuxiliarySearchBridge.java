// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchBookmarkGroup;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Java bridge to provide information for the auxiliary search.
 */
public class AuxiliarySearchBridge {
    private long mNativeBridge;

    /**
     * Constructs a bridge for the auxiliary search provider.
     *
     * @param profile The Profile to retrieve the corresponding information.
     */
    public AuxiliarySearchBridge(@NonNull Profile profile) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_APP_INTEGRATION)
                || profile.isOffTheRecord()) {
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

    @NativeMethods
    @VisibleForTesting
    public interface Natives {
        long getForProfile(Profile profile);
        byte[] getBookmarksSearchableData(long nativeAuxiliarySearchProvider);
    }
}
