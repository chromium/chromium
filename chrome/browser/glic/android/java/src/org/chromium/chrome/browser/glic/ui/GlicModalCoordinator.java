// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import android.app.Activity;
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
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
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

/**
 * Android coordinator for {@code glic::GlicModal}.
 *
 * <p>Presents a {@link ThinWebView} inside a centered {@link ModalDialogManager} card dialog with
 * portrait/landscape sizing, rounded corners, and an optional overlay close button.
 */
@JNINamespace("glic")
@NullMarked
public class GlicModalCoordinator {
    @JniType("::glic::GlicModalAndroid")
    interface NativeGlicModalAndroid extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeGlicModalAndroid> mNativePtr;
    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final ModalDialogManager mModalDialogManager;
    private final WebContents mWebContents;
    private final int mPortraitWidthDp;
    private final int mPortraitHeightDp;
    private final int mLandscapeWidthDp;
    private final int mLandscapeHeightDp;
    private final boolean mShowCloseButton;
    private final boolean mEnableAutoResize;

    private @Nullable View mLoadingView;
    private boolean mRevealed;
    private int mContentHeightDip;
    private int mMaxOfferedHeightPx;
    private int mLastAvailWidthDip;
    private int mLastAvailHeightDip;

    private @Nullable PropertyModel mModel;
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
            boolean enableAutoResize) {
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
                            webContents,
                            portraitWidthDp,
                            portraitHeightDp,
                            landscapeWidthDp,
                            landscapeHeightDp,
                            showCloseButton,
                            loadingView,
                            enableAutoResize);
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
            WebContents webContents,
            int portraitWidthDp,
            int portraitHeightDp,
            int landscapeWidthDp,
            int landscapeHeightDp,
            boolean showCloseButton,
            @Nullable View loadingView,
            boolean enableAutoResize) {
        mNativePtr = nativePtr;
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mModalDialogManager = modalDialogManager;
        mWebContents = webContents;
        mPortraitWidthDp = portraitWidthDp;
        mPortraitHeightDp = portraitHeightDp;
        mLandscapeWidthDp = landscapeWidthDp;
        mLandscapeHeightDp = landscapeHeightDp;
        mShowCloseButton = showCloseButton;
        mLoadingView = loadingView;
        mEnableAutoResize = enableAutoResize;
        mRevealed = (loadingView == null);
        updateTargetSize();
    }

    private void updateTargetSize() {
        Configuration config = mActivity.getResources().getConfiguration();
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

        if (!mRevealed) {
            mThinWebView.getView().setAlpha(0f);
        }

        if (!showInModalDialog()) {
            return false;
        }

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

    private boolean showInModalDialog() {
        if (mThinWebView == null) {
            return false;
        }

        detachChildrenFromParent();

        FrameLayout cardContainer =
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
                        int availWidthPx = capSizeToSpec(mTargetWidthPx, widthMeasureSpec);

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

                    @Override
                    protected void onConfigurationChanged(Configuration newConfig) {
                        super.onConfigurationChanged(newConfig);
                        GlicModalCoordinator.this.onConfigurationChanged();
                    }
                };
        cardContainer.addView(
                mThinWebView.getView(),
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        if (!mRevealed && mLoadingView != null) {
            cardContainer.addView(
                    mLoadingView,
                    new FrameLayout.LayoutParams(
                            LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        }

        if (mRevealed && mEnableAutoResize && mContentHeightDip > 0) {
            applyContentHeight();
        }

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

        mModalDialogManager.addObserver(mDialogObserver);
        mModalDialogManager.showDialog(mModel, ModalDialogType.APP);
        return true;
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
        if (mModel != null) {
            View customView = mModel.get(ModalDialogProperties.CUSTOM_VIEW);
            if (customView != null) {
                customView.requestLayout();
            }
        }
    }

    private void onConfigurationChanged() {
        mMaxOfferedHeightPx = 0;
        mLastAvailWidthDip = 0;
        mLastAvailHeightDip = 0;
        updateTargetSize();
        if (mRevealed && mEnableAutoResize && mContentHeightDip > 0) {
            applyContentHeight();
        }
        if (mModel != null) {
            mModel.set(ModalDialogProperties.MAX_HEIGHT, mTargetHeightPx);
            View customView = mModel.get(ModalDialogProperties.CUSTOM_VIEW);
            if (customView != null) {
                customView.requestLayout();
            }
        }
    }

    @CalledByNative
    private void dismiss() {
        if (mModel != null) {
            PropertyModel model = mModel;
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.DISMISSED_BY_NATIVE);
            if (mModel == model) {
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
        mMaxOfferedHeightPx = 0;
        mLastAvailWidthDip = 0;
        mLastAvailHeightDip = 0;
        mModalDialogManager.removeObserver(mDialogObserver);
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

    public boolean isRevealedForTesting() {
        return mRevealed;
    }

    @NativeMethods
    interface Natives {
        void onDismissed(JniPtr<NativeGlicModalAndroid> self);

        void onAvailableSizeChanged(
                JniPtr<NativeGlicModalAndroid> self, int widthDip, int heightDip);
    }
}
