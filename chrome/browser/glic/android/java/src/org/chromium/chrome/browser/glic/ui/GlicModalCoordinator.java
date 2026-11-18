// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Outline;
import android.graphics.drawable.ColorDrawable;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewOutlineProvider;
import android.view.Window;
import android.widget.FrameLayout;

import androidx.activity.ComponentDialog;
import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.glic.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetType;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChromeImageButton;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Responsive Android coordinator for {@code glic::GlicModal}.
 *
 * <ul>
 *   <li>On tablet or desktop windows ({@code >=600dp}), or when {@link BottomSheetController} is
 *       unavailable, presents the {@link ThinWebView} inside a centered {@link ModalDialogManager}
 *       card dialog.
 *   <li>On phone or compact windows ({@code <600dp}), presents the {@link ThinWebView} inside a
 *       single-state modal {@link BottomSheetController} bottom sheet.
 *   <li>On live window configuration changes across the {@code 600dp} breakpoint, dynamically swaps
 *       the existing {@link ThinWebView} between {@link ModalDialogManager} and {@link
 *       BottomSheetController} without reloading {@link WebContents} or notifying native dismissal.
 * </ul>
 */
@JNINamespace("glic")
@NullMarked
public class GlicModalCoordinator {
    @JniType("::glic::GlicModalAndroid")
    interface NativeGlicModalAndroid extends JniTypeToken {}

    @IntDef({ContainerMode.MODAL_DIALOG, ContainerMode.BOTTOM_SHEET})
    @Retention(RetentionPolicy.SOURCE)
    @interface ContainerMode {
        int MODAL_DIALOG = 0;
        int BOTTOM_SHEET = 1;
    }

    private @Nullable JniRawPtr<NativeGlicModalAndroid> mNativePtr;
    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final ModalDialogManager mModalDialogManager;
    private final @Nullable BottomSheetController mBottomSheetController;
    private final WebContents mWebContents;
    private final int mPortraitWidthDp;
    private final int mPortraitHeightDp;
    private final int mLandscapeWidthDp;
    private final int mLandscapeHeightDp;
    private final boolean mShowCloseButton;
    private final boolean mEnableAutoResize;
    private final @ColorInt int mBackgroundColor;
    // TODO(b/586590010): Remove once <600dp opt-in -> Glic bottom-sheet transition lands.
    private final boolean mAlwaysUseModalDialog;

    private @Nullable View mLoadingView;
    private boolean mRevealed;
    private int mContentHeightDip;
    private int mMaxOfferedHeightPx;
    private int mLastAvailWidthDip;
    private int mLastAvailHeightDip;

    private @ContainerMode int mCurrentContainerMode;
    private boolean mComponentCallbacksRegistered;
    private @Nullable PropertyModel mModel;
    private @Nullable GlicModalBottomSheetContent mSheetContent;
    private @Nullable ThinWebView mThinWebView;
    private @Nullable ContentView mContentView;
    private @Nullable ChromeImageButton mCloseButton;
    private int mTargetWidthPx;
    private int mTargetHeightPx;

    // The dialog window is wider than ModalDialogView, so its background shows as a strip on each
    // side of the web contents. Clear it; the card draws the rounded corners instead. Because the
    // dialog window is transparent and ThinWebView stays at alpha=0f until showContent(), any
    // custom loadingView must draw an opaque background so the scrim behind the dialog does not
    // bleed through.
    private final ModalDialogManagerObserver mDialogObserver =
            new ModalDialogManagerObserver() {
                @Override
                public void onDialogCreated(PropertyModel model, @Nullable ComponentDialog dialog) {
                    if (model != mModel || dialog == null) return;
                    Window window = dialog.getWindow();
                    if (window != null) {
                        window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
                    }
                }
            };

    private final ModalDialogProperties.Controller mDialogController =
            new ModalDialogProperties.Controller() {
                @Override
                public void onClick(
                        PropertyModel model, @ModalDialogProperties.ButtonType int buttonType) {}

                @Override
                public void onDismiss(
                        PropertyModel model, @DialogDismissalCause int dismissalCause) {
                    if (model != mModel) return;
                    onDialogDismissed();
                }
            };

