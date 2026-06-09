// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.chromium.build.NullUtil.assertNonNull;
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

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.base.TimeUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.back_press.BackPressManager;
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
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.magic_stack.HomeModulesCoordinator;
import org.chromium.chrome.browser.magic_stack.ModuleDelegateHost;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.ntp.NewTabPage.OnSearchBoxScrollListener;
import org.chromium.chrome.browser.ntp.search.NtpSearchBox;
import org.chromium.chrome.browser.ntp.search.NtpSearchBoxFactory;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinatorFactory;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.theme.NtpCustomizationPromoManager;
import org.chromium.chrome.browser.omnibox.SearchEngineUtils;
import org.chromium.chrome.browser.omnibox.SearchEngineUtils.SearchBoxHintTextObserver;
import org.chromium.chrome.browser.omnibox.SearchEngineUtils.SearchEngineIconObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.setup_list.SetupListManager;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesCoordinator;
import org.chromium.chrome.browser.suggestions.tile.TileGroup;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.HomeSurfaceTracker;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
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
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.url.GURL;

import java.util.function.Supplier;

/**
 * Layout for the new tab page. This positions the page elements in the correct vertical positions.
 * There are no separate phone and tablet UIs; this layout adapts based on the available space.
 */
@NullMarked
public class NewTabPageCoordinator implements ModuleDelegateHost {
    private static final String TAG = "NewTabPageLayout";
    // Counts of the number of NTPs have been visible to users.
    private static int sCount;

    private final NewTabPageManager mManager;
    private final Activity mActivity;
    private final NewTabPageLayout mNewTabPageLayout;
    private final NewTabPageLayout.Delegate mLayoutDelegate;
    private final PropertyModel mModel;
    private final Tab mTab;
    private final TabModelSelector mTabModelSelector;
    private final OneshotSupplier<ModuleRegistry> mModuleRegistrySupplier;
    private final @Nullable HomeSurfaceTracker mHomeSurfaceTracker;
    private final SettableNullableObservableSupplier<Tab> mMostRecentTabSupplier =
            ObservableSuppliers.createNullable();
    private final WindowAndroid mWindowAndroid;
    private final Profile mProfile;
    private final ActivityResultTracker mActivityResultTracker;
    private final BottomSheetController mBottomSheetController;
    private final ModalDialogManager mModalDialogManager;
    private final SnackbarManager mSnackbarManager;
    private final Boolean mIsTablet;
    private final Supplier<Integer> mTabStripHeightSupplier;
    private final SearchEngineUtils mSearchEngineUtils;
    private final BackPressManager mBackPressManager;
    private final int mNtpSearchBoxTransitionStartOffset;
    private final int mNtpSearchBoxTopMarginWithoutLogo;
    private final boolean mEnableLogs;
    private final int mSearchBoxMaxWidth;

    private @Nullable LogoCoordinator mLogoCoordinator;
    private @Nullable NtpSearchBox mNtpSearchBox;
    private @Nullable MostVisitedTilesCoordinator mMostVisitedTilesCoordinator;
    private @Nullable OnSearchBoxScrollListener mSearchBoxScrollListener;
    private @Nullable UiConfig mUiConfig;
    private @Nullable DisplayStyleObserver mDisplayStyleObserver;
    private CallbackController mCallbackController = new CallbackController();
    private @Nullable SearchEngineIconObserver mSearchEngineIconObserver;
    private @Nullable SearchBoxHintTextObserver mSearchBoxHintTextObserver;

    private @Nullable HomeModulesCoordinator mHomeModulesCoordinator;
    private @Nullable ViewGroup mHomeModulesContainer;
    private SetupListManager.@Nullable Observer mSetupListObserver;
    private @Nullable Point mContextMenuStartPosition;
    private @Nullable NtpCustomizationCoordinator mNtpCustomizationCoordinator;

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
    private boolean mSnapshotSingleTabCardChanged;
    private int mSearchBoxTwoSideMargin;

    /**
     * Vertical inset to add to the top and bottom of the search box bounds. May be 0 if no inset
     * should be applied. See {@link Rect#inset(int, int)}.
     */
    private int mSearchBoxBoundsVerticalInset;

