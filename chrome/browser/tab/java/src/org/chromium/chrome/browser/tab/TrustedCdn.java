// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** Provides a trusted CDN publisher URL for the current web contents in a Tab. */
@NullMarked
public final class TrustedCdn {
    private static @Nullable GURL sPublisherUrlForTesting;

    private TrustedCdn() {}

    /**
     * UnownedUserData shared across all tabs to get the publisher url visibility. This hangs off of
     * an activity via WindowAndroid.
     */
    public interface PublisherUrlVisibility {
        /** The key for accessing this object on an {@link UnownedUserDataHost}. */
        UnownedUserDataKey<PublisherUrlVisibility> KEY = new UnownedUserDataKey<>();

        /**
         * Get the Activity's {@link PublisherUrlVisibility} from the provided {@link
         * WindowAndroid}.
         *
         * @param window The window to get the validator from.
         * @return The Activity's {@link PublisherUrlVisibility}.
         */
        static @Nullable PublisherUrlVisibility from(WindowAndroid window) {
            return KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
        }

        /**
         * Make this instance of PublisherUrlVisibility available through the activity's window.
         *
         * @param window A {@link WindowAndroid} to attach to.
         * @param validator The {@link PublisherUrlVisibility} to attach.
         */
        static void attach(WindowAndroid window, PublisherUrlVisibility validator) {
            KEY.attachToHost(window.getUnownedUserDataHost(), validator);
        }

        /**
         * Detach the provided PublisherUrlVisibility from any host it is associated with.
         *
         * @param validator The {@link PublisherUrlVisibility} to detach.
         */
        static void detach(PublisherUrlVisibility validator) {
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
     * Sets a test override for the publisher URL. Passing {@code null} clears the override rather
     * than forcing a {@code null} publisher URL.
     */
    public static void setPublisherUrlForTesting(@Nullable GURL publisherUrl) {
        sPublisherUrlForTesting = publisherUrl;
        ResettersForTesting.register(() -> sPublisherUrlForTesting = null);
    }

    /**
     * @return The publisher URL if the current page is hosted on a trusted CDN, or null otherwise
     */
    public static @Nullable GURL getPublisherUrl(@Nullable Tab tab) {
        if (tab == null || tab.isDestroyed()) return null;
        if (sPublisherUrlForTesting != null) return sPublisherUrlForTesting;

        WebContents webContents = tab.getWebContents();
        if (webContents == null) return null;

        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return null;

        PublisherUrlVisibility publisherUrlVisibility = PublisherUrlVisibility.from(windowAndroid);
        if (publisherUrlVisibility == null || !publisherUrlVisibility.canShowPublisherUrl(tab)) {
            return null;
        }
        int level = SecurityStateModel.getSecurityLevelForWebContents(webContents);
        if (level != ConnectionSecurityLevel.SECURE) return null;
        GURL publisherUrl = TrustedCdnJni.get().getPublisherUrl(webContents);
        return publisherUrl.isValid() ? publisherUrl : null;
    }

    /**
     * Returns the name of the publisher of the content if it can be reliably extracted, or null
     * otherwise.
     *
     * @param tab Tab object currently being shown.
     * @return The name of the publisher of the content if it can be reliably extracted, or null
     *     otherwise.
     */
    public static @Nullable String getContentPublisher(@Nullable Tab tab) {
        GURL publisherUrl = TrustedCdn.getPublisherUrl(tab);
        if (publisherUrl != null) {
            return UrlUtilities.extractPublisherFromPublisherUrl(publisherUrl);
        }

        return null;
    }

    @NativeMethods
    public interface Natives {
        @JniType("GURL")
        GURL getPublisherUrl(@JniType("content::WebContents*") @Nullable WebContents webContents);
    }
}