    private final ComponentCallbacks mComponentCallbacks =
            new ComponentCallbacks() {
                @Override
                public void onConfigurationChanged(Configuration newConfig) {
                    GlicModalCoordinator.this.onConfigurationChanged(newConfig);
                }

                @Override
                public void onLowMemory() {}
            };

    /**
     * Creates and shows the modal dialog.
     *
     * <p>Note: Because the dialog window background is cleared to {@link Color#TRANSPARENT} and the
     * {@link ThinWebView} remains at {@code alpha=0f} until {@link #showContent()}, a non-null
     * {@code loadingView} must provide an opaque background.
     */
    @CalledByNative
    static @Nullable GlicModalCoordinator show(
            JniRawPtr<NativeGlicModalAndroid> nativePtr,
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid,
            @JniType("content::WebContents*") WebContents webContents,
            int portraitWidthDp,
            int portraitHeightDp,
            int landscapeWidthDp,
            int landscapeHeightDp,
            boolean showCloseButton,
            @Nullable View loadingView,
            boolean enableAutoResize,
            @JniType("std::optional<int32_t>") @Nullable @ColorInt Integer backgroundColor,
            boolean alwaysUseModalDialog) {
        JniRawPtr<NativeGlicModalAndroid> ptrToRelease = nativePtr;
        try {
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

            GlicModalCoordinator coordinator =
                    new GlicModalCoordinator(
                            nativePtr,
                            activity,
                            windowAndroid,
                            modalDialogManager,
                            BottomSheetControllerProvider.from(windowAndroid),
                            webContents,
                            portraitWidthDp,
                            portraitHeightDp,
                            landscapeWidthDp,
                            landscapeHeightDp,
                            showCloseButton,
                            loadingView,
                            enableAutoResize,
                            backgroundColor,
                            alwaysUseModalDialog);
            if (!coordinator.showInternal()) {
                ptrToRelease = null;
                coordinator.destroy();
                return null;
            }
            ptrToRelease = null;
            return coordinator;
        } finally {
            if (ptrToRelease != null) {
                ptrToRelease.release();
            }
        }
    }

    private GlicModalCoordinator(
            JniRawPtr<NativeGlicModalAndroid> nativePtr,
            Activity activity,
            WindowAndroid windowAndroid,
            ModalDialogManager modalDialogManager,
            @Nullable BottomSheetController bottomSheetController,
            WebContents webContents,
            int portraitWidthDp,
            int portraitHeightDp,
            int landscapeWidthDp,
            int landscapeHeightDp,
            boolean showCloseButton,
            @Nullable View loadingView,
            boolean enableAutoResize,
            @Nullable @ColorInt Integer backgroundColor,
            boolean alwaysUseModalDialog) {
        mNativePtr = nativePtr;
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mModalDialogManager = modalDialogManager;
        mBottomSheetController = bottomSheetController;
        mWebContents = webContents;
        mPortraitWidthDp = portraitWidthDp;
        mPortraitHeightDp = portraitHeightDp;
        mLandscapeWidthDp = landscapeWidthDp;
        mLandscapeHeightDp = landscapeHeightDp;
        mShowCloseButton = showCloseButton;
        mLoadingView = loadingView;
        mEnableAutoResize = enableAutoResize;
        mBackgroundColor =
                backgroundColor != null
                        ? backgroundColor
                        : SemanticColorUtils.getDefaultBgColor(activity);
        mAlwaysUseModalDialog = alwaysUseModalDialog;
        mRevealed = (loadingView == null);
        updateTargetSize(mActivity.getResources().getConfiguration());
    }

    private @ContainerMode int pickContainerMode(Configuration config) {
        // TODO(b/586590010): Remove `mAlwaysUseModalDialog` once <600dp opt-in -> Glic
        // bottom-sheet transition lands.
        if (mAlwaysUseModalDialog
                || mBottomSheetController == null
                || config.screenWidthDp == Configuration.SCREEN_WIDTH_DP_UNDEFINED
                || config.screenWidthDp >= 600) {
            return ContainerMode.MODAL_DIALOG;
        }
        return ContainerMode.BOTTOM_SHEET;
    }

