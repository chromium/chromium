// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;

/**
 * A controller to manage showing a permission prompt for auto picture-in-picture for documents.
 * This class is responsible for checking the permission status and showing the UI when needed.
 */
@NullMarked
@JNINamespace("picture_in_picture")
public class AutoPictureInPicturePermissionController {
    private static final String TAG = "AutoPipPermCtrl";

    private final WebContents mWebContents;
    private final GURL mUrl;
    private final Runnable mClosePipCallback;
    private @Nullable AutoPipPermissionDialogView mView;
    private @Nullable AutoPictureInPicturePrivacyMaskView mMaskView;

    // The previous accessibility importance of the WebContents container view. Used to restore
    // state after the prompt is dismissed.
    private int mOldAccessibilityImportance;

    // WeakReference to the view that has been obscured from accessibility. This ensures we restore
    // the state on the exact same view instance we modified, even if the WebContents' container
    // view changes in the meantime.
    private @Nullable WeakReference<View> mObscuredViewWeakRef;

    /**
     * Shows the permission prompt for auto picture-in-picture if the content setting is "ASK".
     *
     * @param activity The activity to display the prompt in.
     * @param tab The tab that is entering auto picture-in-picture.
     * @param closePipCallback A callback to run if the user selects 'Don't Allow' (BLOCK).
     */
    public static void showPromptIfNeeded(
            Activity activity, @Nullable Tab tab, Runnable closePipCallback) {
        if (tab == null || tab.getWebContents() == null) {
            return;
        }

        WebContents webContents = tab.getWebContents();
        // When prompt is triggered, webContents is expected to be valid and can retrieve helper
        // from UserDataHost.
        AutoPictureInPictureTabHelper helper =
                assertNonNull(AutoPictureInPictureTabHelper.fromWebContents(webContents));

        // If the user already selected "Allow Once" for this WebContents or the permission is not
        // "ASK", we shouldn't create a controller at all.
        if (helper.hasAllowOnce()
                || AutoPictureInPicturePermissionControllerJni.get()
                                .getPermissionStatus(webContents)
                        != ContentSetting.ASK) {
            return;
        }

        // Create the controller and register it with the helper. This prevents "fire and forget"
        // by giving the controller a clear owner (the helper attached to the WebContents).
        AutoPictureInPicturePermissionController controller =
                new AutoPictureInPicturePermissionController(webContents, closePipCallback);
        helper.setPermissionController(controller);

        controller.show(activity);
    }

    /**
     * Returns true if auto picture-in-picture has been recently triggered or is currently active
     * for the given WebContents. This is used to determine if a permission prompt is needed.
     *
     * @param webContents The WebContents to check.
     * @return True if auto picture-in-picture is in use or was recently triggered, false otherwise.
     */
    public static boolean isAutoPictureInPictureInUse(WebContents webContents) {
        return AutoPictureInPicturePermissionControllerJni.get()
                .isAutoPictureInPictureInUse(webContents);
    }

    /**
     * Called when the picture-in-picture window is destroyed. Checks if a permission prompt was
     * active and, if so, reports the dismissal and cleans up.
     *
     * @param webContents The WebContents that was in picture-in-picture.
     */
    public static void handleWindowDestruction(WebContents webContents) {
        AutoPictureInPictureTabHelper helper =
                AutoPictureInPictureTabHelper.getIfPresent(webContents);
        if (helper == null) {
            return;
        }

        AutoPictureInPicturePermissionController controller = helper.getPermissionController();
        if (controller != null) {
            AutoPictureInPicturePermissionControllerJni.get()
                    .onPictureInPictureDismissed(webContents);
            controller.dismiss();
        }
    }

    private AutoPictureInPicturePermissionController(
            WebContents webContents, Runnable closePipCallback) {
        mWebContents = webContents;
        mUrl = webContents.getLastCommittedUrl();
        mClosePipCallback = closePipCallback;
    }

