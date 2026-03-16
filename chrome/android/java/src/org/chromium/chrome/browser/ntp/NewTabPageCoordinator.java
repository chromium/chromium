// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.Rect;
import android.text.Editable;
import android.view.DragEvent;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.composeplate.ComposeplateCoordinator;
import org.chromium.chrome.browser.composeplate.ComposeplateMetricsUtils;
import org.chromium.chrome.browser.composeplate.ComposeplateUtils;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.feed.FeedSurfaceScrollDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensMetrics;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.logo.LogoCoordinator;
import org.chromium.chrome.browser.logo.LogoUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.ntp.NewTabPage.OnSearchBoxScrollListener;
import org.chromium.chrome.browser.ntp.search.SearchBoxCoordinator;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.omnibox.SearchEngineUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesCoordinator;
import org.chromium.chrome.browser.suggestions.tile.TileGroup;
import org.chromium.chrome.browser.tab_ui.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.chrome.browser.ui.signin.signin_promo.NtpSigninPromoCoordinator;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.url.GURL;

import java.util.function.Supplier;

/**
 * Layout for the new tab page. This positions the page elements in the correct vertical positions.
 * There are no separate phone and tablet UIs; this layout adapts based on the available space.
 */
@NullMarked
public class NewTabPageCoordinator {
    private static final String TAG = "NewTabPageLayout";

    private final NewTabPageManager mManager;
    private final Activity mActivity;
    private final NewTabPageLayout mNewTabPageLayout;
    private final NewTabPageLayout.Delegate mLayoutDelegate;
    private LogoCoordinator mLogoCoordinator;
    private SearchBoxCoordinator mSearchBoxCoordinator;
    private @Nullable MostVisitedTilesCoordinator mMostVisitedTilesCoordinator;

    private @Nullable OnSearchBoxScrollListener mSearchBoxScrollListener;

    private WindowAndroid mWindowAndroid;
    private Profile mProfile;
    private ActivityResultTracker mActivityResultTracker;
    private BottomSheetController mBottomSheetController;
    private ModalDialogManager mModalDialogManager;
    private SnackbarManager mSnackbarManager;
    private UiConfig mUiConfig;
    private @Nullable DisplayStyleObserver mDisplayStyleObserver;
    private CallbackController mCallbackController = new CallbackController();
    private SearchEngineUtils.@Nullable SearchEngineIconObserver mSearchEngineIconObserver;
    private SearchEngineUtils.@Nullable SearchBoxHintTextObserver mSearchBoxHintTextObserver;

    /**
     * Whether the tiles shown in the layout have finished loading. With {@link #mHasShownView},
     * it's one of the 2 flags used to track initialisation progress.
     */
    private boolean mTilesLoaded;

    /**
     * Whether the view has been shown at least once. With {@link #mTilesLoaded}, it's one of the 2
     * flags used to track initialization progress.
     */
    private boolean mHasShownView;

    private boolean mSearchProviderHasLogo = true;
    private boolean mSearchProviderIsGoogle;
    private boolean mShowingNonStandardGoogleLogo;

    private boolean mInitialized;

    private float mUrlFocusChangePercent;
    private boolean mDisableUrlFocusChangeAnimations;

    /** Flag used to request some layout changes after the next layout pass is completed. */
    private boolean mTileCountChanged;

    private boolean mSnapshotTileGridChanged;
    private int mSearchBoxTwoSideMargin;

    /**
     * Vertical inset to add to the top and bottom of the search box bounds. May be 0 if no inset
     * should be applied. See {@link Rect#inset(int, int)}.
     */
    private int mSearchBoxBoundsVerticalInset;

    private FeedSurfaceScrollDelegate mScrollDelegate;

    private boolean mIsTablet;
    private @Nullable Supplier<Integer> mTabStripHeightSupplier;

    private Callback<Logo> mOnLogoAvailableCallback;

    // mIsComposeplateEnabled is null before checking whether to initialize composeplate view in
    // NewTabPageCoordinator#initialize().
    private @Nullable Boolean mIsComposeplateEnabled;
    private boolean mIsComposeplatePolicyEnabled;
    private boolean mIsComposeplateViewInitialized;
    private Supplier<GURL> mComposeplateUrlSupplier;
    private @Nullable ComposeplateCoordinator mComposeplateCoordinator;
    // Previous visibility states for metrics.
    private @Nullable Boolean mPreviousVoiceSearchButtonVisible;
    private @Nullable Boolean mPreviousLensButtonVisible;
    private SearchEngineUtils mSearchEngineUtils;
    private final int mNtpSearchBoxTransitionStartOffset;
    private final int mNtpSearchBoxTopMarginWithoutLogo;
    private final boolean mEnableLogs;
    private int mCurrentNtpFakeSearchBoxTransitionStartOffset;
    private int mTopInset;
    private @Nullable OnLayoutChangeListener mOnLayoutChangeListener;
    // TODO(crbug.com/451602301): remove @Nullable and all null checks once
    // ENABLE_SEAMLESS_SIGNIN is removed after the experiment.
    private @Nullable NtpSigninPromoCoordinator mSigninPromoCoordinator;

    private @Nullable Boolean mIsWhiteBackgroundOnSearchBoxApplied;

