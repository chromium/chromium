// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.util.List;

/** Java bridge to provide information of top sites for the auxiliary search. */
@NullMarked
public class AuxiliarySearchTopSiteProviderBridge {
    @JniType("::AuxiliarySearchTopSiteProviderBridge")
    interface NativeAuxiliarySearchTopSiteProviderBridge extends JniTypeToken {}

    /** An interface to handle events in {@link MostVisitedSites}. */
    interface Observer {
        /** This is called when the list of most visited URLs is initially available or updated. */
        void onSiteSuggestionsAvailable(@Nullable List<AuxiliarySearchDataEntry> entries);

        /**
         * This is called when a previously uncached icon has been fetched. Parameters guaranteed to
         * be non-null.
         *
         * @param siteUrl URL of site with newly-cached icon.
         */
        void onIconMadeAvailable(GURL siteUrl);
    }

    private @Nullable JniUniquePtr<NativeAuxiliarySearchTopSiteProviderBridge> mNativeBridge;
    private @Nullable Observer mObserver;

    /**
     * Constructs a bridge for the auxiliary search top sites provider.
     *
     * @param profile The Profile to retrieve the corresponding information.
     */
    public AuxiliarySearchTopSiteProviderBridge(Profile profile) {
        if (profile.isOffTheRecord()) {
            mNativeBridge = null;
        } else {
            mNativeBridge = AuxiliarySearchTopSiteProviderBridgeJni.get().init(profile);
        }
    }

    /**
     * Assigns {@link #mObserver}. Requires {@link #mObserver} initially null, then fetches the
     * current most visited site suggestions.
     *
     * @param observer The observer to receive suggestions when they are ready.
     */
    public void setObserver(Observer observer) {
        assert mNativeBridge != null;

        mObserver = observer;
        AuxiliarySearchTopSiteProviderBridgeJni.get().setObserverAndTrigger(mNativeBridge, this);
    }

    /** Destroys the native bridge and remove observer. */
    public void destroy() {
        if (mNativeBridge != null) {
            mNativeBridge.destroy();
            mNativeBridge = null;
        }
        mObserver = null;
    }

    /** Starts a fetch of the current most visited sites suggestions. */
    public void getMostVisitedSites() {
        if (mNativeBridge == null) {
            if (mObserver != null) {
                mObserver.onSiteSuggestionsAvailable(null);
            }
            return;
        }

        AuxiliarySearchTopSiteProviderBridgeJni.get().getMostVisitedSites(mNativeBridge);
    }

    @CalledByNative
    @VisibleForTesting
    static AuxiliarySearchDataEntry addDataEntry(
            @AuxiliarySearchEntryType int type,
            @JniType("GURL") GURL url,
            @JniType("std::u16string") String title,
            long lastActiveTime,
            int tabId,
            @JniType("std::string") @Nullable String appId,
            int visitId,
            int score) {
        return new AuxiliarySearchDataEntry(
                type,
                url,
                title,
                lastActiveTime,
                tabId,
                TextUtils.isEmpty(appId) ? null : appId,
                visitId,
                score);
    }

    @CalledByNative
    @VisibleForTesting
    void onMostVisitedSitesURLsAvailable(
            @JniType("std::vector") List<AuxiliarySearchDataEntry> entries) {
        if (mObserver == null) return;
        mObserver.onSiteSuggestionsAvailable(entries);
    }

    @CalledByNative
    void onIconMadeAvailable(@JniType("GURL") GURL siteUrl) {
        if (mObserver == null) return;
        mObserver.onIconMadeAvailable(siteUrl);
    }

    @Nullable Observer getObserverForTesting() {
        return mObserver;
    }

    @NativeMethods
    interface Natives {
        JniUniquePtr<NativeAuxiliarySearchTopSiteProviderBridge> init(
                @JniType("Profile*") Profile profile);

        void setObserverAndTrigger(
                JniPtr<NativeAuxiliarySearchTopSiteProviderBridge> self,
                AuxiliarySearchTopSiteProviderBridge caller);

        void getMostVisitedSites(JniPtr<NativeAuxiliarySearchTopSiteProviderBridge> self);
    }
}
