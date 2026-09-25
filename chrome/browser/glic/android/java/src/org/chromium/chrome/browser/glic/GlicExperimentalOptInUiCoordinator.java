// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.app.Activity;
import android.util.DisplayMetrics;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * Coordinator for displaying the Glic experimental opt-in dialog on Android using
 * ModalDialogManager. It displays a centered modal card dialog containing a ThinWebView rendering
 * the opt-in WebUI.
 */
@JNINamespace("glic")
@NullMarked
public class GlicExperimentalOptInUiCoordinator {
    private long mNativePtr;
    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final ModalDialogManager mModalDialogManager;
    private final WebContents mWebContents;
    private final int mTargetWidthPx;
    private final int mTargetHeightPx;
    private @Nullable PropertyModel mModel;
    private @Nullable ThinWebView mThinWebView;
    private @Nullable ContentView mContentView;
    private @Nullable ChromeImageButton mCloseButton;

    private final ModalDialogProperties.Controller mDialogController =
            new ModalDialogProperties.Controller() {
                @Override
                public void onClick(
                        PropertyModel model, @ModalDialogProperties.ButtonType int buttonType) {}

                @Override
                public void onDismiss(
                        PropertyModel model, @DialogDismissalCause int dismissalCause) {
                    onDialogDismissed();
                }
            };

    @CalledByNative
    public static @Nullable GlicExperimentalOptInUiCoordinator show(
            long nativePtr,
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid,
            @JniType("content::WebContents*") WebContents webContents) {
        if (windowAndroid.getActivity() == null) {
            return null;
        }
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null || activity.isFinishing() || activity.isDestroyed()) {
            return null;
        }
        ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
        if (modalDialogManager == null) {
            return null;
        }

        GlicExperimentalOptInUiCoordinator coordinator =
                new GlicExperimentalOptInUiCoordinator(
                        nativePtr, activity, windowAndroid, modalDialogManager, webContents);
        if (!coordinator.showInternal()) {
            return null;
        }
        return coordinator;
    }

    private GlicExperimentalOptInUiCoordinator(
            long nativePtr,
            Activity activity,
            WindowAndroid windowAndroid,
            ModalDialogManager modalDialogManager,
            WebContents webContents) {
        mNativePtr = nativePtr;
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mModalDialogManager = modalDialogManager;
        mWebContents = webContents;
        // TODO(crbug.com/559823681): Investigate using ModalDialogProperties for sizing
        // rather than manually calculating target dimensions on the custom view.
        DisplayMetrics displayMetrics = mActivity.getResources().getDisplayMetrics();
        int marginPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.modal_dialog_view_external_margin);
        int maxWidthPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.glic_experimental_opt_in_dialog_max_width);
        int maxHeightPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                ChromeFeatureList.isEnabled(
                                                ChromeFeatureList
                                                        .GLIC_EXPERIMENTAL_OPT_IN_DIALOG_NON_SCROLLABLE)
                                        ? R.dimen
                                                .glic_experimental_opt_in_dialog_non_scrollable_max_height
                                        : R.dimen.glic_experimental_opt_in_dialog_max_height);
        mTargetWidthPx =
                Math.min(maxWidthPx, Math.max(0, displayMetrics.widthPixels - 2 * marginPx));
        mTargetHeightPx =
                Math.min(maxHeightPx, Math.max(0, displayMetrics.heightPixels - 2 * marginPx));
    }

    private boolean showInternal() {
        mContentView = ContentView.createContentView(mActivity, mWebContents);
        mWebContents.setDelegates(
                VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(mContentView),
                mContentView,
                mWindowAndroid,
                WebContents.createDefaultInternalsHolder());

        int backgroundColor = SemanticColorUtils.getDefaultBgColor(mActivity);

        var tracker = mWindowAndroid.getIntentRequestTracker();
        if (tracker == null) {
            return false;
        }

        ThinWebViewConstraints constraints = new ThinWebViewConstraints();
        constraints.supportsOpacity = true;
        constraints.backgroundColor = backgroundColor;
        mThinWebView =
                ThinWebViewFactory.create(
                        mActivity, constraints, tracker, /* enablePermissionRequests= */ true);
        mThinWebView.attachWebContents(
                mWebContents,
                mContentView,
                new ThinWebViewAttachParams.Builder().setSupportTheming(true).build());

        // Card container to size ThinWebView within the modal dialog.
        FrameLayout cardContainer = new FrameLayout(mActivity);
        cardContainer.addView(
                mThinWebView.getView(),
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        cardContainer.setLayoutParams(new ViewGroup.LayoutParams(mTargetWidthPx, mTargetHeightPx));

        // The close button is overlaid on top of the web contents rather than using
        // ModalDialogProperties.TITLE_CLOSE_BUTTON_*. This dialog has no title, so the shared
        // title row would collapse to just the close button, aligning it to the start and
        // exposing a strip of the dialog's window background above the web contents.
        LayoutInflater.from(mActivity)
                .inflate(
                        R.layout.glic_experimental_opt_in_close_button,
                        cardContainer,
                        /* attachToRoot= */ true);
        ChromeImageButton closeButton =
                cardContainer.findViewById(R.id.glic_experimental_opt_in_close_button);
        closeButton.setOnClickListener(v -> dismiss());
        mCloseButton = closeButton;

        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, mDialogController)
                        .with(ModalDialogProperties.CUSTOM_VIEW, cardContainer)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .build();

        mModalDialogManager.showDialog(mModel, ModalDialogManager.ModalDialogType.APP);
        mContentView.requestFocus();
        return true;
    }

    @CalledByNative
    private void dismiss() {
        if (mModel != null) {
            mModalDialogManager.dismissDialog(mModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
        }
    }

    private void onDialogDismissed() {
        if (mNativePtr != 0) {
            GlicExperimentalOptInUiCoordinatorJni.get().onDismissed(mNativePtr);
        }
        destroy();
    }

    private void destroy() {
        mNativePtr = 0;
        if (mThinWebView != null) {
            mThinWebView.destroy();
            mThinWebView = null;
        }
        mCloseButton = null;
        mContentView = null;
        mModel = null;
    }

    @CalledByNative
    void onNativeDestroyed() {
        // Must clear before dismiss(): onDismiss() runs synchronously, so
        // onDialogDismissed() would otherwise call back into a destructing host.
        mNativePtr = 0;
        dismiss();
    }

    /**
     * Dismisses the dialog the same way the user would (e.g. by pressing back), so that tests can
     * exercise the full Java -> native dismissal path. Notifying native directly instead would
     * leave the dialog on screen holding a stale native pointer.
     */
    @CalledByNativeForTesting
    void simulateDismissingForTesting() {
        if (mModel != null) {
            mModalDialogManager.dismissDialog(
                    mModel, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        }
    }

    public @Nullable PropertyModel getPropertyModelForTesting() {
        return mModel;
    }

    public @Nullable ChromeImageButton getCloseButtonForTesting() {
        return mCloseButton;
    }

    @NativeMethods
    interface Natives {
        void onDismissed(long nativeGlicExperimentalOptInUIHostAndroid);
    }
}