    private @Nullable FeedSurfaceScrollDelegate mScrollDelegate;
    private @Nullable Callback<Logo> mOnLogoAvailableCallback;

    // mCanShowComposeplateButton is null before checking whether to initialize composeplate view in
    // NewTabPageCoordinator#initialize().
    private @Nullable Boolean mCanShowComposeplateButton;
    private boolean mIsComposeplatePolicyEnabled;
    private boolean mIsComposeplateViewInitialized;
    private @Nullable Supplier<GURL> mComposeplateUrlSupplier;
    private @Nullable ComposeplateCoordinator mComposeplateCoordinator;
    // Previous visibility states for metrics.
    private @Nullable Boolean mPreviousVoiceSearchButtonVisible;
    private @Nullable Boolean mPreviousLensButtonVisible;
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
     * @param tab The {@link Tab} that contains this new tab page.
     * @param tabModelSelector {@link TabModelSelector} object.
     * @param moduleRegistrySupplier Supplier for the {@link ModuleRegistry}.
     * @param profile The {@link Profile} associated with the NTP. *
     * @param windowAndroid An instance of a {@link WindowAndroid}.
     * @param activityResultTracker Tracker of activity results.
     * @param bottomSheetController Used to interact with the bottom sheet.
     * @param modalDialogManager The instance of {@link ModalDialogManager}
     * @param snackbarManager Manages snackbars shown in the app.
     * @param isTablet {@code true} if the NTP surface is in tablet mode.
     * @param tabStripHeightSupplier Supplier of the tab strip height.
     * @param homeSurfaceTracker Used to decide whether we are the home surface.
     * @param backPressManager Manages back press dispatching.
     */
    public NewTabPageCoordinator(
            NewTabPageManager manager,
            Activity activity,
            NewTabPageLayout newTabPageLayout,
            Tab tab,
            TabModelSelector tabModelSelector,
            OneshotSupplier<ModuleRegistry> moduleRegistrySupplier,
            Profile profile,
            WindowAndroid windowAndroid,
            ActivityResultTracker activityResultTracker,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            SnackbarManager snackbarManager,
            boolean isTablet,
            Supplier<Integer> tabStripHeightSupplier,
            @Nullable HomeSurfaceTracker homeSurfaceTracker,
            BackPressManager backPressManager) {
        mBackPressManager = backPressManager;
        mManager = manager;
        mActivity = activity;
        mNewTabPageLayout = newTabPageLayout;
        mTab = tab;
        mTabModelSelector = tabModelSelector;
        mModuleRegistrySupplier = moduleRegistrySupplier;
        mHomeSurfaceTracker = homeSurfaceTracker;
        mProfile = profile;
        mWindowAndroid = windowAndroid;
        mActivityResultTracker = activityResultTracker;
        mBottomSheetController = bottomSheetController;
        mModalDialogManager = modalDialogManager;
        mSnackbarManager = snackbarManager;
        mIsTablet = isTablet;
        mTabStripHeightSupplier = tabStripHeightSupplier;
        mSearchEngineUtils = SearchEngineUtils.getForProfile(mProfile);

        Resources resources = mActivity.getResources();
        mNtpSearchBoxTopMarginWithoutLogo =
                resources.getDimensionPixelSize(R.dimen.ntp_search_box_top_margin_if_no_logo);
        mNtpSearchBoxTransitionStartOffset =
                resources.getDimensionPixelSize(R.dimen.ntp_search_box_transition_start_offset);
        mSearchBoxMaxWidth = resources.getDimensionPixelSize(R.dimen.ntp_search_box_max_width);

        mEnableLogs = ChromeFeatureList.sNewTabPageCustomizationV2EnableLogs.getValue();

        mModel = new PropertyModel(NewTabPageLayoutProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                mModel, mNewTabPageLayout, NewTabPageLayoutViewBinder::bind);
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
        mModel.set(NewTabPageLayoutProperties.DELEGATE, mLayoutDelegate);
        sCount++;

        NtpCustomizationPromoManager.maybeShowHomepageCustomizationSnackbarOnRecreate(
                mActivity, mSnackbarManager, ApplicationStatus.getTaskId(mActivity));
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
            Supplier<GURL> composeplateUrlSupplier) {
        TraceEvent.begin(TAG + ".initialize()");
        mScrollDelegate = scrollDelegate;
        mUiConfig = uiConfig;
        mComposeplateUrlSupplier = composeplateUrlSupplier;

        mContextMenuStartPosition =
                ReturnToChromeUtil.calculateContextMenuStartPosition(mActivity.getResources());

        if (mIsTablet) {
            mDisplayStyleObserver = this::onDisplayStyleChanged;
            mUiConfig.addObserver(mDisplayStyleObserver);
        } else {
            // On first run, the NewTabPageLayout is initialized behind the First Run Experience,
            // meaning the UiConfig will pickup the screen layout then. However
            // onConfigurationChanged is not called on orientation changes until the FRE is
            // completed. This means that if a user starts the FRE in one orientation, changes an
            // orientation and then leaves the FRE the UiConfig will have the wrong orientation.
            // https://crbug.com/41296612.
            mUiConfig.updateDisplayStyle();
        }

        ViewStub searchBoxStub = mNewTabPageLayout.findViewById(R.id.search_box_stub);
        mNtpSearchBox =
                NtpSearchBoxFactory.createSearchBox(
                        mActivity,
                        searchBoxStub,
                        mIsTablet,
                        lifecycleDispatcher,
                        mProfile.isOffTheRecord(),
                        mWindowAndroid,
                        mManager,
                        mProfile,
                        mBackPressManager);
        mModel.set(NewTabPageLayoutProperties.SEARCH_BOX_VIEW, mNtpSearchBox.getView());

        updateSearchBoxTwoSideMargin();
        initializeLogoCoordinator();
        setSearchProviderInfo(searchProviderHasLogo, searchProviderIsGoogle);
        initializeMostVisitedTilesCoordinator(
                mProfile, lifecycleDispatcher, tileGroupDelegate, touchEnabledDelegate);

        mSearchEngineIconObserver =
                (newIcon) -> assumeNonNull(mNtpSearchBox).setSearchEngineIcon(newIcon);
        mSearchEngineUtils.addIconObserver(mSearchEngineIconObserver);
        setSearchBoxTextAppearance();

        initializeSearchBoxTextView();

        initializeComposeplateFlags(mProfile);
        if (assumeNonNull(mCanShowComposeplateButton)) {
            initializeComposeplate();
        }

        initializeHomeModules();

        // This should be called after both mNtpSearchBox and mComposeplateCoordinator are
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

        mInitialized = true;

        TraceEvent.end(TAG + ".initialize()");
    }

