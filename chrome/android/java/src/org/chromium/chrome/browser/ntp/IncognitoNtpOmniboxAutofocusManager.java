// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.ui.UiUtils;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.url.GURL;

import java.util.HashSet;
import java.util.Set;
import java.util.function.Function;

/**
 * Manages autofocusing the omnibox on new Incognito Tabs showing the New Tab Page (NTP).
 *
 * <p>This class observes tab and layout state changes to determine the precise moment to focus the
 * omnibox. This manager waits until the tab has loaded the NTP, the tab is currently selected, and
 * the browser layout has fully transitioned to the browsing state.
 */
@NullMarked
public class IncognitoNtpOmniboxAutofocusManager {
    private static @Nullable IncognitoNtpOmniboxAutofocusManager sInstanceForTesting;
    private final Set<Tab> mProcessedTabs = new HashSet<>();
    private final Set<Tab> mTabsPendingAutofocus = new HashSet<>();
    private final OmniboxStub mOmniboxStub;
    private final TabModelSelector mTabModelSelector;
    private @Nullable TabObserver mTabObserver;
    private @Nullable TabModelObserver mTabModelObserver;
    private final LayoutManagerImpl mLayoutManager;
    private @Nullable LayoutStateObserver mLayoutStateObserver;
    private final Function<Tab, @Nullable View> mNtpViewProvider;
    private final Function<View, IncognitoNtpUtils.IncognitoNtpContentMetrics>
            mNtpContentMetricsProvider;
    private @Nullable UrlFocusChangeListener mUrlFocusChangeListener;
    private final AccessibilityState.Listener mAccessibilityStateListener;

    /** Whether the autofocus feature is generally enabled and its observers are active. */
    private boolean mIsAutofocusEnabled;

    /** Enable autofocus when it's not the first tab seen in this session. */
    private final boolean mIsAutofocusOnNotFirstTabEnabled;

    /**
     * Enable autofocus only when we predict the keyboard will not hide more than approved portion
     * of the NTP content.
     */
    private final boolean mIsWithPredictionEnabled;

    /** Enable autofocus when a hardware keyboard is available. */
    private final boolean mIsWithHardwareKeyboardEnabled;

    private boolean mIsLayoutInTransition;
    private int mTabsPreviouslyOpenedCount;
    private final @NonNull GestureDetector mNtpSingleTapDetector;
    private boolean mIsAutofocusing;
    private double mTabHeightBeforeFocus;

    /**
     * Overrides the result of {@link #checkAutofocusAllowedWithPrediction(Tab)} for testing
     * purposes.
     */
    public static @Nullable Boolean sAutofocusAllowedWithPredictionForTesting;

    /**
     * Overrides the result of {@link #checkAutofocusAllowedWithHardwareKeyboard()} for testing
     * purposes.
     */
    private static @Nullable Boolean sIsHardwareKeyboardAttachedForTesting;

    private static final int AUTOFOCUS_PREDICTION_AVERAGE_KEYBOARD_HEIGHT_PERCENT = 42;
    private static final int AUTOFOCUS_PREDICTION_MAX_NTP_TEXT_HIDDEN_PERCENT = 25;
    private static final int AUTOFOCUS_PREDICTION_FREE_SPACE_THRESHOLD_PERCENT =
            AUTOFOCUS_PREDICTION_AVERAGE_KEYBOARD_HEIGHT_PERCENT
                    - AUTOFOCUS_PREDICTION_MAX_NTP_TEXT_HIDDEN_PERCENT;

    /**
     * Creates an instance if {@link ChromeFeatureList#sOmniboxAutofocusOnIncognitoNtp} is enabled.
     *
     * @param omniboxStub The {@link OmniboxStub} to use for focusing the omnibox.
     * @param layoutManager The {@link LayoutManagerImpl} to observe for layout changes.
     * @param tabModelSelector The {@link TabModelSelector} to observe.
     * @param ntpViewProvider Provides the NTP view.
     * @param ntpContentMetricsProvider Provides metrics of the main text content area on the
     *     Incognito NTP.
     */
    public static @Nullable IncognitoNtpOmniboxAutofocusManager maybeCreate(
            @NonNull Context context,
            @Nullable OmniboxStub omniboxStub,
            @NonNull LayoutManagerImpl layoutManager,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull Function<Tab, @Nullable View> ntpViewProvider,
            @NonNull
                    Function<View, IncognitoNtpUtils.IncognitoNtpContentMetrics>
                            ntpContentMetricsProvider) {
        if (ChromeFeatureList.sOmniboxAutofocusOnIncognitoNtp.isEnabled() && omniboxStub != null) {
            return new IncognitoNtpOmniboxAutofocusManager(
                    context,
                    omniboxStub,
                    layoutManager,
                    tabModelSelector,
                    ntpViewProvider,
                    ntpContentMetricsProvider);
        }
        return null;
    }

