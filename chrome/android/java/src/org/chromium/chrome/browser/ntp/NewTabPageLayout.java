// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.Rect;
import android.text.Editable;
import android.util.AttributeSet;
import android.view.DragEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.RawRes;
import androidx.annotation.StyleRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.composeplate.ComposeplateCoordinator;
import org.chromium.chrome.browser.composeplate.ComposeplateMetricsUtils;
import org.chromium.chrome.browser.composeplate.ComposeplateUtils;
import org.chromium.chrome.browser.feed.FeedSurfaceScrollDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensMetrics;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.logo.LogoCoordinator;
import org.chromium.chrome.browser.logo.LogoUtils;
import org.chromium.chrome.browser.logo.LogoUtils.DoodleSize;
import org.chromium.chrome.browser.logo.LogoView;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.ntp.NewTabPage.OnSearchBoxScrollListener;
import org.chromium.chrome.browser.ntp.search.SearchBoxCoordinator;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.omnibox.SearchEngineUtils;
import org.chromium.chrome.browser.omnibox.status.StatusProperties;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesCoordinator;
import org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesLayout;
import org.chromium.chrome.browser.suggestions.tile.TileGroup;
import org.chromium.chrome.browser.suggestions.tile.TileGroup.Delegate;
import org.chromium.chrome.browser.tab_ui.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.chrome.browser.ui.signin.signin_promo.NtpSigninPromoCoordinator;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.util.function.Supplier;

/**
 * Layout for the new tab page. This positions the page elements in the correct vertical positions.
 * There are no separate phone and tablet UIs; this layout adapts based on the available space.
 */
