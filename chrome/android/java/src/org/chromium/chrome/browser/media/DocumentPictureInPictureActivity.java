// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static android.view.Display.INVALID_DISPLAY;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.ActivityManager.AppTask;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.FrameLayout;

import androidx.annotation.CallSuper;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatDelegate;

import org.jni_zero.NativeMethods;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tab_activity_glue.PopupCreatorImpl;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.customtabs.PopupCreatorFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.media.document_picture_in_picture_header.DocumentPictureInPictureHeaderCoordinator;
import org.chromium.chrome.browser.media.document_picture_in_picture_header.DocumentPictureInPictureHeaderDelegate;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils.WebContentsOfflinePageLoadUrlDelegate;
import org.chromium.chrome.browser.page_info.ChromePageInfoControllerDelegate;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.toolbar.AppThemeColorProvider;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderCoordinator;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.chrome.browser.util.PictureInPictureWindowOptions;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PageInfoController.OpenedFromSource;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

@NullMarked
public class DocumentPictureInPictureActivity extends AsyncInitializationActivity
        implements DocumentPictureInPictureHeaderDelegate {
    private static final String TAG = "DocumentPiPActivity";
    // Tolerance in DP to filter out small rounding drifts and system snaps in PiP mode.
    private static final int SIZE_TOLERANCE_DP = 3;
    public static final String WEB_CONTENTS_KEY =
            "org.chromium.chrome.browser.media.DocumentPictureInPicture.WebContents";
    public static final String WINDOW_OPTIONS_KEY =
            "org.chromium.chrome.browser.media.DocumentPictureInPicture.WindowOptions";
    public static final String INITIAL_OPENER_ORIGIN_KEY =
            "org.chromium.chrome.browser.media.DocumentPictureInPicture.InitialOpenerOrigin";
    private static final String IS_FROM_ACTIVITY_RECREATION_KEY =
            "org.chromium.chrome.browser.media.DocumentPictureInPicture.IsFromActivityRecreation";
    private WebContents mWebContents;
    private WebContents mParentWebContents;
    private Tab mInitiatorTab;
    private @MonotonicNonNull ThinWebView mThinWebView;
    private @MonotonicNonNull TabObserver mInitiatorTabObserver;
    private @MonotonicNonNull PictureInPictureWindowOptions mWindowOptions;
    private @MonotonicNonNull AppHeaderCoordinator mAppHeaderCoordinator;
    private @MonotonicNonNull DocumentPictureInPictureHeaderCoordinator mHeaderCoordinator;
    private @MonotonicNonNull AppThemeColorProvider mAppThemeColorProvider;
    private boolean mIsRecreating;
    private boolean mIsFromActivityRecreation;
    private @MonotonicNonNull Configuration mConfig;
    private boolean mIsPinned;
    private @Nullable Rect mPromptEnforcedBounds;
    private @Nullable Integer mMinPromptWidthPx;
    private @Nullable Integer mMinPromptHeightPx;
    private @MonotonicNonNull DocumentPictureInPictureNightModeStateProvider
            mNightModeStateProvider;

    private static @Nullable WebContents sWebContentsForTesting;
    private static @Nullable WebContents sParentWebContentsForTesting;
    // TODO(crbug.com/481216447): Remove this testing bypass once CI supports Android B (API 36).
    private static boolean sIgnoreSdkVersionForTesting;

    @Override
    public void performPreInflationStartup() {
        super.performPreInflationStartup();

        Intent intent = getIntent();
        Bundle savedInstanceState = getSavedInstanceState();
        mIsFromActivityRecreation =
                savedInstanceState != null
                        && savedInstanceState.getBoolean(IS_FROM_ACTIVITY_RECREATION_KEY, false);

        WebContents webContents =
                sWebContentsForTesting != null
                        ? sWebContentsForTesting
                        : getWebContentsFromInstanceStateOrIntent(intent, savedInstanceState);
        if (webContents == null || webContents.isDestroyed()) {
            Log.e(TAG, "WebContents is null or destroyed, finishing.");
            finish();
            return;
        }

        mWebContents = webContents;
        WebContents parentWebContents =
                sParentWebContentsForTesting != null
                        ? sParentWebContentsForTesting
                        : mWebContents.getDocumentPictureInPictureOpener();
        mInitiatorTab = TabUtils.fromWebContents(parentWebContents);
        if (parentWebContents == null
                || mInitiatorTab == null
                // During activity recreation, the initiator tab activity may not be available
                // because of the tab reparenting process.
                || (TabUtils.getActivity(mInitiatorTab) == null && !mIsFromActivityRecreation)) {
            Log.e(TAG, "Parent web contents or initiator tab is null, finishing.");
            finish();
            return;
        }
        mParentWebContents = parentWebContents;

        if (!verifyOpenerOrigin(intent, parentWebContents)) {
            finish();
            return;
        }

        Bundle windowOptionsBundle =
                getWindowOptionsBundleFromInstanceStateOrIntent(intent, savedInstanceState);
        if (windowOptionsBundle == null) {
            Log.e(TAG, "Window options bundle is null, finishing.");
            finish();
            return;
        }
        mWindowOptions = new PictureInPictureWindowOptions(windowOptionsBundle);
        mConfig = getResources().getConfiguration();

        goIntoPinnedMode();
    }

    private @Nullable WebContents getWebContentsFromInstanceStateOrIntent(
            Intent intent, @Nullable Bundle savedInstanceState) {
        if (mIsFromActivityRecreation) {
            // It's guaranteed that savedInstanceState is not null if we are coming from activity
            // recreation.
            assert savedInstanceState != null;
            return savedInstanceState.getParcelable(WEB_CONTENTS_KEY);
        }

        intent.setExtrasClassLoader(WebContents.class.getClassLoader());
        return intent.getParcelableExtra(WEB_CONTENTS_KEY);
    }

    private @Nullable Bundle getWindowOptionsBundleFromInstanceStateOrIntent(
            Intent intent, @Nullable Bundle savedInstanceState) {
        if (mIsFromActivityRecreation) {
            // It's guaranteed that savedInstanceState is not null if we are coming from activity
            // recreation.
            assert savedInstanceState != null;
            return savedInstanceState.getBundle(WINDOW_OPTIONS_KEY);
        }

        return intent.getBundleExtra(WINDOW_OPTIONS_KEY);
    }

    /**
     * @return Whether the document pip WebContents and the initiator tab are both initialized.
     */
    @EnsuresNonNullIf({"mWebContents", "mInitiatorTab", "mParentWebContents"})
    private boolean isContentsInitialized() {
        return mWebContents != null && mInitiatorTab != null && mParentWebContents != null;
    }

    @Override
    @SuppressLint("NewAPI")
    public void onStart() {
        super.onStart();
        assert isContentsInitialized();

        mInitiatorTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onClosingStateChanged(Tab tab, boolean closing) {
                        if (closing) {
                            finish();
                        }
                    }

                    @Override
                    public void onDestroyed(Tab tab) {
                        if (tab.isClosing()) {
                            finish();
                        }
                    }

                    @Override
                    public void onCrash(Tab tab) {
                        finish();
                    }
                };
        mInitiatorTab.addObserver(mInitiatorTabObserver);

        // EdgeToEdgeStateProvider is set in ChromeBaseAppCompatActivity#onCreate.
        var edgeToEdgeStateProvider = getEdgeToEdgeStateProvider();
        assert edgeToEdgeStateProvider != null;

        mAppHeaderCoordinator =
                new AppHeaderCoordinator(
                        this,
                        getWindow().getDecorView().getRootView(),
                        new BrowserStateBrowserControlsVisibilityDelegate(
                                ObservableSuppliers.alwaysFalse()),
                        getInsetObserver(),
                        getLifecycleDispatcher(),
                        getSavedInstanceState(),
                        getPersistentInstanceState(),
                        edgeToEdgeStateProvider,
                        null);

        mAppThemeColorProvider =
                new AppThemeColorProvider(this, getLifecycleDispatcher(), mAppHeaderCoordinator);
        mAppThemeColorProvider.onIncognitoStateChanged(mInitiatorTab.isIncognitoBranded());
    }

    private void goIntoPinnedMode() {
        if (!sIgnoreSdkVersionForTesting && Build.VERSION.SDK_INT < Build.VERSION_CODES.BAKLAVA) {
            Log.e(TAG, "SDK version is too low, minimum required is 36.");
            finish();
            return;
        }

        final AconfigFlaggedApiDelegate aconfigFlaggedApiDelegate =
                AconfigFlaggedApiDelegate.getInstance();
        if (aconfigFlaggedApiDelegate == null) {
            Log.e(TAG, "AconfigFlaggedApiDelegate is null");
            finish();
            return;
        }

        final AppTask appTask = AndroidTaskUtils.getAppTaskFromId(this, getTaskId());
        if (appTask == null) {
            Log.e(TAG, "AppTask is null");
            finish();
            return;
        }

        aconfigFlaggedApiDelegate
                .requestPinnedWindowingLayer(appTask, getMainExecutor())
                .then(
                        (unused) -> mIsPinned = true,
                        (e) -> {
                            Log.e(
                                    TAG,
                                    "Failed to request pinned windowing layer."
                                            + (e == null ? "" : e.getMessage()));
                            finish();
                        });
    }

    @Override
    public void initializeCompositor() {
        // Guard against the asynchronous startup gap. Because initializeCompositor()
        // is posted to the UI thread, the opener WebContents could have navigated
        // to a different origin before the child WebContents delegate is attached.
        // If that happens, verify the origin to abort and prevent origin spoofing.
        if (mParentWebContents == null
                || mParentWebContents.isDestroyed()
                || !verifyOpenerOrigin(getIntent(), mParentWebContents)) {
            finish();
            return;
        }
        PopupCreatorFactory.setInstance(new PopupCreatorImpl());
        ActivityWindowAndroid windowAndroid = getWindowAndroid();
        if (windowAndroid == null) {
            windowAndroid = createWindowAndroid();
        }
        ContentView contentView = ContentView.createContentView(this, mWebContents);
        mThinWebView = ThinWebViewFactory.create(this, new ThinWebViewConstraints(), windowAndroid);
        mWebContents.setDelegates(
                VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(contentView),
                contentView,
                windowAndroid,
                WebContents.createDefaultInternalsHolder());
        mThinWebView.attachWebContents(
                mWebContents,
                contentView,
                new ThinWebViewAttachParams.Builder()
                        .setWebContentsDelegate(new DocumentPictureInPictureWebContentsDelegate())
                        .build());

        View rootLayout =
                getLayoutInflater().inflate(R.layout.document_picture_in_picture_main_layout, null);
        FrameLayout contentLayout =
                rootLayout.findViewById(R.id.document_picture_in_picture_content);
        contentLayout.addView(
                mThinWebView.getView(),
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT);
        setContentView(rootLayout);

        mHeaderCoordinator =
                new DocumentPictureInPictureHeaderCoordinator(
                        findViewById(R.id.document_picture_in_picture_header),
                        assumeNonNull(mAppHeaderCoordinator),
                        assumeNonNull(mAppThemeColorProvider),
                        /* context= */ this,
                        /* delegate= */ this,
                        !assumeNonNull(mWindowOptions).disallowReturnToOpener,
                        mParentWebContents,
                        mWebContents);

        if (ChromeFeatureList.sAutoDocPipPermissionPromptAndroid.isEnabled()) {
            WebContents webContents = mParentWebContents;
            if (webContents != null
                    && AutoPictureInPicturePermissionController.isAutoPictureInPictureInUse(
                            webContents)) {
                mThinWebView
                        .getView()
                        .post(
                                () ->
                                        AutoPictureInPicturePermissionController.showPromptIfNeeded(
                                                this, mInitiatorTab, this::finish));
            }
        }

        setupInitialBoundsListener(contentLayout);
    }

    private void setupInitialBoundsListener(View contentLayout) {
        if (mWindowOptions != null && mWindowOptions.windowBounds != null) {
            contentLayout
                    .getViewTreeObserver()
                    .addOnGlobalLayoutListener(
                            new ViewTreeObserver.OnGlobalLayoutListener() {
                                @Override
                                public void onGlobalLayout() {
                                    applyInitialLayoutAndEnlargement();
                                    contentLayout
                                            .getViewTreeObserver()
                                            .removeOnGlobalLayoutListener(this);
                                }
                            });
        }
    }

    /**
     * Applies the initial layout bounds to the PiP window on startup.
     *
     * <p>If an Auto-PiP permission prompt is needed, this method ensures the window is large enough
     * to fit the prompt comfortably by enlarging it to the minimum required dimensions if
     * necessary.
     */
    private void applyInitialLayoutAndEnlargement() {
        var windowOptions = assumeNonNull(mWindowOptions);
        var windowBounds = assumeNonNull(windowOptions.windowBounds);
        int width = windowBounds.width();
        int height = windowBounds.height();

        // Check if we need to show the permission prompt and thus might need to enlarge the window.
        boolean isPermissionPromptNeeded = false;
        if (ChromeFeatureList.sAutoDocPipPermissionPromptAndroid.isEnabled()) {
            isPermissionPromptNeeded =
                    AutoPictureInPicturePermissionController.isPermissionPromptNeeded(
                            mParentWebContents);
        }

        // Enforce minimum dimensions if the prompt is needed and requested bounds are too small.
        if (isPermissionPromptNeeded) {
            DisplayAndroid display = getDisplayAndroid();

            mMinPromptWidthPx =
                    mMinPromptWidthPx == null
                            ? getResources()
                                    .getDimensionPixelSize(
                                            R.dimen
                                                    .document_picture_in_picture_min_width_with_prompt)
                            : mMinPromptWidthPx;
            mMinPromptHeightPx =
                    mMinPromptHeightPx == null
                            ? getResources()
                                    .getDimensionPixelSize(
                                            R.dimen
                                                    .document_picture_in_picture_min_height_with_prompt)
                            : mMinPromptHeightPx;

            final int minWidthDp = DisplayUtil.pxToDp(display, mMinPromptWidthPx);
            final int minHeightDp = DisplayUtil.pxToDp(display, mMinPromptHeightPx);

            if (width < minWidthDp || height < minHeightDp) {
                final int newWidth = Math.max(width, minWidthDp);
                final int newHeight = Math.max(height, minHeightDp);
                mPromptEnforcedBounds =
                        new Rect(
                                windowBounds.left,
                                windowBounds.top,
                                windowBounds.left + newWidth,
                                windowBounds.top + newHeight);
                width = newWidth;
                height = newHeight;
            }
        }

        // Apply the final calculated bounds (either site-requested or prompt-enforced).
        resizeContents(width, height);
    }

    /**
     * Resizes the contents of the activity to the given DP dimensions.
     *
     * <p>This method resizes the contents of the activity to the given DP dimensions by resizing
     * the window. Note that Android has a minimum size (220dp) & a maximum size (70% of display
     * size in width and height) for pinned windows, so the requested size may not be respected.
     */
    @VisibleForTesting
    void resizeContents(int widthDp, int heightDp) {
        FrameLayout contentLayout = findViewById(R.id.document_picture_in_picture_content);
        DisplayAndroid display = getDisplayAndroid();
        int curContentsWidth = DisplayUtil.pxToDp(display, contentLayout.getWidth());
        int curContentsHeight = DisplayUtil.pxToDp(display, contentLayout.getHeight());

        int widthDiff = widthDp - curContentsWidth;
        int heightDiff = heightDp - curContentsHeight;

        resizeWindow(widthDiff, heightDiff);
    }

    @Override
    public DisplayAndroid getDisplayAndroid() {
        return assumeNonNull(getWindowAndroid()).getDisplay();
    }

    @Override
    public void resizeWindow(int widthDiffDp, int heightDiffDp) {
        if (widthDiffDp == 0 && heightDiffDp == 0) {
            return;
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            // This method is not supported on API versions below 30.
            return;
        }

        DisplayAndroid display = getDisplayAndroid();
        Rect currentWindowBounds =
                DisplayUtil.convertLocalPxToGlobalDipCoordinates(
                        display,
                        new Rect(getWindowManager().getCurrentWindowMetrics().getBounds()));

        MultiWindowUtils.moveActivityToBounds(
                this,
                new Rect(
                        currentWindowBounds.left - widthDiffDp,
                        currentWindowBounds.top - heightDiffDp,
                        currentWindowBounds.right,
                        currentWindowBounds.bottom));
    }

    private static boolean areDimensionsApproximatelyEqual(
            int width1, int height1, int width2, int height2) {
        return Math.abs(width1 - width2) <= SIZE_TOLERANCE_DP
                && Math.abs(height1 - height2) <= SIZE_TOLERANCE_DP;
    }

    /**
     * Reverts the window size back to the originally requested bounds once the permission prompt is
     * dismissed.
     *
     * <p>If the user has manually resized the window while the prompt was visible (determined by a
     * {@value #SIZE_TOLERANCE_DP}dp tolerance check against the enforced bounds), the revert is
     * skipped to respect the user's manual adjustment.
     */
    void revertToRequestedBounds() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return;
        }

        // If mPromptEnforcedBounds is null, the window wasn't enlarged for the prompt,
        // so no reversion is needed.
        if (mPromptEnforcedBounds == null) {
            return;
        }

        if (mWindowOptions != null && mWindowOptions.windowBounds != null) {
            DisplayAndroid display = getDisplayAndroid();
            FrameLayout contentLayout = findViewById(R.id.document_picture_in_picture_content);
            final int curContentWidth = DisplayUtil.pxToDp(display, contentLayout.getWidth());
            final int curContentHeight = DisplayUtil.pxToDp(display, contentLayout.getHeight());

            // Allow a small tolerance for density conversion rounding errors.
            if (!areDimensionsApproximatelyEqual(
                    curContentWidth,
                    curContentHeight,
                    mPromptEnforcedBounds.width(),
                    mPromptEnforcedBounds.height())) {
                return;
            }

            final int requestedWidth = mWindowOptions.windowBounds.width();
            final int requestedHeight = mWindowOptions.windowBounds.height();

            resizeContents(requestedWidth, requestedHeight);
        }
    }

    @Override
    protected void triggerLayoutInflation() {
        assert isContentsInitialized();
        onInitialLayoutInflationComplete();
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        OneshotSupplierImpl<ProfileProvider> supplier = new OneshotSupplierImpl<>();
        ProfileProvider profileProvider =
                new ProfileProvider() {

                    @Override
                    public Profile getOriginalProfile() {
                        return mInitiatorTab.getProfile().getOriginalProfile();
                    }

                    @Override
                    public @Nullable Profile getOffTheRecordProfile(boolean createIfNeeded) {
                        if (!mInitiatorTab.getProfile().isOffTheRecord()) {
                            assert !createIfNeeded;
                            return null;
                        }
                        return mInitiatorTab.getProfile();
                    }
                };
        supplier.set(profileProvider);
        return supplier;
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(
                this,
                /* listenToActivityState= */ true,
                getIntentRequestTracker(),
                getInsetObserver(),
                /* occlusionTrackingAllowed= */ true);
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        // EdgeToEdgeStateProvider is set in ChromeBaseAppCompatActivity#onCreate.
        assert getEdgeToEdgeStateProvider() != null;

        return new ModalDialogManager(
                new AppModalPresenter(this),
                ModalDialogManager.ModalDialogType.APP,
                getEdgeToEdgeStateProvider().getSupplier(),
                EdgeToEdgeUtils.isEdgeToEdgeEverywhereEnabled());
    }

    /**
     * Saves the current content area bounds to the cache if conditions are met.
     *
     * <p>This requires that the parent WebContents is still valid, we are not recreating the
     * activity, and the API level is 30 or higher (required for {@code getCurrentWindowMetrics()}).
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void saveBoundsToCache() {
        if (mParentWebContents != null
                && !mParentWebContents.isDestroyed()
                && !mIsRecreating
                && getWindowAndroid() != null
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            DisplayAndroid display = getDisplayAndroid();
            int openerDisplayId = INVALID_DISPLAY;
            WindowAndroid openerWindow = mParentWebContents.getTopLevelNativeWindow();
            if (openerWindow != null) {
                openerDisplayId = openerWindow.getDisplay().getDisplayId();
            }

            int pipDisplayId = display.getDisplayId();

            FrameLayout contentLayout = findViewById(R.id.document_picture_in_picture_content);
            if (contentLayout == null
                    || contentLayout.getWidth() <= 0
                    || contentLayout.getHeight() <= 0) {
                return;
            }
            int[] location = new int[2];
            contentLayout.getLocationOnScreen(location);

            Rect contentBoundsPx =
                    new Rect(
                            location[0],
                            location[1],
                            location[0] + contentLayout.getWidth(),
                            location[1] + contentLayout.getHeight());

            Rect contentBounds =
                    DisplayUtil.convertLocalPxToGlobalDipCoordinates(display, contentBoundsPx);

            if (mWindowOptions != null && mWindowOptions.windowBounds != null) {
                final int requestedWidthDp = mWindowOptions.windowBounds.width();
                final int requestedHeightDp = mWindowOptions.windowBounds.height();

                // Avoid redundant caching if the size hasn't changed from the target bounds.
                // We allow a small tolerance to prevent slow size creeps caused by density
                // rounding drifts or system snaps.
                if (areDimensionsApproximatelyEqual(
                        contentBounds.width(),
                        contentBounds.height(),
                        requestedWidthDp,
                        requestedHeightDp)) {
                    return;
                }

                // Avoid caching the temporary prompt enlargement if the user closed the window
                // before dismissing the prompt and without manually resizing it further.
                if (mPromptEnforcedBounds != null
                        && areDimensionsApproximatelyEqual(
                                contentBounds.width(),
                                contentBounds.height(),
                                mPromptEnforcedBounds.width(),
                                mPromptEnforcedBounds.height())) {
                    return;
                }
            }

            PictureInPictureBoundsCacheBridge.updateCachedBounds(
                    mParentWebContents, contentBounds, openerDisplayId, pipDisplayId);
        }
    }

    @Override
    public void onStop() {
        // Save bounds on stop rather than destroy, to ensure the window is still
        // valid and its metrics can be fetched.
        saveBoundsToCache();
        super.onStop();
    }

    @Override
    @SuppressWarnings("NullAway")
    protected final void onDestroy() {
        if (mThinWebView != null) {
            mThinWebView.destroy();
            mThinWebView = null;
        }
        if (mWebContents != null) {
            if (mIsRecreating) {
                // Hide the web contents instead of destroying it so that it can be reused in the
                // recreated activity, WebContents would be shown when attached to the new
                // ThinWebView.
                mWebContents.updateWebContentsVisibility(Visibility.HIDDEN);
            } else {
                mWebContents.destroy();
                mWebContents = null;
            }
        }

        if (ChromeFeatureList.sAutoDocPipPermissionPromptAndroid.isEnabled()
                && mParentWebContents != null
                && !mParentWebContents.isDestroyed()
                && !mIsRecreating) {
            AutoPictureInPicturePermissionController.handleWindowDestruction(mParentWebContents);
        }

        if (mInitiatorTabObserver != null && mInitiatorTab != null) {
            mInitiatorTab.removeObserver(mInitiatorTabObserver);
        }

        mInitiatorTab = null;
        mInitiatorTabObserver = null;

        if (mHeaderCoordinator != null) {
            mHeaderCoordinator.destroy();
            mHeaderCoordinator = null;
        }

        if (mAppThemeColorProvider != null) {
            mAppThemeColorProvider.destroy();
            mAppThemeColorProvider = null;
        }

        // Destroy method is only available on API 30+, current min API is 29.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                && mAppHeaderCoordinator != null) {
            mAppHeaderCoordinator.destroy();
            mAppHeaderCoordinator = null;
        }

        super.onDestroy();
    }

    @Override
    public void onBackToTab() {
        DocumentPictureInPictureActivityJni.get().onBackToTab();
    }

    @Override
    public void onSecurityIconClicked() {
        // TODO(crbug.com/479732663): Move the click handling to the coordinator.
        PageInfoController.show(
                this,
                mParentWebContents,
                /* contentPublisher= */ null,
                OpenedFromSource.TOOLBAR,
                new ChromePageInfoControllerDelegate(
                        this,
                        mParentWebContents,
                        () -> getModalDialogManagerSupplier().get(),
                        new WebContentsOfflinePageLoadUrlDelegate(mParentWebContents),
                        /* storeInfoActionHandlerSupplier= */ null,
                        /* ephemeralTabCoordinatorSupplier= */ null,
                        ChromePageInfoHighlight.noHighlight(),
                        /* tabCreator= */ null,
                        /* packageName= */ null),
                ChromePageInfoHighlight.noHighlight(),
                Gravity.TOP);
    }

    @Override
    public boolean isWindowPinned() {
        return mIsPinned;
    }

    @Override
    public void performOnConfigurationChanged(Configuration newConfig) {
        super.performOnConfigurationChanged(newConfig);
        if (mConfig != null) {
            if (newConfig.densityDpi != mConfig.densityDpi) {
                recreate();
                return;
            }
        }

        mConfig = newConfig;
    }

    @Override
    protected void initializeNightModeStateProvider() {
        if (mNightModeStateProvider != null) {
            mNightModeStateProvider.initialize(getDelegate());
        }
    }

    @Override
    protected NightModeStateProvider createNightModeStateProvider() {
        mNightModeStateProvider = new DocumentPictureInPictureNightModeStateProvider();
        return mNightModeStateProvider;
    }

    @CallSuper
    @Override
    public void recreate() {
        super.recreate();
        mIsRecreating = true;
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putBoolean(IS_FROM_ACTIVITY_RECREATION_KEY, mIsRecreating);
        outState.putParcelable(WEB_CONTENTS_KEY, mWebContents);
        outState.putBundle(WINDOW_OPTIONS_KEY, assumeNonNull(mWindowOptions).toBundle());
    }

    private class DocumentPictureInPictureWebContentsDelegate extends WebContentsDelegateAndroid {
        @Override
        public void closeContents() {
            finish();
        }

        @Override
        public void openNewTab(
                GURL url,
                String extraHeaders,
                ResourceRequestBody postData,
                int disposition,
                boolean isRendererInitiated) {
            finish();
        }

        @Override
        public void setContentsBounds(WebContents source, Rect bounds) {
            resizeContents(bounds.width(), bounds.height());
        }
    }

    public WebContents getWebContentsForTesting() {
        return mWebContents;
    }

    public static void setWebContentsForTesting(WebContents webContents) {
        sWebContentsForTesting = webContents;
        ResettersForTesting.register(() -> sWebContentsForTesting = null);
    }

    /**
     * Sets the parent WebContents to be used during activity startup for testing. Use this in
     * integration tests before the activity is launched.
     */
    public static void setParentWebContentsForTesting(WebContents webContents) {
        sParentWebContentsForTesting = webContents;
        ResettersForTesting.register(() -> sParentWebContentsForTesting = null);
    }

    /**
     * Enters Picture-in-Picture mode for testing. This is intended for test environments that
     * launch the Activity directly.
     */
    public static void onActivityStartForTesting(
            WebContents parentWebContents, WebContents webContents) {
        DocumentPictureInPictureActivityJni.get()
                .onActivityStartForTesting(parentWebContents, webContents); // IN-TEST
    }

    /**
     * Verifies that the current opener's origin matches the origin captured when the PiP window was
     * requested. This protects against a race condition where the opener window navigates during
     * the asynchronous Activity startup.
     *
     * @param intent The launch intent.
     * @param parentWebContents The opener's WebContents.
     * @return True if the origins match, or if verification is skipped; false on mismatch.
     */
    private boolean verifyOpenerOrigin(Intent intent, WebContents parentWebContents) {
        if (mIsFromActivityRecreation) {
            return true; // Already verified on initial startup.
        }
        final String initialOpenerOriginStr = intent.getStringExtra(INITIAL_OPENER_ORIGIN_KEY);
        if (initialOpenerOriginStr == null) {
            Log.e(TAG, "No initial opener origin in intent! Finishing.");
            return false;
        }
        final GURL currentOpenerUrl = parentWebContents.getLastCommittedUrl();
        final String currentOpenerOriginStr = Origin.create(currentOpenerUrl).toString();
        if (!initialOpenerOriginStr.equals(currentOpenerOriginStr)) {
            Log.e(
                    TAG,
                    "Opener origin mismatch! Initial: "
                            + initialOpenerOriginStr
                            + ", Current: "
                            + currentOpenerOriginStr);
            return false;
        }
        return true;
    }

    /**
     * Sets the parent WebContents directly on this instance for testing. Use this in unit tests
     * where the activity is created without running the full startup flow.
     */
    void setParentWebContentsOnInstanceForTesting(WebContents webContents) {
        mParentWebContents = webContents;
    }

    public static void setIgnoreSdkVersionForTesting(boolean ignore) {
        sIgnoreSdkVersionForTesting = ignore;
        ResettersForTesting.register(() -> sIgnoreSdkVersionForTesting = false);
    }

    void setWindowOptionsForTesting(PictureInPictureWindowOptions windowOptions) {
        mWindowOptions = windowOptions;
    }

    void setPromptEnforcedBoundsForTesting(Rect bounds) {
        mPromptEnforcedBounds = bounds;
    }

    @NativeMethods
    public interface Natives {
        void onActivityStartForTesting( // IN-TEST
                WebContents parentWebContent, WebContents webContents);

        void onBackToTab();
    }

    static class DocumentPictureInPictureNightModeStateProvider implements NightModeStateProvider {
        public void initialize(AppCompatDelegate delegate) {
            delegate.setLocalNightMode(AppCompatDelegate.MODE_NIGHT_YES);
        }

        @Override
        public boolean isInNightMode() {
            return true;
        }

        @Override
        public void addObserver(Observer observer) {}

        @Override
        public void removeObserver(Observer observer) {}
    }
}