    /**
     * Constructor of the NewTabPageCoordinator.
     *
     * @param manager NewTabPageManager used to perform various actions when the user interacts with
     *     the page.
     * @param activity The activity that currently owns the new tab page
     * @param newTabPageLayout The new tab page layout.
     */
    public NewTabPageCoordinator(
            NewTabPageManager manager, Activity activity, NewTabPageLayout newTabPageLayout) {
        mManager = manager;
        mActivity = activity;
        mNewTabPageLayout = newTabPageLayout;

        Resources resources = mActivity.getResources();
        mNtpSearchBoxTopMarginWithoutLogo =
                resources.getDimensionPixelSize(R.dimen.mvt_container_top_margin);
        mNtpSearchBoxTransitionStartOffset =
                resources.getDimensionPixelSize(R.dimen.ntp_search_box_transition_start_offset);

        mEnableLogs = ChromeFeatureList.sNewTabPageCustomizationV2EnableLogs.getValue();

        mLayoutDelegate =
                new NewTabPageLayout.Delegate() {
                    @Override
                    public void onMeasure(int width) {
                        NewTabPageCoordinator.this.onMeasure(width);
                    }

                    @Override
                    public void onAttachedToWindow() {
                        NewTabPageCoordinator.this.onAttachedToWindow();
                    }

                    @Override
                    public void updateActionButtonVisibility() {
                        NewTabPageCoordinator.this.updateActionButtonVisibility();
                    }
                };
        mNewTabPageLayout.setDelegate(mLayoutDelegate);
    }