    private void updateTargetSize(Configuration config) {
        // Despite the "screen" prefix, Configuration.screenWidthDp and screenHeightDp reflect the
        // Activity window's available dimensions (including in split-screen and freeform windows).
        boolean useLandscapeBounds =
                (config.screenWidthDp != Configuration.SCREEN_WIDTH_DP_UNDEFINED
                                && config.screenHeightDp
                                        != Configuration.SCREEN_HEIGHT_DP_UNDEFINED)
                        ? config.screenWidthDp > config.screenHeightDp
                        : config.orientation == Configuration.ORIENTATION_LANDSCAPE;
        int widthDp = useLandscapeBounds ? mLandscapeWidthDp : mPortraitWidthDp;
        int heightDp = useLandscapeBounds ? mLandscapeHeightDp : mPortraitHeightDp;
        mTargetWidthPx = ViewUtils.dpToPx(mActivity, widthDp);
        mTargetHeightPx = ViewUtils.dpToPx(mActivity, heightDp);
    }

    /** Returns capped pixel size for {@code targetPx} bounded by {@code spec}. */
    private static int capSizeToSpec(int targetPx, int spec) {
        int size = targetPx;
        if (MeasureSpec.getMode(spec) != MeasureSpec.UNSPECIFIED) {
            size = Math.min(size, MeasureSpec.getSize(spec));
        }
        return size;
    }

    private boolean showInternal() {
        mContentView = ContentView.createContentView(mActivity, mWebContents);
        mWebContents.setDelegates(
                VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(mContentView),
                mContentView,
                mWindowAndroid,
                WebContents.createDefaultInternalsHolder());

        var tracker = mWindowAndroid.getIntentRequestTracker();
        if (tracker == null) {
            return false;
        }

        ThinWebViewConstraints constraints = new ThinWebViewConstraints();
        constraints.supportsOpacity = true;
        constraints.backgroundColor = mBackgroundColor;
        mThinWebView =
                ThinWebViewFactory.create(
                        mActivity, constraints, tracker, /* enablePermissionRequests= */ true);
        mThinWebView.attachWebContents(
                mWebContents,
                mContentView,
                new ThinWebViewAttachParams.Builder().setSupportTheming(true).build());

        if (!mRevealed) {
            mThinWebView.getView().setAlpha(0f);
        }

        @ContainerMode int mode = pickContainerMode(mActivity.getResources().getConfiguration());
        if (mode != ContainerMode.BOTTOM_SHEET || !showInBottomSheet()) {
            showInModalDialog();
        }

        mActivity.registerComponentCallbacks(mComponentCallbacks);
        mComponentCallbacksRegistered = true;

        if (mRevealed) {
            mContentView.requestFocus();
        }
        return true;
    }

    private void detachChildrenFromParent() {
        if (mThinWebView != null) {
            UiUtils.removeViewFromParent(mThinWebView.getView());
        }
        if (mLoadingView != null) {
            UiUtils.removeViewFromParent(mLoadingView);
        }
    }

    private void maybeAttachCloseButton(FrameLayout container) {
        if (!mShowCloseButton) {
            mCloseButton = null;
            return;
        }
        LayoutInflater.from(mActivity)
                .inflate(R.layout.glic_modal_close_button, container, /* attachToRoot= */ true);
        ChromeImageButton closeButton = container.findViewById(R.id.glic_modal_close_button);
        closeButton.setOnClickListener(v -> dismiss());
        mCloseButton = closeButton;
    }

    private void applyContentHeight() {
        if (mThinWebView == null) return;
        int heightPx = ViewUtils.dpToPx(mActivity, mContentHeightDip);
        View webView = mThinWebView.getView();
        LayoutParams layoutParams = webView.getLayoutParams();
        if (layoutParams != null) {
            layoutParams.height = heightPx;
            webView.setLayoutParams(layoutParams);
        }
    }

    private @Nullable View getActiveContainerView() {
        if (mCurrentContainerMode == ContainerMode.MODAL_DIALOG && mModel != null) {
            return mModel.get(ModalDialogProperties.CUSTOM_VIEW);
        }
        if (mCurrentContainerMode == ContainerMode.BOTTOM_SHEET && mSheetContent != null) {
            return mSheetContent.getContentView();
        }
        return null;
    }

