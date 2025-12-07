// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_suggestion.toolbar.GroupSuggestionsButtonController;
import org.chromium.chrome.browser.tab_group_suggestion.toolbar.GroupSuggestionsButtonControllerFactory;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.segmentation_platform.Constants;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.segmentation_platform.ProcessedValue;

import java.util.HashMap;
import java.util.function.Supplier;

/**
 * Central class for contextual page actions bridging between UI and backend. Registers itself with
 * segmentation platform for on-demand model execution on page load triggers. Provides updated
 * button data to the toolbar when asked for it.
 */
@NullMarked
public class ContextualPageActionController {
    /**
     * The interface to be implemented by the individual feature backends to provide signals
     * necessary for the controller in an uniform manner.
     */
    public interface ActionProvider extends Destroyable {
        /**
         * Called during a page load to fetch the relevant signals from the action provider.
         *
         * @param tab The current tab for which the action would be shown.
         * @param signalAccumulator An accumulator into which the provider would populate relevant
         *     signals.
         */
        void getAction(Tab tab, SignalAccumulator signalAccumulator);

        /**
         * Called when any contextual page action is shown.
         *
         * @param tab The current tab for which the action was shown.
         * @param action Enum value of the action shown.
         */
        default void onActionShown(@Nullable Tab tab, @AdaptiveToolbarButtonVariant int action) {}

        @Override
        default void destroy() {}
    }

    private final ObservableSupplier<Profile> mProfileSupplier;
    private final ObservableSupplier<@Nullable Tab> mTabSupplier;
    private final AdaptiveToolbarButtonController mAdaptiveToolbarButtonController;
    private @Nullable CurrentTabObserver mCurrentTabObserver;
    private @Nullable SignalAccumulator mSignalAccumulator;
    private OneshotSupplier<Boolean> mButtonVisibilitySupplier;

    // The action provider backends.
    protected final HashMap<Integer, ActionProvider> mActionProviders = new HashMap<>();