    /** Sets the height of the search box and mSearchBoxBoundsVerticalInset. */
    private void setSearchBoxHeightBoundsVerticalInset() {
        Resources resources = mActivity.getResources();
        int searchBoxHeight =
                NtpCustomizationUtils.getSearchBoxHeight(
                        resources, assumeNonNull(mCanShowComposeplateButton));
        if (mNtpSearchBox != null) {
            mNtpSearchBox.setHeight(searchBoxHeight);
        }

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
        if (mNtpSearchBox != null) {
            mNtpSearchBox.enableSearchBoxEditText(enable);
        }
    }

    /**
     * @return The {@link FeedSurfaceScrollDelegate} for this class.
     */
    @Nullable FeedSurfaceScrollDelegate getScrollDelegate() {
        return mScrollDelegate;
    }

    /** Sets up the hint text and event handlers for the search box text view. */
    private void initializeSearchBoxTextView() {
        TraceEvent.begin(TAG + ".initializeSearchBoxTextView()");

        assumeNonNull(mNtpSearchBox);

        // @TODO(crbug.com/41492572): Add test case for search box OnDragListener.
        mNtpSearchBox.setSearchBoxDragListener(
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

        mNtpSearchBox.setSearchBoxTextWatcher(
                new EmptyTextWatcher() {
                    @Override
                    public void afterTextChanged(Editable s) {
                        if (s.length() == 0 || mNtpSearchBox == null) return;
                        mManager.focusSearchBox(
                                false, AutocompleteRequestType.SEARCH, false, s.toString());
                        mNtpSearchBox.setSearchText("");
                    }
                });
        TraceEvent.end(TAG + ".initializeSearchBoxTextView()");
    }

    public void onSearchBoxHintTextChanged() {
        if (mNtpSearchBox != null) {
            mNtpSearchBox.setSearchBoxHintText(
                    mSearchEngineUtils.getOmniboxHintText(
                            AutocompleteRequestType.SEARCH, /* fuseboxSessionState= */ null));
        }
    }

    private void setSearchBoxTextAppearance() {
        if (mNtpSearchBox == null) return;

        boolean shouldApplyWhiteBackground =
                NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox();

        if (shouldApplyWhiteBackground) {
            mNtpSearchBox.setSearchBoxTextAppearance(
                    R.style.TextAppearance_FakeSearchBoxTextMediumDark);
        } else {
            mNtpSearchBox.setSearchBoxTextAppearance(
                    R.style.TextAppearance_FakeSearchBoxTextMedium);
        }
    }

    private void initializeComposeplateFlags(Profile profile) {
        mCanShowComposeplateButton = ComposeplateUtils.canShowComposeplateButtonOnNtp(profile);
        mIsComposeplatePolicyEnabled =
                mCanShowComposeplateButton && ComposeplateUtils.isEnabledByPolicy(profile);
    }

    private void initializeComposeplate() {
        if (mIsComposeplateViewInitialized) return;

        mIsComposeplateViewInitialized = true;

        boolean shouldApplyWhiteBackgroundOnSearchBox =
                NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox();

        ViewStub composeplateViewStub = mNewTabPageLayout.findViewById(R.id.composeplate_view_stub);
        ViewGroup composeplateView = (ViewGroup) composeplateViewStub.inflate();
        mComposeplateCoordinator = new ComposeplateCoordinator(composeplateView, mProfile);
        mComposeplateCoordinator.setIncognitoClickListener(this::onIncognitoButtonClicked);
        // Don't log click metrics in this listener, since the mComposeplateCoordinator will
        // log.
        mComposeplateCoordinator.setComposeplateButtonClickListener(
                this::onComposeplateButtonClicked);

        if (shouldApplyWhiteBackgroundOnSearchBox) {
            // It is safe to call mComposeplateCoordinator.applyWhiteBackground() again since it is
            // no-op if the white background has been applied.
            mComposeplateCoordinator.applyWhiteBackground(/* apply= */ true);
        }
    }

    private void onComposeplateButtonClicked(View view) {
        if (OmniboxFeatures.isMultimodalInputEnabled(mActivity)
                && OmniboxFeatures.sRedirectComposeplateButton.getValue()
                && !mIsTablet
                && mIsComposeplatePolicyEnabled) {
            mManager.focusSearchBox(false, AutocompleteRequestType.AI_MODE, false, null);
            return;
        }

        GURL composeplateUrl = assumeNonNull(mComposeplateUrlSupplier).get();
        if (composeplateUrl == null) return;

        mManager.getNativePageHost()
                .loadUrl(new LoadUrlParams(composeplateUrl), /* incognito= */ false);
    }

    private void onIncognitoButtonClicked(View view) {
        if (!IncognitoUtils.isIncognitoModeEnabled(mProfile)) return;

        UrlConstantResolver resolver = UrlConstantResolverFactory.getForProfile(mProfile);
        mManager.getNativePageHost().loadUrl(new LoadUrlParams(resolver.getNtpUrl()), true);
    }

    @VisibleForTesting
    void initializeLayoutChangeListener() {
        TraceEvent.begin(TAG + ".initializeLayoutChangeListener()");
        mOnLayoutChangeListener = this::onLayoutChanged;
        mModel.set(NewTabPageLayoutProperties.ON_LAYOUT_CHANGE_LISTENER, mOnLayoutChangeListener);
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
        if (mScrollDelegate != null && mScrollDelegate.isScrollViewInitialized()) {
            mScrollDelegate.snapScroll();
        }
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
        mMostVisitedTilesCoordinator.updateMvtVisibility();
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
        if (mDisableUrlFocusChangeAnimations || mNtpSearchBox == null) return;

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
        if (mNtpSearchBox == null || mScrollDelegate == null) return 0f;

        return mNtpSearchBox.getToolbarTransitionPercentage(
                mScrollDelegate,
                mTabStripHeightSupplier,
                mCurrentNtpFakeSearchBoxTransitionStartOffset);
    }

    /**
     * @return The fake search box view.
     */
    public View getSearchBoxView() {
        return assumeNonNull(mNtpSearchBox).getView();
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
        if (mCanShowComposeplateButton != null) {
            // When mSearchProviderIsGoogle is changed, mCanShowComposeplateButton might be changed
            // too, recalculate its value.
            if (isSearchProviderIsGoogleChanged) {
                boolean previousCanShowComposeplateButton = mCanShowComposeplateButton;
                initializeComposeplateFlags(mProfile);
                if (!previousCanShowComposeplateButton
                        && mCanShowComposeplateButton
                        && mComposeplateCoordinator == null) {
                    // If the composeplate view is enabled while mComposeplateCoordinator hasn't
                    // been initialized yet, initialize it now.
                    initializeComposeplate();
                }

                if (previousCanShowComposeplateButton != mCanShowComposeplateButton) {
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
        if (disable) {
            // Force reset layout translation Y to prevent elements from staying stuck off-screen.
            mModel.set(NewTabPageLayoutProperties.TRANSITION_Y, 0f);
            // Force restore fake search box and logo alphas to fully visible (1.f).
            setSearchBoxAlpha(1.f);
            setSearchProviderLogoAlpha(1.f);
        }
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
        if (mDisableUrlFocusChangeAnimations
                || mIsTablet
                || mNtpSearchBox == null
                || mScrollDelegate == null) {
            return;
        }

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
        mModel.set(NewTabPageLayoutProperties.TRANSITION_Y, translationY);
    }

    /**
     * Updates the opacity of the search box when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setSearchBoxAlpha(float alpha) {
        if (mDisableUrlFocusChangeAnimations) return;

        if (mNtpSearchBox != null) {
            mNtpSearchBox.setAlpha(alpha);
        }
    }

    /**
     * Updates the opacity of the search provider logo when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setSearchProviderLogoAlpha(float alpha) {
        if (mDisableUrlFocusChangeAnimations) return;

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
        if (mNtpSearchBox != null && mScrollDelegate != null) {
            mNtpSearchBox.getSearchBoxBounds(
                    bounds,
                    translation,
                    parentView,
                    mScrollDelegate,
                    mSearchBoxBoundsVerticalInset);
        }
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

        if (mNtpSearchBox != null) {
            int topMargin = showFakeSearchBoxWithoutLogo ? mNtpSearchBoxTopMarginWithoutLogo : 0;
            mNtpSearchBox.setTopMargin(topMargin);
        }

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
            if (NtpCustomizationPromoManager.canTriggerCustomizationBottomSheet(
                    mWindowAndroid, mIsTablet, sCount)) {
                triggerCustomizationBottomSheet();
            }
            TraceEvent.instant("NewTabPageSearchAvailable");
        }
    }

    /** Update the visibility of the action buttons. */
    public void updateActionButtonVisibility() {
        if (mNtpSearchBox == null) return;

        boolean shouldShowVoiceSearchButton = mManager.isVoiceSearchEnabled();
        boolean shouldShowLensButton = mNtpSearchBox.isLensEnabled(LensEntryPoint.NEW_TAB_PAGE);

        // Skips now if the composeplate flag hasn't been initialized. This prevents logging the
        // impression metrics incorrectly due to the status of whether to show the composeplate
        // button hasn't been initialized.
        if (mCanShowComposeplateButton == null) return;

        mNtpSearchBox.setVoiceSearchButtonVisibility(shouldShowVoiceSearchButton);
        mNtpSearchBox.setLensButtonVisibility(shouldShowLensButton);
        boolean shouldShowComposeplateButton = false;
        // As long as mComposeplateCoordinator has been initialized, we should update its
        // visibility.
        if (mComposeplateCoordinator != null) {
            shouldShowComposeplateButton =
                    mCanShowComposeplateButton
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
        return mSnapshotTileGridChanged || mSnapshotSingleTabCardChanged;
    }

    /**
     * Should be called before a thumbnail of the parent view is captured.
     *
     * @see InvalidationAwareThumbnailProvider#captureThumbnail(Canvas)
     */
    public void onPreCaptureThumbnail() {
        if (mLogoCoordinator != null) mLogoCoordinator.endFadeAnimation();
        mSnapshotTileGridChanged = false;
        mSnapshotSingleTabCardChanged = false;
    }

    private boolean shouldShowLogo() {
        return mSearchProviderHasLogo;
    }

    private boolean hasLoadCompleted() {
        return mHasShownView && mTilesLoaded;
    }

    /** Initialize the magic stack on NTP. */
    @VisibleForTesting
    void initializeHomeModules() {
        boolean isTrackingTabReady =
                mHomeSurfaceTracker != null && mHomeSurfaceTracker.isHomeSurfaceTab(mTab);
        // The magic stack is shown on every NTP. There are three cases:
        // 1) on any normal NewTabPage. Initialize the magic stack here.
        // 2) The home surface NewTabPage which is created via back operations. Initialize the
        // magic stack here, and re-show the single Tab card with the previously tracked Tab.
        // 3) The home surface NewTabPage which is created at startup. The magic stack will be
        // initialized later since its tracking Tab hasn't been available yet.
        // The launch type of a home surface NTP is TabLaunchType.FROM_STARTUP.
        if (isTrackingTabReady) {
            assumeNonNull(mHomeSurfaceTracker);
            // Case 2) on home surface NTP via back operations.
            showHomeSurfaceUiOnNtp(mHomeSurfaceTracker.getLastActiveTabToTrack());
        } else if (mTab.getLaunchType() != TabLaunchType.FROM_STARTUP) {
            // Case 1) on normal NTP.
            showHomeSurfaceUiOnNtp(null);
        }

        if (isTrackingTabReady) {
            ReturnToChromeUtil.recordHomeSurfaceShown();
        }
    }

    /**
     * Called to update the home modules.
     *
     * @param isLoaded Whether the host surface has been loaded.
     */
    void maybeUpdateHomeModules(boolean isLoaded) {
        if (isLoaded && mHomeModulesCoordinator != null) {
            mHomeModulesCoordinator.updateModules();
        }
    }

    /**
     * Shows the magic stack with the last active Tab if exists on the home surface NTP.
     *
     * @param mostRecentTab The last shown Tab if exists. It is non null for NTP home surface only.
     */
    public void showHomeSurfaceUiOnNtp(@Nullable Tab mostRecentTab) {
        if (mModuleRegistrySupplier.get() == null) {
            return;
        }

        if (mostRecentTab != null && !UrlUtilities.isNtpUrl(mostRecentTab.getUrl())) {
            mMostRecentTabSupplier.set(mostRecentTab);
        }

        Profile profile = mProfile;
        if (profile != null) {
            SetupListManager.getInstance().maybePrimeCompletionStatus(profile.getOriginalProfile());
        }

        if (!NtpCustomizationUtils.isNtpSimplificationEnabledOnDesktop()
                && mHomeModulesCoordinator == null) {
            initializeHomeModulesImpl();
        }
        if (mHomeModulesCoordinator != null) {
            mHomeModulesCoordinator.show(this::onHomeModulesShown);
        }
    }

    /**
     * Initializes the magic stack to show home modules on the current new tab page which is used as
     * the home surface.
     */
    @EnsuresNonNull({"mHomeModulesContainer", "mHomeModulesCoordinator"})
    private void initializeHomeModulesImpl() {
        mHomeModulesContainer =
                (ViewGroup)
                        ((ViewStub)
                                        mNewTabPageLayout.findViewById(
                                                R.id.home_modules_recycler_view_stub))
                                .inflate();
        MonotonicObservableSupplier<Profile> profileSupplier =
                ObservableSuppliers.createMonotonic(mProfile);
        mHomeModulesCoordinator =
                new HomeModulesCoordinator(
                        mActivity,
                        this,
                        mNewTabPageLayout,
                        HomeModulesConfigManager.getInstance(),
                        profileSupplier,
                        assumeNonNull(mModuleRegistrySupplier.get()));

        if (SetupListManager.getInstance().isSetupListActive()) {
            mSetupListObserver =
                    () -> {
                        if (mHomeModulesCoordinator != null) {
                            mHomeModulesCoordinator.refreshModules();
                        }
                    };
            SetupListManager.getInstance().addObserver(mSetupListObserver);
        }
    }

    @VisibleForTesting
    void onHomeModulesShown(boolean isVisible) {
        assumeNonNull(mHomeModulesContainer);
        mHomeModulesContainer.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    private void onTabClicked(int tabId) {
        TabModelUtils.selectTabById(mTabModelSelector, tabId, TabSelectionType.FROM_USER);

        mTabModelSelector
                .getModel(false)
                .getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTab(mTab).allowUndo(false).build(),
                        /* allowDialog= */ false);
        if (mHomeSurfaceTracker != null) {
            // Updates the mHomeSurfaceTracker since the Tab of the NTP is closed.
            mHomeSurfaceTracker.updateHomeSurfaceAndTrackingTabs(null, null);
        }
    }

    /** Shows the NTP theme tip bottom sheet. */
    void triggerCustomizationBottomSheet() {
        mNtpCustomizationCoordinator =
                NtpCustomizationCoordinatorFactory.getInstance()
                        .create(
                                mActivity,
                                mBottomSheetController,
                                mTab::getProfile,
                                NtpCustomizationCoordinator.BottomSheetType.THEME_TIP,
                                mWindowAndroid,
                                mModuleRegistrySupplier.get(),
                                mSnackbarManager);
        mNtpCustomizationCoordinator.showBottomSheet();
        NtpCustomizationUtils.setThemeTipBottomSheetShownTimestampToSharedPreference(
                TimeUtils.currentTimeMillis());
    }

    // ModuleDelegateHost implementation

    @Override
    public @Nullable Point getContextMenuStartPoint() {
        return mContextMenuStartPosition;
    }

    @Override
    public @Nullable UiConfig getUiConfig() {
        return mIsTablet ? mUiConfig : null;
    }

    @Override
    public void onUrlClicked(GURL gurl) {
        mTab.loadUrl(new LoadUrlParams(gurl));
    }

    @Override
    public void onTabSelected(int tabId) {
        onTabClicked(tabId);
    }

    @Override
    public void onCaptureThumbnailStatusChanged() {
        mSnapshotSingleTabCardChanged = true;
    }

    @Override
    public void customizeSettings() {
        NtpCustomizationCoordinatorFactory.getInstance()
                .create(
                        mActivity,
                        mBottomSheetController,
                        mTab::getProfile,
                        NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS,
                        mWindowAndroid,
                        mModuleRegistrySupplier.get(),
                        mSnackbarManager)
                .showBottomSheet();
    }

    @Override
    public int getStartMargin() {
        boolean isInNarrowWindowOnTablet =
                mIsTablet
                        && NtpCustomizationUtils.isInNarrowWindowOnTablet(
                                mIsTablet, assumeNonNull(mUiConfig));
        int marginResourceId =
                isInNarrowWindowOnTablet
                        ? R.dimen.ntp_search_box_lateral_margin_narrow_window_tablet
                        : R.dimen.mvt_container_lateral_margin;
        return mActivity.getResources().getDimensionPixelSize(marginResourceId);
    }

    @Override
    public @Nullable Tab getTrackingTab() {
        return mMostRecentTabSupplier.get();
    }

    @Override
    public boolean isHomeSurface() {
        // Can only show a local tab to resume if we we have a tracked tab. The presence of the
        // local tab to resume module is effectively what being a home surface is.
        return mMostRecentTabSupplier.get() != null;
    }

    @SuppressWarnings("NullAway")
    public void destroy() {
        if (mNtpCustomizationCoordinator != null) {
            mNtpCustomizationCoordinator.destroy();
            mNtpCustomizationCoordinator = null;
        }

        mMostRecentTabSupplier.set(null);

        if (mSearchBoxHintTextObserver != null) {
            mSearchEngineUtils.removeSearchBoxHintTextObserver(mSearchBoxHintTextObserver);
            mSearchBoxHintTextObserver = null;
        }

        if (mSearchEngineIconObserver != null) {
            mSearchEngineUtils.removeIconObserver(mSearchEngineIconObserver);
            mSearchEngineIconObserver = null;
        }

        if (mSigninPromoCoordinator != null) {
            mSigninPromoCoordinator.destroy();
            mSigninPromoCoordinator = null;
        }

        mModel.set(NewTabPageLayoutProperties.ON_LAYOUT_CHANGE_LISTENER, null);
        mOnLayoutChangeListener = null;

        if (mSetupListObserver != null) {
            SetupListManager.getInstance().removeObserver(mSetupListObserver);
            mSetupListObserver = null;
        }

        if (mHomeModulesCoordinator != null) {
            mHomeModulesCoordinator.destroy();
            mHomeModulesCoordinator = null;
        }

        if (mHomeModulesContainer != null) {
            mHomeModulesContainer = null;
        }

        if (mComposeplateCoordinator != null) {
            mComposeplateCoordinator.destroy();
            mComposeplateCoordinator = null;
        }

        if (mNtpSearchBox != null) {
            mModel.set(NewTabPageLayoutProperties.SEARCH_BOX_VIEW, null);
            mNtpSearchBox.destroy();
            mNtpSearchBox = null;
        }

        if (mMostVisitedTilesCoordinator != null) {
            mMostVisitedTilesCoordinator.destroy();
            mMostVisitedTilesCoordinator = null;
        }

        if (mLogoCoordinator != null) {
            mLogoCoordinator.destroy();
            mLogoCoordinator = null;
        }

        if (mOnLogoAvailableCallback != null) {
            mOnLogoAvailableCallback = null;
        }

        if (mDisplayStyleObserver != null) {
            if (mUiConfig != null) {
                mUiConfig.removeObserver(mDisplayStyleObserver);
            }
            mDisplayStyleObserver = null;
        }
        mUiConfig = null;

        mModel.set(NewTabPageLayoutProperties.DELEGATE, null);

        mSearchBoxScrollListener = null;
        mComposeplateUrlSupplier = null;
        mScrollDelegate = null;

        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
    }

    /** Makes the Search Box and Logo as wide as Most Visited. */
    private void unifyElementWidths(int width) {
        int boundedSearchBoxWidth = Math.min(width - mSearchBoxTwoSideMargin, mSearchBoxMaxWidth);
        if (mNtpSearchBox != null) {
            mNtpSearchBox.setLayoutWidth(boundedSearchBoxWidth);
        }

        if (mLogoCoordinator != null) {
            mLogoCoordinator.setLayoutWidth(width);
        }

        if (mComposeplateCoordinator != null) {
            mComposeplateCoordinator.setLayoutWidth(boundedSearchBoxWidth);
        }

        mContextMenuStartPosition = null;
    }

    LogoCoordinator getLogoCoordinatorForTesting() {
        return assertNonNull(mLogoCoordinator);
    }

    public boolean isMagicStackVisibleForTesting() {
        if (mHomeModulesContainer == null) return false;

        return mHomeModulesContainer.getVisibility() == View.VISIBLE;
    }

    public boolean getSnapshotSingleTabCardChangedForTesting() {
        return mSnapshotSingleTabCardChanged;
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
                        mActivity.getResources(), assertNonNull(mUiConfig), mIsTablet);
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
        mModel.set(NewTabPageLayoutProperties.TOP_INSET_PX, toolbarHeightNoShadow + mTopInset);

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
        if (mCanShowComposeplateButton == null) {
            return;
        }

        // If the background has been updated before and it should remain the same, returns now.
        if (mIsWhiteBackgroundOnSearchBoxApplied != null
                && mIsWhiteBackgroundOnSearchBoxApplied == applyWhiteBackgroundOnSearchBox) {
            return;
        }

        // If the fake search box hasn't been initialized, returns now. It is fine to skip here
        // because applyWhiteBackground() will be called immediately after the mNtpSearchBox
        // is initialized.
        if (mNtpSearchBox == null) return;

        mIsWhiteBackgroundOnSearchBoxApplied = applyWhiteBackgroundOnSearchBox;

        if (mNtpSearchBox != null) {
            mNtpSearchBox.applyWhiteBackground(applyWhiteBackgroundOnSearchBox);
        }

        if (mComposeplateCoordinator != null) {
            mComposeplateCoordinator.applyWhiteBackground(applyWhiteBackgroundOnSearchBox);
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

    public @Nullable HomeModulesCoordinator getHomeModulesCoordinatorForTesting() {
        return mHomeModulesCoordinator;
    }

    public PropertyModel getModelForTesting() {
        return mModel;
    }

    public @Nullable ViewGroup getHomeModulesContainerForTesting() {
        return mHomeModulesContainer;
    }

    public static void setCountForTesting(int count) {
        sCount = count;
    }
}
