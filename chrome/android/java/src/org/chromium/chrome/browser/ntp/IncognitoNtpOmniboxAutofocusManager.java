// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.NonNull;

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
import org.chromium.ui.UiUtils;
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
public class IncognitoNtpOmniboxAutofocusManager implements TabModelObserver {
    private final Set<Tab> mProcessedTabs = new HashSet<>();
    private final Set<Tab> mTabsPendingAutofocus = new HashSet<>();
    private final OmniboxStub mOmniboxStub;
    private final TabModelSelector mTabModelSelector;
    private final TabObserver mTabObserver;
    private final LayoutManagerImpl mLayoutManager;
    private final LayoutStateObserver mLayoutStateObserver;
    private final Function<Tab, @Nullable View> mNtpViewProvider;
    private final Function<View, Double> mNtpContentHeightProvider;
    private final UrlFocusChangeListener mUrlFocusChangeListener;

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

    /**
     * Overrides the result of {@link #checkAutofocusAllowedWithPrediction(Tab)} for testing
     * purposes.
     */
    public static @Nullable Boolean sAutofocusAllowedWithPredictionForTesting;

    /**
     * Overrides the result of {@link #checkAutofocusAllowedWithHardwareKeyboard()} for testing
     * purposes.
     */
    public static @Nullable Boolean sIsHardwareKeyboardAttachedForTesting;

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
     * @param ntpContentHeightProvider Provides the height of the main text content area on the
     *     Incognito NTP, excluding all paddings after very last TextView.
     */
    public static @Nullable IncognitoNtpOmniboxAutofocusManager maybeCreate(
            @NonNull Context context,
            @Nullable OmniboxStub omniboxStub,
            @NonNull LayoutManagerImpl layoutManager,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull Function<Tab, @Nullable View> ntpViewProvider,
            @NonNull Function<View, Double> ntpContentHeightProvider) {
        if (ChromeFeatureList.sOmniboxAutofocusOnIncognitoNtp.isEnabled() && omniboxStub != null) {
            return new IncognitoNtpOmniboxAutofocusManager(
                    context,
                    omniboxStub,
                    layoutManager,
                    tabModelSelector,
                    ntpViewProvider,
                    ntpContentHeightProvider);
        }
        return null;
    }

    private IncognitoNtpOmniboxAutofocusManager(
            @NonNull Context context,
            @NonNull OmniboxStub omniboxStub,
            @NonNull LayoutManagerImpl layoutManager,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull Function<Tab, @Nullable View> ntpViewProvider,
            @NonNull Function<View, Double> ntpContentHeightProvider) {
        mOmniboxStub = omniboxStub;
        mTabModelSelector = tabModelSelector;
        mLayoutManager = layoutManager;
        mNtpViewProvider = ntpViewProvider;
        mNtpContentHeightProvider = ntpContentHeightProvider;
        mTabsPreviouslyOpenedCount = 0;
        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        handlePageLoadFinished(tab);
                    }
                };

        mIsAutofocusOnNotFirstTabEnabled =
                ChromeFeatureList.sOmniboxAutofocusOnIncognitoNtpNotFirstTab.getValue();
        mIsWithPredictionEnabled =
                ChromeFeatureList.sOmniboxAutofocusOnIncognitoNtpWithPrediction.getValue();
        mIsWithHardwareKeyboardEnabled =
                ChromeFeatureList.sOmniboxAutofocusOnIncognitoNtpWithHardwareKeyboard.getValue();

        mNtpSingleTapDetector =
                new GestureDetector(
                        context,
                        new GestureDetector.SimpleOnGestureListener() {
                            @Override
                            public boolean onSingleTapConfirmed(MotionEvent e) {
                                mOmniboxStub.setUrlBarFocus(
                                        false, null, OmniboxFocusReason.UNFOCUS);
                                return false;
                            }
                        });

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

        for (TabModel model : mTabModelSelector.getModels()) {
            if (model.isIncognitoBranded()) {
                model.addObserver(this);
            }
        }
    }

    /** Destroy the instance and unregister observers. */
    public void destroy() {
        mOmniboxStub.removeUrlFocusChangeListener(mUrlFocusChangeListener);
        mLayoutManager.removeObserver(mLayoutStateObserver);
        for (TabModel model : mTabModelSelector.getModels()) {
            if (model.isIncognitoBranded()) {
                model.removeObserver(this);
            }
        }
        mProcessedTabs.clear();
        mTabsPendingAutofocus.clear();
    }

    @Override
    public void didAddTab(
            Tab tab,
            @TabLaunchType int type,
            @TabCreationState int creationState,
            boolean markedForSelection) {
        if (!tab.isIncognitoBranded()) return;
        ++mTabsPreviouslyOpenedCount;
        tab.addObserver(mTabObserver);
    }

    @Override
    public void tabClosureCommitted(Tab tab) {
        if (!tab.isIncognitoBranded()) return;
        tab.removeObserver(mTabObserver);
        mProcessedTabs.remove(tab);
        mTabsPendingAutofocus.remove(tab);
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
                                    mIsAutofocusOnNotFirstTabEnabled && checkAutofocusAllowedNotFirstTab();
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
        mOmniboxStub.setUrlBarFocus(true, null, OmniboxFocusReason.OMNIBOX_TAP);

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
            final double ntpTextContentHeight = mNtpContentHeightProvider.apply(ntpView);

            final double freeSpacePercentage =
                    (tabViewHeight - ntpTextContentHeight) / tabViewHeight * 100d;

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
        tab.removeObserver(mTabObserver);
    }
}
