// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.android.material.appbar.AppBarLayout;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.feed.FeedReliabilityLogger;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.ntp.IncognitoCookieControlsManager;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.OriginalProfileSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.query_tiles.QueryTileSection;
import org.chromium.chrome.browser.query_tiles.QueryTileUtils;
import org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesCoordinator;
import org.chromium.chrome.browser.suggestions.tile.TileGroupDelegateImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate.TabSwitcherType;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegateProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherCustomViewManager;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.features.start_surface.MostVisitedSuggestionsUiDelegate;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Coordinator for displaying task-related surfaces (Tab Switcher, MV Tiles, Omnibox, etc.).
 *  Concrete implementation of {@link TasksSurface}.
 */
public class TasksSurfaceCoordinator implements TasksSurface {
    private static final int MAX_TILE_ROWS_FOR_GRID_MVT = 2;

    private final TabSwitcher mTabSwitcher;
    private final TasksView mView;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private final TasksSurfaceMediator mMediator;
    private final PropertyModel mPropertyModel;
    private final @TabSwitcherType int mTabSwitcherType;
    private final SnackbarManager mSnackbarManager;
    private final Supplier<DynamicResourceLoader> mDynamicResourceLoaderSupplier;
    private final TabContentManager mTabContentManager;
    private final ModalDialogManager mModalDialogManager;
    private final Activity mActivity;
    private final Supplier<Tab> mParentTabSupplier;

    private MostVisitedTilesCoordinator mMostVisitedCoordinator;
    private MostVisitedSuggestionsUiDelegate mSuggestionsUiDelegate;
    private TileGroupDelegateImpl mTileGroupDelegate;
    private OneshotSupplier<Profile> mQueryTileProfileSupplier;
    private QueryTileSection mQueryTileSection;

    /**
     * This flag should be reset once {@link MostVisitedTilesCoordinator#destroyMvtiles} is called.
     */
    private boolean mIsMVTilesInitialized;

