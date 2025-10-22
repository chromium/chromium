// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.os.Build;
import android.view.View.OnClickListener;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.sensitive_content.SensitiveContentFeatures;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.function.DoubleConsumer;
import java.util.function.Supplier;

/** A {@link Pane} representing the incognito tab switcher. */
@NullMarked
public class IncognitoTabSwitcherPane extends TabSwitcherPaneBase {
    /** The means through which the tab was closed. */
    @IntDef({
        TabCloseMethod.SWIPE,
        TabCloseMethod.TAB_LIST_EDITOR,
        TabCloseMethod.TAB_GRID_DIALOG,
        TabCloseMethod.CLOSED_WHILE_REAUTH_VISIBLE,
        TabCloseMethod.OTHER,
    })
    @Retention(RetentionPolicy.CLASS)
    private @interface TabCloseMethod {
        int SWIPE = 0;
        int TAB_LIST_EDITOR = 1;
        int TAB_GRID_DIALOG = 2;
        int CLOSED_WHILE_REAUTH_VISIBLE = 3;
        int OTHER = 4;
    }

    private final IncognitoTabModelObserver mIncognitoTabModelObserver =
            new IncognitoTabModelObserver() {
                @Override
                public void wasFirstTabCreated() {
                    mReferenceButtonDataSupplier.set(mReferenceButtonData);
                }

                @Override
                public void didBecomeEmpty() {
                    runIncognitoTabSwitcherPaneCleanup();
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
                            || !incognitoTabGroupModelFilter.getTabModel().isActiveModel()) {
                        return;
                    }

                    coordinator.resetWithListOfTabs(
                            incognitoTabGroupModelFilter.getRepresentativeTabList());
                    coordinator.setInitialScrollIndexOffset();
                    coordinator.requestAccessibilityFocusOnCurrentTab();

                    setNewTabButtonEnabledState(/* enabled= */ true);

                    mHubSearchEnabledStateSupplier.set(true);
                }

