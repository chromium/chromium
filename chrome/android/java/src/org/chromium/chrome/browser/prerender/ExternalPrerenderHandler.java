// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prerender;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.Pair;
import android.view.WindowManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

/**
 * A handler class for prerender requests coming from  other applications.
 */
@JNINamespace("prerender")
public class ExternalPrerenderHandler {

    private long mNativeExternalPrerenderHandler;

    public ExternalPrerenderHandler() {
        mNativeExternalPrerenderHandler = ExternalPrerenderHandlerJni.get().init();
    }

    /**
     * Add a prerender for the given url and given content view dimensions.
     * <p>
     * The generated {@link WebContents} does not actually contain the prerendered contents but
     * must be used as the container that you load the prerendered URL into.
     *
     * @param profile The profile to use for the prerender.
     * @param url The url to prerender.
     * @param referrer The referrer for the prerender request.
     * @param bounds The bounds for the content view (render widget host view) for the prerender.
     * @param prerenderOnCellular Whether the prerender should happen if the device has a cellular
     *                            connection.
     * @return A Pair containing the dummy {@link WebContents} that is linked to this prerender
     *         through session storage namespace id and will be replaced soon and the prerendering
     *         {@link WebContents} that owned by the prerender manager.
     *         {@code null} if unsuccessful.
     */
    public Pair<WebContents, WebContents> addPrerender(Profile profile, String url, String referrer,
            Rect bounds, boolean prerenderOnCellular) {
        WebContents dummyWebContents = WebContentsFactory.createWebContents(false, false);
        WebContents prerenderingWebContents =
                addPrerender(profile, dummyWebContents, url, referrer, bounds, prerenderOnCellular);
        if (dummyWebContents == null) return null;
        if (prerenderingWebContents != null) {
            return Pair.create(dummyWebContents, prerenderingWebContents);
        }
        dummyWebContents.destroy();
        return null;
    }

    /**
     * Adds a prerender for the given URL to an existing {@link WebContents} with the given
     * dimensions.
     *
     * @param profile The profile to use for the prerender.
     * @param webContents The WebContents to add the prerender to.
     * @param url The url to prerender.
     * @param referrer The referrer for the prerender request.
     * @param bounds The bounds for the content view (render widget host view) for the prerender.
     * @param prerenderOnCellular Whether the prerender should happen if the device has a cellular
     *                            connection.
     * @return The prerendering {@link WebContents} owned by PrerenderManager.
     */
    public WebContents addPrerender(Profile profile, WebContents webContents, String url,
            String referrer, Rect bounds, boolean prerenderOnCellular) {
        return ExternalPrerenderHandlerJni.get().addPrerender(mNativeExternalPrerenderHandler,
                profile, webContents, url, referrer, bounds.top, bounds.left, bounds.bottom,
                bounds.right, prerenderOnCellular);
    }

    /**
     * Cancel the current prerender action on this {@link ExternalPrerenderHandler}.
     */
    public void cancelCurrentPrerender() {
        ExternalPrerenderHandlerJni.get().cancelCurrentPrerender(mNativeExternalPrerenderHandler);
    }

    /**
     * Check whether a given url has been prerendering for the given profile and session id for the
     * given web contents.
     * @param profile The profile to check for prerendering.
     * @param url The url to check for prerender.
     * @param webContents The {@link WebContents} for which to compare the session info.
     * @return Whether the given url was prerendered.
     */
    @VisibleForTesting
    public static boolean hasPrerenderedUrl(Profile profile, String url, WebContents webContents)  {
        return ExternalPrerenderHandlerJni.get().hasPrerenderedUrl(profile, url, webContents);
    }

    /**
     * Provides an estimate of the contents size.
     *
     * The estimate is likely to be incorrect. This is not a problem, as the aim
     * is to avoid getting a different layout and resources than needed at
     * render time.
     * @param context The application context.
     * @param convertToDp Whether the value should be converted to dp from pixels.
     * @return The estimated prerender size in pixels or dp.
     */
    public static Rect estimateContentSize(Context context, boolean convertToDp) {
        // The size is estimated as:
        // X = screenSizeX
        // Y = screenSizeY - top bar - bottom bar - custom tabs bar
        // The bounds rectangle includes the bottom bar and the custom tabs bar as well.
        Rect screenBounds = new Rect();
        Point screenSize = new Point();
        WindowManager wm = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        wm.getDefaultDisplay().getSize(screenSize);
        Resources resources = context.getResources();
        int statusBarId = resources.getIdentifier("status_bar_height", "dimen", "android");
        try {
            screenSize.y -= resources.getDimensionPixelSize(statusBarId);
        } catch (Resources.NotFoundException e) {
            // Nothing, this is just a best effort estimate.
        }
        screenBounds.set(0,
                resources.getDimensionPixelSize(R.dimen.custom_tabs_control_container_height),
                screenSize.x, screenSize.y);

        if (convertToDp) {
            float density = resources.getDisplayMetrics().density;
            screenBounds.top = (int) Math.ceil(screenBounds.top / density);
            screenBounds.left = (int) Math.ceil(screenBounds.left / density);
            screenBounds.right = (int) Math.ceil(screenBounds.right / density);
            screenBounds.bottom = (int) Math.ceil(screenBounds.bottom / density);
        }
        return screenBounds;
    }

    @VisibleForTesting
    public static boolean hasRecentlyPrefetchedUrlForTesting(Profile profile, String url) {
        return ExternalPrerenderHandlerJni.get().hasRecentlyPrefetchedUrlForTesting(profile, url);
    }

    @VisibleForTesting
    public static void clearPrefetchInformationForTesting(Profile profile) {
        ExternalPrerenderHandlerJni.get().clearPrefetchInformationForTesting(profile);
    }

    @NativeMethods
    interface Natives {
        long init();
        WebContents addPrerender(long nativeExternalPrerenderHandlerAndroid, Profile profile,
                WebContents webContents, String url, String referrer, int top, int left, int bottom,
                int right, boolean prerenderOnCellular);
        boolean hasPrerenderedUrl(Profile profile, String url, WebContents webContents);
        void cancelCurrentPrerender(long nativeExternalPrerenderHandlerAndroid);
        boolean hasRecentlyPrefetchedUrlForTesting(Profile profile, String url);
        void clearPrefetchInformationForTesting(Profile profile);
    }
}