    /** {@see TabManagementDelegate#createTasksSurface} */
    public TasksSurfaceCoordinator(@NonNull Activity activity,
            @NonNull ScrimCoordinator scrimCoordinator, @NonNull PropertyModel propertyModel,
            @TabSwitcherType int tabSwitcherType, @NonNull Supplier<Tab> parentTabSupplier,
            boolean hasMVTiles, boolean hasQueryTiles, @NonNull WindowAndroid windowAndroid,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull TabModelSelector tabModelSelector, @NonNull SnackbarManager snackbarManager,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull TabContentManager tabContentManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ViewGroup rootView,
            @Nullable OneshotSupplier<IncognitoReauthController>
                    incognitoReauthControllerSupplier) {
        mActivity = activity;
        mView = (TasksView) LayoutInflater.from(activity).inflate(R.layout.tasks_view_layout, null);
        mView.initialize(activityLifecycleDispatcher,
                parentTabSupplier.hasValue() && parentTabSupplier.get().isIncognito(),
                windowAndroid);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(propertyModel, mView, TasksViewBinder::bind);
        mPropertyModel = propertyModel;
        mTabSwitcherType = tabSwitcherType;
        mSnackbarManager = snackbarManager;
        mDynamicResourceLoaderSupplier = dynamicResourceLoaderSupplier;
        mTabContentManager = tabContentManager;
        mModalDialogManager = modalDialogManager;
        mParentTabSupplier = parentTabSupplier;
        if (tabSwitcherType == TabSwitcherType.CAROUSEL) {
            mTabSwitcher = TabManagementDelegateProvider.getDelegate().createCarouselTabSwitcher(
                    activity, activityLifecycleDispatcher, tabModelSelector, tabContentManager,
                    browserControlsStateProvider, tabCreatorManager, menuOrKeyboardActionController,
                    mView.getCarouselTabSwitcherContainer(), multiWindowModeStateDispatcher,
                    scrimCoordinator, rootView, dynamicResourceLoaderSupplier, snackbarManager,
                    modalDialogManager);
        } else if (tabSwitcherType == TabSwitcherType.GRID) {
            assert incognitoReauthControllerSupplier
                    != null : "Valid Incognito re-auth controller supplier needed to create GTS.";
            mTabSwitcher = TabManagementDelegateProvider.getDelegate().createGridTabSwitcher(
                    activity, activityLifecycleDispatcher, tabModelSelector, tabContentManager,
                    browserControlsStateProvider, tabCreatorManager, menuOrKeyboardActionController,
                    mView.getBodyViewContainer(), multiWindowModeStateDispatcher, scrimCoordinator,
                    rootView, dynamicResourceLoaderSupplier, snackbarManager, modalDialogManager,
                    incognitoReauthControllerSupplier, null /*BackPressManager*/);
        } else if (tabSwitcherType == TabSwitcherType.SINGLE) {
            mTabSwitcher = new SingleTabSwitcherCoordinator(activity,
                    mView.getCarouselTabSwitcherContainer(), null, tabModelSelector,
                    /* isTablet= */ false, /* isScrollableMvtEnabled */ true,
                    /* mostRecentTab= */ null, /* singleTabCardClickedCallback */ null,
                    /* snapshotParentViewRunnable */ null);
        } else if (tabSwitcherType == TabSwitcherType.NONE) {
            mTabSwitcher = null;
        } else {
            mTabSwitcher = null;
            assert false : "Unsupported tab switcher type";
        }

        View.OnClickListener incognitoLearnMoreClickListener = v -> {
            Profile profile = Profile.getLastUsedRegularProfile().getPrimaryOTRProfile(
                    /*createIfNeeded=*/true);
            HelpAndFeedbackLauncherImpl.getForProfile(profile).show(
                    activity, activity.getString(R.string.help_context_incognito_learn_more), null);
        };
        IncognitoCookieControlsManager incognitoCookieControlsManager =
                new IncognitoCookieControlsManager();
        mMediator = new TasksSurfaceMediator(propertyModel, incognitoLearnMoreClickListener,
                incognitoCookieControlsManager, tabSwitcherType == TabSwitcherType.CAROUSEL);

        if (hasMVTiles) {
            boolean isScrollableMVTEnabled =
                    !ReturnToChromeUtil.shouldImproveStartWhenFeedIsDisabled(mActivity);
            int maxRowsForGridMVT = getQueryTilesVisibility()
                    ? QueryTileSection.getMaxRowsForMostVisitedTiles(activity)
                    : MAX_TILE_ROWS_FOR_GRID_MVT;
            View mvTilesContainer = mView.findViewById(R.id.mv_tiles_container);
            mMostVisitedCoordinator = new MostVisitedTilesCoordinator(activity,
                    activityLifecycleDispatcher, mvTilesContainer, windowAndroid,
                    TabUiFeatureUtilities.supportInstantStart(
                            DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity),
                            mActivity),
                    isScrollableMVTEnabled,
                    isScrollableMVTEnabled ? Integer.MAX_VALUE : maxRowsForGridMVT,
                    /*snapshotTileGridChangedRunnable=*/null,
                    /*tileCountChangedRunnable=*/null);
        }

