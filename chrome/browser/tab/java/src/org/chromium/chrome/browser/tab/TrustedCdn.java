// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.NativeMethods;

import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** Provides a trusted CDN publisher URL for the current web contents in a Tab. */
public class TrustedCdn extends TabWebContentsUserData {
    @VisibleForTesting public static final Class<TrustedCdn> USER_DATA_KEY = TrustedCdn.class;

    private final Tab mTab;
    private final long mNativeTrustedCdn;

    /**
     * UnownedUserData shared across all tabs to get the publisher url visibility.
     * This hangs off of an activity via WindowAndroid.
     */
    public static interface PublisherUrlVisibility extends UnownedUserData {
        /** The key for accessing this object on an {@link UnownedUserDataHost}. */
        public static final UnownedUserDataKey<PublisherUrlVisibility> KEY =
                new UnownedUserDataKey<>(PublisherUrlVisibility.class);

        /**
         * Get the Activity's {@link PublisherUrlVisibility} from the provided
         * {@link WindowAndroid}.
         * @param window The window to get the validator from.
         * @return The Activity's {@link PublisherUrlVisibility}.
         */
        public static @Nullable PublisherUrlVisibility from(WindowAndroid window) {
            return KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
        }

        /**
         * Make this instance of PublisherUrlVisibility available through the activity's window.
         * @param window A {@link WindowAndroid} to attach to.
         * @param validator The {@link PublisherUrlVisibility} to attach.
         */
        public static void attach(WindowAndroid window, PublisherUrlVisibility validator) {
            KEY.attachToHost(window.getUnownedUserDataHost(), validator);
        }

        /**
         * Detach the provided PublisherUrlVisibility from any host it is associated with.
         * @param validator The {@link PublisherUrlVisibility} to detach.
         */
        public static void detach(PublisherUrlVisibility validator) {
            KEY.detachFromAllHosts(validator);
        }

        /**
         * Whether the encomapssing activity can show the publisher URL from a trusted CDN.
         * @param tab Tab object currently being shown.
         * @return {@code true} if the publisher URL from a trusted CDN can be shown.
         */
        boolean canShowPublisherUrl(Tab tab);
    }

    /**
     * @return The publisher URL if the current page is hosted on a trusted CDN, or null otherwise
     */
    public static @Nullable GURL getPublisherUrl(@Nullable Tab tab) {
        TrustedCdn cdn = get(tab);
        return cdn != null ? cdn.getPublisherUrl() : null;
    }

    /**
     * @param tab Tab object currently being shown.
     * @return The name of the publisher of the content if it can be reliably extracted, or null
     *         otherwise.
     */
    public static String getContentPublisher(Tab tab) {
        if (tab == null) return null;

        GURL publisherUrl = TrustedCdn.getPublisherUrl(tab);
        if (publisherUrl != null) {
            return UrlUtilities.extractPublisherFromPublisherUrl(publisherUrl);
        }

        return null;
    }

    static TrustedCdn from(@NonNull Tab tab) {
        TrustedCdn trustedCdn = get(tab);
        if (trustedCdn == null) {
            trustedCdn = tab.getUserDataHost().setUserData(USER_DATA_KEY, new TrustedCdn(tab));
        }
        return trustedCdn;
    }

    public static void initForTesting(@NonNull Tab tab) {
        from(tab);
    }

    private static TrustedCdn get(@Nullable Tab tab) {
        return tab != null ? tab.getUserDataHost().getUserData(USER_DATA_KEY) : null;
    }

    private TrustedCdn(Tab tab) {
        super(tab);
        mTab = tab;
        mNativeTrustedCdn = TrustedCdnJni.get().init(TrustedCdn.this);
    }

    @Override
    public void initWebContents(WebContents webContents) {
        TrustedCdnJni.get().setWebContents(mNativeTrustedCdn, TrustedCdn.this, webContents);
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {
        TrustedCdnJni.get().resetWebContents(mNativeTrustedCdn, TrustedCdn.this);
    }

    @Override
    public void destroyInternal() {
        TrustedCdnJni.get().onDestroyed(mNativeTrustedCdn, TrustedCdn.this);
    }

    @Nullable
    @VisibleForTesting
    public GURL getPublisherUrl() {
        WebContents webContents = mTab.getWebContents();
        if (webContents == null) return null;

        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return null;

        PublisherUrlVisibility publisherUrlVisibility = PublisherUrlVisibility.from(windowAndroid);
        if (publisherUrlVisibility == null || !publisherUrlVisibility.canShowPublisherUrl(mTab)) {
            return null;
        }
        int level = SecurityStateModel.getSecurityLevelForWebContents(mTab.getWebContents());
        if (level == ConnectionSecurityLevel.DANGEROUS) return null;
        GURL publisherUrl = TrustedCdnJni.get().getPublisherUrl(mNativeTrustedCdn);
        return publisherUrl.isValid() ? publisherUrl : null;
    }

    @NativeMethods
    public interface Natives {
        long init(TrustedCdn caller);

        void onDestroyed(long nativeTrustedCdn, TrustedCdn caller);

        void setWebContents(long nativeTrustedCdn, TrustedCdn caller, WebContents webContents);

        void resetWebContents(long nativeTrustedCdn, TrustedCdn caller);

        GURL getPublisherUrl(long nativeTrustedCdn);
    }
}
