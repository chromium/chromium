// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BROWSER_CONTROLS_STATE_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.MODE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.PANE_KEYS;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Rect;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.Callback;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab.TitleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabGridDialogMediator.DialogController;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.GridCardOnClickListenerProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for a {@link TabSwitcherPaneBase}'s UI. */
public class TabSwitcherPaneCoordinator implements BackPressHandler {
    private static final String COMPONENT_NAME = "GridTabSwitcher";

    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final Callback<Boolean> mOnVisibilityChanged = this::onVisibilityChanged;
    private final ObservableSupplier<Boolean> mIsVisibleSupplier;
    private final TabSwitcherPaneMediator mMediator;
    private final Supplier<Boolean> mTabGridDialogVisibilitySupplier = this::isTabGridDialogVisible;
    private final MultiThumbnailCardProvider mMultiThumbnailCardProvider;
    private final TabListCoordinator mTabListCoordinator;
    private final PropertyModel mContainerViewModel;
    private final PropertyModelChangeProcessor mContainerViewChangeProcessor;
    private final LazyOneshotSupplier<DialogController> mDialogControllerSupplier;
    private final TabListEditorManager mTabListEditorManager;
    private final TabSwitcherMessageManager mMessageManager;

    /** Lazily initialized when shown. */
    private @Nullable TabGridDialogCoordinator mTabGridDialogCoordinator;