    private FrameLayout createContentContainer(
            ThinWebView thinWebView, @ContainerMode int containerMode) {
        detachChildrenFromParent();

        FrameLayout container =
                new FrameLayout(mActivity) {
                    @Override
                    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                        if (MeasureSpec.getMode(heightMeasureSpec) == MeasureSpec.AT_MOST) {
                            mMaxOfferedHeightPx =
                                    Math.max(
                                            mMaxOfferedHeightPx,
                                            MeasureSpec.getSize(heightMeasureSpec));
                        }
                        int availHeightPx =
                                mMaxOfferedHeightPx > 0
                                        ? Math.min(mTargetHeightPx, mMaxOfferedHeightPx)
                                        : capSizeToSpec(mTargetHeightPx, heightMeasureSpec);
                        int availWidthPx =
                                (containerMode == ContainerMode.BOTTOM_SHEET
                                                && MeasureSpec.getMode(widthMeasureSpec)
                                                        != MeasureSpec.UNSPECIFIED)
                                        ? MeasureSpec.getSize(widthMeasureSpec)
                                        : capSizeToSpec(mTargetWidthPx, widthMeasureSpec);

                        float density = getResources().getDisplayMetrics().density;
                        int availWidthDip = (int) Math.floor(availWidthPx / density);
                        int availHeightDip = (int) Math.floor(availHeightPx / density);

                        if (availWidthDip != mLastAvailWidthDip
                                || availHeightDip != mLastAvailHeightDip) {
                            mLastAvailWidthDip = availWidthDip;
                            mLastAvailHeightDip = availHeightDip;
                            if (mNativePtr != null) {
                                GlicModalCoordinatorJni.get()
                                        .onAvailableSizeChanged(
                                                mNativePtr, availWidthDip, availHeightDip);
                            }
                        }

                        int childHeightMode =
                                (mEnableAutoResize && mRevealed && mContentHeightDip > 0)
                                        ? MeasureSpec.AT_MOST
                                        : MeasureSpec.EXACTLY;
                        super.onMeasure(
                                MeasureSpec.makeMeasureSpec(availWidthPx, MeasureSpec.EXACTLY),
                                MeasureSpec.makeMeasureSpec(availHeightPx, childHeightMode));
                    }
                };
        container.addView(
                thinWebView.getView(),
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        if (!mRevealed && mLoadingView != null) {
            container.addView(
                    mLoadingView,
                    new FrameLayout.LayoutParams(
                            LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        }

        if (mRevealed && mEnableAutoResize && mContentHeightDip > 0) {
            applyContentHeight();
        }

        return container;
    }

    private void showInModalDialog() {
        if (mThinWebView == null) {
            return;
        }

        FrameLayout cardContainer =
                createContentContainer(mThinWebView, ContainerMode.MODAL_DIALOG);
        cardContainer.setLayoutParams(
                new FrameLayout.LayoutParams(
                        LayoutParams.WRAP_CONTENT,
                        LayoutParams.WRAP_CONTENT,
                        Gravity.CENTER_HORIZONTAL));
        int cornerRadiusPx =
                mActivity.getResources().getDimensionPixelSize(R.dimen.dialog_corner_radius);
        cardContainer.setOutlineProvider(
                new ViewOutlineProvider() {
                    @Override
                    public void getOutline(View view, Outline outline) {
                        outline.setRoundRect(
                                0, 0, view.getWidth(), view.getHeight(), cornerRadiusPx);
                    }
                });
        cardContainer.setClipToOutline(true);

        maybeAttachCloseButton(cardContainer);

        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, mDialogController)
                        .with(ModalDialogProperties.CUSTOM_VIEW, cardContainer)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .with(ModalDialogProperties.MAX_HEIGHT, mTargetHeightPx)
                        .build();

        mCurrentContainerMode = ContainerMode.MODAL_DIALOG;
        mModalDialogManager.addObserver(mDialogObserver);
        mModalDialogManager.showDialog(mModel, ModalDialogType.APP);
    }

    private boolean showInBottomSheet() {
        if (mThinWebView == null || mBottomSheetController == null) {
            return false;
        }

        FrameLayout sheetContainer =
                createContentContainer(mThinWebView, ContainerMode.BOTTOM_SHEET);
        sheetContainer.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        maybeAttachCloseButton(sheetContainer);

        GlicModalBottomSheetContent sheetContent = new GlicModalBottomSheetContent(sheetContainer);
        if (!mBottomSheetController.requestShowContent(sheetContent, /* animate= */ true)) {
            mBottomSheetController.hideContent(sheetContent, /* animate= */ false);
            return false;
        }
        mSheetContent = sheetContent;
        mCurrentContainerMode = ContainerMode.BOTTOM_SHEET;
        return true;
    }

    private void swapContainer(@ContainerMode int targetMode) {
        if (targetMode == ContainerMode.MODAL_DIALOG) {
            if (mSheetContent != null && mBottomSheetController != null) {
                GlicModalBottomSheetContent content = mSheetContent;
                mSheetContent = null;
                mBottomSheetController.hideContent(content, /* animate= */ false);
            }
            showInModalDialog();
        } else {
            if (mModel != null) {
                PropertyModel model = mModel;
                mModel = null;
                mModalDialogManager.removeObserver(mDialogObserver);
                mModalDialogManager.dismissDialog(model, DialogDismissalCause.DISMISSED_BY_NATIVE);
            }
            if (!showInBottomSheet()) {
                showInModalDialog();
            }
        }
    }

    @CalledByNative
    void onContentSizeChanged(int heightDip) {
        mContentHeightDip = heightDip;
        if (!mRevealed) {
            return;
        }
        applyContentHeight();
    }

    @CalledByNative
    void showContent() {
        if (mRevealed) {
            return;
        }
        mRevealed = true;
        if (mEnableAutoResize && mContentHeightDip > 0) {
            applyContentHeight();
        }
        if (mThinWebView != null) {
            mThinWebView.getView().setAlpha(1f);
        }
        if (mLoadingView != null) {
            UiUtils.removeViewFromParent(mLoadingView);
            mLoadingView = null;
        }
        if (mContentView != null) {
            mContentView.requestFocus();
        }
        View activeContainer = getActiveContainerView();
        if (activeContainer != null) {
            ViewUtils.requestLayout(activeContainer, "GlicModalCoordinator.showContent");
        }
    }

    private void onConfigurationChanged(Configuration newConfig) {
        if (mModel == null && mSheetContent == null) {
            return;
        }
        mMaxOfferedHeightPx = 0;
        mLastAvailWidthDip = 0;
        mLastAvailHeightDip = 0;
        updateTargetSize(newConfig);

        @ContainerMode int targetMode = pickContainerMode(newConfig);
        if (targetMode != mCurrentContainerMode) {
            swapContainer(targetMode);
            return;
        }

        if (mRevealed && mEnableAutoResize && mContentHeightDip > 0) {
            applyContentHeight();
        }

        if (mCurrentContainerMode == ContainerMode.MODAL_DIALOG && mModel != null) {
            mModel.set(ModalDialogProperties.MAX_HEIGHT, mTargetHeightPx);
        }
        View activeContainer = getActiveContainerView();
        if (activeContainer != null) {
            ViewUtils.requestLayout(activeContainer, "GlicModalCoordinator.onConfigurationChanged");
        }
    }

    @CalledByNative
    private void dismiss() {
        if (mCurrentContainerMode == ContainerMode.MODAL_DIALOG && mModel != null) {
            PropertyModel model = mModel;
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.DISMISSED_BY_NATIVE);
            if (mModel == model) {
                onDialogDismissed();
            }
        } else if (mCurrentContainerMode == ContainerMode.BOTTOM_SHEET
                && mSheetContent != null
                && mBottomSheetController != null) {
            GlicModalBottomSheetContent sheetContent = mSheetContent;
            mBottomSheetController.hideContent(
                    sheetContent, /* animate= */ false, StateChangeReason.INTERACTION_COMPLETE);
            if (mSheetContent == sheetContent) {
                onDialogDismissed();
            }
        }
    }

