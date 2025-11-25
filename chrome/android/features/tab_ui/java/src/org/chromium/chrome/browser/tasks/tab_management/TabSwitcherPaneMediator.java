// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BLOCK_TOUCH_INPUT;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.FOCUS_TAB_INDEX_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.INITIAL_SCROLL_INDEX;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.Configuration;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Px;

import org.chromium.base.Callback;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.hub.HubUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabSwitcherCustomViewManager;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceWelcomeMessageReviewActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabGridDialogMediator.DialogController;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.GridCardOnClickListenerProvider;
import org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Mediator for {@link TabSwitcherPaneCoordinator}. */
@NullMarked
public class TabSwitcherPaneMediator
        implements GridCardOnClickListenerProvider,
                PriceWelcomeMessageReviewActionProvider,
                TabSwitcherCustomViewManager.Delegate,
                BackPressHandler {

    private static final int PINNED_TABS_SHOW_SEARCH_BOX_DURATION = 10;
    private static final int PINNED_TABS_HIDE_SEARCH_BOX_DURATION = 100;
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mIsDialogVisibleSupplier =
            new ObservableSupplierImpl<>();
    private final TabActionListener mTabGridDialogOpener =
            new TabActionListener() {
                @Override
                public void run(View view, int tabId, @Nullable MotionEventInfo triggeringMotion) {
                    openTabGroupDialog(tabId);
                    RecordUserAction.record("TabGridDialog.ExpandedFromSwitcher");
                }

                @Override
                public void run(
                        View view, String syncId, @Nullable MotionEventInfo triggeringMotion) {
                    // Intentional no-op.
                }
            };
    private final Callback<@Nullable TabGroupModelFilter> mOnTabGroupModelFilterChanged =
            new ValueChangedCallback<>(this::onTabGroupModelFilterChanged);
    private final Callback<Boolean> mOnDialogShowingOrAnimatingCallback =
            this::onDialogShowingOrAnimatingChanged;

    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void tabClosureUndone(Tab tab) {
                    notifyBackPressStateChangedInternal();
                }

                @Override
                public void onFinishingTabClosure(Tab tab, @TabClosingSource int closingSource) {
                    // If tab is closed by the site itself rather than user's input,
                    // onTabClosePending & tabClosureCommitted won't be called.
                    notifyBackPressStateChangedInternal();
                }

                @Override
                public void tabRemoved(Tab tab) {
                    notifyBackPressStateChangedInternal();
                }

                @Override
                public void onTabClosePending(
                        List<Tab> tabs, boolean isAllTabs, @TabClosingSource int closingSource) {
                    notifyBackPressStateChangedInternal();
                }

                @Override
                public void restoreCompleted() {
                    // The tab model just finished restoring. If this pane is visible we should try
                    // to show tabs. `resetWithListOfTabs` will handle any necessary state
                    // complexity
                    // such as incognito reauth.
                    showTabsIfVisible();
                }
            };

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetOpened(@StateChangeReason int reason) {
                    suppressAccessibility(true);
                }

                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    suppressAccessibility(false);
                }
            };

    private final Callback<Boolean> mOnAnimatingChanged = this::onAnimatingChanged;
    private final Callback<Boolean> mOnVisibilityChanged = this::onVisibilityChanged;
    private final Callback<Boolean> mNotifyBackPressedCallback =
            ignored -> {
                notifyBackPressStateChangedInternal();
            };

    /** Interface for getting scroll positions of tabs. */
    @FunctionalInterface
    public interface TabIndexLookup {
        /**
         * Returns the scroll position of a tab from its filter index in the TabListRecyclerView.
         */
        int getNthTabIndexInModel(int filterIndex);
    }

    private final Context mContext;
    private final TabSwitcherResetHandler mResetHandler;
    private final ObservableSupplier<@Nullable TabGroupModelFilter> mTabGroupModelFilterSupplier;
    private final LazyOneshotSupplier<DialogController> mTabGridDialogControllerSupplier;
    private final PropertyModel mContainerViewModel;
    private final ViewGroup mContainerView;
    private final ObservableSupplier<Boolean> mIsVisibleSupplier;
    private final ObservableSupplier<Boolean> mIsAnimatingSupplier;
    private final Runnable mOnTabSwitcherShown;
    private final Callback<Integer> mOnTabClickCallback;
    private final TabIndexLookup mTabIndexLookup;
    private final BottomSheetController mBottomSheetController;
    private final Runnable mAddOnLayoutChangedAfterInitialScrollListener;
    private final AnimationHandler mSupplementaryContainerAnimationHandler = new AnimationHandler();
    private @Nullable ObservableSupplier<TabListEditorController> mTabListEditorControllerSupplier;
    private final ObservableSupplierImpl<Boolean> mHubSearchBoxVisibilitySupplier;
    private @Nullable ObservableSupplier<Boolean> mCurrentTabListEditorControllerBackSupplier;
    private @Nullable View mCustomView;
    private @Nullable Runnable mCustomViewBackPressRunnable;
    private final @Px int mSearchBoxGapPx;

    private boolean mTryToShowOnFilterChanged;

    /**
     * @param context The context for retrieving resources.
     * @param resetHandler The reset handler for updating the {@link TabListCoordinator}.
     * @param tabGroupModelFilterSupplier The supplier of the {@link TabGroupModelFilter}. This
     *     should usually only ever be set once.
     * @param tabGridDialogControllerSupplier The supplier of the {@link DialogController}.
     * @param containerViewModel The {@link PropertyModel} for the {@link TabListRecyclerView}.
     * @param containerView The view that hosts the {@link TabListRecyclerView}.
     * @param onTabSwitcherShown Runnable executed once the view becomes visible.
     * @param isVisibleSupplier Supplier for visibility of the pane.
     * @param isAnimatingSupplier Supplier for when the pane is animating in or out of visibility.
     * @param onTabClickCallback Callback to invoke when a tab is clicked.
     * @param tabIndexLookup Lookup for scroll position from tab index.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param addOnLayoutChangedAfterInitialScrollListener Runnable that adds a listener after an
     *     initial scroll event.
     * @param hubSearchBoxVisibilitySupplier Supplier to control the visibility of the Hub search
     *     box.
     */
    public TabSwitcherPaneMediator(
            Context context,
            TabSwitcherResetHandler resetHandler,
            ObservableSupplier<@Nullable TabGroupModelFilter> tabGroupModelFilterSupplier,
            LazyOneshotSupplier<DialogController> tabGridDialogControllerSupplier,
            PropertyModel containerViewModel,
            ViewGroup containerView,
            Runnable onTabSwitcherShown,
            ObservableSupplier<Boolean> isVisibleSupplier,
            ObservableSupplier<Boolean> isAnimatingSupplier,
            Callback<Integer> onTabClickCallback,
            TabIndexLookup tabIndexLookup,
            BottomSheetController bottomSheetController,
            Runnable addOnLayoutChangedAfterInitialScrollListener,
            ObservableSupplierImpl<Boolean> hubSearchBoxVisibilitySupplier) {
        mContext = context;
        mResetHandler = resetHandler;
        mTabIndexLookup = tabIndexLookup;
        mOnTabClickCallback = onTabClickCallback;
        mTabGroupModelFilterSupplier = tabGroupModelFilterSupplier;
        var filter = mTabGroupModelFilterSupplier.addObserver(mOnTabGroupModelFilterChanged);
        mTryToShowOnFilterChanged = filter == null || !filter.isTabModelRestored();

        mTabGridDialogControllerSupplier = tabGridDialogControllerSupplier;
        tabGridDialogControllerSupplier.onAvailable(
                tabGridDialogController -> {
                    tabGridDialogController
                            .getHandleBackPressChangedSupplier()
                            .addObserver(mNotifyBackPressedCallback);
                    tabGridDialogController
                            .getShowingOrAnimationSupplier()
                            .addObserver(mOnDialogShowingOrAnimatingCallback);
                });

        mContainerViewModel = containerViewModel;
        // TODO(crbug.com/40946413): Remove the containerView dependency. It is only used for adding
        // and
        // removing custom views for incognito reauth and it breaks the intended encapsulation of
        // views not being accessible to the mediator.
        mContainerView = containerView;
        mOnTabSwitcherShown = onTabSwitcherShown;

        mIsVisibleSupplier = isVisibleSupplier;
        isVisibleSupplier.addObserver(mOnVisibilityChanged);
        mIsAnimatingSupplier = isAnimatingSupplier;
        isAnimatingSupplier.addObserver(mOnAnimatingChanged);
        mBottomSheetController = bottomSheetController;
        mBottomSheetController.addObserver(mBottomSheetObserver);
        mAddOnLayoutChangedAfterInitialScrollListener =
                addOnLayoutChangedAfterInitialScrollListener;
        mHubSearchBoxVisibilitySupplier = hubSearchBoxVisibilitySupplier;
        mSearchBoxGapPx = mContext.getResources().getDimensionPixelSize(R.dimen.hub_search_box_gap);

        notifyBackPressStateChangedInternal();
    }

    /** Destroys the mediator unregistering all its observers. */
    public void destroy() {
        hideDialogs();
        mTabGroupModelFilterSupplier.removeObserver(mOnTabGroupModelFilterChanged);
        removeTabModelObserver(mTabGroupModelFilterSupplier.get());

        mIsVisibleSupplier.removeObserver(mOnVisibilityChanged);
        mIsAnimatingSupplier.removeObserver(mOnAnimatingChanged);
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        DialogController controller = getTabGridDialogController();
        if (controller != null) {
            controller
                    .getHandleBackPressChangedSupplier()
                    .removeObserver(mNotifyBackPressedCallback);
            controller
                    .getShowingOrAnimationSupplier()
                    .removeObserver(mOnDialogShowingOrAnimatingCallback);
        }
        if (mCurrentTabListEditorControllerBackSupplier != null) {
            mCurrentTabListEditorControllerBackSupplier.removeObserver(mNotifyBackPressedCallback);
        }
    }

    /** Returns a supplier that indicates whether any dialogs are visible. */
    public ObservableSupplier<Boolean> getIsDialogVisibleSupplier() {
        return mIsDialogVisibleSupplier;
    }

    /** Requests accessibility focus on the currently selected tab. */
    public void requestAccessibilityFocusOnCurrentTab() {
        TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();
        assumeNonNull(filter);
        mContainerViewModel.set(
                FOCUS_TAB_INDEX_FOR_ACCESSIBILITY, filter.getCurrentRepresentativeTabIndex());
    }

    /** Scrolls to the currently selected tab. */
    public void setInitialScrollIndexOffset() {
        TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();
        assumeNonNull(filter);
        scrollToTab(
                mTabIndexLookup.getNthTabIndexInModel(filter.getCurrentRepresentativeTabIndex()));
    }

    @Override
    public @BackPressResult int handleBackPress() {
        TabListEditorController editorController = getTabListEditorController();
        if (editorController != null && editorController.handleBackPressed()) {
            return BackPressResult.SUCCESS;
        }
        if (mCustomViewBackPressRunnable != null) {
            mCustomViewBackPressRunnable.run();
            return BackPressResult.SUCCESS;
        }

        if (Boolean.TRUE.equals(mIsAnimatingSupplier.get())) {
            // crbug.com/1420410: intentionally do nothing to wait for tab-to-GTS transition to be
            // finished. Note this has to be before following if-branch since during transition, the
            // container is still invisible. On tablet, the translation transition replaces the
            // tab-to-GTS (expand/shrink) animation, which does not suffer from the same issue.
            return BackPressResult.SUCCESS;
        }

        if (Boolean.FALSE.equals(mIsVisibleSupplier.get())) {
            assert false : "Invisible container backpress should be handled.";
            return BackPressResult.FAILURE;
        }

        DialogController controller = getTabGridDialogController();
        if (controller != null && controller.handleBackPressed()) {
            return BackPressResult.SUCCESS;
        }

        // The signal to select a tab and exit is handled at the pane level.

        return BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressChangedSupplier;
    }

    @Override
    public @Nullable TabActionListener openTabGridDialog(Tab tab) {
        if (!ableToOpenDialog(tab)) return null;
        return mTabGridDialogOpener;
    }

    @Override
    public @Nullable TabActionListener openTabGridDialog(String syncId) {
        // Intentional no-op.
        return null;
    }

    @Override
    public void onTabSelecting(int tabId, boolean fromActionButton) {
        mOnTabClickCallback.onResult(tabId);
    }

    @Override
    public void scrollToTab(int tabIndexInModel) {
        mAddOnLayoutChangedAfterInitialScrollListener.run();
        mContainerViewModel.set(INITIAL_SCROLL_INDEX, tabIndexInModel);
    }

    /** Scroll to a given tab or tab group by id. */
    public void scrollToTabById(int tabId) {
        TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();
        assumeNonNull(filter);
        TabModel tabModel = filter.getTabModel();
        Tab tab = tabModel.getTabById(tabId);

        // TODO(crbug.com/375309394): Figure out why the tab is null here and prevent it.
        boolean hasTab = tab != null;
        RecordHistogram.recordBooleanHistogram(
                "Tabs.GridTabSwitcher.ScrollToTabById.HasTab", hasTab);
        if (!hasTab) return;

        int index = filter.representativeIndexOf(tab);
        scrollToTab(mTabIndexLookup.getNthTabIndexInModel(index));
    }

    @Override
    public void addCustomView(
            View customView, @Nullable Runnable backPressRunnable, boolean clearTabList) {
        assert mCustomView == null : "Only one custom view may be showing at a time.";

        hideDialogs();

        if (clearTabList) {
            mResetHandler.resetWithListOfTabs(null);
        }

        mContainerView.addView(customView);
        mCustomView = customView;
        mCustomViewBackPressRunnable = backPressRunnable;
        notifyBackPressStateChangedInternal();
    }

    @Override
    public void removeCustomView(View customView) {
        assert mCustomView != null : "No custom view client has added a view.";
        mContainerView.removeView(customView);
        mCustomView = null;
        mCustomViewBackPressRunnable = null;
        notifyBackPressStateChangedInternal();
    }

    void setTabListEditorControllerSupplier(
            ObservableSupplier<TabListEditorController> tabListEditorControllerSupplier) {
        assert mTabListEditorControllerSupplier == null
                : "setTabListEditorControllerSupplier should be called only once.";
        mTabListEditorControllerSupplier = tabListEditorControllerSupplier;
        mCurrentTabListEditorControllerBackSupplier =
                tabListEditorControllerSupplier.createTransitive(
                        BackPressHandler::getHandleBackPressChangedSupplier);
        mCurrentTabListEditorControllerBackSupplier.addObserver(mNotifyBackPressedCallback);
    }

    void hideDialogs() {
        DialogController controller = getTabGridDialogController();
        if (controller != null) {
            controller.hideDialog(false);
        }
        TabListEditorController editorController = getTabListEditorController();
        if (editorController != null && editorController.isVisible()) {
            editorController.hide();
        }
    }

    /** Sets the visibility of the Hub search box. */
    void setHubSearchBoxVisibility(boolean isVisible) {
        mHubSearchBoxVisibilitySupplier.set(isVisible);
    }

    /** Translates the pinned strip to make space for the search box. */
    void maybeTranslatePinnedStrip(boolean shouldShowSearchBox, boolean forced) {
        // Show search box always when screen size is less than tablet and search box movement is
        // disabled.
        if (!PinnedTabStripUtils.isSearchBoxMovementEnabledForPinnedTabs()) {
            shouldShowSearchBox = true;
        }

        Configuration config = mContext.getResources().getConfiguration();
        boolean isTabletOrLandscape = HubUtils.isScreenWidthTablet(config.screenWidthDp);
        boolean shouldShow = shouldShowSearchBox && !isTabletOrLandscape;

        animateSupplementaryDataContainer(shouldShow, forced);
    }

    private void animateSupplementaryDataContainer(boolean isSearchBoxVisible, boolean forced) {
        // Early out if the animation is already running.
        if (mSupplementaryContainerAnimationHandler.isAnimationPresent()) return;

        LinearLayout supplementaryDataContainer =
                mContainerView.findViewById(R.id.supplementary_data_container);
        int translationHeight = isSearchBoxVisible ? mSearchBoxGapPx : 0;

        // Early out if we are already in the correct state.
        if (!forced
                && isSearchBoxVisible
                && supplementaryDataContainer.getTranslationY() == translationHeight) {
            return;
        }

        int duration =
                isSearchBoxVisible
                        ? PINNED_TABS_SHOW_SEARCH_BOX_DURATION
                        : PINNED_TABS_HIDE_SEARCH_BOX_DURATION;

        // TODO(crbug.com/455919135): Move view manipulation to View binder with relevant property.
        ValueAnimator translateAnimator =
                ObjectAnimator.ofFloat(
                        supplementaryDataContainer,
                        View.TRANSLATION_Y,
                        supplementaryDataContainer.getTranslationY(),
                        translationHeight);
        translateAnimator.setDuration(duration);
        translateAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        if (!isSearchBoxVisible) {
                            setHubSearchBoxVisibility(false);
                        }
                    }

                    @Override
                    public void onAnimationEnd(@NonNull Animator animation, boolean isReverse) {
                        if (isSearchBoxVisible) {
                            setHubSearchBoxVisibility(true);
                        }
                    }
                });

        mSupplementaryContainerAnimationHandler.startAnimation(translateAnimator);
    }

    /**
     * Adds or removes the search box space.
     *
     * @param isTabletOrLandscape Whether the device is a tablet or landscape.
     */
    void setIsTabletOrLandscape(boolean isTabletOrLandscape) {
        mContainerViewModel.set(
                TabListContainerProperties.IS_TABLET_OR_LANDSCAPE, isTabletOrLandscape);
    }

    private boolean ableToOpenDialog(Tab tab) {
        TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();
        assumeNonNull(filter);
        return filter.getTabModel().isIncognito() == tab.isIncognito()
                && filter.isTabInTabGroup(tab);
    }

    public void openTabGroupDialog(int tabId) {
        List<Tab> relatedTabs =
                assumeNonNull(mTabGroupModelFilterSupplier.get()).getRelatedTabList(tabId);
        if (relatedTabs.size() == 0) {
            relatedTabs = null;
        }
        assumeNonNull(mTabGridDialogControllerSupplier.get()).resetWithListOfTabs(relatedTabs);
    }

    private void notifyBackPressStateChangedInternal() {
        if (Boolean.FALSE.equals(mIsVisibleSupplier.get())) return;

        mIsDialogVisibleSupplier.set(isDialogVisible());
        mBackPressChangedSupplier.set(shouldInterceptBackPress());
    }

    private boolean isDialogVisible() {
        TabListEditorController editorController = getTabListEditorController();
        if (editorController != null && editorController.isVisible()) {
            return true;
        }
        DialogController dialogController = getTabGridDialogController();
        if (dialogController != null && dialogController.isVisible()) {
            return true;
        }
        return false;
    }

    private boolean shouldInterceptBackPress() {
        if (isDialogVisible()) return true;
        if (mCustomViewBackPressRunnable != null) return true;

        // TODO(crbug.com/40946413) consider restricting to grid + phone only.
        if (Boolean.TRUE.equals(mIsAnimatingSupplier.get())) return true;

        // TODO(crbug.com/40946413): Figure out whether we care about tab selection/start surface
        // here.
        return false;
    }

    @Nullable TabListEditorController getTabListEditorController() {
        return mTabListEditorControllerSupplier == null
                ? null
                : mTabListEditorControllerSupplier.get();
    }

    private @Nullable DialogController getTabGridDialogController() {
        var supplier = mTabGridDialogControllerSupplier;
        return !supplier.hasValue() ? null : supplier.get();
    }

    private void removeTabModelObserver(@Nullable TabGroupModelFilter filter) {
        if (filter == null) return;

        filter.removeObserver(mTabModelObserver);
    }

    private void onTabGroupModelFilterChanged(
            @Nullable TabGroupModelFilter newFilter, @Nullable TabGroupModelFilter oldFilter) {
        removeTabModelObserver(oldFilter);

        if (newFilter != null) {
            newFilter.addObserver(mTabModelObserver);
            // The tab model may already be restored and `restoreCompleted` will be skipped, but
            // this pane is visible. To avoid an empty state, try to show tabs now.
            // `resetWithListOfTabs` will skip in the case the tab model is not initialized so this
            // will no-op if it is racing with `restoreCompleted`. Only do this if in the
            // constructor there was no TabGroupModelFilter or it wasn't initialized.
            if (mTryToShowOnFilterChanged) {
                showTabsIfVisible();
                mTryToShowOnFilterChanged = false;
            }
        }
    }

    private void onAnimatingChanged(boolean animating) {
        updateBlockTouchInput();
        DialogController controller = getTabGridDialogController();
        if (controller != null && animating) {
            controller.hideDialog(true);
        }
        notifyBackPressStateChangedInternal();
    }

    private void onVisibilityChanged(boolean visible) {
        if (visible) {
            mOnTabSwitcherShown.run();
        } else {
            hideDialogs();
        }

        notifyBackPressStateChangedInternal();
    }

    private void onDialogShowingOrAnimatingChanged(boolean showingOrAnimation) {
        updateBlockTouchInput();
    }

    private void updateBlockTouchInput() {
        boolean blockTouchInput = mIsAnimatingSupplier.get() || isDialogShowingOrAnimating();
        mContainerViewModel.set(BLOCK_TOUCH_INPUT, blockTouchInput);
    }

    private boolean isDialogShowingOrAnimating() {
        @Nullable DialogController dialogController = getTabGridDialogController();
        if (dialogController == null) {
            return false;
        }

        return dialogController.getShowingOrAnimationSupplier().get();
    }

    private void showTabsIfVisible() {
        if (Boolean.TRUE.equals(mIsVisibleSupplier.get())) {
            mResetHandler.resetWithListOfTabs(
                    assumeNonNull(mTabGroupModelFilterSupplier.get()).getRepresentativeTabList());
            setInitialScrollIndexOffset();
        }
    }

    private void suppressAccessibility(boolean suppress) {
        mContainerViewModel.set(TabListContainerProperties.SUPPRESS_ACCESSIBILITY, suppress);
    }
}