    /**
     * @param activity The {@link Activity} that hosts the pane.
     * @param lifecycleDispatcher The lifecycle dispatcher for the activity.
     * @param profileProviderSupplier The supplier for profiles.
     * @param tabModelFilterSupplier The supplier of the tab model filter fo rthis pane.
     * @param regularTabModelSupplier The supplier of the regular tab model.
     * @param tabContentManager For management of thumbnails.
     * @param tabCreatorManager For creating new tabs.
     * @param titleProvider The default title provider for tabs and tab groups.
     * @param browserControlsStateProvider For determining thumbnail size.
     * @param multiWindowModeStateDispatcher For managing behavior in multi-window.
     * @param scrimCoordinator The scrim coordinator to use for the tab grid dialog.
     * @param snackbarManager The activity level snackbar manager.
     * @param modalDialogManager The modal dialog manager for the activity.
     * @param parentView The view to use as a parent.
     * @param resetHandler The tab list reset handler for the pane.
     * @param isVisibleSupplier The supplier of the pane's visibility.
     * @param isAnimatingSupplier Whether the pane is animating into or out of view.
     * @param onTabClickCallback Callback to invoke when a tab is clicked.
     * @param mode The {@link TabListMode} to use.
     * @param supportsEmptyState Whether empty state UI should be shown when the model is empty.
     */
    public TabSwitcherPaneCoordinator(
            @NonNull Activity activity,
            @NonNull ActivityLifecycleDispatcher lifecycleDispatcher,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @NonNull ObservableSupplier<TabModelFilter> tabModelFilterSupplier,
            @NonNull Supplier<TabModel> regularTabModelSupplier,
            @NonNull TabContentManager tabContentManager,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull TitleProvider titleProvider,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull ViewGroup parentView,
            @NonNull TabSwitcherResetHandler resetHandler,
            @NonNull ObservableSupplier<Boolean> isVisibleSupplier,
            @NonNull ObservableSupplier<Boolean> isAnimatingSupplier,
            @NonNull Callback<Integer> onTabClickCallback,
            @TabListMode int mode,
            boolean supportsEmptyState) {
        mProfileProviderSupplier = profileProviderSupplier;
        mIsVisibleSupplier = isVisibleSupplier;
        isVisibleSupplier.addObserver(mOnVisibilityChanged);
        assert mode != TabListMode.STRIP : "TabListMode.STRIP not supported.";

        ViewGroup coordinatorView = activity.findViewById(R.id.coordinator);

        PropertyModel containerViewModel =
                new PropertyModel.Builder(PANE_KEYS)
                        .with(BROWSER_CONTROLS_STATE_PROVIDER, browserControlsStateProvider)
                        .with(MODE, mode)
                        .build();
        mContainerViewModel = containerViewModel;

        mDialogControllerSupplier =
                LazyOneshotSupplier.fromSupplier(
                        () -> {
                            mTabGridDialogCoordinator =
                                    new TabGridDialogCoordinator(
                                            activity,
                                            browserControlsStateProvider,
                                            tabModelFilterSupplier,
                                            regularTabModelSupplier,
                                            tabContentManager,
                                            tabCreatorManager,
                                            coordinatorView,
                                            resetHandler,
                                            getGridCardOnClickListenerProvider(),
                                            TabSwitcherPaneCoordinator.this
                                                    ::getTabGridDialogAnimationSourceView,
                                            scrimCoordinator,
                                            getTabGroupTitleEditor(),
                                            /* rootView= */ coordinatorView);
                            return mTabGridDialogCoordinator.getDialogController();
                        });

        mMediator =
                new TabSwitcherPaneMediator(
                        resetHandler,
                        tabModelFilterSupplier,
                        mDialogControllerSupplier,
                        containerViewModel,
                        parentView,
                        this::onTabSwitcherShown,
                        isVisibleSupplier,
                        isAnimatingSupplier,
                        onTabClickCallback);

        mMultiThumbnailCardProvider =
                new MultiThumbnailCardProvider(
                        activity,
                        browserControlsStateProvider,
                        tabContentManager,
                        tabModelFilterSupplier);

        @DrawableRes
        int emptyImageResId =
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)
                        ? R.drawable.tablet_tab_switcher_empty_state_illustration
                        : R.drawable.phone_tab_switcher_empty_state_illustration;
        TabListCoordinator tabListCoordinator =
                new TabListCoordinator(
                        mode,
                        activity,
                        browserControlsStateProvider,
                        tabModelFilterSupplier,
                        regularTabModelSupplier,
                        mMultiThumbnailCardProvider,
                        titleProvider,
                        /* actionOnRelatedTabs= */ true,
                        getGridCardOnClickListenerProvider(),
                        /* dialogHandler= */ null,
                        TabProperties.UiType.CLOSABLE,
                        /* selectionDelegateProvider= */ null,
                        this::getPriceWelcomeMessageController,
                        parentView,
                        /* attachToParent= */ true,
                        COMPONENT_NAME,
                        /* rootView= */ parentView,
                        /* onModelTokenChange= */ null,
                        /* hasEmptyView= */ supportsEmptyState,
                        supportsEmptyState ? emptyImageResId : Resources.ID_NULL,
                        supportsEmptyState
                                ? R.string.tabswitcher_no_tabs_empty_state
                                : Resources.ID_NULL,
                        supportsEmptyState
                                ? R.string.tabswitcher_no_tabs_open_to_visit_different_pages
                                : Resources.ID_NULL);
        mTabListCoordinator = tabListCoordinator;

        TabListRecyclerView recyclerView = tabListCoordinator.getContainerView();
        recyclerView.setVisibility(View.VISIBLE);
        mContainerViewChangeProcessor =
                PropertyModelChangeProcessor.create(
                        containerViewModel, recyclerView, TabListContainerViewBinder::bind);

        TabListEditorManager tabListEditorManager =
                new TabListEditorManager(
                        activity,
                        coordinatorView,
                        /* rootView= */ parentView,
                        browserControlsStateProvider,
                        tabModelFilterSupplier,
                        regularTabModelSupplier,
                        tabContentManager,
                        tabListCoordinator,
                        mode);
        mTabListEditorManager = tabListEditorManager;
        mMediator.setTabListEditorControllerSupplier(mTabListEditorManager.getControllerSupplier());

