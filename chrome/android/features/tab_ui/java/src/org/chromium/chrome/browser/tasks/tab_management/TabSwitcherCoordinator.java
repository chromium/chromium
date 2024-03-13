// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Handler;
import android.os.SystemClock;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionsOrchestrator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator.SystemUiScrimDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.List;

/**
 * Parent coordinator that is responsible for showing a grid or carousel of tabs for the main
 * TabSwitcher UI.
 */
public class TabSwitcherCoordinator
        implements DestroyObserver,
                TabSwitcher,
                TabSwitcher.TabListDelegate,
                TabSwitcherResetHandler,
                TabGridItemTouchHelperCallback.OnLongPressTabItemEventListener {
    // TODO(crbug.com/982018): Rename 'COMPONENT_NAME' so as to add different metrics for carousel
    // tab switcher.
    static final String COMPONENT_NAME = "GridTabSwitcher";
    private final Activity mActivity;
    private final PropertyModelChangeProcessor mContainerViewChangeProcessor;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private final TabListCoordinator mTabListCoordinator;
    private final TabSwitcherMediator mMediator;
    private final MultiThumbnailCardProvider mMultiThumbnailCardProvider;
    private final ScrimCoordinator mGridDialogScrimCoordinator;
    @Nullable private TabGridDialogCoordinator mTabGridDialogCoordinator;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final TabModelSelector mTabModelSelector;
    private final @TabListCoordinator.TabListMode int mMode;
    private final Supplier<DynamicResourceLoader> mDynamicResourceLoaderSupplier;
    private final SnackbarManager mSnackbarManager;
    private final ModalDialogManager mModalDialogManager;
    private final TabSwitcherMessageManager mMessageManager;
    private final TabListEditorManager mTabListEditorManager;
    private TabSuggestionsOrchestrator mTabSuggestionsOrchestrator;
    private TabAttributeCache mTabAttributeCache;
    private ViewGroup mContainer;
    private TabCreatorManager mTabCreatorManager;
    private boolean mIsInitialized;
    private SharedPreferences.OnSharedPreferenceChangeListener mPriceAnnotationsPrefListener;
    private final ViewGroup mCoordinatorView;
    private final ViewGroup mRootView;
    private TabContentManager mTabContentManager;
    private final @NonNull BottomSheetController mBottomSheetController;

    /**
     * TODO(crbug.com/1227656): Refactor this to pass a supplier instead to ensure we re-use the
     * same instance of {@link IncognitoReauthManager} across the codebase.
     */
    private IncognitoReauthManager mIncognitoReauthManager;

    private final MenuOrKeyboardActionController.MenuOrKeyboardActionHandler
            mTabSwitcherMenuActionHandler;
    private final TabSwitcherCustomViewManager mTabSwitcherCustomViewManager =
            new TabSwitcherCustomViewManager();

    /** {@see TabManagementDelegate#createCarouselTabSwitcher} */
    // Suppress to observe SharedPreferences, which is discouraged; use another messaging channel
    // instead.
    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
    public TabSwitcherCoordinator(
            @NonNull Activity activity,
            @NonNull ActivityLifecycleDispatcher lifecycleDispatcher,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager,
            @NonNull BrowserControlsStateProvider browserControls,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull ViewGroup container,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ScrimCoordinator scrimCoordinator,
            @TabListMode int mode,
            @NonNull ViewGroup rootView,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull BottomSheetController bottomSheetController,
            @Nullable OneshotSupplier<IncognitoReauthController> incognitoReauthControllerSupplier,
            @Nullable BackPressManager backPressManager,
            @Nullable OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier) {
        try (TraceEvent e = TraceEvent.scoped("TabSwitcherCoordinator.constructor")) {
            mActivity = activity;
            mMode = mode;
            assert mode != TabListMode.STRIP : "Strip mode not supported for TabSwitcher.";
            mBrowserControlsStateProvider = browserControls;
            mTabModelSelector = tabModelSelector;
            mContainer = container;
            mCoordinatorView = activity.findViewById(R.id.coordinator);
            mTabCreatorManager = tabCreatorManager;
            mRootView = rootView;
            mTabContentManager = tabContentManager;
            mDynamicResourceLoaderSupplier = dynamicResourceLoaderSupplier;
            mSnackbarManager = snackbarManager;
            mModalDialogManager = modalDialogManager;
            mBottomSheetController = bottomSheetController;

            PropertyModel containerViewModel =
                    new PropertyModel.Builder(TabListContainerProperties.ALL_KEYS)
                            .with(
                                    TabListContainerProperties.BROWSER_CONTROLS_STATE_PROVIDER,
                                    mBrowserControlsStateProvider)
                            .build();

            mGridDialogScrimCoordinator =
                    DeviceFormFactor.isNonMultiDisplayContextOnTablet(mRootView.getContext())
                            ? createScrimCoordinator()
                            : scrimCoordinator;
            LazyOneshotSupplier<TabGridDialogMediator.DialogController> dialogControllerSupplier =
                    LazyOneshotSupplier.fromSupplier(
                            () -> {
                                initTabGridDialogCoordinator();
                                return mTabGridDialogCoordinator.getDialogController();
                            });
            mMediator =
                    new TabSwitcherMediator(
                            activity,
                            this,
                            containerViewModel,
                            tabModelSelector,
                            browserControls,
                            container,
                            new Handler(),
                            mode,
                            incognitoReauthControllerSupplier,
                            backPressManager,
                            dialogControllerSupplier,
                            this::onTabSwitcherShown,
                            layoutStateProviderSupplier);

            mTabSwitcherCustomViewManager.setDelegate(mMediator);

            var currentTabModelFilterSupplier =
                    tabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilterSupplier();
            mMultiThumbnailCardProvider =
                    new MultiThumbnailCardProvider(
                            activity,
                            mBrowserControlsStateProvider,
                            tabContentManager,
                            currentTabModelFilterSupplier);

            PseudoTab.TitleProvider titleProvider =
                    (context, pseudoTab) -> {
                        TabGroupModelFilter filter =
                                (TabGroupModelFilter)
                                        tabModelSelector
                                                .getTabModelFilterProvider()
                                                .getCurrentTabModelFilterSupplier()
                                                .get();
                        Tab tab = TabModelUtils.getTabById(filter.getTabModel(), pseudoTab.getId());
                        assert tab != null;
                        if (!filter.isTabInTabGroup(tab)) return tab.getTitle();

                        return TabGroupTitleEditor.getDefaultTitle(
                                context, filter.getRelatedTabCountForRootId(tab.getRootId()));
                    };

            long startTimeMs = SystemClock.uptimeMillis();

            int emptyImageResId =
                    DeviceFormFactor.isNonMultiDisplayContextOnTablet(mRootView.getContext())
                            ? R.drawable.tablet_tab_switcher_empty_state_illustration
                            : R.drawable.phone_tab_switcher_empty_state_illustration;
            int emptyHeadingStringResId = R.string.tabswitcher_no_tabs_empty_state;
            int emptySubheadingStringResId =
                    R.string.tabswitcher_no_tabs_open_to_visit_different_pages;
            // Note: getMessageManager is used because () -> mMessageManager warns that the object
            // might not be initialized. This is safe because downstream has null checks and only
            // uses the object once tabs are added to the model which happens after this constructor
            // finishes.
            mTabListCoordinator =
                    new TabListCoordinator(
                            mode,
                            activity,
                            mBrowserControlsStateProvider,
                            currentTabModelFilterSupplier,
                            () -> tabModelSelector.getModel(false),
                            mMultiThumbnailCardProvider,
                            titleProvider,
                            true,
                            mMediator,
                            null,
                            TabProperties.UiType.CLOSABLE,
                            null,
                            this::getMessageManager,
                            container,
                            true,
                            COMPONENT_NAME,
                            mRootView,
                            null,
                            true,
                            emptyImageResId,
                            emptyHeadingStringResId,
                            emptySubheadingStringResId);

            mTabListCoordinator.setOnLongPressTabItemEventListener(this);

            mContainerViewChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            containerViewModel,
                            mTabListCoordinator.getContainerView(),
                            TabListContainerViewBinder::bind);

            RecordHistogram.recordTimesHistogram(
                    "Android.TabSwitcher.SetupRecyclerView.Time",
                    SystemClock.uptimeMillis() - startTimeMs);

            mTabListEditorManager =
                    new TabListEditorManager(
                            activity,
                            mCoordinatorView,
                            rootView,
                            browserControls,
                            currentTabModelFilterSupplier,
                            () -> mTabModelSelector.getModel(false),
                            tabContentManager,
                            mTabListCoordinator,
                            mode);
            mMediator.setTabListEditorControllerSupplier(
                    mTabListEditorManager.getControllerSupplier());

            var tabListEditorControllerSupplier =
                    LazyOneshotSupplier.fromSupplier(
                            () -> {
                                mTabListEditorManager.initTabListEditor();
                                return mTabListEditorManager.getControllerSupplier().get();
                            });
            mMessageManager =
                    new TabSwitcherMessageManager(
                            activity,
                            lifecycleDispatcher,
                            currentTabModelFilterSupplier,
                            container,
                            multiWindowModeStateDispatcher,
                            snackbarManager,
                            modalDialogManager,
                            mTabListCoordinator,
                            tabListEditorControllerSupplier,
                            mMediator,
                            mode);

            mMenuOrKeyboardActionController = menuOrKeyboardActionController;
            mTabSwitcherMenuActionHandler =
                    new MenuOrKeyboardActionController.MenuOrKeyboardActionHandler() {
                        @Override
                        public boolean handleMenuOrKeyboardAction(int id, boolean fromMenu) {
                            if (id == R.id.menu_select_tabs) {
                                mTabListEditorManager.showTabListEditor();
                                RecordUserAction.record("MobileMenuSelectTabs");
                                return true;
                            }
                            return false;
                        }
                    };
            mMenuOrKeyboardActionController.registerMenuOrKeyboardActionHandler(
                    mTabSwitcherMenuActionHandler);

            if (ChromeFeatureList.sInstantStart.isEnabled()) {
                mTabAttributeCache = new TabAttributeCache(mTabModelSelector);
            }

            mLifecycleDispatcher = lifecycleDispatcher;
            mLifecycleDispatcher.register(this);
        }
    }

    // @Todo(crbug.com/1464841) can use VisibilityListener instead of this callback.
    public void onTabSwitcherShown() {
        mTabListCoordinator.attachEmptyView();
    }

    private void initTabGridDialogCoordinator() {
        var currentTabModelFilterSupplier =
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilterSupplier();
        mTabGridDialogCoordinator =
                new TabGridDialogCoordinator(
                        mActivity,
                        mBrowserControlsStateProvider,
                        mBottomSheetController,
                        currentTabModelFilterSupplier,
                        () -> mTabModelSelector.getModel(false),
                        mTabContentManager,
                        mTabCreatorManager,
                        mCoordinatorView,
                        TabSwitcherCoordinator.this,
                        mMediator,
                        TabSwitcherCoordinator.this::getTabGridDialogAnimationSourceView,
                        mGridDialogScrimCoordinator,
                        mTabListCoordinator.getTabGroupTitleEditor(),
                        mRootView);
    }

    private ScrimCoordinator createScrimCoordinator() {
        ViewGroup coordinator = mActivity.findViewById(R.id.coordinator);
        SystemUiScrimDelegate delegate =
                new SystemUiScrimDelegate() {
                    @Override
                    public void setStatusBarScrimFraction(float scrimFraction) {}

                    @Override
                    public void setNavigationBarScrimFraction(float scrimFraction) {}
                };
        return new ScrimCoordinator(
                mActivity,
                delegate,
                coordinator,
                coordinator.getContext().getColor(R.color.omnibox_focused_fading_background_color));
    }

    @Override
    public void initWithNative() {
        if (mIsInitialized) return;
        try (TraceEvent e = TraceEvent.scoped("TabSwitcherCoordinator.initWithNative")) {
            final boolean shouldUseDynamicResource =
                    mMode == TabListCoordinator.TabListMode.GRID
                            && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)
                            && !ChromeFeatureList.sGridTabSwitcherAndroidAnimations.isEnabled();

            Profile profile = mTabModelSelector.getModel(false).getProfile();
            assert profile != null;
            mTabListCoordinator.initWithNative(
                    profile,
                    shouldUseDynamicResource ? mDynamicResourceLoaderSupplier.get() : null);

            mMessageManager.initWithNative(profile);

            mMultiThumbnailCardProvider.initWithNative(profile);
            mMediator.initWithNative(mSnackbarManager);

            if (mMode == TabListCoordinator.TabListMode.GRID
                    && PriceTrackingFeatures.isPriceTrackingEnabled(profile)) {
                mPriceAnnotationsPrefListener =
                        (sharedPrefs, key) -> {
                            if (PriceTrackingUtilities.TRACK_PRICES_ON_TABS.equals(key)
                                    && !mTabModelSelector.isIncognitoSelected()
                                    && mTabModelSelector.isTabStateInitialized()) {
                                resetWithTabList(
                                        mTabModelSelector
                                                .getTabModelFilterProvider()
                                                .getCurrentTabModelFilter(),
                                        false);
                            }
                        };
                ContextUtils.getAppSharedPreferences()
                        .registerOnSharedPreferenceChangeListener(mPriceAnnotationsPrefListener);
            }

            mIsInitialized = true;
        }
    }


    // TabSwitcher implementation.
    @Override
    public void setOnTabSelectingListener(OnTabSelectingListener listener) {
        mMediator.setOnTabSelectingListener(listener);
    }

    @Override
    public Controller getController() {
        return mMediator;
    }

    @Override
    public TabListDelegate getTabListDelegate() {
        return this;
    }

    @Override
    public Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        // mTabGridDialogCoordinator is lazily created when first displaying something in the
        // dialog. Return false until it has shown something.
        return () -> {
            if (mTabGridDialogCoordinator != null) {
                return mTabGridDialogCoordinator.isVisible();
            }
            return false;
        };
    }

    @Override
    public TabSwitcherCustomViewManager getTabSwitcherCustomViewManager() {
        return mTabSwitcherCustomViewManager;
    }

    @Override
    public int getTabSwitcherTabListModelSize() {
        return mTabListCoordinator.getTabListModelSize();
    }

    @Override
    public void setTabSwitcherRecyclerViewPosition(RecyclerViewPosition recyclerViewPosition) {
        mTabListCoordinator.setRecyclerViewPosition(recyclerViewPosition);
    }

    @Override
    public int getTabListTopOffset() {
        return mTabListCoordinator.getTabListTopOffset();
    }

    @Override
    public Rect getRecyclerViewLocation() {
        return mTabListCoordinator.getRecyclerViewLocation();
    }

    @Override
    public int getListModeForTesting() {
        return mMode;
    }

    @Override
    public void requestFocusOnCurrentTab() {
        // TODO(crbug.com/1447564): Ideally, this shouldn't be called directly and instead mediator
        // should listen for |requestFocusOnCurrentTab| signal implicitly and apply changes. This
        // would require refactoring TabSwitcher.TabListDelegate and its implementation.
        mMediator.requestAccessibilityFocusOnCurrentTab();
    }

    @Override
    public boolean prepareTabSwitcherView() {
        boolean quick = mMediator.prepareTabSwitcherView();
        mTabListCoordinator.prepareTabSwitcherView();
        return quick;
    }

    @Override
    public void postHiding() {
        mTabListCoordinator.destroyEmptyView();
        mTabListCoordinator.postHiding();
        mMediator.postHiding();
    }

    @Override
    public @NonNull Rect getThumbnailLocationOfCurrentTab() {
        if (mTabGridDialogCoordinator != null && mTabGridDialogCoordinator.isVisible()) {
            Rect thumbnail = mTabGridDialogCoordinator.getGlobalLocationOfCurrentThumbnail();
            // Adjust to the relative coordinate.
            Rect root = mTabListCoordinator.getRecyclerViewLocation();
            thumbnail.offset(-root.left, -root.top);
            return thumbnail;
        }
        return mTabListCoordinator.getThumbnailLocationOfCurrentTab();
    }

    @Override
    public @NonNull Size getThumbnailSize() {
        return mTabListCoordinator.getThumbnailSize();
    }

    // TabListDelegate implementation.
    @Override
    public int getResourceId() {
        return mTabListCoordinator.getResourceId();
    }

    @Override
    public void setBitmapCallbackForTesting(Callback<Bitmap> callback) {
        TabListMediator.ThumbnailFetcher.sBitmapCallbackForTesting = callback;
        ResettersForTesting.register(
                () -> TabListMediator.ThumbnailFetcher.sBitmapCallbackForTesting = null);
    }

    @Override
    public int getBitmapFetchCountForTesting() {
        return TabListMediator.ThumbnailFetcher.sFetchCountForTesting;
    }

    @Override
    public void resetBitmapFetchCountForTesting() {
        TabListMediator.ThumbnailFetcher.sFetchCountForTesting = 0;
    }

    // ResetHandler implementation.
    @Override
    public boolean resetWithTabList(@Nullable TabList tabList, boolean quickMode) {
        return resetWithTabs(PseudoTab.getListOfPseudoTab(tabList), quickMode);
    }

    @Override
    public boolean resetWithTabs(@Nullable List<PseudoTab> tabs, boolean quickMode) {
        mMessageManager.beforeReset();
        boolean showQuickly = mTabListCoordinator.resetWithListOfTabs(tabs, quickMode);
        mMessageManager.afterReset(tabs == null ? 0 : tabs.size());

        return showQuickly;
    }

    // OnLongPressTabItemEventListener implementation
    @Override
    public void onLongPressEvent(int tabId) {
        mTabListEditorManager.showTabListEditor();
        RecordUserAction.record("TabMultiSelectV2.OpenLongPressInGrid");
    }


    private View getTabGridDialogAnimationSourceView(int tabId) {
        int index = mTabListCoordinator.getTabIndexFromTabId(tabId);
        // TODO(crbug.com/999372): This is band-aid fix that will show basic fade-in/fade-out
        // animation when we cannot find the animation source view holder. This is happening due to
        // current group id in TabGridDialog can not be indexed in TabListModel, which should never
        // happen. Remove this when figure out the actual cause.
        ViewHolder sourceViewHolder =
                mTabListCoordinator.getContainerView().findViewHolderForAdapterPosition(index);
        if (sourceViewHolder == null) return null;
        return sourceViewHolder.itemView;
    }

    @Override
    public void softCleanup() {
        mTabListCoordinator.softCleanup();
    }

    @Override
    public void hardCleanup() {
        mTabListCoordinator.hardCleanup();
    }

    // ResetHandler implementation.
    //
    // Suppress to observe SharedPreferences, which is discouraged; use another messaging channel
    // instead.
    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
    @Override
    public void onDestroy() {
        if (mTabSwitcherMenuActionHandler != null) {
            mMenuOrKeyboardActionController.unregisterMenuOrKeyboardActionHandler(
                    mTabSwitcherMenuActionHandler);
        }
        mTabListCoordinator.onDestroy();
        mContainerViewChangeProcessor.destroy();
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.destroy();
        }
        mMultiThumbnailCardProvider.destroy();
        mTabListEditorManager.destroy();
        mMediator.destroy();
        mMessageManager.destroy();
        mLifecycleDispatcher.unregister(this);
        if (mTabAttributeCache != null) {
            mTabAttributeCache.destroy();
        }
        if (mPriceAnnotationsPrefListener != null) {
            ContextUtils.getAppSharedPreferences()
                    .unregisterOnSharedPreferenceChangeListener(mPriceAnnotationsPrefListener);
        }
    }

    @Override
    public void runAnimationOnNextLayout(Runnable runnable) {
        mTabListCoordinator.runAnimationOnNextLayout(runnable);
    }

    @Override
    public void showQuickDeleteAnimation(Runnable onAnimationEnd, List<Tab> tabs) {
        mTabListCoordinator.showQuickDeleteAnimation(onAnimationEnd, tabs);
    }

    private TabSwitcherMessageManager getMessageManager() {
        return mMessageManager;
    }
}
