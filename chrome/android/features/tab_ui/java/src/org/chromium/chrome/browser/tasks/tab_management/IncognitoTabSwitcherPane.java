// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.os.Build;
import android.view.View.OnClickListener;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.DelegateButtonData;
import org.chromium.chrome.browser.hub.DisplayButtonData;
import org.chromium.chrome.browser.hub.FullButtonData;
import org.chromium.chrome.browser.hub.HubColorScheme;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneHubController;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.ResourceButtonData;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager.IncognitoReauthCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.sensitive_content.SensitiveContentFeatures;

import java.util.function.DoubleConsumer;

/** A {@link Pane} representing the incognito tab switcher. */
public class IncognitoTabSwitcherPane extends TabSwitcherPaneBase {
    private final IncognitoTabModelObserver mIncognitoTabModelObserver =
            new IncognitoTabModelObserver() {
                @Override
                public void wasFirstTabCreated() {
                    mReferenceButtonDataSupplier.set(mReferenceButtonData);
                }

                @Override
                public void didBecomeEmpty() {
                    IncognitoTabSwitcherPaneCleaner cleanupHelper =
                            initIncognitoTabSwitcherPaneCleanup();
                    cleanupHelper.coordinateCleanUp();
                }
            };

    private final IncognitoReauthCallback mIncognitoReauthCallback =
            new IncognitoReauthCallback() {
                @Override
                public void onIncognitoReauthNotPossible() {}

                @Override
                public void onIncognitoReauthSuccess() {
                    TabGroupModelFilter incognitoTabGroupModelFilter =
                            mIncognitoTabGroupModelFilterSupplier.get();
                    @Nullable
                    TabSwitcherPaneCoordinator coordinator = getTabSwitcherPaneCoordinator();
                    if (!getIsVisibleSupplier().get()
                            || coordinator == null
                            || !incognitoTabGroupModelFilter.isCurrentlySelectedFilter()) {
                        return;
                    }

                    coordinator.resetWithTabList(incognitoTabGroupModelFilter);
                    coordinator.setInitialScrollIndexOffset();
                    coordinator.requestAccessibilityFocusOnCurrentTab();

                    setNewTabButtonEnabledState(/* enabled= */ true);
                }

                @Override
                public void onIncognitoReauthFailure() {}
            };

    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void onFinishingTabClosure(Tab tab) {
                    mLastClosedTabId = tab.getId();
                }
            };

    /** Not safe to use until initWithNative. */
    private final @NonNull Supplier<TabGroupModelFilter> mIncognitoTabGroupModelFilterSupplier;

    private final @NonNull ResourceButtonData mReferenceButtonData;
    private final @NonNull FullButtonData mEnabledNewTabButtonData;
    private final @NonNull FullButtonData mDisabledNewTabButtonData;

    private boolean mIsNativeInitialized;
    private int mLastClosedTabId;
    private @Nullable IncognitoReauthController mIncognitoReauthController;
    private @Nullable CallbackController mCallbackController;

    /**
     * @param context The activity context.
     * @param factory The factory used to construct {@link TabSwitcherPaneCoordinator}s.
     * @param incognitoTabGroupModelFilterSupplier The incognito tab model filter.
     * @param newTabButtonClickListener The {@link OnClickListener} for the new tab button.
     * @param incognitoReauthControllerSupplier Supplier for the incognito reauth controller.
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param userEducationHelper Used for showing IPHs.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     * @param compositorViewHolderSupplier Supplier to the {@link CompositorViewHolder} instance.
     */
    IncognitoTabSwitcherPane(
            @NonNull Context context,
            @NonNull TabSwitcherPaneCoordinatorFactory factory,
            @NonNull Supplier<TabGroupModelFilter> incognitoTabGroupModelFilterSupplier,
            @NonNull OnClickListener newTabButtonClickListener,
            @Nullable OneshotSupplier<IncognitoReauthController> incognitoReauthControllerSupplier,
            @NonNull DoubleConsumer onToolbarAlphaChange,
            @NonNull UserEducationHelper userEducationHelper,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            @NonNull ObservableSupplier<CompositorViewHolder> compositorViewHolderSupplier) {
        super(
                context,
                factory,
                /* isIncognito= */ true,
                onToolbarAlphaChange,
                userEducationHelper,
                edgeToEdgeSupplier,
                compositorViewHolderSupplier);

        mIncognitoTabGroupModelFilterSupplier = incognitoTabGroupModelFilterSupplier;
        mLastClosedTabId = Tab.INVALID_TAB_ID;

        // TODO(crbug.com/40946413): Update this string to not be an a11y string and it should
        // probably
        // just say "Incognito".
        mReferenceButtonData =
                new ResourceButtonData(
                        R.string.accessibility_tab_switcher_incognito_stack,
                        R.string.accessibility_tab_switcher_incognito_stack,
                        R.drawable.ic_incognito);

        ResourceButtonData newTabButtonData =
                new ResourceButtonData(
                        R.string.button_new_tab,
                        R.string.button_new_incognito_tab,
                        R.drawable.new_tab_icon);
        mEnabledNewTabButtonData =
                new DelegateButtonData(
                        newTabButtonData,
                        () -> {
                            newTabButtonClickListener.onClick(null);
                        });
        mDisabledNewTabButtonData = new DelegateButtonData(newTabButtonData, null);

        if (incognitoReauthControllerSupplier != null) {
            mCallbackController = new CallbackController();
            incognitoReauthControllerSupplier.onAvailable(
                    mCallbackController.makeCancelable(
                            incognitoReauthController -> {
                                mIncognitoReauthController = incognitoReauthController;
                                incognitoReauthController.addIncognitoReauthCallback(
                                        mIncognitoReauthCallback);
                            }));
            setNewTabButtonEnabledState(/* enabled= */ false);
        } else {
            setNewTabButtonEnabledState(/* enabled= */ true);
        }
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.INCOGNITO_TAB_SWITCHER;
    }

    @Override
    public @HubColorScheme int getColorScheme() {
        return HubColorScheme.INCOGNITO;
    }

    @Override
    public void destroy() {
        super.destroy();
        IncognitoTabModel incognitoTabModel = getIncognitoTabModel();
        if (incognitoTabModel != null) {
            incognitoTabModel.removeIncognitoObserver(mIncognitoTabModelObserver);
            incognitoTabModel.removeObserver(mTabModelObserver);
        }
        if (mIncognitoReauthController != null) {
            mIncognitoReauthController.removeIncognitoReauthCallback(mIncognitoReauthCallback);
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
        }
    }

    @Override
    public void initWithNative() {
        super.initWithNative();
        mIsNativeInitialized = true;
        IncognitoTabModel incognitoTabModel = getIncognitoTabModel();
        incognitoTabModel.addIncognitoObserver(mIncognitoTabModelObserver);
        incognitoTabModel.addObserver(mTabModelObserver);
        if (incognitoTabModel.getCount() > 0) {
            mIncognitoTabModelObserver.wasFirstTabCreated();
        }
    }

    @Override
    public void showAllTabs() {
        resetWithTabList(mIncognitoTabGroupModelFilterSupplier.get(), false);
    }

    @Override
    public @Nullable Tab getCurrentTab() {
        return TabModelUtils.getCurrentTab(
                mIncognitoTabGroupModelFilterSupplier.get().getTabModel());
    }

    @Override
    public boolean shouldEagerlyCreateCoordinator() {
        return mReferenceButtonDataSupplier.get() != null;
    }

    @Override
    public boolean resetWithTabList(@Nullable TabList tabList, boolean quickMode) {
        @Nullable TabSwitcherPaneCoordinator coordinator = getTabSwitcherPaneCoordinator();
        if (coordinator == null) return false;

        @Nullable TabGroupModelFilter filter = mIncognitoTabGroupModelFilterSupplier.get();
        if (filter == null || !filter.isTabModelRestored()) {
            // The tab list is trying to show without the filter being ready. This happens when
            // first trying to show a the pane. If this happens an attempt to show will be made
            // when the filter's restoreCompleted() method is invoked in TabSwitcherPaneMediator.
            // Start a timer to measure how long it takes for tab state to be initialized and for
            // this UI to show i.e. isTabModelRestored becomes true. This timer will emit a
            // histogram when we successfully show. This timer is cancelled if: 1) the pane becomes
            // invisible in TabSwitcherPaneBase#notifyLoadHint, or 2) the filter becomes ready and
            // nothing gets shown.
            startWaitForTabStateInitializedTimer();
            return false;
        }

        boolean isNotVisibleOrSelected =
                !getIsVisibleSupplier().get() || !filter.isCurrentlySelectedFilter();
        boolean incognitoReauthShowing =
                mIncognitoReauthController != null
                        && mIncognitoReauthController.isIncognitoReauthPending();

        if (isNotVisibleOrSelected || incognitoReauthShowing) {
            coordinator.resetWithTabList(null);
            cancelWaitForTabStateInitializedTimer();
        } else {
            // TODO(crbug.com/373850469): Add unit tests when robolectric supports Android V.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                    && ChromeFeatureList.isEnabled(SensitiveContentFeatures.SENSITIVE_CONTENT)
                    && ChromeFeatureList.isEnabled(
                            SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)) {
                TabUiUtils.updateViewContentSensitivityForTabs(
                        filter.getTabModel(),
                        coordinator::setTabSwitcherContentSensitivity,
                        "SensitiveContent.TabSwitching.IncognitoTabSwitcherPane.Sensitivity");
            }
            coordinator.resetWithTabList(tabList);
            finishWaitForTabStateInitializedTimer();
        }

        setNewTabButtonEnabledState(/* enabled= */ !incognitoReauthShowing);
        return true;
    }

    @Override
    protected void requestAccessibilityFocusOnCurrentTab() {
        if (mIncognitoReauthController != null
                && mIncognitoReauthController.isReauthPageShowing()) {
            return;
        }

        super.requestAccessibilityFocusOnCurrentTab();
    }

    @Override
    protected Runnable getOnTabGroupCreationRunnable() {
        return null;
    }

    @Override
    protected void tryToTriggerOnShownIphs() {}

    @Override
    public void openInvitationModal(String invitationId) {
        assert false : "Not reached.";
    }

    @Override
    public boolean requestOpenTabGroupDialog(int tabId) {
        assert false : "Not reached.";
        return false;
    }

    private IncognitoTabModel getIncognitoTabModel() {
        if (!mIsNativeInitialized) return null;

        TabGroupModelFilter incognitoTabGroupModelFilter =
                mIncognitoTabGroupModelFilterSupplier.get();
        assert incognitoTabGroupModelFilter != null;
        return (IncognitoTabModel) incognitoTabGroupModelFilter.getTabModel();
    }

    private void setNewTabButtonEnabledState(boolean enabled) {
        mNewTabButtonDataSupplier.set(
                enabled ? mEnabledNewTabButtonData : mDisabledNewTabButtonData);
    }

    private IncognitoTabSwitcherPaneCleaner initIncognitoTabSwitcherPaneCleanup() {
        TabSwitcherPaneCoordinator paneCoordinator = getTabSwitcherPaneCoordinator();
        assert paneCoordinator != null;

        Supplier<Integer> recentlySwipedTabIdSupplier =
                paneCoordinator.getRecentlySwipedTabIdSupplier();
        boolean wasFinalTabSwiped =
                recentlySwipedTabIdSupplier.get() != null
                        && recentlySwipedTabIdSupplier.get() != Tab.INVALID_TAB_ID
                        && recentlySwipedTabIdSupplier.get() == mLastClosedTabId;

        @Nullable
        ObservableSupplier<Boolean> isAnimatingSupplier =
                paneCoordinator.getIsRecyclerViewAnimatorRunning();
        @Nullable
        ObservableSupplier<Boolean> dialogShowingOrAnimationSupplier =
                paneCoordinator.getTabGridDialogShowingOrAnimationSupplier();

        Runnable cleanUpRunnable =
                () -> {
                    destroyTabSwitcherPaneCoordinator();
                    mLastClosedTabId = Tab.INVALID_TAB_ID;
                };

        return new IncognitoTabSwitcherPaneCleaner(
                isAnimatingSupplier,
                dialogShowingOrAnimationSupplier,
                mReferenceButtonDataSupplier,
                cleanUpRunnable,
                getPaneHubController(),
                isFocused(),
                wasFinalTabSwiped);
    }

    /**
     * A helper class to manage the cleanup of the Incognito Tab Switcher pane. This class ensures
     * that the pane clean up is coordinated after the animations and dialogs associated with it
     * have finished.
     */
    private static class IncognitoTabSwitcherPaneCleaner {
        @Nullable private final ObservableSupplier<Boolean> mIsAnimatingSupplier;
        @Nullable private final ObservableSupplier<Boolean> mDialogShowingOrAnimationSupplier;
        private final Callback<Boolean> mOnAnimationStatusChange = this::onAnimationStatusChange;
        private final ObservableSupplierImpl<DisplayButtonData> mReferenceButtonDataSupplier;
        private final Runnable mCleanUpRunnable;
        private final @Nullable PaneHubController mController;
        private final boolean mIsFocused;
        private final boolean mWasFinalTabSwiped;
        private boolean mStartedAnimating;
        private boolean mForceCleanup;

        /**
         * @param isAnimatingSupplier Provides the animation status.
         * @param dialogShowingOrAnimationSupplier Provides the visibility of the tab group dialog.
         * @param referenceButtonDataSupplier Provides the reference button data.
         * @param cleanUpRunnable Runnable to run when cleanup should occur.
         * @param controller The controller to focus hub panes.
         * @param isFocused Whether the pane is focused.
         * @param wasFinalTabSwiped Whether the final tab was swiped close.
         */
        public IncognitoTabSwitcherPaneCleaner(
                @Nullable ObservableSupplier<Boolean> isAnimatingSupplier,
                @Nullable ObservableSupplier<Boolean> dialogShowingOrAnimationSupplier,
                ObservableSupplierImpl<DisplayButtonData> referenceButtonDataSupplier,
                Runnable cleanUpRunnable,
                @Nullable PaneHubController controller,
                boolean isFocused,
                boolean wasFinalTabSwiped) {
            mIsAnimatingSupplier = isAnimatingSupplier;
            mDialogShowingOrAnimationSupplier = dialogShowingOrAnimationSupplier;
            mReferenceButtonDataSupplier = referenceButtonDataSupplier;
            mCleanUpRunnable = cleanUpRunnable;
            mController = controller;
            mIsFocused = isFocused;
            mWasFinalTabSwiped = wasFinalTabSwiped;
        }

        /**
         * Coordinates the cleanup process. Observes various attributes to determine when it is safe
         * to clean up the pane.
         */
        void coordinateCleanUp() {
            // In case the isAnimatingSupplier is null, we can force a cleanup.
            if (mIsAnimatingSupplier != null) {
                mIsAnimatingSupplier.addObserver(mOnAnimationStatusChange);
            } else {
                mForceCleanup = true;
                mOnAnimationStatusChange.onResult(false);
            }
        }

        /** Determines whether to clean up upon the animation status changing. */
        private void onAnimationStatusChange(Boolean isAnimating) {
            boolean isTabGridDialogVisible =
                    mDialogShowingOrAnimationSupplier != null
                            && mDialogShowingOrAnimationSupplier.get();

            if (shouldNotCleanup(isAnimating, isTabGridDialogVisible)) {
                mStartedAnimating = isAnimating;
                return;
            }
            cleanUp();
            if (mIsAnimatingSupplier != null) {
                mIsAnimatingSupplier.removeObserver(mOnAnimationStatusChange);
            }
        }

        /**
         * This ensures that we delay the clean up iff we have started any final animation prior to
         * changing tab switcher panes.
         *
         * <p>This also means the clean up doesn't wait for animations:
         *
         * <p>a) When the tab grid dialog is visible.
         *
         * <p>b) When the final tab is swiped.
         *
         * @param isAnimating Whether {@link TabListItemAnimator} is currently running.
         * @param isTabGridDialogVisible Whether the tab group dialog is visible.
         */
        private boolean shouldNotCleanup(Boolean isAnimating, boolean isTabGridDialogVisible) {
            return !mForceCleanup
                    && !mWasFinalTabSwiped
                    && !isTabGridDialogVisible
                    && (isAnimating || !mStartedAnimating);
        }

        /** Performs the cleanup of the Incognito Tab Switcher pane. */
        private void cleanUp() {
            mReferenceButtonDataSupplier.set(null);
            if (mIsFocused) {
                assert mController != null : "isFocused requires a non-null PaneHubController.";
                mController.focusPane(PaneId.TAB_SWITCHER);
            }
            mCleanUpRunnable.run();
        }
    }
}