    private IncognitoNtpOmniboxAutofocusManager(
            @NonNull Context context,
            @NonNull OmniboxStub omniboxStub,
            @NonNull LayoutManagerImpl layoutManager,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull Function<Tab, @Nullable View> ntpViewProvider,
            @NonNull
                    Function<View, IncognitoNtpUtils.IncognitoNtpContentMetrics>
                            ntpContentMetricsProvider) {
        sInstanceForTesting = this;
        mOmniboxStub = omniboxStub;
        mTabModelSelector = tabModelSelector;
        mLayoutManager = layoutManager;
        mNtpViewProvider = ntpViewProvider;
        mNtpContentMetricsProvider = ntpContentMetricsProvider;
        mTabsPreviouslyOpenedCount = 0;
        mNtpSingleTapDetector =
                new GestureDetector(
                        context,
                        new GestureDetector.SimpleOnGestureListener() {
                            @Override
                            public boolean onSingleTapConfirmed(MotionEvent e) {
                                mOmniboxStub.setUrlBarFocus(
                                        false,
                                        null,
                                        OmniboxFocusReason.UNFOCUS,
                                        AutocompleteRequestType.SEARCH);
                                return false;
                            }
                        });

        mIsAutofocusOnNotFirstTabEnabled =
                ChromeFeatureList.sOmniboxAutofocusOnIncognitoNtpNotFirstTab.getValue();
        mIsWithPredictionEnabled =
                ChromeFeatureList.sOmniboxAutofocusOnIncognitoNtpWithPrediction.getValue();
        mIsWithHardwareKeyboardEnabled =
                ChromeFeatureList.sOmniboxAutofocusOnIncognitoNtpWithHardwareKeyboard.getValue();

        mAccessibilityStateListener =
                (oldState, newState) -> {
                    // Autofocus is disabled when accessibility is enabled to avoid interfering with
                    // screen readers.
                    updateAutofocusEnabledState(
                            /* enabled= */ !AccessibilityState.isAccessibilityEnabled());
                };
        AccessibilityState.addListener(mAccessibilityStateListener);
        updateAutofocusEnabledState(/* enabled= */ !AccessibilityState.isAccessibilityEnabled());
    }