    private void show(Activity activity) {
        if (mWebContents.isDestroyed() || activity.isFinishing() || activity.isDestroyed()) {
            return;
        }

        ViewGroup rootView = activity.findViewById(android.R.id.content);
        if (rootView == null) {
            Log.w(TAG, "Could not find root view to attach Auto-PiP prompt.");
            return;
        }

        // Add the privacy mask first, so it's behind the prompt.
        mMaskView = new AutoPictureInPicturePrivacyMaskView(activity, null);
        rootView.addView(mMaskView);
        mMaskView.show();

        obscureContentFromAccessibility();

        mView =
                new AutoPipPermissionDialogView(
                        activity,
                        activity.getString(R.string.permission_allow_every_visit),
                        activity.getString(R.string.permission_allow_this_time),
                        activity.getString(R.string.permission_dont_allow),
                        this::onUiResult);

        String formattedOrigin =
                UrlFormatter.formatUrlForSecurityDisplay(
                        mUrl.getOrigin(), SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        mView.setOrigin(formattedOrigin);

        // Add the view to the root of the activity.
        rootView.addView(mView);
        mView.bringToFront();
    }

    /** Dismisses the prompt and cleans up views. */
    public void dismiss() {
        if (mView != null) {
            ViewParent parent = mView.getParent();
            if (parent instanceof ViewGroup) {
                ((ViewGroup) parent).removeView(mView);
            }
            mView = null;
        }
        if (mMaskView != null) {
            mMaskView.hide();
            mMaskView = null;
        }

        // Ensure the helper releases its reference to this controller.
        if (!mWebContents.isDestroyed()) {
            restoreContentAccessibility();

            AutoPictureInPictureTabHelper helper =
                    AutoPictureInPictureTabHelper.getIfPresent(mWebContents);
            if (helper != null && helper.getPermissionController() == this) {
                helper.setPermissionController(null);
            }
        }
    }

    /**
     * Obscures the WebContents from accessibility tools.
     *
     * <p>This hides the descendants of the WebContents' container view to prevent accessibility
     * services from navigating to the underlying content while the permission prompt is shown.
     */
    private void obscureContentFromAccessibility() {
        if (mObscuredViewWeakRef != null) return;

        View containerView = getWebContentsContainerView();
        if (containerView == null) return;

        mOldAccessibilityImportance = containerView.getImportantForAccessibility();
        containerView.setImportantForAccessibility(
                View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);

        // Use WeakReference to ensure we restore state on this exact view and avoid leaks.
        mObscuredViewWeakRef = new WeakReference<>(containerView);
    }

    /** Restores the previous accessibility importance of the WebContents. */
    private void restoreContentAccessibility() {
        if (mObscuredViewWeakRef == null) return;

        View containerView = mObscuredViewWeakRef.get();
        if (containerView != null) {
            containerView.setImportantForAccessibility(mOldAccessibilityImportance);
        }

        mObscuredViewWeakRef.clear();
        mObscuredViewWeakRef = null;
    }

    /**
     * Returns the container view of the WebContents. This is the view that holds the web content
     * and is part of the view hierarchy that needs to be obscured.
     */
    private @Nullable View getWebContentsContainerView() {
        if (mWebContents.isDestroyed()) return null;
        ViewAndroidDelegate delegate = mWebContents.getViewAndroidDelegate();
        return delegate != null ? delegate.getContainerView() : null;
    }

    private void onUiResult(int result) {
        if (mWebContents.isDestroyed()) {
            Log.w(TAG, "WebContents gone before UI result processed.");
            return;
        }

        switch (result) {
            case AutoPipPermissionDialogView.UiResult.ALLOW_ON_EVERY_VISIT:
                AutoPictureInPicturePermissionControllerJni.get()
                        .setPermissionStatus(mWebContents, ContentSetting.ALLOW);
                break;
            case AutoPipPermissionDialogView.UiResult.BLOCK:
                AutoPictureInPicturePermissionControllerJni.get()
                        .setPermissionStatus(mWebContents, ContentSetting.BLOCK);
                mClosePipCallback.run();
                break;
            case AutoPipPermissionDialogView.UiResult.ALLOW_ONCE:
                assertNonNull(AutoPictureInPictureTabHelper.fromWebContents(mWebContents))
                        .setHasAllowOnce(true);
                break;
        }

        dismiss();
    }

    @NativeMethods
    interface Natives {
        @ContentSetting
        int getPermissionStatus(@JniType("content::WebContents*") WebContents webContents);

        void setPermissionStatus(
                @JniType("content::WebContents*") WebContents webContents,
                @ContentSetting int status);

        boolean isAutoPictureInPictureInUse(
                @JniType("content::WebContents*") WebContents webContents);

        void onPictureInPictureDismissed(@JniType("content::WebContents*") WebContents webContents);
    }
}
