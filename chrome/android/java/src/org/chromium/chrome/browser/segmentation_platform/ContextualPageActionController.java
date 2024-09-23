// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.segmentation_platform.Constants;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.segmentation_platform.ProcessedValue;

import java.util.ArrayList;
import java.util.List;

/**
 * Central class for contextual page actions bridging between UI and backend. Registers itself with
 * segmentation platform for on-demand model execution on page load triggers. Provides updated
 * button data to the toolbar when asked for it.
 */
public class ContextualPageActionController {
    /**
     * The interface to be implemented by the individual feature backends to provide signals
     * necessary for the controller in an uniform manner.
     */
    public interface ActionProvider {
        /**
         * Called during a page load to fetch the relevant signals from the action provider.
         * @param tab The current tab for which the action would be shown.
         * @param signalAccumulator An accumulator into which the provider would populate relevant
         *         signals.
         */
        void getAction(Tab tab, SignalAccumulator signalAccumulator);

        /**
         * Called when any contextual page action is shown.
         * @param tab The current tab for which the action was shown.
         * @param action Enum value of the action shown.
         */
        default void onActionShown(Tab tab, @AdaptiveToolbarButtonVariant int action) {}
    }

    private final ObservableSupplier<Profile> mProfileSupplier;
    private ObservableSupplier<Tab> mTabSupplier;
    private final AdaptiveToolbarButtonController mAdaptiveToolbarButtonController;
    private CurrentTabObserver mCurrentTabObserver;

    // The action provider backends.
    protected final List<ActionProvider> mActionProviders = new ArrayList<>();

    /**
     * Constructor.
     * @param profileSupplier The supplier for current profile.
     * @param tabSupplier The supplier of the current tab.
     * @param adaptiveToolbarButtonController The {@link AdaptiveToolbarButtonController} that
     *         handles the logic to decide between multiple buttons to show.
     */
    public ContextualPageActionController(
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<Tab> tabSupplier,
            AdaptiveToolbarButtonController adaptiveToolbarButtonController,
            Supplier<ShoppingService> shoppingServiceSupplier,
            Supplier<BookmarkModel> bookmarkModelSupplier) {
        mProfileSupplier = profileSupplier;
        mTabSupplier = tabSupplier;
        mAdaptiveToolbarButtonController = adaptiveToolbarButtonController;
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

    @VisibleForTesting
    protected void initActionProviders(
            Supplier<ShoppingService> shoppingServiceSupplier,
            Supplier<BookmarkModel> bookmarkModelSupplier) {
        mActionProviders.clear();
        mActionProviders.add(
                new PriceTrackingActionProvider(
                        shoppingServiceSupplier, bookmarkModelSupplier, mProfileSupplier));
        mActionProviders.add(new ReaderModeActionProvider());
        if (AdaptiveToolbarFeatures.isPriceInsightsPageActionEnabled()) {
            mActionProviders.add(new PriceInsightsActionProvider(shoppingServiceSupplier));
        }
        if (AdaptiveToolbarFeatures.isDiscountsPageActionEnabled()) {
            mActionProviders.add(new DiscountsActionProvider(shoppingServiceSupplier));
        }
    }

    /** Called on destroy. */
    public void destroy() {
        if (mCurrentTabObserver != null) mCurrentTabObserver.destroy();
    }

    private void activeTabChanged(Tab tab) {
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
        final SignalAccumulator signalAccumulator =
                new SignalAccumulator(new Handler(Looper.getMainLooper()), tab, mActionProviders);
        signalAccumulator.getSignals(() -> findBestAction(signalAccumulator));
    }

    private void findBestAction(SignalAccumulator signalAccumulator) {
        Tab tab = getValidActiveTab();
        if (tab == null) return;
        InputContext inputContext = new InputContext();
        inputContext.addEntry(
                Constants.CONTEXTUAL_PAGE_ACTIONS_PRICE_TRACKING_INPUT,
                ProcessedValue.fromFloat(signalAccumulator.hasPriceTracking() ? 1.0f : 0.0f));
        inputContext.addEntry(
                Constants.CONTEXTUAL_PAGE_ACTIONS_READER_MODE_INPUT,
                ProcessedValue.fromFloat(signalAccumulator.hasReaderMode() ? 1.0f : 0.0f));
        inputContext.addEntry(
                Constants.CONTEXTUAL_PAGE_ACTIONS_PRICE_INSIGHTS_INPUT,
                ProcessedValue.fromFloat(signalAccumulator.hasPriceInsights() ? 1.0f : 0.0f));
        inputContext.addEntry(
                Constants.CONTEXTUAL_PAGE_ACTIONS_DISCOUNTS_INPUT,
                ProcessedValue.fromFloat(signalAccumulator.hasDiscounts() ? 1.0f : 0.0f));
        inputContext.addEntry("url", ProcessedValue.fromGURL(tab.getUrl()));

        ContextualPageActionControllerJni.get()
                .computeContextualPageAction(
                        mProfileSupplier.get(),
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
        for (ActionProvider actionProvider : mActionProviders) {
            actionProvider.onActionShown(mTabSupplier.get(), action);
        }

        // TODO(crbug.com/40242242): Add logic to inform reader mode backend.
        mAdaptiveToolbarButtonController.showDynamicAction(action);
    }

    /** @return The active regular tab. Null for incognito. */
    private Tab getValidActiveTab() {
        if (mProfileSupplier == null || mProfileSupplier.get().isOffTheRecord()) return null;
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