        var tabListEditorControllerSupplier =
                LazyOneshotSupplier.fromSupplier(
                        () -> {
                            tabListEditorManager.initTabListEditor();
                            return tabListEditorManager.getControllerSupplier().get();
                        });

        mMessageManager =
                new TabSwitcherMessageManager(
                        activity,
                        lifecycleDispatcher,
                        tabModelFilterSupplier,
                        parentView,
                        multiWindowModeStateDispatcher,
                        snackbarManager,
                        modalDialogManager,
                        tabListCoordinator,
                        tabListEditorControllerSupplier,
                        /* priceWelcomeMessageReviewActionProvider= */ mMediator,
                        mode);
    }

    /** Destroys the coordinator. */
    public void destroy() {
        mMediator.destroy();
        mTabListCoordinator.onDestroy();
        mContainerViewChangeProcessor.destroy();
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.destroy();
        }
        mIsVisibleSupplier.removeObserver(mOnVisibilityChanged);
        mMultiThumbnailCardProvider.destroy();
        mTabListEditorManager.destroy();
        mMessageManager.destroy();
    }

    /** Post native initialization. */
    public void initWithNative() {
        ProfileProvider profileProvider = mProfileProviderSupplier.get();
        assert profileProvider != null;
        Profile originalProfile = profileProvider.getOriginalProfile();

        mTabListCoordinator.initWithNative(originalProfile, /* dynamicResourceLoader= */ null);

        mMessageManager.initWithNative(originalProfile);

        mMultiThumbnailCardProvider.initWithNative(originalProfile);
    }

    /** Shows the tab list editor. */
    public void showTabListEditor() {
        mTabListEditorManager.showTabListEditor();
    }

    /**
     * Resets the UI with the specified tabs.
     *
     * @param tabList The {@link TabList} to show tabs for.
     */
    public void resetWithTabList(@Nullable TabList tabList) {
        var pseudoTabList = PseudoTab.getListOfPseudoTab(tabList);
        mMessageManager.beforeReset();
        // Quick mode being false here ensures the selected tab's thumbnail gets updated. With Hub
        // the TabListCoordinator no longer triggers thumbnail captures so this shouldn't guard
        // against the large amount of work that is used to.
        mTabListCoordinator.resetWithListOfTabs(pseudoTabList, /* quickMode= */ false);
        mMessageManager.afterReset(pseudoTabList == null ? 0 : pseudoTabList.size());
    }

    /** Performs soft cleanup which removes thumbnails to relieve memory usage. */
    public void softCleanup() {
        mTabListCoordinator.softCleanup();
    }

    /** Performs hard cleanup which saves price drop information. */
    public void hardCleanup() {
        mTabListCoordinator.hardCleanup();
        // TODO(crbug/1505772): The pre-fork implementation resets the tab list, this seems
        // suboptimal. Consider not doing this.
        resetWithTabList(null);
    }

    /**
     * Scrolls so that the selected tab in the current model is in the middle of the screen or as
     * close as possible if at the start/end of the recycler view.
     */
    public void setInitialScrollIndexOffset() {
        mMediator.setInitialScrollIndexOffset();
    }

    /** Requests accessibility focus on the current tab. */
    public void requestAccessibilityFocusOnCurrentTab() {
        mMediator.requestAccessibilityFocusOnCurrentTab();
    }

    /** Returns a {@link Supplier} that provides dialog visibility information. */
    public @Nullable Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        return mTabGridDialogVisibilitySupplier;
    }

    /** Returns a {@link TabSwitcherCustomViewManager.Delegate} for supplying custom views. */
    public @Nullable TabSwitcherCustomViewManager.Delegate
            getTabSwitcherCustomViewManagerDelegate() {
        return mMediator;
    }

    /** Returns the number of elements in the tab switcher's tab list model. */
    public int getTabSwitcherTabListModelSize() {
        return mTabListCoordinator.getTabListModelSize();
    }

    /** Set the tab switcher's RecyclerViewPosition. */
    public void setTabSwitcherRecyclerViewPosition(RecyclerViewPosition position) {
        mTabListCoordinator.setRecyclerViewPosition(position);
    }

    /** Returns the {@link Rect} of the recyclerview in global coordinates. */
    public @NonNull Rect getRecyclerViewRect() {
        return mTabListCoordinator.getRecyclerViewLocation();
    }

    /**
     * @param tabId The tab ID to get a rect for.
     * @return a {@link Rect} for the tab's thumbnail (may be an empty rect if the tab is not
     *     found).
     */
    public @NonNull Rect getTabThumbnailRect(int tabId) {
        TabGridDialogCoordinator dialogCoordinator = mTabGridDialogCoordinator;
        if (dialogCoordinator != null && dialogCoordinator.isVisible()) {
            return dialogCoordinator.getTabThumbnailRect(tabId);
        }
        return mTabListCoordinator.getTabThumbnailRect(tabId);
    }

    /** Returns the {@link Rect} of the recyclerview in global coordinates. */
    public @NonNull Size getThumbnailSize() {
        TabGridDialogCoordinator dialogCoordinator = mTabGridDialogCoordinator;
        if (dialogCoordinator != null && dialogCoordinator.isVisible()) {
            return dialogCoordinator.getThumbnailSize();
        }
        return mTabListCoordinator.getThumbnailSize();
    }

    /**
     * @param tabId The tab ID whose view to wait for.
     * @param r A runnable to be executed on the next layout pass or immediately if one is not
     *     scheduled.
     */
    public void waitForLayoutWithTab(int tabId, Runnable r) {
        TabGridDialogCoordinator dialogCoordinator = mTabGridDialogCoordinator;
        if (dialogCoordinator != null && dialogCoordinator.isVisible()) {
            dialogCoordinator.waitForLayoutWithTab(tabId, r);
            return;
        }
        mTabListCoordinator.waitForLayoutWithTab(tabId, r);
    }

    @Override
    public @BackPressResult int handleBackPress() {
        return mMediator.handleBackPress();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mMediator.getHandleBackPressChangedSupplier();
    }

    private boolean isTabGridDialogVisible() {
        return mTabGridDialogCoordinator == null ? false : mTabGridDialogCoordinator.isVisible();
    }

    private void onTabSwitcherShown() {
        mTabListCoordinator.attachEmptyView();
    }

    private View getTabGridDialogAnimationSourceView(int tabId) {
        TabListCoordinator coordinator = mTabListCoordinator;
        int index = coordinator.getTabIndexFromTabId(tabId);
        ViewHolder sourceViewHolder =
                coordinator.getContainerView().findViewHolderForAdapterPosition(index);
        // TODO(crbug.com/999372): This is band-aid fix that will show basic fade-in/fade-out
        // animation when we cannot find the animation source view holder. This is happening due to
        // current group id in TabGridDialog can not be indexed in TabListModel, which should never
        // happen. Remove this when figure out the actual cause.
        return sourceViewHolder == null ? null : sourceViewHolder.itemView;
    }

    private void onVisibilityChanged(boolean visible) {
        if (visible) {
            mTabListCoordinator.prepareTabSwitcherPaneView();
        } else {
            mTabListCoordinator.postHiding();
        }
    }

    private GridCardOnClickListenerProvider getGridCardOnClickListenerProvider() {
        return mMediator;
    }

    private TabGroupTitleEditor getTabGroupTitleEditor() {
        return mTabListCoordinator.getTabGroupTitleEditor();
    }

    private PriceWelcomeMessageController getPriceWelcomeMessageController() {
        return mMessageManager;
    }

    /** Returns the container view property model for testing. */
    @NonNull
    PropertyModel getContainerViewModelForTesting() {
        return mContainerViewModel;
    }

    /** Returns the dialog controller for testing. */
    @NonNull
    DialogController getTabGridDialogControllerForTesting() {
        return mDialogControllerSupplier.get();
    }
}