    /**
     * Constructor.
     *
     * @param profileSupplier The supplier for current profile.
     * @param tabSupplier The supplier of the current tab.
     * @param adaptiveToolbarButtonController The {@link AdaptiveToolbarButtonController} that
     *     handles the logic to decide between multiple buttons to show.
     */
    public ContextualPageActionController(
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<@Nullable Tab> tabSupplier,
            AdaptiveToolbarButtonController adaptiveToolbarButtonController,
            Supplier<ShoppingService> shoppingServiceSupplier,
            Supplier<BookmarkModel> bookmarkModelSupplier) {
        mProfileSupplier = profileSupplier;
        mTabSupplier = tabSupplier;
        mAdaptiveToolbarButtonController = adaptiveToolbarButtonController;
        var defaultButtonVis = new OneshotSupplierImpl<Boolean>();
        defaultButtonVis.set(true);
        mButtonVisibilitySupplier = defaultButtonVis; // true by default for tabbed browser.
        profileSupplier.addObserver(
                profile -> {
                    if (profile.isOffTheRecord()) return;

                    // The profile supplier observer will be invoked every time the profile is
                    // changed.
                    // Ignore the subsequent calls since we are only interested in initializing tab
                    // observers once.
                    if (mCurrentTabObserver != null) return;
                    mAdaptiveToolbarButtonController.initializePageLoadMetricsRecorder(tabSupplier);

                    if (!AdaptiveToolbarFeatures.isContextualPageActionsEnabled()) return;

                    // TODO(shaktisahu): Observe the right method to handle tab switch, same-page
                    // navigations. Also handle chrome:// URLs if not already handled.
                    mCurrentTabObserver =
                            new CurrentTabObserver(
                                    tabSupplier,
                                    new EmptyTabObserver() {
                                        @Override
                                        public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                                            if (tab != null) maybeShowContextualPageAction();
                                        }
                                    },
                                    this::activeTabChanged);

                    initActionProviders(shoppingServiceSupplier, bookmarkModelSupplier);
                });
    }

    /**
     * Sets a boolean supplier that tells us if the contextual page action button is visible in the
     * UI, used to handle cases such as the button being hidden because of screen width or other
     * buttons.
     *
     * @param buttonVisibilitySupplier The boolean supplier of the button visibility.
     */
    public void setButtonVisibilitySupplier(OneshotSupplier<Boolean> buttonVisibilitySupplier) {
        mButtonVisibilitySupplier = buttonVisibilitySupplier;
    }

    @VisibleForTesting
    protected void initActionProviders(
            Supplier<ShoppingService> shoppingServiceSupplier,
            Supplier<BookmarkModel> bookmarkModelSupplier) {
        removeProviders();
        mActionProviders.put(
                AdaptiveToolbarButtonVariant.PRICE_TRACKING,
                new PriceTrackingActionProvider(shoppingServiceSupplier, bookmarkModelSupplier));
        mActionProviders.put(
                AdaptiveToolbarButtonVariant.READER_MODE,
                new ReaderModeActionProvider(mButtonVisibilitySupplier));
        mActionProviders.put(
                AdaptiveToolbarButtonVariant.PRICE_INSIGHTS,
                new PriceInsightsActionProvider(shoppingServiceSupplier));
        mActionProviders.put(
                AdaptiveToolbarButtonVariant.DISCOUNTS,
                new DiscountsActionProvider(shoppingServiceSupplier));

        if (AdaptiveToolbarFeatures.isTabGroupingPageActionEnabled()) {
            Supplier<@Nullable GroupSuggestionsButtonController>
                    groupSuggestionButtonControllerSupplier =
                            this::getGroupSuggestionsButtonController;
            mActionProviders.put(
                    AdaptiveToolbarButtonVariant.TAB_GROUPING,
                    new TabGroupingActionProvider(groupSuggestionButtonControllerSupplier));
        }
    }

    @Nullable
    private GroupSuggestionsButtonController getGroupSuggestionsButtonController() {
        Profile profile = mProfileSupplier.get();
        if (profile == null || profile.isOffTheRecord()) {
            return null;
        }
        return GroupSuggestionsButtonControllerFactory.getForProfile(profile);
    }

    /** Called on destroy. */
    public void destroy() {
        if (mCurrentTabObserver != null) {
            mCurrentTabObserver.destroy();
        }
        GroupSuggestionsButtonController groupSuggestionsButtonController =
                getGroupSuggestionsButtonController();
        if (groupSuggestionsButtonController != null) {
            groupSuggestionsButtonController.destroy();
        }
        removeProviders();
    }

    /**
     * @return Whether the page is price insights eligible. The eligibility represents the most
     *     recent price insights state, which could be from a previous page load or tab. Default is
     *     false.
     */
    public boolean hasPriceInsights() {
        return mSignalAccumulator == null
                ? false
                : mSignalAccumulator.getSignal(AdaptiveToolbarButtonVariant.PRICE_INSIGHTS);
    }

    public boolean hasReaderMode() {
        return mSignalAccumulator == null
                ? false
                : mSignalAccumulator.getSignal(AdaptiveToolbarButtonVariant.READER_MODE);
    }

    private void removeProviders() {
        for (ActionProvider provider : mActionProviders.values()) {
            provider.destroy();
        }
        mActionProviders.clear();
    }

    private void activeTabChanged(@Nullable Tab tab) {
        // If the tab is loading or if it's going to load later then we'll also get a call to
        // onPageLoadFinished.
        if (tab != null && !tab.isLoading() && !tab.isFrozen()) {
            maybeShowContextualPageAction();
        }
    }

    private void maybeShowContextualPageAction() {
        Tab tab = getValidActiveTab();
        if (tab == null) {
            // On incognito tabs revert back to static action.
            showDynamicAction(AdaptiveToolbarButtonVariant.UNKNOWN);
            return;
        }
        collectSignals(tab);
    }

    private void collectSignals(Tab tab) {
        if (mActionProviders.isEmpty()) return;
        mSignalAccumulator =
                new SignalAccumulator(new Handler(Looper.getMainLooper()), tab, mActionProviders);
        mSignalAccumulator.getSignals(this::findBestAction);
    }

    private void findBestAction() {
        Tab tab = getValidActiveTab();
        if (tab == null) return;
        InputContext inputContext = new InputContext();
        assumeNonNull(mSignalAccumulator);
        inputContext.addEntry(
                Constants.CONTEXTUAL_PAGE_ACTIONS_PRICE_TRACKING_INPUT,
                ProcessedValue.fromFloat(
                        mSignalAccumulator.getSignal(AdaptiveToolbarButtonVariant.PRICE_TRACKING)
                                ? 1.0f
                                : 0.0f));
        inputContext.addEntry(
                Constants.CONTEXTUAL_PAGE_ACTIONS_READER_MODE_INPUT,
                ProcessedValue.fromFloat(
                        mSignalAccumulator.getSignal(AdaptiveToolbarButtonVariant.READER_MODE)
                                ? 1.0f
                                : 0.0f));
        inputContext.addEntry(
                Constants.CONTEXTUAL_PAGE_ACTIONS_PRICE_INSIGHTS_INPUT,
                ProcessedValue.fromFloat(
                        mSignalAccumulator.getSignal(AdaptiveToolbarButtonVariant.PRICE_INSIGHTS)
                                ? 1.0f
                                : 0.0f));
        inputContext.addEntry(
                Constants.CONTEXTUAL_PAGE_ACTIONS_DISCOUNTS_INPUT,
                ProcessedValue.fromFloat(
                        mSignalAccumulator.getSignal(AdaptiveToolbarButtonVariant.DISCOUNTS)
                                ? 1.0f
                                : 0.0f));
        inputContext.addEntry(
                Constants.CONTEXTUAL_PAGE_ACTIONS_TAB_GROPING_INPUT,
                ProcessedValue.fromFloat(
                        mSignalAccumulator.getSignal(AdaptiveToolbarButtonVariant.TAB_GROUPING)
                                ? 1.0f
                                : 0.0f));
        inputContext.addEntry("url", ProcessedValue.fromGURL(tab.getUrl()));

        ContextualPageActionControllerJni.get()
                .computeContextualPageAction(
                        assertNonNull(mProfileSupplier.get()),
                        inputContext,
                        result -> {
                            if (tab.isDestroyed()) return;

                            boolean isSameTab =
                                    mTabSupplier.get() != null
                                            && mTabSupplier.get().getId() == tab.getId();
                            if (!isSameTab) return;

                            showDynamicAction(result);
                        });
    }

    private void showDynamicAction(@AdaptiveToolbarButtonVariant int action) {
        for (ActionProvider actionProvider : mActionProviders.values()) {
            actionProvider.onActionShown(mTabSupplier.get(), action);
        }

        // TODO(crbug.com/40242242): Add logic to inform reader mode backend.
        mAdaptiveToolbarButtonController.showDynamicAction(action);
    }

    /**
     * @return The active regular tab. Null for incognito.
     */
    private @Nullable Tab getValidActiveTab() {
        Profile profile = mProfileSupplier.get();
        if (profile == null || profile.isOffTheRecord()) {
            return null;
        }
        Tab tab = mTabSupplier.get();
        if (tab == null || tab.isIncognito() || tab.isDestroyed()) return null;
        return tab;
    }

    @NativeMethods
    interface Natives {
        void computeContextualPageAction(
                @JniType("Profile*") Profile profile,
                InputContext inputContext,
                Callback<Integer> callback);
    }
}
