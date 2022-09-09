// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.supervised_user.website_approval.WebsiteApprovalCoordinator;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * Requests approval from a parent of a supervised user to unblock navigation to a given URL.
 */
class WebsiteParentApproval {
    // Favicon default specs
    private static final int FAVICON_MIN_SOURCE_SIZE_PIXEL = 16;
    private static final int FAVICON_DESIRED_SIZE_PIXEL = 32;

    /**
     * Wrapper class used to store a fetched favicon.
     */
    private static final class FaviconHelper {
        Bitmap mFavicon;

        public void setFavicon(Bitmap favicon) {
            mFavicon = favicon;
        }

        public Bitmap getFavicon() {
            return mFavicon;
        }
    }

    /**
     * Whether or not local (i.e. on-device) approval is supported.
     *
     * This method should be called before {@link requestLocalApproval()}.
     */
    @CalledByNative
    private static boolean isLocalApprovalSupported() {
        ParentAuthDelegate delegate = new ParentAuthDelegateImpl();
        return delegate.isLocalAuthSupported();
    }

    /**
     * Request local approval from a parent for viewing a given website.
     *
     * This method handles displaying relevant UI, and when complete calls the provided callback
     * with the result.  It should only be called after {@link isLocalApprovalSupported} has
     * returned true (it will perform a no-op if local approvals are unsupported).
     *
     * @param windowAndroid the window to which the approval UI should be attached
     * @param url the full URL the supervised user navigated to
     *
     * TODO(crbug.com/1272462): add favicon, callback parameters and specify callback result
     * values.
     * */
    @CalledByNative
    private static void requestLocalApproval(WindowAndroid windowAndroid, GURL url) {
        // First ask the parent to authenticate.
        ParentAuthDelegate delegate = new ParentAuthDelegateImpl();
        FaviconHelper faviconHelper = new FaviconHelper();
        delegate.requestLocalAuth(windowAndroid, url,
                (success) -> { onParentAuthComplete(success, windowAndroid, url, faviconHelper); });

        WebsiteParentApprovalJni.get().fetchFavicon(url, FAVICON_MIN_SOURCE_SIZE_PIXEL,
                FAVICON_DESIRED_SIZE_PIXEL, (Bitmap favicon) -> faviconHelper.setFavicon(favicon));
    }

    /** Displays the screen giving the parent the option to approve or deny the website.*/
    private static void onParentAuthComplete(
            boolean success, WindowAndroid windowAndroid, GURL url, FaviconHelper faviconHelper) {
        if (!success) {
            WebsiteParentApprovalJni.get().onCompletion(false);
            return;
        }

        // Launch the bottom sheet.
        WebsiteApprovalCoordinator websiteApprovalUi = new WebsiteApprovalCoordinator(
                windowAndroid, url, new WebsiteApprovalCoordinator.CompletionCallback() {
                    @Override
                    public void onWebsiteApproved() {
                        // TODO(crbug.com/1330897): add metrics.
                        WebsiteParentApprovalJni.get().onCompletion(true);
                    }

                    @Override
                    public void onWebsiteDenied() {
                        // TODO(crbug.com/1330897): add metrics.
                        WebsiteParentApprovalJni.get().onCompletion(false);
                    }
                });
        // TODO(crbug.com/1340912): consume faviconHelper to set the favicon.
        websiteApprovalUi.show();
    }

    @NativeMethods
    interface Natives {
        void onCompletion(boolean success);

        void fetchFavicon(GURL url, int minSourceSizePixel, int desiredSizePixel,
                Callback<Bitmap> onFaviconFetched);
    }
}