                @Override
                public void onIncognitoReauthFailure() {}
            };

    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void onFinishingTabClosure(Tab tab, @TabClosingSource int closingSource) {
                    mLastClosedTabId = tab.getId();
                }
            };

    /** Not safe to use until initWithNative. */
    private final Supplier<TabGroupModelFilter> mIncognitoTabGroupModelFilterSupplier;

    private final ResourceButtonData mReferenceButtonData;
    private final FullButtonData mEnabledNewTabButtonData;
    private final FullButtonData mDisabledNewTabButtonData;
    private final ObservableSupplierImpl<Boolean> mHubSearchEnabledStateSupplier =
            new ObservableSupplierImpl<>();

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
     * @param tabGroupCreationUiDelegate Orchestrates the tab group creation UI flow.
     * @param xrSpaceModeObservableSupplier Supplies current XR space mode status. True for XR full
     *     space mode, false otherwise.
     */
    IncognitoTabSwitcherPane(
            Context context,
            TabSwitcherPaneCoordinatorFactory factory,
            Supplier<TabGroupModelFilter> incognitoTabGroupModelFilterSupplier,
            OnClickListener newTabButtonClickListener,
            @Nullable OneshotSupplier<IncognitoReauthController> incognitoReauthControllerSupplier,
            DoubleConsumer onToolbarAlphaChange,
            UserEducationHelper userEducationHelper,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            ObservableSupplier<CompositorViewHolder> compositorViewHolderSupplier,
            TabGroupCreationUiDelegate tabGroupCreationUiDelegate,
            @Nullable ObservableSupplier<Boolean> xrSpaceModeObservableSupplier) {
        super(
                context,
                factory,
                /* isIncognito= */ true,
                onToolbarAlphaChange,
                userEducationHelper,
                edgeToEdgeSupplier,
                compositorViewHolderSupplier,
                tabGroupCreationUiDelegate,
                xrSpaceModeObservableSupplier);

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
        assumeNonNull(incognitoTabModel);
        incognitoTabModel.addIncognitoObserver(mIncognitoTabModelObserver);
        incognitoTabModel.addObserver(mTabModelObserver);
        if (incognitoTabModel.getCount() > 0) {
            mIncognitoTabModelObserver.wasFirstTabCreated();
        }
    }

    @Override
    public void showAllTabs() {
        resetWithListOfTabs(mIncognitoTabGroupModelFilterSupplier.get().getRepresentativeTabList());
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
    public void resetWithListOfTabs(@Nullable List<Tab> tabs) {
        @Nullable TabSwitcherPaneCoordinator coordinator = getTabSwitcherPaneCoordinator();
        if (coordinator == null) return;

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
            return;
        }

        boolean isNotVisibleOrSelected =
                !getIsVisibleSupplier().get() || !filter.getTabModel().isActiveModel();
        boolean incognitoReauthShowing = isIncognitoReauthPending();

        if (isNotVisibleOrSelected || incognitoReauthShowing) {
            coordinator.resetWithListOfTabs(null);
            cancelWaitForTabStateInitializedTimer();

            if (incognitoReauthShowing) {
                mHubSearchEnabledStateSupplier.set(false);
            }
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
            coordinator.resetWithListOfTabs(tabs);
            finishWaitForTabStateInitializedTimer();
        }

        setNewTabButtonEnabledState(/* enabled= */ !incognitoReauthShowing);
    }

    @Override
    protected void requestAccessibilityFocusOnCurrentTab() {
        if (isIncognitoReauthShowing()) return;

        super.requestAccessibilityFocusOnCurrentTab();
    }

    @Override
    protected @Nullable Runnable getOnTabGroupCreationRunnable() {
        return null;
    }

    @Override
    protected void tryToTriggerOnShownIphs() {}

    private @Nullable IncognitoTabModel getIncognitoTabModel() {
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

    private void runIncognitoTabSwitcherPaneCleanup() {
        TabSwitcherPaneCoordinator paneCoordinator = getTabSwitcherPaneCoordinator();
        if (paneCoordinator == null) {
            IncognitoTabSwitcherPaneCleaner.cleanUp(
                    mReferenceButtonDataSupplier, isFocused(), getPaneHubController(), null);
            return;
        }

        IncognitoTabSwitcherPaneCleaner cleaner =
                initIncognitoTabSwitcherPaneCleaner(paneCoordinator);
        cleaner.coordinateCleanUp();
    }

    private IncognitoTabSwitcherPaneCleaner initIncognitoTabSwitcherPaneCleaner(
            TabSwitcherPaneCoordinator paneCoordinator) {
        @TabCloseMethod int finalTabCloseMethod = getFinalTabCloseMethod(paneCoordinator);
        ObservableSupplier<Boolean> isAnimatingSupplier =
                paneCoordinator.getIsRecyclerViewAnimatorRunning();

        Runnable cleanUpRunnable =
                () -> {
                    destroyTabSwitcherPaneCoordinator();
                    mLastClosedTabId = Tab.INVALID_TAB_ID;
                };

        return new IncognitoTabSwitcherPaneCleaner(
                isAnimatingSupplier,
                mReferenceButtonDataSupplier,
                cleanUpRunnable,
                getPaneHubController(),
                isFocused(),
                finalTabCloseMethod);
    }

    private @TabCloseMethod int getFinalTabCloseMethod(TabSwitcherPaneCoordinator paneCoordinator) {
        Supplier<Integer> recentlySwipedTabIdSupplier =
                paneCoordinator.getRecentlySwipedTabIdSupplier();
        @Nullable
        Supplier<Boolean> dialogShowingOrAnimationSupplier =
                paneCoordinator.getTabGridDialogShowingOrAnimationSupplier();
        boolean wasClosedViaSwipe = wasFinalTabSwiped(recentlySwipedTabIdSupplier);

        // We can tell if the final tab was closed via the Tab List Editor by checking to see if
        // the Tab List Editor requires a clean up, which is not complete until after we initialize
        // our clean up sequence.
        boolean wasClosedViaTabListEditor = paneCoordinator.doesTabListEditorNeedCleanup();

        boolean isTabGridDialogVisible =
                dialogShowingOrAnimationSupplier != null && dialogShowingOrAnimationSupplier.get();

        if (isIncognitoReauthShowing()) {
            return TabCloseMethod.CLOSED_WHILE_REAUTH_VISIBLE;
        } else if (wasClosedViaSwipe) {
            return TabCloseMethod.SWIPE;
        } else if (wasClosedViaTabListEditor) {
            return TabCloseMethod.TAB_LIST_EDITOR;
        } else if (isTabGridDialogVisible) {
            return TabCloseMethod.TAB_GRID_DIALOG;
        } else {
            return TabCloseMethod.OTHER;
        }
    }

    /** Returns whether the final tab was swiped close. */
    private boolean wasFinalTabSwiped(Supplier<Integer> recentlySwipedTabIdSupplier) {
        return recentlySwipedTabIdSupplier.get() != null
                && recentlySwipedTabIdSupplier.get() != Tab.INVALID_TAB_ID
                && recentlySwipedTabIdSupplier.get() == mLastClosedTabId;
    }

    /**
     * A helper class to manage the cleanup of the Incognito Tab Switcher pane. This class ensures
     * that the pane clean up is coordinated after the animations and dialogs associated with it
     * have finished.
     */
    private static class IncognitoTabSwitcherPaneCleaner {
        private final @Nullable ObservableSupplier<Boolean> mIsAnimatingSupplier;
        private final Callback<Boolean> mOnAnimationStatusChange = this::onAnimationStatusChange;
        private final ObservableSupplierImpl<@Nullable DisplayButtonData>
                mReferenceButtonDataSupplier;
        private final Runnable mCleanUpRunnable;
        private final @Nullable PaneHubController mController;
        private final boolean mIsFocused;
        private final @TabCloseMethod int mFinalTabCloseMethod;
        private boolean mStartedAnimating;
        private boolean mForceCleanup;

        /**
         * @param isAnimatingSupplier Provides the animation status.
         * @param referenceButtonDataSupplier Provides the reference button data.
         * @param cleanUpRunnable Runnable to run when cleanup should occur.
         * @param controller The controller to focus hub panes.
         * @param isFocused Whether the pane is focused.
         * @param finalTabCloseMethod How the final tab was closed.
         */
        public IncognitoTabSwitcherPaneCleaner(
                @Nullable ObservableSupplier<Boolean> isAnimatingSupplier,
                ObservableSupplierImpl<@Nullable DisplayButtonData> referenceButtonDataSupplier,
                Runnable cleanUpRunnable,
                @Nullable PaneHubController controller,
                boolean isFocused,
                @TabCloseMethod int finalTabCloseMethod) {
            mIsAnimatingSupplier = isAnimatingSupplier;
            mReferenceButtonDataSupplier = referenceButtonDataSupplier;
            mCleanUpRunnable = cleanUpRunnable;
            mController = controller;
            mIsFocused = isFocused;
            mFinalTabCloseMethod = finalTabCloseMethod;
        }

        /**
         * Coordinates the cleanup process. Observes various attributes to determine when it is safe
         * to clean up the pane.
         */
        void coordinateCleanUp() {
            // In case the isAnimatingSupplier is null and the pane is focused, we can force a
            // cleanup.
            if (shouldForceCleanUp()) {
                mForceCleanup = true;
                mOnAnimationStatusChange.onResult(false);
            } else {
                mIsAnimatingSupplier.addObserver(mOnAnimationStatusChange);
            }
        }

        /** Determines whether to clean up upon the animation status changing. */
        private void onAnimationStatusChange(Boolean isAnimating) {
            if (shouldNotCleanup(isAnimating)) {
                mStartedAnimating = isAnimating;
                return;
            }
            cleanUp(mReferenceButtonDataSupplier, mIsFocused, mController, mCleanUpRunnable);
            if (mIsAnimatingSupplier != null) {
                mIsAnimatingSupplier.removeObserver(mOnAnimationStatusChange);
            }
        }

        /**
         * Determines whether to force a clean up. Forcing a cleanup means we don't want to wait for
         * animations to complete.
         *
         * <p>Force cleanups when:
         *
         * <ul>
         *   <li>The tab switcher is not animating.
         *   <li>The incognito tab switcher is not focused.
         *   <li>The final tab was closed via the tab list editor.
         *   <li>The final tab was closed via a swipe.
         *   <li>The tab grid dialog is visible.
         *   <li>The incognito reauth screen is visible.
         * </ul>
         */
        @EnsuresNonNullIf(
                value = {"mIsAnimatingSupplier"},
                result = false)
        @SuppressWarnings("NullAway")
        private boolean shouldForceCleanUp() {
            return mIsAnimatingSupplier == null
                    || !mIsFocused
                    || mFinalTabCloseMethod != TabCloseMethod.OTHER;
        }

        /**
         * This ensures that we delay the clean up iff we have started any final animation prior to
         * changing tab switcher panes.
         *
         * @param isAnimating Whether {@link TabListItemAnimator} is currently running.
         */
        private boolean shouldNotCleanup(Boolean isAnimating) {
            return !mForceCleanup && (isAnimating || !mStartedAnimating);
        }

        /**
         * Performs the cleanup of the Incognito Tab Switcher pane.
         *
         * @param referenceButtonDataSupplier Provides the reference button data.
         * @param isFocused Whether the pane is focused.
         * @param controller The controller to focus hub panes.
         * @param cleanUpRunnable Runnable to run when cleanup should occur.
         */
        public static void cleanUp(
                ObservableSupplierImpl<@Nullable DisplayButtonData> referenceButtonDataSupplier,
                boolean isFocused,
                @Nullable PaneHubController controller,
                @Nullable Runnable cleanUpRunnable) {
            referenceButtonDataSupplier.set(null);
            if (isFocused) {
                assert controller != null : "isFocused requires a non-null PaneHubController.";
                controller.focusPane(PaneId.TAB_SWITCHER);
            }
            if (cleanUpRunnable != null) {
                cleanUpRunnable.run();
            }
        }
    }

    @Override
    public ObservableSupplier<Boolean> getHubSearchEnabledStateSupplier() {
        return mHubSearchEnabledStateSupplier;
    }

    /** Returns whether the incognito reauth screen is showing. */
    private boolean isIncognitoReauthShowing() {
        return mIncognitoReauthController != null
                && mIncognitoReauthController.isReauthPageShowing();
    }

    /** Returns whether the incognito reauth is pending the reauth screen may not be visible. */
    private boolean isIncognitoReauthPending() {
        return mIncognitoReauthController != null
                && mIncognitoReauthController.isIncognitoReauthPending();
    }
}