    private void registerObservers() {
        if (mIsAutofocusEnabled) return;
        mIsAutofocusEnabled = true;

        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        handlePageLoadFinished(tab);
                    }
                };

        mUrlFocusChangeListener =
                new UrlFocusChangeListener() {
                    @Override
                    public void onUrlFocusChange(boolean hasFocus) {
                        final Tab tab = mTabModelSelector.getCurrentTab();

                        if (tab == null
                                || !tab.isIncognitoBranded()
                                || !UrlUtilities.isNtpUrl(tab.getUrl())
                                || tab.getView() == null) {
                            return;
                        }

                        View ntpView = mNtpViewProvider.apply(tab);
                        if (ntpView == null) {
                            return;
                        }

                        if (hasFocus) {
                            boolean wasTriggeredByAutofocus = mIsAutofocusing;
                            mIsAutofocusing = false;

                            final IncognitoNtpUtils.IncognitoNtpContentMetrics ntpMetrics =
                                    mNtpContentMetricsProvider.apply(ntpView);
                            IncognitoNtpOmniboxAutofocusTracker
                                    .collectLayoutMetricsOnKeyboardVisible(
                                            tab,
                                            mTabHeightBeforeFocus,
                                            ntpMetrics,
                                            wasTriggeredByAutofocus);
                            mTabHeightBeforeFocus = 0;

                            ntpView.setOnTouchListener(
                                    (v, event) -> {
                                        boolean consumed =
                                                mNtpSingleTapDetector.onTouchEvent(event);
                                        if (event.getAction() == MotionEvent.ACTION_UP
                                                && !consumed) {
                                            v.performClick();
                                        }
                                        return true;
                                    });
                        } else {
                            ntpView.setOnTouchListener(null);
                        }
                    }

                    @Override
                    public void onUrlFocusWillBeRequested(@Nullable Tab tab) {
                        if (tab != null
                                && tab.isIncognitoBranded()
                                && UrlUtilities.isNtpUrl(tab.getUrl())
                                && tab.getView() != null) {
                            mTabHeightBeforeFocus = tab.getView().getHeight();
                        }
                    }
                };

        mLayoutStateObserver =
                new LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(int layoutType) {
                        mIsLayoutInTransition = true;
                    }

                    @Override
                    public void onFinishedShowing(int layoutType) {
                        mIsLayoutInTransition = false;

                        @Nullable Tab tab = mTabModelSelector.getCurrentTab();
                        if (tab != null) {
                            tryAutofocus(tab);
                        }
                    }

                    @Override
                    public void onStartedHiding(int layoutType) {
                        mIsLayoutInTransition = true;
                    }

                    @Override
                    public void onFinishedHiding(int layoutType) {
                        mIsLayoutInTransition = false;
                    }
                };
        mOmniboxStub.addUrlFocusChangeListener(mUrlFocusChangeListener);
        mLayoutManager.addObserver(mLayoutStateObserver);

        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        if (!tab.isIncognitoBranded() || mTabObserver == null) return;
                        ++mTabsPreviouslyOpenedCount;
                        tab.addObserver(mTabObserver);
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        if (!tab.isIncognitoBranded() || mTabObserver == null) return;
                        tab.removeObserver(mTabObserver);
                        mProcessedTabs.remove(tab);
                        mTabsPendingAutofocus.remove(tab);
                    }
                };
        for (TabModel model : mTabModelSelector.getModels()) {
            if (model.isIncognitoBranded()) {
                // Handle already existing Incognito tabs, (e.g. that were added before observers
                // were registered).
                for (int i = 0; i < model.getCount(); i++) {
                    Tab tab = model.getTabAt(i);
                    if (tab == null) continue;

                    ++mTabsPreviouslyOpenedCount;
                    tab.addObserver(mTabObserver);

                    // Handle already loaded NTPs.
                    View ntpView = mNtpViewProvider.apply(tab);
                    if (!tab.isLoading() && ntpView != null) {
                        handlePageLoadFinished(tab);
                    }
                }
                model.addObserver(mTabModelObserver);
            }
        }
    }

    private void unregisterObservers() {
        if (!mIsAutofocusEnabled) return;
        mIsAutofocusEnabled = false;

        if (mUrlFocusChangeListener != null) {
            mOmniboxStub.removeUrlFocusChangeListener(mUrlFocusChangeListener);
            mUrlFocusChangeListener = null;
        }
        if (mLayoutStateObserver != null) {
            mLayoutManager.removeObserver(mLayoutStateObserver);
            mLayoutStateObserver = null;
        }
        if (mTabModelObserver != null) {
            for (TabModel model : mTabModelSelector.getModels()) {
                if (model.isIncognitoBranded()) {
                    model.removeObserver(mTabModelObserver);
                }
            }
            mTabModelObserver = null;
        }
        mTabObserver = null;
        mProcessedTabs.clear();
        mTabsPendingAutofocus.clear();
    }

    /** Destroy the instance and unregister observers. */
    public void destroy() {
        unregisterObservers();
    }

    /**
     * Enables or disables the autofocus functionality by registering or unregistering observers.
     *
     * @param enabled True to enable the feature and register observers, false otherwise.
     */
    private void updateAutofocusEnabledState(boolean enabled) {
        if (enabled) {
            registerObservers();
        } else {
            unregisterObservers();
        }
    }

    /**
     * Called when a page load finished. If the tab is an Incognito NTP and was not previously
     * processed, it's marked as pending autofocus.
     */
    private void handlePageLoadFinished(Tab tab) {
        // Check if autofocus has already been processed for this Incognito tab to ensure it only
        // runs once.
        if (!tab.isIncognitoBranded()
                || mProcessedTabs.contains(tab)
                || mTabsPendingAutofocus.contains(tab)) {
            return;
        }

        if (UrlUtilities.isNtpUrl(tab.getUrl())) {
            mTabsPendingAutofocus.add(tab);
            tryAutofocus(tab);
        } else {
            // Mark the tab as processed because it was not opened as NTP first.
            markTabAsProcessed(tab);
        }
    }

    /**
     * Attempts to autofocus the omnibox if all conditions are met: the current tab is an Incognito
     * NTP pending focus, and the UI is in a non-transitioning browsing state.
     *
     * @param tab The tab to potentially autofocus.
     */
    private void tryAutofocus(Tab tab) {
        if (tab.getView() == null
                || !tab.isIncognitoBranded()
                || !mTabsPendingAutofocus.contains(tab)) {
            return;
        }

        // The tab must be the currently selected tab.
        if (mTabModelSelector.getCurrentTab() != tab) {
            return;
        }

        // The layout must be the main browser view and not in the middle of a transition
        if (mLayoutManager.getActiveLayoutType() != LayoutType.BROWSING || mIsLayoutInTransition) {
            return;
        }

        tab.getView()
                .post(
                        () -> {
                            boolean noConditionsConfigured =
                                    !mIsAutofocusOnNotFirstTabEnabled
                                            && !mIsWithPredictionEnabled
                                            && !mIsWithHardwareKeyboardEnabled;
                            boolean isAutofocusAllowedNotFirstTab =
                                    mIsAutofocusOnNotFirstTabEnabled
                                            && checkAutofocusAllowedNotFirstTab();
                            boolean isAutofocusAllowedWithPrediction =
                                    mIsWithPredictionEnabled
                                            && checkAutofocusAllowedWithPrediction(tab);
                            boolean isAutofocusAllowedWithHardwareKeyboard =
                                    mIsWithHardwareKeyboardEnabled
                                            && checkAutofocusAllowedWithHardwareKeyboard();

                            // If no specific conditions are configured, autofocus is triggered
                            // unconditionally.
                            // If one or more conditions are configured, autofocus is triggered if
                            // any of them are met.
                            if (noConditionsConfigured
                                    || isAutofocusAllowedNotFirstTab
                                    || isAutofocusAllowedWithPrediction
                                    || isAutofocusAllowedWithHardwareKeyboard) {
                                autofocus(tab);
                            }
                        });
    }

    /** Performs the actual focus action and updates the state. */
    private void autofocus(Tab tab) {
        mIsAutofocusing = true;
        mOmniboxStub.setUrlBarFocus(
                true, null, OmniboxFocusReason.OMNIBOX_TAP, AutocompleteRequestType.SEARCH);

        // Mark the tab as processed to prevent future autofocus attempts.
        markTabAsProcessed(tab);
    }

    /**
     * Checks if autofocus is allowed for a tab that is not the very first Incognito tab opened in
     * the current session.
     */
    private boolean checkAutofocusAllowedNotFirstTab() {
        return mTabsPreviouslyOpenedCount > 1;
    }

    /** Checks if autofocus is allowed when a hardware keyboard is connected. */
    private boolean checkAutofocusAllowedWithHardwareKeyboard() {
        if (sIsHardwareKeyboardAttachedForTesting != null) {
            return sIsHardwareKeyboardAttachedForTesting;
        }
        return UiUtils.isHardwareKeyboardAttached();
    }

    /**
     * Checks if autofocus is allowed based on the predicted available free space on the Incognito
     * tab.
     */
    private boolean checkAutofocusAllowedWithPrediction(Tab tab) {
        if (sAutofocusAllowedWithPredictionForTesting != null) {
            return sAutofocusAllowedWithPredictionForTesting;
        }

        if (tab.getView() == null) {
            return false;
        }

        View ntpView = mNtpViewProvider.apply(tab);
        if (ntpView != null) {
            final double tabViewHeight = tab.getView().getHeight();
            final IncognitoNtpUtils.IncognitoNtpContentMetrics ntpMetrics =
                    mNtpContentMetricsProvider.apply(ntpView);
            final double ntpTextContentHeightWithTopPadding =
                    ntpMetrics.textContentHeightPx + ntpMetrics.textContentTopPaddingPx;

            final double freeSpacePercentage =
                    (tabViewHeight - ntpTextContentHeightWithTopPadding) / tabViewHeight * 100d;

            if (freeSpacePercentage >= AUTOFOCUS_PREDICTION_FREE_SPACE_THRESHOLD_PERCENT) {
                return true;
            }
        }

        return false;
    }

    /**
     * Marks a tab as processed to prevent future autofocus attempts. This involves adding it to the
     * processed set, removing it from the pending set, and unregistering its observer.
     *
     * @param tab The tab to mark as processed.
     */
    private void markTabAsProcessed(Tab tab) {
        mProcessedTabs.add(tab);
        mTabsPendingAutofocus.remove(tab);
        if (mTabObserver != null) {
            tab.removeObserver(mTabObserver);
        }
    }

    public static void setAutofocusEnabledForTesting(boolean enabled) {
        if (sInstanceForTesting != null) {
            sInstanceForTesting.updateAutofocusEnabledState(enabled);
        }
    }

    public static void setIsHardwareKeyboardAttachedForTesting(Boolean isAttached) {
        Boolean oldValue = sIsHardwareKeyboardAttachedForTesting;
        sIsHardwareKeyboardAttachedForTesting = isAttached;
        ResettersForTesting.register(() -> sIsHardwareKeyboardAttachedForTesting = oldValue);
    }
}
