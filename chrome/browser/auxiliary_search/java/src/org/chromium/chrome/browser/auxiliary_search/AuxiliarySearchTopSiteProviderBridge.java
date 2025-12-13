// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.util.List;

/** Java bridge to provide information of top sites for the auxiliary search. */
@NullMarked
public class AuxiliarySearchTopSiteProviderBridge {
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

    private long mNativeBridge;
    private @Nullable Observer mObserver;

    /**
     * Constructs a bridge for the auxiliary search top sites provider.
     *
     * @param profile The Profile to retrieve the corresponding information.
     */
    public AuxiliarySearchTopSiteProviderBridge(Profile profile) {
        if (profile.isOffTheRecord()) {
            mNativeBridge = 0;
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
        assert mNativeBridge != 0;

        mObserver = observer;
        AuxiliarySearchTopSiteProviderBridgeJni.get().setObserverAndTrigger(mNativeBridge, this);
    }

    /** Destroys the native bridge and remove observer. */
    public void destroy() {
        AuxiliarySearchTopSiteProviderBridgeJni.get().destroy(mNativeBridge);
        mObserver = null;
        mNativeBridge = 0;
    }

    /** Starts a fetch of the current most visited sites suggestions. */
    public void getMostVisitedSites() {
        if (mNativeBridge == 0) {
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
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        long init(@JniType("Profile*") Profile profile);

        void setObserverAndTrigger(
                long nativeAuxiliarySearchTopSiteProviderBridge,
                AuxiliarySearchTopSiteProviderBridge self);

        void destroy(long nativeAuxiliarySearchTopSiteProviderBridge);

        void getMostVisitedSites(long nativeAuxiliarySearchTopSiteProviderBridge);
    }
}