@NullMarked
public class NewTabPageLayout extends LinearLayout
        implements SearchEngineUtils.SearchBoxHintTextObserver,
                SearchEngineUtils.SearchEngineIconObserver {
    private static final String TAG = "NewTabPageLayout";

    private int mSearchBoxTwoSideMargin;
    private final Context mContext;

    private LogoCoordinator mLogoCoordinator;
    private LogoView mLogoView;
    private SearchBoxCoordinator mSearchBoxCoordinator;
    private ViewGroup mMvTilesContainerLayout;
    private MostVisitedTilesCoordinator mMostVisitedTilesCoordinator;

    private @Nullable OnSearchBoxScrollListener mSearchBoxScrollListener;

    private NewTabPageManager mManager;
    private Activity mActivity;
    private Profile mProfile;
    private UiConfig mUiConfig;
    private @Nullable DisplayStyleObserver mDisplayStyleObserver;
    private CallbackController mCallbackController = new CallbackController();

    /**
     * Whether the tiles shown in the layout have finished loading.
     * With {@link #mHasShownView}, it's one of the 2 flags used to track initialisation progress.
     */
    private boolean mTilesLoaded;

    /**
     * Whether the view has been shown at least once.
     * With {@link #mTilesLoaded}, it's one of the 2 flags used to track initialization progress.
     */
    private boolean mHasShownView;

    private boolean mSearchProviderHasLogo = true;
    private boolean mSearchProviderIsGoogle;
    private boolean mShowingNonStandardGoogleLogo;
    private boolean mIsOmniboxMobileParityUpdateV2Enabled;

    private boolean mInitialized;

    private float mUrlFocusChangePercent;
    private boolean mDisableUrlFocusChangeAnimations;
    private boolean mIsViewMoving;

    /** Flag used to request some layout changes after the next layout pass is completed. */
    private boolean mTileCountChanged;

    private boolean mSnapshotTileGridChanged;
    private WindowAndroid mWindowAndroid;

    /**
     * Vertical inset to add to the top and bottom of the search box bounds. May be 0 if no inset
     * should be applied. See {@link Rect#inset(int, int)}.
     */
    private int mSearchBoxBoundsVerticalInset;

    private FeedSurfaceScrollDelegate mScrollDelegate;

    private boolean mMvtContentFits;
    private float mTransitionEndOffset;
    private boolean mIsTablet;
    private ObservableSupplier<Integer> mTabStripHeightSupplier;
    private boolean mIsInNarrowWindowOnTablet;
    // This variable is only valid when the NTP surface is in tablet mode.
    private boolean mIsInMultiWindowModeOnTablet;
    private Callback<Logo> mOnLogoAvailableCallback;
    private boolean mIsComposeplateEnabled;
    private boolean mIsComposeplateV2Enabled;
    private boolean mIsComposeplatePolicyEnabled;
    private @Nullable Supplier<GURL> mComposeplateUrlSupplier;
    private OnClickListener mVoiceSearchButtonClickListener;
    private OnClickListener mLensButtonClickListener;
    private View.@Nullable OnClickListener mComposeplateButtonClickListener;
    private @Nullable ComposeplateCoordinator mComposeplateCoordinator;
    // Previous visibility states for metrics.
    private @Nullable Boolean mPreviousVoiceSearchButtonVisible;
    private @Nullable Boolean mPreviousLensButtonVisible;
    private @Nullable ImageView mDseIconView;
    private SearchEngineUtils mSearchEngineUtils;
    private final int mNtpSearchBoxTransitionStartOffset;
    private final int mNtpSearchBoxTopMarginWithoutLogo;
    private final int mFakeSearchBoxStartPadding;
    private final int mFakeSearchBoxStartPaddingWithDseLogo;
    private int mCurrentNtpFakeSearchBoxTransitionStartOffset;
    private int mTopInset;
    private @Nullable OnLayoutChangeListener mOnLayoutChangeListener;
    // TODO(crbug.com/451602301): remove @Nullable and all null checks once
    // ENABLE_SEAMLESS_SIGNIN is removed after the experiment.
    private @Nullable NtpSigninPromoCoordinator mSigninPromoCoordinator;

    /** Constructor for inflating from XML. */
    public NewTabPageLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        Resources resources = getResources();
        mNtpSearchBoxTopMarginWithoutLogo =
                resources.getDimensionPixelSize(R.dimen.mvt_container_top_margin);
        mNtpSearchBoxTransitionStartOffset =
                resources.getDimensionPixelSize(R.dimen.ntp_search_box_transition_start_offset);

        mFakeSearchBoxStartPadding =
                resources.getDimensionPixelSize(R.dimen.fake_search_box_start_padding);
        mFakeSearchBoxStartPaddingWithDseLogo =
                resources.getDimensionPixelSize(
                        R.dimen.fake_search_box_start_padding_with_dse_logo);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        setBackgroundColor(
                getResources()
                        .getColor(R.color.home_surface_background_color, getContext().getTheme()));

        // TODO(crbug.com/347509698): Remove the log statements after fixing the bug.
        Log.i(TAG, "NewTabPageLayout.onFinishInflate before insertSiteSectionView");

        initializeSiteSectionView();

        Log.i(TAG, "NewTabPageLayout.onFinishInflate after insertSiteSectionView");
    }

    /**
     * Initializes the NewTabPageLayout. This must be called immediately after inflation, before
     * this object is used in any other way.
     *
     * @param manager NewTabPageManager used to perform various actions when the user interacts with
     *     the page.
     * @param activity The activity that currently owns the new tab page
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
     * @param isTablet {@code true} if the NTP surface is in tablet mode.
     * @param tabStripHeightSupplier Supplier of the tab strip height.
     */
    @Initializer
    public void initialize(
            NewTabPageManager manager,
            Activity activity,
            Delegate tileGroupDelegate,
            boolean searchProviderHasLogo,
            boolean searchProviderIsGoogle,
            FeedSurfaceScrollDelegate scrollDelegate,
            TouchEnabledDelegate touchEnabledDelegate,
            UiConfig uiConfig,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            Profile profile,
            WindowAndroid windowAndroid,
            boolean isTablet,
            ObservableSupplier<Integer> tabStripHeightSupplier,
            Supplier<GURL> composeplateUrlSupplier) {
        TraceEvent.begin(TAG + ".initialize()");
        mScrollDelegate = scrollDelegate;
        mManager = manager;
        mActivity = activity;
        mProfile = profile;
        mUiConfig = uiConfig;
        mWindowAndroid = windowAndroid;
        mIsTablet = isTablet;
        mTabStripHeightSupplier = tabStripHeightSupplier;
        mIsComposeplateEnabled = ComposeplateUtils.isComposeplateEnabled(mIsTablet, profile);
        mSearchEngineUtils = SearchEngineUtils.getForProfile(mProfile);
        mIsComposeplateV2Enabled =
                mIsComposeplateEnabled
                        && ChromeFeatureList.sAndroidComposeplateV2Enabled.getValue();
        mIsComposeplatePolicyEnabled =
                mIsComposeplateV2Enabled && ComposeplateUtils.isEnabledByPolicy(profile);
        if (mIsComposeplateEnabled) {
            mComposeplateUrlSupplier = composeplateUrlSupplier;
        }
        mIsOmniboxMobileParityUpdateV2Enabled =
                OmniboxFeatures.sOmniboxMobileParityUpdateV2.isEnabled();

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

        mSearchBoxCoordinator = new SearchBoxCoordinator(getContext(), this);
        mSearchBoxCoordinator.initialize(
                lifecycleDispatcher, mProfile.isOffTheRecord(), mWindowAndroid);
        if (mIsComposeplateV2Enabled) {
            mSearchBoxCoordinator.setHeight(
                    getResources().getDimensionPixelSize(R.dimen.ntp_search_box_height_tall));
        }
        int searchBoxHeight = mSearchBoxCoordinator.getView().getLayoutParams().height;
        mSearchBoxBoundsVerticalInset =
                (searchBoxHeight
                                - getResources()
                                        .getDimensionPixelSize(R.dimen.toolbar_height_no_shadow))
                        / 2;
        mTransitionEndOffset =
                !mIsTablet
                        ? getResources()
                                .getDimensionPixelSize(R.dimen.ntp_search_box_transition_end_offset)
                        : 0;

        updateSearchBoxWidth();
        initializeLogoCoordinator(searchProviderHasLogo, searchProviderIsGoogle);
        initializeMostVisitedTilesCoordinator(
                mProfile, lifecycleDispatcher, tileGroupDelegate, touchEnabledDelegate);
        initializeDseIconView(shouldShowDseIcon());
        initializeSearchBoxTextView();
        initializeVoiceSearchButton();
        initializeLensButton();
        initializeComposeplate();

        // This should be called after both mSearchBoxCoordinator and mComposeplateCoordinator are
        // initialized.
        if (NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox()) {
            onCustomizedBackgroundChanged(/* applyWhiteBackgroundOnSearchBox= */ true);
        }

        updateActionButtonVisibility();
        initializeLayoutChangeListener();
        if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)) {
            initializeSigninPromoCoordinator();
        }

        // Initialize Searchbox observers
        mSearchEngineUtils.addSearchBoxHintTextObserver(this);

        manager.addDestructionObserver(NewTabPageLayout.this::onDestroy);
        mInitialized = true;

        TraceEvent.end(TAG + ".initialize()");
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
                new OnDragListener() {
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

    private void initializeDseIconView(boolean shouldShowDesIconView) {
        View fakeSearchBoxLayout = findViewById(R.id.search_box);
        mDseIconView = fakeSearchBoxLayout.findViewById(R.id.search_box_engine_icon);

        // Configures icon rounding.
        mDseIconView.setOutlineProvider(
                new RoundedCornerOutlineProvider(
                        getResources()
                                        .getDimensionPixelSize(
                                                R.dimen.omnibox_search_engine_logo_composed_size)
                                / 2));
        mDseIconView.setClipToOutline(true);
        mSearchEngineUtils.addIconObserver(this);
        ImageViewCompat.setImageTintList(mDseIconView, null);
        setDseIconViewVisibility(shouldShowDesIconView);
    }

    @Override
    public void onSearchEngineIconChanged(StatusProperties.@Nullable StatusIconResource newIcon) {
        if (mDseIconView == null) return;
        if (newIcon == null) {
            mDseIconView.setImageResource(R.drawable.ic_search);
            return;
        }

        // When DSE is Google, onSearchEngineIconChanged() is called before setSearchProviderInfo().
        // Thus, we check the icon's resource id to change the icon to be
        // R.drawable.ic_logo_googleg_24dp which doesn't have a padding.
        if (newIcon.getIconRes() == R.drawable.ic_logo_googleg_20dp) {
            mDseIconView.setImageResource(R.drawable.ic_logo_googleg_24dp);
            return;
        }

        mDseIconView.setImageDrawable(newIcon.getDrawable(mContext, mContext.getResources()));
    }

    @Override
    public void onSearchBoxHintTextChanged() {
        mSearchBoxCoordinator.setSearchBoxHintText(
                mSearchEngineUtils.getOmniboxHintText(AutocompleteRequestType.SEARCH));
    }

    private void setDseIconViewVisibility(boolean isVisible) {
        if (mDseIconView == null) return;

        int visibility = isVisible ? VISIBLE : GONE;
        if (mDseIconView.getVisibility() == visibility) return;

        mDseIconView.setVisibility(visibility);
        boolean shouldApplyWhiteBackground =
                NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox();

        if (isVisible) {
            mSearchBoxCoordinator.setStartPadding(mFakeSearchBoxStartPaddingWithDseLogo);
            if (shouldApplyWhiteBackground) {
                mSearchBoxCoordinator.setSearchBoxTextAppearance(
                        R.style.TextAppearance_FakeSearchBoxTextMediumDark);
            } else {
                mSearchBoxCoordinator.setSearchBoxTextAppearance(
                        R.style.TextAppearance_FakeSearchBoxTextMedium);
            }
        } else {
            mSearchBoxCoordinator.setStartPadding(mFakeSearchBoxStartPadding);
            if (shouldApplyWhiteBackground) {
                mSearchBoxCoordinator.setSearchBoxTextAppearance(
                        R.style.TextAppearance_FakeSearchBoxTextDark);
            } else {
                mSearchBoxCoordinator.setSearchBoxTextAppearance(
                        R.style.TextAppearance_FakeSearchBoxText);
            }
        }
    }

    private void initializeVoiceSearchButton() {
        TraceEvent.begin(TAG + ".initializeVoiceSearchButton()");
        mVoiceSearchButtonClickListener =
                v -> mManager.focusSearchBox(true, AutocompleteRequestType.SEARCH, null);
        mSearchBoxCoordinator.addVoiceSearchButtonClickListener(mVoiceSearchButtonClickListener);
        TraceEvent.end(TAG + ".initializeVoiceSearchButton()");
    }

    private void initializeLensButton() {
        TraceEvent.begin(TAG + ".initializeLensButton()");
        mLensButtonClickListener =
                v -> {
                    LensMetrics.recordClicked(LensEntryPoint.NEW_TAB_PAGE);
                    mSearchBoxCoordinator.startLens(LensEntryPoint.NEW_TAB_PAGE);
                };
        mSearchBoxCoordinator.addLensButtonClickListener(mLensButtonClickListener);
        TraceEvent.end(TAG + ".initializeLensButton()");
    }

    private void initializeComposeplate() {
        if (!mIsComposeplateEnabled) return;

        boolean shouldApplyWhiteBackgroundOnSearchBox =
                NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox();
        ColorStateList colorStateList =
                NtpCustomizationUtils.getSearchBoxIconColorTint(
                        mContext, shouldApplyWhiteBackgroundOnSearchBox);
        @StyleRes
        int textStyleResId =
                NtpCustomizationUtils.getSearchBoxTextStyleResId(
                        shouldApplyWhiteBackgroundOnSearchBox);

        if (!mIsComposeplateV2Enabled) {
            mComposeplateButtonClickListener =
                    view -> {
                        onComposeplateButtonClicked(view);
                        ComposeplateMetricsUtils.recordFakeSearchBoxComposeplateButtonClick();
                    };
            mSearchBoxCoordinator.setComposeplateButtonClickListener(
                    mComposeplateButtonClickListener);
            @RawRes
            int iconRawResId =
                    !shouldApplyWhiteBackgroundOnSearchBox && ColorUtils.inNightMode(mContext)
                            ? R.raw.composeplate_loop_dark
                            : R.raw.composeplate_loop_light;
            mSearchBoxCoordinator.setComposeplateButtonIconRawResId(iconRawResId);

            ViewStub composeplateViewStub = findViewById(R.id.composeplate_view_stub);
            ViewGroup composeplateView = (ViewGroup) composeplateViewStub.inflate();
            mComposeplateCoordinator =
                    new ComposeplateCoordinator(
                            composeplateView, mProfile, colorStateList, textStyleResId);

            assert mVoiceSearchButtonClickListener != null && mLensButtonClickListener != null;
            mComposeplateCoordinator.setVoiceSearchClickListener(mVoiceSearchButtonClickListener);
            mComposeplateCoordinator.setLensClickListener(mLensButtonClickListener);
            mComposeplateCoordinator.setIncognitoClickListener(this::onIncognitoButtonClicked);
            return;
        }

        ViewStub composeplateViewStub = findViewById(R.id.composeplate_view_v2_stub);
        ViewGroup composeplateView = (ViewGroup) composeplateViewStub.inflate();
        mComposeplateCoordinator =
                new ComposeplateCoordinator(
                        composeplateView, mProfile, colorStateList, textStyleResId);
        mComposeplateCoordinator.setIncognitoClickListener(this::onIncognitoButtonClicked);
        // Don't log click metrics in this listener, since the mComposeplateCoordinator will
        // log.
        mComposeplateButtonClickListener = this::onComposeplateButtonClicked;
        mComposeplateCoordinator.setComposeplateButtonClickListener(
                mComposeplateButtonClickListener);
    }

    private void onComposeplateButtonClicked(View view) {
        if (OmniboxFeatures.sOmniboxMultimodalInput.isEnabled()
                && !mIsTablet
                && mIsComposeplatePolicyEnabled) {
            mManager.focusSearchBox(false, AutocompleteRequestType.AI_MODE, null);
            return;
        }

        if (mComposeplateUrlSupplier == null) return;

        GURL composeplateUrl = mComposeplateUrlSupplier.get();
        if (composeplateUrl == null) return;

        mManager.getNativePageHost()
                .loadUrl(new LoadUrlParams(composeplateUrl), /* incognito= */ false);
    }

    private void onIncognitoButtonClicked(View view) {
        if (!IncognitoUtils.isIncognitoModeEnabled(mProfile)) return;

        mManager.getNativePageHost().loadUrl(new LoadUrlParams(UrlConstants.NTP_URL), true);
    }

    private void initializeLayoutChangeListener() {
        TraceEvent.begin(TAG + ".initializeLayoutChangeListener()");
        mOnLayoutChangeListener =
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
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
                };
        addOnLayoutChangeListener(mOnLayoutChangeListener);
        TraceEvent.end(TAG + ".initializeLayoutChangeListener()");
    }

    private void initializeLogoCoordinator(
            boolean searchProviderHasLogo, boolean searchProviderIsGoogle) {
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
                        });

        mLogoView = findViewById(R.id.search_provider_logo);

        mLogoCoordinator =
                new LogoCoordinator(
                        mContext,
                        logoClickedCallback,
                        mLogoView,
                        mOnLogoAvailableCallback,
                        /* visibilityObserver= */ null);
        mLogoCoordinator.setDoodleSize(
                mIsInMultiWindowModeOnTablet ? DoodleSize.TABLET_SPLIT_SCREEN : DoodleSize.REGULAR);
        mLogoCoordinator.initWithNative(mProfile);
        setSearchProviderInfo(searchProviderHasLogo, searchProviderIsGoogle);
    }

    private void initializeMostVisitedTilesCoordinator(
            Profile profile,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            TileGroup.Delegate tileGroupDelegate,
            TouchEnabledDelegate touchEnabledDelegate) {
        assert mMvTilesContainerLayout != null;

        mMostVisitedTilesCoordinator =
                new MostVisitedTilesCoordinator(
                        mActivity,
                        activityLifecycleDispatcher,
                        mMvTilesContainerLayout,
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
        ViewStub signinPromoViewContainerStub = findViewById(R.id.signin_promo_view_container_stub);
        mSigninPromoCoordinator =
                new NtpSigninPromoCoordinator(
                        mContext,
                        mProfile,
                        SigninAndHistorySyncActivityLauncherImpl.get(),
                        signinPromoViewContainerStub);
    }

    /** Updates the search box when the parent view's scroll position is changed. */
    void updateSearchBoxOnScroll() {
        if (mDisableUrlFocusChangeAnimations || mIsViewMoving) return;

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
        // During startup the view may not be fully initialized.
        if (!mScrollDelegate.isScrollViewInitialized()) return 0f;

        if (isSearchBoxOffscreen()) {
            // getVerticalScrollOffset is valid only for the scroll view if the first item is
            // visible. If the search box view is offscreen, we must have scrolled quite far and we
            // know the toolbar transition should be 100%. This might be the initial scroll position
            // due to the scroll restore feature, so the search box will not have been laid out yet.
            return 1f;
        }

        // During startup the view may not be fully initialized, so we only calculate the current
        // percentage if some basic view properties (position of the search box) are sane.
        int searchBoxTop = getSearchBoxView().getTop();
        if (searchBoxTop == 0) return 0f;

        // For all other calculations, add the search box padding, because it defines where the
        // visible "border" of the search box is.
        searchBoxTop += getSearchBoxView().getPaddingTop();

        final int scrollY = mScrollDelegate.getVerticalScrollOffset();
        // Use int pixel size instead of float dimension to avoid precision error on the percentage.
        final float transitionLength =
                mCurrentNtpFakeSearchBoxTransitionStartOffset + mTransitionEndOffset;
        // Tab strip height is zero on phones, and may vary on tablets.
        int tabStripHeight = mTabStripHeightSupplier.get();

        // When scrollY equals searchBoxTop + tabStripHeight -transitionStartOffset, it marks the
        // start point of the transition. When scrollY equals searchBoxTop plus transitionEndOffset
        // plus tabStripHeight, it marks the end point of the transition.
        return MathUtils.clamp(
                (scrollY
                                - (searchBoxTop + mTransitionEndOffset)
                                + tabStripHeight
                                + transitionLength)
                        / transitionLength,
                0f,
                1f);
    }

    private void initializeSiteSectionView() {
        mMvTilesContainerLayout =
                (ViewGroup) ((ViewStub) findViewById(R.id.mv_tiles_layout_stub)).inflate();
        mMvTilesContainerLayout.setVisibility(View.VISIBLE);
        // The page contents are initially hidden; otherwise they'll be drawn centered on the
        // page before the tiles are available and then jump upwards to make space once the
        // tiles are available.
        if (getVisibility() != View.VISIBLE) setVisibility(View.VISIBLE);
    }

    /**
     * @return The fake search box view.
     */
    public View getSearchBoxView() {
        return mSearchBoxCoordinator.getView();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mIsTablet) {
            calculateTabletMvtWidth(MeasureSpec.getSize(widthMeasureSpec));
        }

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        unifyElementWidths();
    }

    /** Updates the width of the MV tiles container when used in NTP on the tablet. */
    private void calculateTabletMvtWidth(int totalWidth) {
        if (mMvTilesContainerLayout.getVisibility() == GONE) return;

        MostVisitedTilesLayout mvTilesLayout = findViewById(R.id.mv_tiles_layout);
        mMvtContentFits = mvTilesLayout.contentFitsOnTablet(totalWidth);
        updateMvtOnTablet();
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
    public void setSearchProviderInfo(boolean hasLogo, boolean isGoogle) {
        if (hasLogo == mSearchProviderHasLogo
                && isGoogle == mSearchProviderIsGoogle
                && mInitialized) {
            return;
        }
        mSearchProviderHasLogo = hasLogo;
        mSearchProviderIsGoogle = isGoogle;

        if (!mSearchProviderIsGoogle) {
            mShowingNonStandardGoogleLogo = false;
        }

        setSearchProviderTopMargin();
        setSearchProviderBottomMargin();

        updateTilesLayoutMargins();

        // Hide or show the views above the most visited tiles as needed, including search box, and
        // spacers. The visibility of Logo is handled by LogoCoordinator.
        mSearchBoxCoordinator.setVisibility(isInSingleUrlMode());
        if (mDseIconView != null) {
            setDseIconViewVisibility(shouldShowDseIcon());
        }
        if (mIsComposeplateEnabled) {
            updateActionButtonVisibility();
        }

        onUrlFocusAnimationChanged();

        mSnapshotTileGridChanged = true;
    }

    /** Updates the margins for the most visited tiles layout based on what is shown above it. */
    private void updateTilesLayoutMargins() {
        if (!mIsTablet) {
            return;
        }

        MarginLayoutParams marginLayoutParams =
                (MarginLayoutParams) mMvTilesContainerLayout.getLayoutParams();
        marginLayoutParams.topMargin =
                getResources()
                        .getDimensionPixelSize(
                                shouldShowLogo()
                                        ? R.dimen.mvt_container_top_margin
                                        : R.dimen.tile_layout_no_logo_top_margin);
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
     * Specifies the percentage the URL is focused during an animation.  1.0 specifies that the URL
     * bar has focus and has completed the focus animation.  0 is when the URL bar is does not have
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
        if (mDisableUrlFocusChangeAnimations || mIsViewMoving || mIsTablet) return;

        // Translate so that the search box is at the top, but only upwards.
        float percent = isInSingleUrlMode() ? mUrlFocusChangePercent : 0;
        int basePosition = mScrollDelegate.getVerticalScrollOffset() + getPaddingTop();
        int target =
                Math.max(
                        basePosition,
                        getSearchBoxView().getBottom()
                                - getSearchBoxView().getPaddingBottom()
                                - mSearchBoxBoundsVerticalInset);

        float translationY = percent * (basePosition - target);
        if (OmniboxFeatures.shouldAnimateSuggestionsListAppearance()) {
            setTranslationYOfFakeboxAndAbove(translationY);
        } else {
            setTranslationY(translationY);
        }
    }

    /**
     * Sets the translation_y of the fakebox and all views above it, but not the views below. Used
     * when the url focus animation is combined with the omnibox suggestions list animation to
     * reduce the number of visual elements in motion.
     */
    private void setTranslationYOfFakeboxAndAbove(float translationY) {
        for (int i = 0; i < getChildCount(); i++) {
            View view = getChildAt(i);
            view.setTranslationY(translationY);
            if (view.getId() == R.id.search_box) return;
        }
    }

    /**
     * Sets whether this view is currently moving within its parent view. When the view is moving
     * certain animations will be disabled or prevented.
     *
     * @param isViewMoving Whether this view is currently moving.
     */
    void setIsViewMoving(boolean isViewMoving) {
        mIsViewMoving = isViewMoving;
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
        int searchBoxX = (int) getSearchBoxView().getX();
        int searchBoxY = (int) getSearchBoxView().getY();
        bounds.set(
                searchBoxX,
                searchBoxY,
                searchBoxX + getSearchBoxView().getWidth(),
                searchBoxY + getSearchBoxView().getHeight());

        translation.set(0, 0);

        if (isSearchBoxOffscreen()) {
            translation.y = Integer.MIN_VALUE;
        } else {
            View view = getSearchBoxView();
            while (true) {
                view = (View) view.getParent();
                if (view == null) {
                    // The |mSearchBoxView| is not a child of this view. This can happen if the
                    // RecyclerView detaches the NewTabPageLayout after it has been scrolled out of
                    // view. Set the translation to the minimum Y value as an approximation.
                    translation.y = Integer.MIN_VALUE;
                    break;
                }
                translation.offset(-view.getScrollX(), -view.getScrollY());
                if (view == parentView) break;
                translation.offset((int) view.getX(), (int) view.getY());
            }
        }

        bounds.offset(translation.x, translation.y);
        if (translation.y != Integer.MIN_VALUE) {
            bounds.inset(0, mSearchBoxBoundsVerticalInset);
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
        boolean showFakeSearchBoxWithoutLogo =
                !mSearchProviderHasLogo && mIsOmniboxMobileParityUpdateV2Enabled;
        mCurrentNtpFakeSearchBoxTransitionStartOffset =
                getNtpSearchBoxTransitionStartOffset(showFakeSearchBoxWithoutLogo);

        int topMargin = showFakeSearchBoxWithoutLogo ? mNtpSearchBoxTopMarginWithoutLogo : 0;
        mSearchBoxCoordinator.setTopMargin(topMargin);

        if (mLogoCoordinator != null) {
            mLogoCoordinator.setTopMargin(getLogoMargin(/* isTopMargin= */ true));
        }
    }

    private void setSearchProviderBottomMargin() {
        if (mLogoCoordinator == null) return;
        mLogoCoordinator.setBottomMargin(getLogoMargin(/* isTopMargin= */ false));
    }

    /**
     * @param isTopMargin True to return the top margin; False to return bottom margin.
     * @return The top margin or bottom margin of the logo.
     */
    // TODO(crbug.com/40226731): Remove this method when the Feed position experiment is
    // cleaned up.
    private int getLogoMargin(boolean isTopMargin) {
        return isTopMargin ? getLogoTopMargin() : getLogoBottomMargin();
    }

    private int getLogoTopMargin() {
        Resources resources = getResources();

        if (mShowingNonStandardGoogleLogo && mSearchProviderHasLogo) {
            return LogoUtils.getTopMarginForDoodle(resources);
        }

        return resources.getDimensionPixelSize(R.dimen.ntp_logo_margin_top);
    }

    private int getLogoBottomMargin() {
        return getResources().getDimensionPixelSize(R.dimen.ntp_logo_margin_bottom);
    }

    /**
     * @return Whether the search box view is scrolled off the screen.
     */
    private boolean isSearchBoxOffscreen() {
        if (!mScrollDelegate.isScrollViewInitialized()) return false;

        return !mScrollDelegate.isChildVisibleAtPosition(0)
                || mScrollDelegate.getVerticalScrollOffset()
                        > getSearchBoxView().getTop() + mTransitionEndOffset;
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

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        assert mManager != null;

        if (!mHasShownView) {
            mHasShownView = true;
            onInitializationProgressChanged();
            TraceEvent.instant("NewTabPageSearchAvailable");
        }
    }

    /** Update the visibility of the action buttons. */
    void updateActionButtonVisibility() {
        boolean shouldShowVoiceSearchButton = mManager.isVoiceSearchEnabled();
        boolean shouldShowLensButton =
                mSearchBoxCoordinator.isLensEnabled(LensEntryPoint.NEW_TAB_PAGE);
        if (!mIsComposeplateEnabled || mIsComposeplateV2Enabled) {
            mSearchBoxCoordinator.setVoiceSearchButtonVisibility(shouldShowVoiceSearchButton);
            mSearchBoxCoordinator.setLensButtonVisibility(shouldShowLensButton);
            boolean shouldShowComposeplateButton = false;
            if (mIsComposeplateV2Enabled) {
                shouldShowComposeplateButton =
                        mSearchProviderIsGoogle && IncognitoUtils.isIncognitoModeEnabled(mProfile);
                if (mComposeplateCoordinator != null) {
                    mComposeplateCoordinator.setVisibility(
                            shouldShowComposeplateButton, mManager.isCurrentPage());
                }
            }
            updatePreviousButtonVisibilityAndRecordMetrics(
                    shouldShowVoiceSearchButton,
                    shouldShowLensButton,
                    shouldShowComposeplateButton);
            return;
        }

        boolean shouldShowComposeplateButton =
                mSearchProviderIsGoogle && shouldShowVoiceSearchButton && shouldShowLensButton;
        boolean isVoiceSearchButtonVisible =
                !shouldShowComposeplateButton && shouldShowVoiceSearchButton;
        boolean isLensButtonVisible = !shouldShowComposeplateButton && shouldShowLensButton;
        mSearchBoxCoordinator.setVoiceSearchButtonVisibility(isVoiceSearchButtonVisible);
        mSearchBoxCoordinator.setLensButtonVisibility(isLensButtonVisible);
        mSearchBoxCoordinator.setComposeplateButtonVisibility(shouldShowComposeplateButton);
        if (mComposeplateCoordinator != null) {
            mComposeplateCoordinator.setVisibilityV1(
                    shouldShowComposeplateButton, mManager.isCurrentPage());
        }

        updatePreviousButtonVisibilityAndRecordMetrics(
                isVoiceSearchButtonVisible, isLensButtonVisible, shouldShowComposeplateButton);
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
                || (mSearchBoxCoordinator.getView().getVisibility() != View.VISIBLE)
                || (mPreviousVoiceSearchButtonVisible != null
                        && isVoiceSearchButtonVisible == mPreviousVoiceSearchButtonVisible
                        && mPreviousLensButtonVisible != null
                        && isLensButtonVisible == mPreviousLensButtonVisible)) {
            return;
        }

        if (mPreviousLensButtonVisible == null
                || isLensButtonVisible != mPreviousLensButtonVisible) {
            // The lens button will be shown either in the fake search box or the composeplate view.
            LensMetrics.recordShown(
                    LensEntryPoint.NEW_TAB_PAGE,
                    isLensButtonVisible || isComposeplateButtonVisible);
        }

        ComposeplateMetricsUtils.recordFakeSearchBoxImpression2();
        ComposeplateMetricsUtils.recordFakeSearchBoxComposeplateButtonImpression2(
                isComposeplateButtonVisible);

        mPreviousVoiceSearchButtonVisible = isVoiceSearchButtonVisible;
        mPreviousLensButtonVisible = isLensButtonVisible;
    }

    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);

        if (visibility == VISIBLE) {
            updateActionButtonVisibility();
        }
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
        SearchEngineUtils.getForProfile(mProfile).removeSearchBoxHintTextObserver(this);

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
            mSearchEngineUtils.removeSearchBoxHintTextObserver(this);
            mSearchEngineUtils.removeIconObserver(this);
            mSearchEngineUtils = null;
        }

        removeOnLayoutChangeListener(mOnLayoutChangeListener);
        mOnLayoutChangeListener = null;

        if (mComposeplateCoordinator != null) {
            mComposeplateCoordinator.destroy();
            mComposeplateCoordinator = null;
        }

        if (mSigninPromoCoordinator != null) {
            mSigninPromoCoordinator.destroy();
            mSigninPromoCoordinator = null;
        }

        mComposeplateButtonClickListener = null;
        mLensButtonClickListener = null;
        mVoiceSearchButtonClickListener = null;
        mSearchBoxScrollListener = null;
        mComposeplateUrlSupplier = null;
    }

    MostVisitedTilesCoordinator getMostVisitedTilesCoordinatorForTesting() {
        return mMostVisitedTilesCoordinator;
    }

    /** Makes the Search Box and Logo as wide as Most Visited. */
    private void unifyElementWidths() {
        View searchBoxView = getSearchBoxView();
        final int width = getMeasuredWidth();
        int searchBoxWidth = width - mSearchBoxTwoSideMargin;
        measureExactly(searchBoxView, searchBoxWidth, searchBoxView.getMeasuredHeight());

        if (mLogoCoordinator != null) mLogoCoordinator.measureExactlyLogoView(width);

        if (mComposeplateCoordinator != null) {
            mComposeplateCoordinator.measureExactlyComposeplateView(searchBoxWidth);
        }
    }

    /**
     * Convenience method to call measure() on the given View with MeasureSpecs converted from the
     * given dimensions (in pixels) with MeasureSpec.EXACTLY.
     */
    private static void measureExactly(View view, int widthPx, int heightPx) {
        view.measure(
                MeasureSpec.makeMeasureSpec(widthPx, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(heightPx, MeasureSpec.EXACTLY));
    }

    LogoCoordinator getLogoCoordinatorForTesting() {
        return mLogoCoordinator;
    }

    private void onDisplayStyleChanged(UiConfig.DisplayStyle newDisplayStyle) {
        if (!mIsTablet) return;

        mIsInNarrowWindowOnTablet = isInNarrowWindowOnTablet(mIsTablet, mUiConfig);

        updateDoodleOnTablet();
        updateMvtOnTablet();
        updateSearchBoxWidth();
    }

    /**
     * Adjusts the doodle size while the tablet transitions to or from a multi-screen layout,
     * ensuring the change occurs post-logo initialization.
     */
    private void updateDoodleOnTablet() {
        if (!mIsTablet) return;

        boolean isInMultiWindowModeOnTabletPreviousValue = mIsInMultiWindowModeOnTablet;
        mIsInMultiWindowModeOnTablet =
                MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity);

        if (mLogoView != null
                && isInMultiWindowModeOnTabletPreviousValue != mIsInMultiWindowModeOnTablet) {
            int doodleSize =
                    mIsInMultiWindowModeOnTablet
                            ? DoodleSize.TABLET_SPLIT_SCREEN
                            : DoodleSize.REGULAR;
            if (mLogoCoordinator != null) mLogoCoordinator.setDoodleSize(doodleSize);

            if (mShowingNonStandardGoogleLogo) {
                LogoUtils.setLogoViewLayoutParamsForDoodle(mLogoView, getResources(), doodleSize);
            }
        }
    }

    /**
     * Updates whether the MV tiles layout stays in the center of the container when used in NTP on
     * the tablet by changing the width of its container. Also updates the lateral margins.
     */
    private void updateMvtOnTablet() {
        MarginLayoutParams marginLayoutParams =
                (MarginLayoutParams) mMvTilesContainerLayout.getLayoutParams();
        marginLayoutParams.width =
                mMvtContentFits
                        ? ViewGroup.LayoutParams.WRAP_CONTENT
                        : ViewGroup.LayoutParams.MATCH_PARENT;

        int lateralPaddingId =
                mIsInNarrowWindowOnTablet
                        ? R.dimen.ntp_search_box_lateral_margin_narrow_window_tablet
                        : R.dimen.mvt_container_lateral_margin;
        int lateralPaddingsForNtp = getResources().getDimensionPixelSize(lateralPaddingId);
        marginLayoutParams.leftMargin = lateralPaddingsForNtp;
        marginLayoutParams.rightMargin = lateralPaddingsForNtp;
    }

    private void updateSearchBoxWidth() {
        if (mIsInNarrowWindowOnTablet) {
            mSearchBoxTwoSideMargin =
                    getResources()
                                    .getDimensionPixelSize(
                                            R.dimen
                                                    .ntp_search_box_lateral_margin_narrow_window_tablet)
                            * 2;
        } else if (mIsTablet) {
            mSearchBoxTwoSideMargin =
                    getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.ntp_search_box_lateral_margin_tablet)
                            * 2;
        } else {
            mSearchBoxTwoSideMargin =
                    getResources().getDimensionPixelSize(R.dimen.mvt_container_lateral_margin) * 2;
        }
    }

    /** Returns whether the current window is a narrow one on tablet. */
    @VisibleForTesting
    public static boolean isInNarrowWindowOnTablet(boolean isTablet, UiConfig uiConfig) {
        return isTablet
                && uiConfig.getCurrentDisplayStyle().horizontal < HorizontalDisplayStyle.WIDE;
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
                getNtpSearchBoxTransitionStartOffset(
                                !mSearchProviderHasLogo && mIsOmniboxMobileParityUpdateV2Enabled)
                        + mTopInset;

        // Top padding is applied to the NTP layout, ensuring all UI components remain in their
        // original positions after Status bar is hidden.
        setPaddingRelative(
                getPaddingStart(),
                getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow) + mTopInset,
                getPaddingEnd(),
                getPaddingBottom());
    }

    /**
     * Called when a customized background image is selected or deselected.
     *
     * @param applyWhiteBackgroundOnSearchBox Whether to apply a white background color to the fake
     *     search box.
     */
    void onCustomizedBackgroundChanged(boolean applyWhiteBackgroundOnSearchBox) {
        // applyWhiteBackgroundWithShadow() will be called immediately after mSearchBoxCoordinator
        // is initialized, it is fine to skip here.
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

    private boolean isInSingleUrlMode() {
        return mSearchProviderHasLogo || mIsOmniboxMobileParityUpdateV2Enabled;
    }

    private boolean shouldShowDseIcon() {
        return mSearchProviderIsGoogle || mIsOmniboxMobileParityUpdateV2Enabled;
    }
}