        if (hasQueryTiles) {
            if (ProfileManager.isInitialized()) {
                initializeQueryTileSection(Profile.getLastUsedRegularProfile());
            } else {
                mQueryTileProfileSupplier = new OriginalProfileSupplier();
                mQueryTileProfileSupplier.onAvailable(this::initializeQueryTileSection);
            }
        } else {
            storeQueryTilesVisibility(false);
        }
    }

    private void initializeQueryTileSection(Profile profile) {
        assert profile != null;
        if (!QueryTileUtils.isQueryTilesEnabledOnStartSurface()) {
            storeQueryTilesVisibility(false);
            return;
        }
        mQueryTileSection =
                new QueryTileSection(mView.findViewById(R.id.query_tiles_layout), profile,
                        query -> mMediator.performSearchQuery(query.queryText, query.searchParams));
        storeQueryTilesVisibility(true);
        mQueryTileProfileSupplier = null;
    }

    /**
     * TasksSurface implementation.
     */
    @Override
    public void initialize() {
        assert LibraryLoader.getInstance().isInitialized();
        mMediator.initialize();
    }

    @Override
    public void initializeMVTiles() {
        if (!LibraryLoader.getInstance().isInitialized() || mIsMVTilesInitialized
                || mMostVisitedCoordinator == null) {
            return;
        }

        Profile profile = Profile.getLastUsedRegularProfile();
        MostVisitedTileNavigationDelegate navigationDelegate =
                new MostVisitedTileNavigationDelegate(mActivity, profile, mParentTabSupplier);
        mSuggestionsUiDelegate = new MostVisitedSuggestionsUiDelegate(
                mView, navigationDelegate, profile, mSnackbarManager);
        mTileGroupDelegate = new TileGroupDelegateImpl(mActivity, profile, navigationDelegate,
                mSnackbarManager, BrowserUiUtils.HostSurface.START_SURFACE);

        mMostVisitedCoordinator.initWithNative(
                mSuggestionsUiDelegate, mTileGroupDelegate, enabled -> {});
        mIsMVTilesInitialized = true;
    }

    @Override
    public void setOnTabSelectingListener(TabSwitcher.OnTabSelectingListener listener) {
        if (mTabSwitcher != null) {
            mTabSwitcher.setOnTabSelectingListener(listener);
        }
    }

    @Override
    public @Nullable TabSwitcher.Controller getController() {
        return mTabSwitcher != null ? mTabSwitcher.getController() : null;
    }

    @Override
    public @Nullable TabSwitcher.TabListDelegate getTabListDelegate() {
        return mTabSwitcher != null ? mTabSwitcher.getTabListDelegate() : null;
    }

    @Override
    public Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        if (mTabSwitcherType != TabSwitcherType.CAROUSEL
                && mTabSwitcherType != TabSwitcherType.GRID) {
            return null;
        }
        assert mTabSwitcher != null;
        return mTabSwitcher.getTabGridDialogVisibilitySupplier();
    }

    @Override
    public ViewGroup getBodyViewContainer() {
        return mView.getBodyViewContainer();
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public void onFinishNativeInitialization(Context context, OmniboxStub omniboxStub,
            @Nullable FeedReliabilityLogger feedReliabilityLogger) {
        if (mTabSwitcher != null) mTabSwitcher.initWithNative();

        mMediator.initWithNative(omniboxStub, feedReliabilityLogger);
    }

    @Override
    public void addHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener) {
        mView.addHeaderOffsetChangeListener(onOffsetChangedListener);
    }

    @Override
    public void removeHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener) {
        mView.removeHeaderOffsetChangeListener(onOffsetChangedListener);
    }

    @Override
    public void updateFakeSearchBox(int height, int topMargin, int endPadding, float translationX,
            int buttonSize, int lensButtonLeftMargin) {
        mView.updateFakeSearchBox(
                height, topMargin, endPadding, translationX, buttonSize, lensButtonLeftMargin);
    }

    @Override
    public void onHide() {
        if (mSuggestionsUiDelegate != null) {
            mSuggestionsUiDelegate.onDestroy();
            mSuggestionsUiDelegate = null;
        }
        if (mTileGroupDelegate != null) {
            mTileGroupDelegate.destroy();
            mTileGroupDelegate = null;
        }

        if (mMostVisitedCoordinator != null) {
            mMostVisitedCoordinator.destroyMvtiles();
            mIsMVTilesInitialized = false;
        }

        mTabSwitcher.getTabListDelegate().postHiding();
    }

    @VisibleForTesting
    @Override
    public boolean isMVTilesCleanedUp() {
        assert mMostVisitedCoordinator != null;
        return mMostVisitedCoordinator.isMVTilesCleanedUp();
    }

    @VisibleForTesting
    @Override
    public boolean isMVTilesInitialized() {
        return mIsMVTilesInitialized;
    }

    @VisibleForTesting
    @Override
    public TileGroupDelegateImpl getTileGroupDelegate() {
        return mTileGroupDelegate;
    }

    @Override
    public @Nullable TabSwitcherCustomViewManager getTabSwitcherCustomViewManager() {
        return (mTabSwitcher != null) ? mTabSwitcher.getTabSwitcherCustomViewManager() : null;
    }

    private void storeQueryTilesVisibility(boolean isShown) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.QUERY_TILES_SHOWN_ON_START_SURFACE, isShown);
    }

    private boolean getQueryTilesVisibility() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.QUERY_TILES_SHOWN_ON_START_SURFACE, false);
    }
}