    /**
     * Initializes the NewTabPageLayout. This must be called immediately after inflation, before is
     * object is used in any other way.
     *
     * @param tileGroupDelegate Delegate for {@link TileGroup}.
     * @param searchProviderHasLogo Whether the search provider has a logo.
     * @param searchProviderIsGoogle Whether the search provider is Google.
     * @param scrollDelegate The delegate used to obtain information about scroll state.
     * @param touchEnabledDelegate The {@link TouchEnabledDelegate} for handling whether touch
     *     events are allowed.
     * @param uiConfig UiConfig that provides display information about this view.
     * @param lifecycleDispatcher Activity lifecycle dispatcher.
     * @param profile The {@link Profile} associated with the NTP.
     * @param windowAndroid An instance of a {@link WindowAndroid}.
     * @param activityResultTracker Tracker of activity results.
     * @param bottomSheetController Used to interact with the bottom sheet.
     * @param snackbarManager Manages snackbars shown in the app.
     * @param isTablet {@code true} if the NTP surface is in tablet mode.
     * @param tabStripHeightSupplier Supplier of the tab strip height.
     */
    @Initializer
    public void initialize(
            TileGroup.Delegate tileGroupDelegate,
            boolean searchProviderHasLogo,
            boolean searchProviderIsGoogle,
            FeedSurfaceScrollDelegate scrollDelegate,
            TouchEnabledDelegate touchEnabledDelegate,
            UiConfig uiConfig,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            Profile profile,
            WindowAndroid windowAndroid,
            ActivityResultTracker activityResultTracker,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            SnackbarManager snackbarManager,
            boolean isTablet,
            Supplier<Integer> tabStripHeightSupplier,
            Supplier<GURL> composeplateUrlSupplier) {
        TraceEvent.begin(TAG + ".initialize()");
        mScrollDelegate = scrollDelegate;
        mProfile = profile;
        mUiConfig = uiConfig;
        mWindowAndroid = windowAndroid;
        mActivityResultTracker = activityResultTracker;
        mBottomSheetController = bottomSheetController;
        mModalDialogManager = modalDialogManager;
        mSnackbarManager = snackbarManager;
        mIsTablet = isTablet;
        mTabStripHeightSupplier = tabStripHeightSupplier;
        mSearchEngineUtils = SearchEngineUtils.getForProfile(mProfile);
        mComposeplateUrlSupplier = composeplateUrlSupplier;

        if (mIsTablet) {
            mDisplayStyleObserver = this::onDisplayStyleChanged;
            mUiConfig.addObserver(mDisplayStyleObserver);
        } else {
            // On first run, the NewTabPageLayout is initialized behind the First Run Experience,
            // meaning the UiConfig will pickup the screen layout then. However
            // onConfigurationChanged is not called on orientation changes until the FRE is
            // completed. This means that if a user starts the FRE in one orientation, changes an
            // orientation and then leaves the FRE the UiConfig will have the wrong orientation.
            // https://crbug.com/683886.
            mUiConfig.updateDisplayStyle();
        }

        mSearchBoxCoordinator = new SearchBoxCoordinator(mActivity, mNewTabPageLayout, mIsTablet);
        mSearchBoxCoordinator.initialize(
                lifecycleDispatcher, mProfile.isOffTheRecord(), mWindowAndroid);

        updateSearchBoxTwoSideMargin();
        initializeLogoCoordinator();
        setSearchProviderInfo(searchProviderHasLogo, searchProviderIsGoogle);
        initializeMostVisitedTilesCoordinator(
                mProfile, lifecycleDispatcher, tileGroupDelegate, touchEnabledDelegate);

        mSearchEngineIconObserver = (newIcon) -> mSearchBoxCoordinator.setSearchEngineIcon(newIcon);
        mSearchEngineUtils.addIconObserver(mSearchEngineIconObserver);
        setSearchBoxTextAppearance();

        initializeSearchBoxTextView();
        initializeVoiceSearchButton();
        initializeLensButton();

        initializeComposeplateFlags(mProfile);
        if (assumeNonNull(mIsComposeplateEnabled)) {
            initializeComposeplate();
        }

        // This should be called after both mSearchBoxCoordinator and mComposeplateCoordinator are
        // initialized.
        onCustomizedBackgroundChanged(
                NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox());

        // This should called after flags of composeplate view are initialized.
        setSearchBoxHeightBoundsVerticalInset();

        updateActionButtonVisibility();
        initializeLayoutChangeListener();
        if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)) {
            initializeSigninPromoCoordinator();
        }

        // Initialize Searchbox observers
        mSearchBoxHintTextObserver = this::onSearchBoxHintTextChanged;
        mSearchEngineUtils.addSearchBoxHintTextObserver(mSearchBoxHintTextObserver);

        // TODO(https://crbug.com/487641528): Destroy NewTabPageCoordinator in
        // NewTabPage#destroy().
        mManager.addDestructionObserver(NewTabPageCoordinator.this::onDestroy);
        mInitialized = true;

        TraceEvent.end(TAG + ".initialize()");
    }

    /** Sets the height of the search box and mSearchBoxBoundsVerticalInset. */
    private void setSearchBoxHeightBoundsVerticalInset() {
        Resources resources = mActivity.getResources();
        int searchBoxHeight =
                NtpCustomizationUtils.getSearchBoxHeightWithShadows(
                        resources, assumeNonNull(mIsComposeplateEnabled));
        mSearchBoxCoordinator.setHeight(searchBoxHeight);

        mSearchBoxBoundsVerticalInset =
                Math.round(
                        (searchBoxHeight
                                        - resources.getDimensionPixelSize(
                                                R.dimen.toolbar_height_no_shadow))
                                / 2f);
    }

    public void reload() {
        // TODO(crbug.com/41487877): Add handler in Magic Stack and dispatcher.
    }

    public void enableSearchBoxEditText(boolean enable) {
        mSearchBoxCoordinator.enableSearchBoxEditText(enable);
    }

    /**
     * @return The {@link FeedSurfaceScrollDelegate} for this class.
     */
    FeedSurfaceScrollDelegate getScrollDelegate() {
        return mScrollDelegate;
    }

    /** Sets up the hint text and event handlers for the search box text view. */
    private void initializeSearchBoxTextView() {
        TraceEvent.begin(TAG + ".initializeSearchBoxTextView()");

        mSearchBoxCoordinator.setSearchBoxClickListener(
                v -> mManager.focusSearchBox(false, AutocompleteRequestType.SEARCH, null));

        // @TODO(crbug.com/41492572): Add test case for search box OnDragListener.
        mSearchBoxCoordinator.setSearchBoxDragListener(
                new View.OnDragListener() {
                    @Override
                    public boolean onDrag(View view, DragEvent dragEvent) {
                        // Disable search box EditText when browser content is dropped, its
                        // re-enabled in {@link ChromeTabbedOnDragListener}, since a disabled view
                        // will stop receiving further drag events. Given the child-first drag event
                        // dispatch, disabling the TextView at ACTION_DRAG_STARTED is necessary to
                        // prevent it from registering as a drop target and consuming the
                        // ACTION_DROP event, thereby ensuring {@link ChromeTabbedOnDragListener}
                        // receives it.
                        if (MimeTypeUtils.clipDescriptionHasBrowserContent(
                                        dragEvent.getClipDescription())
                                && dragEvent.getAction() == DragEvent.ACTION_DRAG_STARTED) {
                            enableSearchBoxEditText(false);
                        }
                        return false;
                    }
                });

        mSearchBoxCoordinator.setSearchBoxTextWatcher(
                new EmptyTextWatcher() {
                    @Override
                    public void afterTextChanged(Editable s) {
                        if (s.length() == 0) return;
                        mManager.focusSearchBox(
                                false, AutocompleteRequestType.SEARCH, s.toString());
                        mSearchBoxCoordinator.setSearchText("");
                    }
                });
        TraceEvent.end(TAG + ".initializeSearchBoxTextView()");
    }

    public void onSearchBoxHintTextChanged() {
        mSearchBoxCoordinator.setSearchBoxHintText(
                mSearchEngineUtils.getOmniboxHintText(
                        AutocompleteRequestType.SEARCH, /* fuseboxSessionState= */ null));
    }

    private void setSearchBoxTextAppearance() {
        boolean shouldApplyWhiteBackground =
                NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox();

        if (shouldApplyWhiteBackground) {
            mSearchBoxCoordinator.setSearchBoxTextAppearance(
                    R.style.TextAppearance_FakeSearchBoxTextMediumDark);
        } else {
            mSearchBoxCoordinator.setSearchBoxTextAppearance(
                    R.style.TextAppearance_FakeSearchBoxTextMedium);
        }
    }

    private void initializeVoiceSearchButton() {
        TraceEvent.begin(TAG + ".initializeVoiceSearchButton()");
        View.OnClickListener voiceSearchButtonClickListener =
                v -> mManager.focusSearchBox(true, AutocompleteRequestType.SEARCH, null);
        mSearchBoxCoordinator.addVoiceSearchButtonClickListener(voiceSearchButtonClickListener);
        TraceEvent.end(TAG + ".initializeVoiceSearchButton()");
    }

    private void initializeLensButton() {
        TraceEvent.begin(TAG + ".initializeLensButton()");
        View.OnClickListener lensButtonClickListener =
                v -> {
                    LensMetrics.recordClicked(LensEntryPoint.NEW_TAB_PAGE);
                    mSearchBoxCoordinator.startLens(LensEntryPoint.NEW_TAB_PAGE);
                };
        mSearchBoxCoordinator.addLensButtonClickListener(lensButtonClickListener);
        TraceEvent.end(TAG + ".initializeLensButton()");
    }

    private void initializeComposeplateFlags(Profile profile) {
        mIsComposeplateEnabled = ComposeplateUtils.isComposeplateEnabled(mIsTablet, profile);
        mIsComposeplatePolicyEnabled =
                mIsComposeplateEnabled && ComposeplateUtils.isEnabledByPolicy(profile);
    }

    private void initializeComposeplate() {
        if (mIsComposeplateViewInitialized) return;

        mIsComposeplateViewInitialized = true;

        boolean shouldApplyWhiteBackgroundOnSearchBox =
                NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox();

        ViewStub composeplateViewStub =
                mNewTabPageLayout.findViewById(R.id.composeplate_view_v2_stub);
        ViewGroup composeplateView = (ViewGroup) composeplateViewStub.inflate();
        mComposeplateCoordinator = new ComposeplateCoordinator(composeplateView, mProfile);
        mComposeplateCoordinator.setIncognitoClickListener(this::onIncognitoButtonClicked);
        // Don't log click metrics in this listener, since the mComposeplateCoordinator will
        // log.
        mComposeplateCoordinator.setComposeplateButtonClickListener(
                this::onComposeplateButtonClicked);

        if (shouldApplyWhiteBackgroundOnSearchBox) {
            // It is safe to call mComposeplateCoordinator.applyWhiteBackgroundWithShadow() again
            // since it is no-op if the white background has been applied.
            mComposeplateCoordinator.applyWhiteBackgroundWithShadow(/* apply= */ true);
        }
    }

    private void onComposeplateButtonClicked(View view) {
        if (OmniboxFeatures.sOmniboxMultimodalInput.isEnabled()
                && OmniboxFeatures.sRedirectComposeplateButton.getValue()
                && !mIsTablet
                && mIsComposeplatePolicyEnabled) {
            mManager.focusSearchBox(false, AutocompleteRequestType.AI_MODE, null);
            return;
        }

        GURL composeplateUrl = mComposeplateUrlSupplier.get();
        if (composeplateUrl == null) return;

        mManager.getNativePageHost()
                .loadUrl(new LoadUrlParams(composeplateUrl), /* incognito= */ false);
    }

    private void onIncognitoButtonClicked(View view) {
        if (!IncognitoUtils.isIncognitoModeEnabled(mProfile)) return;

        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);
        mManager.getNativePageHost().loadUrl(new LoadUrlParams(resolver.getNtpUrl()), true);
    }

    private void initializeLayoutChangeListener() {
        TraceEvent.begin(TAG + ".initializeLayoutChangeListener()");
        mOnLayoutChangeListener = this::onLayoutChanged;
        mNewTabPageLayout.addOnLayoutChangeListener(mOnLayoutChangeListener);
        TraceEvent.end(TAG + ".initializeLayoutChangeListener()");
    }

    private void onLayoutChanged(
            View view,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        int oldHeight = oldBottom - oldTop;
        int newHeight = bottom - top;

        if (oldHeight == newHeight && !mTileCountChanged) return;
        mTileCountChanged = false;

        // Re-apply the url focus change amount after a rotation to ensure the views are
        // correctly placed with their new layout configurations.
        onUrlFocusAnimationChanged();
        updateSearchBoxOnScroll();

        // The positioning of elements may have been changed (since the elements expand
        // to fill the available vertical space), so adjust the scroll.
        if (mScrollDelegate.isScrollViewInitialized()) mScrollDelegate.snapScroll();
    }

    private void initializeLogoCoordinator() {
        Callback<LoadUrlParams> logoClickedCallback =
                mCallbackController.makeCancelable(
                        (urlParams) -> {
                            mManager.getNativePageHost().loadUrl(urlParams, /* incognito= */ false);
                            BrowserUiUtils.recordModuleClickHistogram(
                                    ModuleTypeOnStartAndNtp.DOODLE);
                        });
        mOnLogoAvailableCallback =
                mCallbackController.makeCancelable(
                        (logo) -> {
                            mSnapshotTileGridChanged = true;
                            mShowingNonStandardGoogleLogo = logo != null && mSearchProviderIsGoogle;
                            NtpCustomizationConfigManager.getInstance()
                                    .setDefaultSearchEngineLogoBitmap(
                                            logo == null ? null : logo.image);
                        });

        mLogoCoordinator =
                new LogoCoordinator(
                        mActivity,
                        logoClickedCallback,
                        mNewTabPageLayout,
                        mOnLogoAvailableCallback,
                        /* visibilityObserver= */ null,
                        () -> MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity));
        mLogoCoordinator.initWithNative(mProfile);
    }

    private void initializeMostVisitedTilesCoordinator(
            Profile profile,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            TileGroup.Delegate tileGroupDelegate,
            TouchEnabledDelegate touchEnabledDelegate) {
        View mvTilesContainerLayout = mNewTabPageLayout.findViewById(R.id.mv_tiles_container);
        assert mvTilesContainerLayout != null;

        mMostVisitedTilesCoordinator =
                new MostVisitedTilesCoordinator(
                        mActivity,
                        activityLifecycleDispatcher,
                        mvTilesContainerLayout,
                        () -> mSnapshotTileGridChanged = true,
                        () -> {
                            if (mUrlFocusChangePercent == 1f) mTileCountChanged = true;
                        });

        mMostVisitedTilesCoordinator.initWithNative(
                profile, mManager, tileGroupDelegate, touchEnabledDelegate);

        if (ChromeFeatureList.sNewTabPageCustomizationForMvt.isEnabled()) {
            mMostVisitedTilesCoordinator.updateMvtVisibility();
        }
    }

    private void initializeSigninPromoCoordinator() {
        ViewStub signinPromoViewContainerStub =
                mNewTabPageLayout.findViewById(R.id.signin_promo_view_container_stub);
        mSigninPromoCoordinator =
                new NtpSigninPromoCoordinator(
                        mWindowAndroid,
                        mActivity,
                        mProfile,
                        mActivityResultTracker,
                        SigninAndHistorySyncActivityLauncherImpl.get(),
                        mBottomSheetController,
                        mModalDialogManager,
                        mSnackbarManager,
                        DeviceLockActivityLauncherImpl.get(),
                        signinPromoViewContainerStub,
                        SetupListModuleUtils::isSetupListActive);
    }

    /** Updates the search box when the parent view's scroll position is changed. */
    void updateSearchBoxOnScroll() {
        if (mDisableUrlFocusChangeAnimations) return;

        // When the page changes (tab switching or new page loading), it is possible that events
        // (e.g. delayed view change notifications) trigger calls to these methods after
        // the current page changes. We check it again to make sure we don't attempt to update the
        // wrong page.
        if (!mManager.isCurrentPage()) return;

        if (mSearchBoxScrollListener != null) {
            mSearchBoxScrollListener.onNtpScrollChanged(getToolbarTransitionPercentage());
        }
    }

    /**
     * Calculates the percentage (between 0 and 1) of the transition from the search box to the
     * omnibox at the top of the New Tab Page, which is determined by the amount of scrolling and
     * the position of the search box.
     *
     * @return the transition percentage
     */
    float getToolbarTransitionPercentage() {
        return mSearchBoxCoordinator.getToolbarTransitionPercentage(
                mScrollDelegate,
                mTabStripHeightSupplier,
                mCurrentNtpFakeSearchBoxTransitionStartOffset);
    }

    /**
     * @return The fake search box view.
     */
    public View getSearchBoxView() {
        return mSearchBoxCoordinator.getView();
    }

    public void onSwitchToForeground() {
        if (mMostVisitedTilesCoordinator != null) {
            mMostVisitedTilesCoordinator.onSwitchToForeground();
        }
    }

    /**
     * Should be called every time one of the flags used to track initialization progress changes.
     * Finalizes initialization once all the preliminary steps are complete.
     *
     * @see #mHasShownView
     * @see #mTilesLoaded
     */
    private void onInitializationProgressChanged() {
        if (!hasLoadCompleted()) return;

        mManager.onLoadingComplete();

        // Load the logo after everything else is finished, since it's lower priority.
        if (mLogoCoordinator != null) mLogoCoordinator.loadSearchProviderLogoWithAnimation();
    }

    /**
     * To be called to notify that the tiles have finished loading. Will do nothing if a load was
     * previously completed.
     */
    public void onTilesLoaded() {
        if (mTilesLoaded) return;
        mTilesLoaded = true;

        onInitializationProgressChanged();
    }

    /**
     * Changes the layout depending on whether the selected search provider (e.g. Google, Bing) has
     * a logo.
     *
     * @param hasLogo Whether the search provider has a logo.
     * @param isGoogle Whether the search provider is Google.
     */
    void setSearchProviderInfo(boolean hasLogo, boolean isGoogle) {
        if (hasLogo == mSearchProviderHasLogo
                && isGoogle == mSearchProviderIsGoogle
                && mInitialized) {
            return;
        }
        boolean isSearchProviderIsGoogleChanged = mSearchProviderIsGoogle != isGoogle;
        mSearchProviderHasLogo = hasLogo;
        mSearchProviderIsGoogle = isGoogle;

        if (!mSearchProviderIsGoogle) {
            mShowingNonStandardGoogleLogo = false;
        }

        setSearchProviderTopMargin();
        setLogoViewBottomMargin();

        updateTilesLayoutMargins();

        // Hide or show the views above the most visited tiles as needed, e.g, spacers. The
        // visibility of Logo is handled by LogoCoordinator.
        setSearchBoxTextAppearance();

        // Skips if the flag hasn't been initialized since the initialization of the following
        // components will be called again in #initialize().
        if (mIsComposeplateEnabled != null) {
            // when mSearchProviderIsGoogle is changed, mIsComposeplateEnabled might be changed too,
            // recalculate its value.
            if (isSearchProviderIsGoogleChanged) {
                boolean previousIsComposeplateEnabled = mIsComposeplateEnabled;
                initializeComposeplateFlags(mProfile);
                if (!previousIsComposeplateEnabled
                        && mIsComposeplateEnabled
                        && mComposeplateCoordinator == null) {
                    // If the composeplate view is enabled while mComposeplateCoordinator hasn't
                    // been initialized yet, initialize it now.
                    initializeComposeplate();
                }

                if (previousIsComposeplateEnabled != mIsComposeplateEnabled) {
                    // When the flag value is changed, the height of search box might be changed.
                    setSearchBoxHeightBoundsVerticalInset();
                    // Updates the composeplate view's visibility.
                    updateActionButtonVisibility();
                }
            }
        }

        onUrlFocusAnimationChanged();

        mSnapshotTileGridChanged = true;
    }

    /** Updates the margins for the most visited tiles layout based on what is shown above it. */
    private void updateTilesLayoutMargins() {
        if (mMostVisitedTilesCoordinator == null) return;

        mMostVisitedTilesCoordinator.updateTilesLayoutMargins(shouldShowLogo(), mIsTablet);
    }

    /**
     * Updates whether the NewTabPage should animate on URL focus changes.
     *
     * @param disable Whether to disable the animations.
     */
    void setUrlFocusAnimationsDisabled(boolean disable) {
        if (disable == mDisableUrlFocusChangeAnimations) return;
        mDisableUrlFocusChangeAnimations = disable;
        if (!disable) onUrlFocusAnimationChanged();
    }

    /**
     * @return Whether URL focus animations are currently disabled.
     */
    boolean urlFocusAnimationsDisabled() {
        return mDisableUrlFocusChangeAnimations;
    }

    /**
     * Specifies the percentage the URL is focused during an animation. 1.0 specifies that the URL
     * bar has focus and has completed the focus animation. 0 is when the URL bar is does not have
     * any focus.
     *
     * @param percent The percentage of the URL bar focus animation.
     */
    void setUrlFocusChangeAnimationPercent(float percent) {
        mUrlFocusChangePercent = percent;
        onUrlFocusAnimationChanged();
    }

    /**
     * @return The percentage that the URL bar is focused during an animation.
     */
    @VisibleForTesting
    float getUrlFocusChangeAnimationPercent() {
        return mUrlFocusChangePercent;
    }

    void onUrlFocusAnimationChanged() {
        /*
         * Avoid Y-translation when animation is disabled, view is moving or on tablet form-factor.
         * Context for tablets - Unlike phones, this method is not called on tablets during URL
         * focus post NTP load. However when physical keyboard is present, we try to auto-focus URL
         * during page load causing this method to be called. Disabling this for all cases on this
         * form-factor since this translation does not WAI. (see crbug.com/40910640)
         */
        if (mDisableUrlFocusChangeAnimations || mIsTablet) return;

        // Translate so that the search box is at the top, but only upwards.
        int basePosition =
                mScrollDelegate.getVerticalScrollOffset() + mNewTabPageLayout.getPaddingTop();
        int target =
                Math.max(
                        basePosition,
                        getSearchBoxView().getBottom()
                                - getSearchBoxView().getPaddingBottom()
                                - mSearchBoxBoundsVerticalInset);

        float translationY = mUrlFocusChangePercent * (basePosition - target);
        setTranslationYOfFakeboxAndAbove(translationY);
    }

    /**
     * Sets the translation_y of the fakebox and all views above it, but not the views below. Used
     * when the url focus animation is combined with the omnibox suggestions list animation to
     * reduce the number of visual elements in motion.
     */
    private void setTranslationYOfFakeboxAndAbove(float translationY) {
        for (int i = 0; i < mNewTabPageLayout.getChildCount(); i++) {
            View view = mNewTabPageLayout.getChildAt(i);
            view.setTranslationY(translationY);
            if (view.getId() == R.id.search_box) return;
        }
    }

    /**
     * Updates the opacity of the search box when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setSearchBoxAlpha(float alpha) {
        mSearchBoxCoordinator.setAlpha(alpha);
    }

    /**
     * Updates the opacity of the search provider logo when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setSearchProviderLogoAlpha(float alpha) {
        if (mLogoCoordinator != null) mLogoCoordinator.setAlpha(alpha);
    }

    /**
     * Get the bounds of the search box in relation to the top level {@code parentView}.
     *
     * @param bounds The current drawing location of the search box.
     * @param translation The translation applied to the search box by the parent view hierarchy up
     *     to the {@code parentView}.
     * @param parentView The top level parent view used to translate search box bounds.
     */
    void getSearchBoxBounds(Rect bounds, Point translation, View parentView) {
        mSearchBoxCoordinator.getSearchBoxBounds(
                bounds, translation, parentView, mScrollDelegate, mSearchBoxBoundsVerticalInset);
    }

    /** Returns the fake search box's transition start offset on NTP. */
    private int getNtpSearchBoxTransitionStartOffset(boolean showFakeSearchBoxWithoutLogo) {
        if (mIsTablet && showFakeSearchBoxWithoutLogo) {
            // On tablets, it is possible to show fake search box if DSE doesn't have logo if DSE
            // mobile parity v2 is enabled. The mNTPFakeSearchBoxTransitionStartOffset is used to
            // calculate scrolling percentage in getToolbarTransitionPercentage(). Reset to 0 when
            // no doodle is shown for 3p DSE to prevent the alpha of fake search box being set to 0
            // (transparent) by ToolbarTablet#updateNtp().
            return 0;
        } else {
            return mNtpSearchBoxTransitionStartOffset;
        }
    }

    private void setSearchProviderTopMargin() {
        boolean showFakeSearchBoxWithoutLogo = !mSearchProviderHasLogo;
        mCurrentNtpFakeSearchBoxTransitionStartOffset =
                getNtpSearchBoxTransitionStartOffset(showFakeSearchBoxWithoutLogo);

        int topMargin = showFakeSearchBoxWithoutLogo ? mNtpSearchBoxTopMarginWithoutLogo : 0;
        mSearchBoxCoordinator.setTopMargin(topMargin);

        if (mLogoCoordinator != null) {
            mLogoCoordinator.setTopMargin(getLogoTopMargin());
        }
    }

    private void setLogoViewBottomMargin() {
        if (mLogoCoordinator == null) return;

        int logoViewBottomMarginPx =
                NtpCustomizationUtils.getLogoViewBottomMarginPx(mActivity.getResources());
        mLogoCoordinator.setBottomMargin(logoViewBottomMarginPx);
    }

    private int getLogoTopMargin() {
        Resources resources = mActivity.getResources();

        if (mShowingNonStandardGoogleLogo && mSearchProviderHasLogo) {
            return LogoUtils.getTopMarginForDoodle(resources);
        }

        return resources.getDimensionPixelSize(R.dimen.ntp_logo_margin_top);
    }

    /**
     * Sets the listener for search box scroll changes.
     *
     * @param listener The listener to be notified on changes.
     */
    void setSearchBoxScrollListener(@Nullable OnSearchBoxScrollListener listener) {
        mSearchBoxScrollListener = listener;
        if (mSearchBoxScrollListener != null) updateSearchBoxOnScroll();
    }

    // NewTabPageLayout.Delegate implementations.
    public void onMeasure(int width) {
        if (mIsTablet && mMostVisitedTilesCoordinator != null) {
            mMostVisitedTilesCoordinator.calculateTabletMvtWidth(width);
        }

        unifyElementWidths(width);
    }

    public void onAttachedToWindow() {
        if (!mHasShownView) {
            mHasShownView = true;
            onInitializationProgressChanged();
            TraceEvent.instant("NewTabPageSearchAvailable");
        }
    }

    /** Update the visibility of the action buttons. */
    public void updateActionButtonVisibility() {
        boolean shouldShowVoiceSearchButton = mManager.isVoiceSearchEnabled();
        boolean shouldShowLensButton =
                mSearchBoxCoordinator.isLensEnabled(LensEntryPoint.NEW_TAB_PAGE);
        if (mIsComposeplateEnabled == null) return;

        mSearchBoxCoordinator.setVoiceSearchButtonVisibility(shouldShowVoiceSearchButton);
        mSearchBoxCoordinator.setLensButtonVisibility(shouldShowLensButton);
        boolean shouldShowComposeplateButton = false;
        // As long as mComposeplateCoordinator has been initialized, we should update its
        // visibility.
        if (mComposeplateCoordinator != null) {
            shouldShowComposeplateButton =
                    mIsComposeplateEnabled
                            && mSearchProviderIsGoogle
                            && IncognitoUtils.isIncognitoModeEnabled(mProfile);
            mComposeplateCoordinator.setVisibility(
                    shouldShowComposeplateButton, mManager.isCurrentPage());
        }
        updatePreviousButtonVisibilityAndRecordMetrics(
                shouldShowVoiceSearchButton, shouldShowLensButton, shouldShowComposeplateButton);
    }

    /**
     * Updates the previous visibility state of the voice search and lens buttons and records
     * metrics if their visibility has changed on the current page.
     *
     * @param isVoiceSearchButtonVisible The new visibility state of the voice search button.
     * @param isLensButtonVisible The new visibility state of the lens button.
     * @param isComposeplateButtonVisible The new visibility state of the composeplate button.
     */
    private void updatePreviousButtonVisibilityAndRecordMetrics(
            boolean isVoiceSearchButtonVisible,
            boolean isLensButtonVisible,
            boolean isComposeplateButtonVisible) {
        if (!mManager.isCurrentPage()
                || (mPreviousVoiceSearchButtonVisible != null
                        && isVoiceSearchButtonVisible == mPreviousVoiceSearchButtonVisible
                        && mPreviousLensButtonVisible != null
                        && isLensButtonVisible == mPreviousLensButtonVisible)) {
            return;
        }

        if (mPreviousLensButtonVisible == null
                || isLensButtonVisible != mPreviousLensButtonVisible) {
            LensMetrics.recordShown(LensEntryPoint.NEW_TAB_PAGE, isLensButtonVisible);
        }

        ComposeplateMetricsUtils.recordFakeSearchBoxImpression2();
        ComposeplateMetricsUtils.recordFakeSearchBoxComposeplateButtonImpression2(
                isComposeplateButtonVisible);

        mPreviousVoiceSearchButtonVisible = isVoiceSearchButtonVisible;
        mPreviousLensButtonVisible = isLensButtonVisible;
    }

    /**
     * @see InvalidationAwareThumbnailProvider#shouldCaptureThumbnail()
     */
    public boolean shouldCaptureThumbnail() {
        return mSnapshotTileGridChanged;
    }

    /**
     * Should be called before a thumbnail of the parent view is captured.
     *
     * @see InvalidationAwareThumbnailProvider#captureThumbnail(Canvas)
     */
    public void onPreCaptureThumbnail() {
        if (mLogoCoordinator != null) mLogoCoordinator.endFadeAnimation();
        mSnapshotTileGridChanged = false;
    }

    private boolean shouldShowLogo() {
        return mSearchProviderHasLogo;
    }

    private boolean hasLoadCompleted() {
        return mHasShownView && mTilesLoaded;
    }

    @SuppressWarnings("NullAway")
    private void onDestroy() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }

        if (mLogoCoordinator != null) {
            mLogoCoordinator.destroy();
            mLogoCoordinator = null;
        }

        mSearchBoxCoordinator.destroy();
        mSearchBoxCoordinator = null;

        if (mMostVisitedTilesCoordinator != null) {
            mMostVisitedTilesCoordinator.destroyMvtiles();
            mMostVisitedTilesCoordinator = null;
        }

        if (mIsTablet) {
            mUiConfig.removeObserver(mDisplayStyleObserver);
            mDisplayStyleObserver = null;
        }

        if (mSearchEngineUtils != null) {
            if (mSearchBoxHintTextObserver != null) {
                mSearchEngineUtils.removeSearchBoxHintTextObserver(mSearchBoxHintTextObserver);
                mSearchBoxHintTextObserver = null;
            }
            if (mSearchEngineIconObserver != null) {
                mSearchEngineUtils.removeIconObserver(mSearchEngineIconObserver);
                mSearchEngineIconObserver = null;
            }
            mSearchEngineUtils = null;
        }

        mNewTabPageLayout.removeOnLayoutChangeListener(mOnLayoutChangeListener);
        mOnLayoutChangeListener = null;
        mNewTabPageLayout.setDelegate(null);

        if (mComposeplateCoordinator != null) {
            mComposeplateCoordinator.destroy();
            mComposeplateCoordinator = null;
        }

        if (mSigninPromoCoordinator != null) {
            mSigninPromoCoordinator.destroy();
            mSigninPromoCoordinator = null;
        }

        mSearchBoxScrollListener = null;
        mComposeplateUrlSupplier = null;
    }

    /** Makes the Search Box and Logo as wide as Most Visited. */
    private void unifyElementWidths(int width) {
        int searchBoxWidth = width - mSearchBoxTwoSideMargin;
        if (mSearchBoxCoordinator != null) {
            mSearchBoxCoordinator.setLayoutWidth(searchBoxWidth);
        }

        if (mLogoCoordinator != null) {
            mLogoCoordinator.setLayoutWidth(width);
        }

        if (mComposeplateCoordinator != null) {
            mComposeplateCoordinator.setLayoutWidth(searchBoxWidth);
        }
    }

    LogoCoordinator getLogoCoordinatorForTesting() {
        return mLogoCoordinator;
    }

    private void onDisplayStyleChanged(UiConfig.DisplayStyle newDisplayStyle) {
        if (!mIsTablet) return;

        updateDoodleOnTablet();
        if (mMostVisitedTilesCoordinator != null) {
            mMostVisitedTilesCoordinator.updateMvtOnTablet();
        }
        updateSearchBoxTwoSideMargin();
    }

    /**
     * Adjusts the doodle size while the tablet transitions to or from a multi-screen layout,
     * ensuring the change occurs post-logo initialization.
     */
    private void updateDoodleOnTablet() {
        if (!mIsTablet || mLogoCoordinator == null) return;

        mLogoCoordinator.updateDoodleOnTablet(mShowingNonStandardGoogleLogo);
    }

    private void updateSearchBoxTwoSideMargin() {
        mSearchBoxTwoSideMargin =
                NtpCustomizationUtils.getSearchBoxTwoSideMargin(
                        mActivity.getResources(), mUiConfig, mIsTablet);
    }

    /**
     * Called when the layout changes between edge-to-edge and standard.
     *
     * @param systemTopInset The system's top inset, i.e., the height of the Status bar. It is
     *     always bigger than 0.
     * @param supportsEdgeToEdgeOnTop Determines if the NTP should consume this top inset, extending
     *     itself to the Status bar area.
     */
    void onToEdgeChange(int systemTopInset, boolean supportsEdgeToEdgeOnTop) {
        // Exits early if the NTP's top padding doesn't require adjustment.
        if (NtpCustomizationUtils.shouldSkipTopInsetsChange(
                mTopInset, systemTopInset, supportsEdgeToEdgeOnTop)) {
            return;
        }

        mTopInset = supportsEdgeToEdgeOnTop ? systemTopInset : 0;
        mCurrentNtpFakeSearchBoxTransitionStartOffset =
                getNtpSearchBoxTransitionStartOffset(!mSearchProviderHasLogo) + mTopInset;

        int toolbarHeightNoShadow =
                mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
        // Top padding is applied to the NTP layout, ensuring all UI components remain in their
        // original positions after Status bar is hidden.
        mNewTabPageLayout.setPaddingRelative(
                mNewTabPageLayout.getPaddingStart(),
                toolbarHeightNoShadow + mTopInset,
                mNewTabPageLayout.getPaddingEnd(),
                mNewTabPageLayout.getPaddingBottom());

        if (mEnableLogs) {
            Log.i(TAG, "The top padding to add on the NTP is %d.", mTopInset);
        }
    }

    /**
     * Called when a customized background image is selected or deselected.
     *
     * @param applyWhiteBackgroundOnSearchBox Whether to apply a white background color to the fake
     *     search box.
     */
    void onCustomizedBackgroundChanged(boolean applyWhiteBackgroundOnSearchBox) {
        // If shouldn't apply a white background and the background hasn't been updated before,
        // returns now.
        if (mIsWhiteBackgroundOnSearchBoxApplied == null && !applyWhiteBackgroundOnSearchBox) {
            return;
        }

        // If composeplate view flags haven't been initialized yet, returns now.
        if (mIsComposeplateEnabled == null) {
            return;
        }

        // If the background has been updated before and it should remain the same, returns now.
        if (mIsWhiteBackgroundOnSearchBoxApplied != null
                && mIsWhiteBackgroundOnSearchBoxApplied == applyWhiteBackgroundOnSearchBox) {
            return;
        }

        // If the fake search box hasn't been initialized, returns now. It is fine to skip here
        // because applyWhiteBackgroundWithShadow() will be called immediately after the
        // mSearchBoxCoordinator is initialized.
        if (mSearchBoxCoordinator == null) return;

        mIsWhiteBackgroundOnSearchBoxApplied = applyWhiteBackgroundOnSearchBox;

        if (mSearchBoxCoordinator != null) {
            mSearchBoxCoordinator.applyWhiteBackgroundWithShadow(applyWhiteBackgroundOnSearchBox);
        }

        if (mComposeplateCoordinator != null) {
            mComposeplateCoordinator.applyWhiteBackgroundWithShadow(
                    applyWhiteBackgroundOnSearchBox);
        }
    }

    /** Returns the top inset of the NTP. */
    int getTopInset() {
        return mTopInset;
    }

    /** Returns the vertical inset applied to search box bounds. */
    int getSearchBoxBoundsVerticalInset() {
        return mSearchBoxBoundsVerticalInset;
    }

    NewTabPageLayout getNewTabPageLayout() {
        return mNewTabPageLayout;
    }
}