    private void onDialogDismissed() {
        JniRawPtr<NativeGlicModalAndroid> ptr = mNativePtr;
        mNativePtr = null;
        try {
            destroy();
            if (ptr != null) {
                GlicModalCoordinatorJni.get().onDismissed(ptr);
            }
        } finally {
            if (ptr != null) {
                ptr.release();
            }
        }
    }

    private void destroy() {
        if (mNativePtr != null) {
            mNativePtr.release();
            mNativePtr = null;
        }
        if (mComponentCallbacksRegistered) {
            mActivity.unregisterComponentCallbacks(mComponentCallbacks);
            mComponentCallbacksRegistered = false;
        }
        mMaxOfferedHeightPx = 0;
        mLastAvailWidthDip = 0;
        mLastAvailHeightDip = 0;
        mModalDialogManager.removeObserver(mDialogObserver);
        mSheetContent = null;
        if (mThinWebView != null) {
            mThinWebView.destroy();
            mThinWebView = null;
        }
        if (mLoadingView != null) {
            UiUtils.removeViewFromParent(mLoadingView);
            mLoadingView = null;
        }
        mCloseButton = null;
        mContentView = null;
        mModel = null;
    }

    @CalledByNative
    void onNativeDestroyed() {
        // Must clear before dismiss(): onDismiss() runs synchronously, so
        // onDialogDismissed() would otherwise call back into a destructing host.
        if (mNativePtr != null) {
            mNativePtr.release();
            mNativePtr = null;
        }
        dismiss();
        destroy();
    }

    /**
     * Dismisses the modal the same way the user would (e.g. by pressing back), so that tests can
     * exercise the full Java -> native dismissal path.
     */
    @CalledByNativeForTesting
    private void simulateDismissingForTesting() {
        if (mCurrentContainerMode == ContainerMode.MODAL_DIALOG && mModel != null) {
            mModalDialogManager.dismissDialog(
                    mModel, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        } else if (mCurrentContainerMode == ContainerMode.BOTTOM_SHEET
                && mSheetContent != null
                && mBottomSheetController != null) {
            GlicModalBottomSheetContent sheetContent = mSheetContent;
            mBottomSheetController.hideContent(
                    sheetContent, /* animate= */ true, StateChangeReason.BACK_PRESS);
            if (mSheetContent == sheetContent
                    && mBottomSheetController.getCurrentSheetContent() != sheetContent) {
                onDialogDismissed();
            }
        }
    }

    private class GlicModalBottomSheetContent implements BottomSheetContent {
        private static final BottomSheetType BOTTOM_SHEET_TYPE =
                new BottomSheetType.Builder().setUserInitiated(true).setModal(true).build();

        private final View mSheetView;

        GlicModalBottomSheetContent(View sheetView) {
            mSheetView = sheetView;
        }

        @Override
        public View getContentView() {
            return mSheetView;
        }

        @Override
        public @Nullable View getToolbarView() {
            return null;
        }

        @Override
        public int getVerticalScrollOffset() {
            if (!mWebContents.isDestroyed()) {
                return RenderCoordinates.fromWebContents(mWebContents).getScrollYPixInt();
            }
            return 0;
        }

        @Override
        public void destroy() {
            if (mSheetContent == this) {
                onDialogDismissed();
            }
        }

        @Override
        public BottomSheetType getSheetType() {
            return BOTTOM_SHEET_TYPE;
        }

        @Override
        public boolean swipeToDismissEnabled() {
            return true;
        }

        @Override
        public float getHalfHeightRatio() {
            return HeightMode.DISABLED;
        }

        @Override
        public float getFullHeightRatio() {
            return HeightMode.WRAP_CONTENT;
        }

        @Override
        public boolean showHandlebar() {
            return true;
        }

        @Override
        public @ColorInt int getSheetBackgroundColorOverride() {
            return mBackgroundColor;
        }

        @Override
        public @StringRes int getSheetHalfHeightAccessibilityStringId() {
            return R.string.glic_bottom_sheet_half_height_a11y_label;
        }

        @Override
        public @StringRes int getSheetFullHeightAccessibilityStringId() {
            return R.string.glic_bottom_sheet_full_height_a11y_label;
        }

        @Override
        public @StringRes int getSheetClosedAccessibilityStringId() {
            return R.string.glic_bottom_sheet_hidden_a11y_label;
        }
    }

    @ContainerMode
    int getContainerModeForTesting() {
        return mCurrentContainerMode;
    }

    @Nullable PropertyModel getPropertyModelForTesting() {
        return mModel;
    }

    @Nullable BottomSheetContent getBottomSheetContentForTesting() {
        return mSheetContent;
    }

    @Nullable ChromeImageButton getCloseButtonForTesting() {
        return mCloseButton;
    }

    boolean isRevealedForTesting() {
        return mRevealed;
    }

    @NativeMethods
    interface Natives {
        void onDismissed(JniPtr<NativeGlicModalAndroid> self);

        void onAvailableSizeChanged(
                JniPtr<NativeGlicModalAndroid> self, int widthDip, int heightDip);
    }
}
