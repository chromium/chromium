// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.LAST_USED_PROFILE;

import android.content.Context;
import android.os.Handler;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.view.View;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.contextual_suggestions.ContextualSuggestionsBridge.ContextualSuggestionsResult;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager.FullscreenListener;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.ToolbarPhone;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.browser.widget.ListMenuButton;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.StateChangeReason;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.chrome.browser.widget.textbubble.ImageTextBubble;
import org.chromium.chrome.browser.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.Collections;
import java.util.List;

import javax.inject.Inject;
import javax.inject.Named;
import javax.inject.Provider;

/**
 * A mediator for the contextual suggestions UI component responsible for interacting with
 * the contextual suggestions C++ components (via a bridge), updating the model, and communicating
 * with the component coordinator(s).
 */
@ActivityScope
class ContextualSuggestionsMediator
        implements EnabledStateMonitor.Observer, FetchHelper.Delegate, ListMenuButton.Delegate {
    private static final int IPH_AUTO_DISMISS_TIMEOUT_MS = 6000;
    private static final int IPH_AUTO_DISMISS_ACCESSIBILITY_TIMEOUT_MS = 10000;
    private static boolean sOverrideIPHTimeoutForTesting;

    private final Profile mProfile;
    private final TabModelSelector mTabModelSelector;
    private final ContextualSuggestionsModel mModel;
    private final ChromeFullscreenManager mFullscreenManager;
    private final ToolbarManager mToolbarManager;
    private final EnabledStateMonitor mEnabledStateMonitor;
    private final Handler mHandler = new Handler();
    private final Provider<ContextualSuggestionsSource> mSuggestionSourceProvider;
    private @Nullable final OverviewModeBehavior mOverviewModeBehavior;
    private final boolean mRequireReverseScrollForIPH;

    private ContextualSuggestionsCoordinator mCoordinator;

    private @Nullable ContextualSuggestionsSource mSuggestionsSource;
    private @Nullable FetchHelper mFetchHelper;
    private @Nullable String mCurrentRequestUrl;
    private @Nullable BottomSheetObserver mSheetObserver;
    private @Nullable TextBubble mHelpBubble;

    private boolean mModelPreparedForCurrentTab;
    private boolean mSuggestionsSetOnBottomSheet;
    private boolean mHasRecordedButtonShownForTab;

    /**
     * Whether the browser controls have fully hidden at least once since the last time
     * #clearSuggestions() was called. This is used as a proxy for whether the user has scrolled
     * down on the current page.
     */
    private boolean mHaveBrowserControlsFullyHidden;
    private int mFullscreenToken = FullscreenManager.INVALID_TOKEN;

    private boolean mHasPeekDelayPassed;

    /** Whether the content sheet is observed to be opened for the first time. */
    private boolean mHasSheetBeenOpened;

    /**
     * Whether in-product help may be shown. This is set to false if the IPH system indicates that
     * it wouldn't trigger our IPH if requested and when we attempt to show the IPH bubble.
     */
    private boolean mCanShowIph;

    /**
     * Construct a new {@link ContextualSuggestionsMediator}.
     * @param profile The last used {@link Profile}.
     * @param tabModelSelector The {@link TabModelSelector} for the containing activity.
     * @param fullscreenManager The {@link ChromeFullscreenManager} to listen for browser controls
     *         events.
     * @param model The {@link ContextualSuggestionsModel} for the component.
     * @param toolbarManager The {@link ToolbarManager} for the containing activity.
     * @param layoutManager The {@link LayoutManager} used to retrieve the
     *         {@link OverviewModeBehavior} if it exists.
     * @param enabledStateMonitor The state monitor that will alert the mediator if the enabled
     *         state for contextual suggestions changes.
     * @param suggestionSourceProvider The provider of {@link ContextualSuggestionsSource}
     *         instances.
     */
    @Inject
    ContextualSuggestionsMediator(@Named(LAST_USED_PROFILE) Profile profile,
            TabModelSelector tabModelSelector, ChromeFullscreenManager fullscreenManager,
            ContextualSuggestionsModel model, ToolbarManager toolbarManager,
            LayoutManager layoutManager, EnabledStateMonitor enabledStateMonitor,
            Provider<ContextualSuggestionsSource> suggestionSourceProvider) {
        mProfile = profile.getOriginalProfile();
        mTabModelSelector = tabModelSelector;
        mModel = model;
        mFullscreenManager = fullscreenManager;

        mToolbarManager = toolbarManager;
        mSuggestionSourceProvider = suggestionSourceProvider;

        mEnabledStateMonitor = enabledStateMonitor;
        mEnabledStateMonitor.addObserver(this);
        if (mEnabledStateMonitor.getEnabledState()) {
            enable();
        }

        mFullscreenManager.addListener(new FullscreenListener() {
            @Override
            public void onContentOffsetChanged(float offset) {}

            @Override
            public void onControlsOffsetChanged(
                    float topOffset, float bottomOffset, boolean needsAnimate) {
                if (!mHaveBrowserControlsFullyHidden) {
                    mHaveBrowserControlsFullyHidden =
                            mFullscreenManager.areBrowserControlsOffScreen();
                } else if (mCanShowIph && mRequireReverseScrollForIPH
                        && mFullscreenManager.areBrowserControlsFullyVisible()) {
                    mHandler.postDelayed(() -> maybeShowHelpBubble(),
                            ToolbarPhone.LOC_BAR_WIDTH_CHANGE_ANIMATION_DURATION_MS);
                }
                reportToolbarButtonShown();
            }

            @Override
            public void onToggleOverlayVideoMode(boolean enabled) {}

            @Override
            public void onBottomControlsHeightChanged(int bottomControlsHeight) {}
        });

        if (layoutManager instanceof LayoutManagerChrome) {
            mOverviewModeBehavior = (LayoutManagerChrome) layoutManager;
            mOverviewModeBehavior.addOverviewModeObserver(new EmptyOverviewModeObserver() {
                @Override
                public void onOverviewModeFinishedHiding() {
                    reportToolbarButtonShown();
                }
            });
        } else {
            mOverviewModeBehavior = null;
        }

        mRequireReverseScrollForIPH = ChromeFeatureList.isEnabled(
                ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_IPH_REVERSE_SCROLL);
    }

    /**
     * Sets the {@link ContextualSuggestionsCoordinator} for bidirectional communication.
     */
    void initialize(ContextualSuggestionsCoordinator coordinator) {
        // TODO(pshmakov): get rid of this circular dependency by establishing an observer-observable
        // relationship between Mediator and Coordinator;
        mCoordinator = coordinator;
    }

    /** Destroys the mediator. */
    void destroy() {
        if (mFetchHelper != null) {
            mFetchHelper.destroy();
            mFetchHelper = null;
        }

        if (mSuggestionsSource != null) {
            mSuggestionsSource.destroy();
            mSuggestionsSource = null;
        }

        if (mHelpBubble != null) mHelpBubble.dismiss();

        mEnabledStateMonitor.removeObserver(this);
    }

    @Override
    public void onEnabledStateChanged(boolean enabled) {
        if (enabled) {
            enable();
        } else {
            disable();
        }
    }

    private void enable() {
        mSuggestionsSource = mSuggestionSourceProvider.get();
        mFetchHelper = new FetchHelper(this, mTabModelSelector);

        Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
        tracker.addOnInitializedCallback(success -> {
            if (!success) return;
            mCanShowIph =
                    tracker.wouldTriggerHelpUI(FeatureConstants.CONTEXTUAL_SUGGESTIONS_FEATURE);
        });
    }

    private void disable() {
        clearSuggestions();

        if (mFetchHelper != null) {
            mFetchHelper.destroy();
            mFetchHelper = null;
        }

        if (mSuggestionsSource != null) {
            mSuggestionsSource.destroy();
            mSuggestionsSource = null;
        }
    }

    @Override
    public void onSettingsStateChanged(boolean enabled) {}

    @Override
    public void requestSuggestions(String url) {
        // Guard against null tabs when requesting suggestions. https://crbug.com/836672.
        if (mTabModelSelector.getCurrentTab() == null
                || mTabModelSelector.getCurrentTab().getWebContents() == null) {
            assert false;
            return;
        }

        reportEvent(ContextualSuggestionsEvent.FETCH_REQUESTED);
        mCurrentRequestUrl = url;
        mSuggestionsSource.fetchSuggestions(url, (suggestionsResult) -> {
            if (mTabModelSelector.getCurrentTab() == null
                    || mTabModelSelector.getCurrentTab().getWebContents() == null
                    || mSuggestionsSource == null) {
                return;
            }
            assert mFetchHelper != null;

            // Avoiding double fetches causing suggestions for incorrect context.
            if (!TextUtils.equals(url, mCurrentRequestUrl)) return;

            List<ContextualSuggestionsCluster> clusters = suggestionsResult.getClusters();

            if (clusters.isEmpty() || clusters.get(0).getSuggestions().isEmpty()) return;

            for (ContextualSuggestionsCluster cluster : clusters) {
                cluster.buildChildren();
            }

            prepareModel(clusters, suggestionsResult.getPeekText());

            mToolbarManager.enableExperimentalButton(
                    view -> onToolbarButtonClicked(),
                    R.drawable.contextual_suggestions,
                    R.string.contextual_suggestions_button_description);
            RecordHistogram.recordBooleanHistogram(
                    "ContextualSuggestions.ResultsReturnedInOverviewMode", isOverviewModeVisible());
            reportToolbarButtonShown();
        });
    }

    private void onToolbarButtonClicked() {
        if (mSuggestionsSetOnBottomSheet || !mModelPreparedForCurrentTab) return;

        maybeShowContentInSheet();
        mCoordinator.showSuggestions(mSuggestionsSource);
        mCoordinator.expandBottomSheet();
    }

    // TODO(twellington): Use peek criteria to determine when to show toolbar button or remove
    //                   entirely.
    private void setPeekConditions(ContextualSuggestionsResult suggestionsResult) {
        PeekConditions peekConditions = suggestionsResult.getPeekConditions();
        long remainingDelay =
                mFetchHelper.getFetchTimeBaselineMillis(mTabModelSelector.getCurrentTab())
                + Math.round(peekConditions.getMinimumSecondsOnPage() * 1000)
                - SystemClock.uptimeMillis();

        if (remainingDelay <= 0) {
            // Don't postDelayed if the minimum delay has passed so that the suggestions may
            // be shown through the following call to show contents in the bottom sheet.
            mHasPeekDelayPassed = true;
        } else {
            // Once delay expires, the bottom sheet can be peeked if the browser controls are
            // already hidden, or the next time the browser controls are fully hidden and
            // reshown. Note that this triggering on the latter case is handled by
            // FullscreenListener#onControlsOffsetChanged() in this class.
            mHandler.postDelayed(() -> {
                mHasPeekDelayPassed = true;
                maybeShowContentInSheet();
            }, remainingDelay);
        }
    }

    private void reportToolbarButtonShown() {
        if (mHasRecordedButtonShownForTab || !mFullscreenManager.areBrowserControlsFullyVisible()
                || isOverviewModeVisible() || mSuggestionsSource == null
                || !mModel.hasSuggestions()) {
            return;
        }

        mHasRecordedButtonShownForTab = true;
        reportEvent(ContextualSuggestionsEvent.UI_BUTTON_SHOWN);
        TrackerFactory.getTrackerForProfile(mProfile).notifyEvent(
                EventConstants.CONTEXTUAL_SUGGESTIONS_BUTTON_SHOWN);
        if (mCanShowIph && !mRequireReverseScrollForIPH) {
            mHandler.postDelayed(() -> maybeShowHelpBubble(),
                    ToolbarPhone.LOC_BAR_WIDTH_CHANGE_ANIMATION_DURATION_MS);
        }
    }

    @Override
    public void clearState() {
        clearSuggestions();
    }

    @Override
    public void reportFetchDelayed(WebContents webContents) {
        if (mTabModelSelector.getCurrentTab() != null
                && mTabModelSelector.getCurrentTab().getWebContents() == webContents) {
            reportEvent(ContextualSuggestionsEvent.FETCH_DELAYED);
        }
    }

    // ListMenuButton.Delegate implementation.
    @Override
    public ListMenuButton.Item[] getItems() {
        final Context context = ContextUtils.getApplicationContext();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_OPT_OUT)) {
            return new ListMenuButton.Item[] {
                    new ListMenuButton.Item(context, R.string.menu_preferences, true),
                    new ListMenuButton.Item(context, R.string.menu_send_feedback, true)};
        } else {
            return new ListMenuButton.Item[] {
                    new ListMenuButton.Item(context, R.string.menu_send_feedback, true)};
        }
    }

    @Override
    public void onItemSelected(ListMenuButton.Item item) {
        if (item.getTextId() == R.string.menu_preferences) {
            mCoordinator.showSettings();
        } else if (item.getTextId() == R.string.menu_send_feedback) {
            mCoordinator.showFeedback();
        } else {
            assert false : "Unhandled item detected.";
        }
    }

    private void removeSuggestionsFromSheet() {
        if (mSheetObserver != null) {
            mCoordinator.removeBottomSheetObserver(mSheetObserver);
            mSheetObserver = null;
        }
        mCoordinator.removeSuggestions();

        // Wait until suggestions are fully removed to reset {@code mSuggestionsSetOnBottomSheet}.
        mCoordinator.addBottomSheetObserver(new EmptyBottomSheetObserver() {
            @Override
            public void onSheetContentChanged(@Nullable BottomSheet.BottomSheetContent newContent) {
                if (!(newContent instanceof ContextualSuggestionsBottomSheetContent)) {
                    mSuggestionsSetOnBottomSheet = false;
                    mCoordinator.removeBottomSheetObserver(this);
                }
            }
        });
    }

    /**
     * Called when suggestions are cleared either due to the user explicitly dismissing
     * suggestions via the close button or due to the FetchHelper signaling state should
     * be cleared.
     */
    private void clearSuggestions() {
        mModelPreparedForCurrentTab = false;

        // Remove suggestions before clearing model state so that views don't respond to model
        // changes while suggestions are hiding. See https://crbug.com/840579.
        removeSuggestionsFromSheet();
        mToolbarManager.disableExperimentalButton();

        mHasRecordedButtonShownForTab = false;
        mHasSheetBeenOpened = false;
        mHandler.removeCallbacksAndMessages(null);
        mHasPeekDelayPassed = false;
        mHaveBrowserControlsFullyHidden = false;
        mModel.setClusterList(Collections.emptyList());
        mModel.setCloseButtonOnClickListener(null);
        mModel.setMenuButtonDelegate(null);
        mModel.setTitle(null);
        mCurrentRequestUrl = "";

        if (mSuggestionsSource != null) mSuggestionsSource.clearState();

        if (mHelpBubble != null) mHelpBubble.dismiss();
    }

    private void prepareModel(List<ContextualSuggestionsCluster> clusters, String title) {
        if (mSuggestionsSource == null) return;

        mModel.setClusterList(clusters);
        mModel.setCloseButtonOnClickListener(view -> {
            TrackerFactory.getTrackerForProfile(mProfile).notifyEvent(
                    EventConstants.CONTEXTUAL_SUGGESTIONS_DISMISSED);
            @ContextualSuggestionsEvent
            int openedEvent =
                    mHasSheetBeenOpened ? ContextualSuggestionsEvent.UI_DISMISSED_AFTER_OPEN
                                        : ContextualSuggestionsEvent.UI_DISMISSED_WITHOUT_OPEN;
            reportEvent(openedEvent);
            removeSuggestionsFromSheet();

        });
        mModel.setMenuButtonDelegate(this);
        mModel.setTitle(!TextUtils.isEmpty(title)
                        ? title
                        : ContextUtils.getApplicationContext().getResources().getString(
                                  R.string.contextual_suggestions_button_description));

        mModelPreparedForCurrentTab = true;
    }

    private void maybeShowContentInSheet() {
        if (!mModel.hasSuggestions() || mSuggestionsSource == null) return;

        mSuggestionsSetOnBottomSheet = true;

        mSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
                if (mHelpBubble != null) mHelpBubble.dismiss();
            }

            @Override
            public void onSheetOpened(@StateChangeReason int reason) {
                if (!mHasSheetBeenOpened) {
                    mHasSheetBeenOpened = true;
                    TrackerFactory.getTrackerForProfile(mProfile).notifyEvent(
                            EventConstants.CONTEXTUAL_SUGGESTIONS_OPENED);
                    reportEvent(ContextualSuggestionsEvent.UI_OPENED);
                }
            }

            @Override
            public void onSheetClosed(int reason) {
                removeSuggestionsFromSheet();
            }
        };

        mCoordinator.addBottomSheetObserver(mSheetObserver);
        mCoordinator.showContentInSheet();
    }

    private void maybeShowHelpBubble() {
        View anchorView = mToolbarManager.getExperimentalButtonView();
        if (!mCanShowIph || mToolbarManager.isUrlBarFocused() || anchorView == null
                || anchorView.getVisibility() != View.VISIBLE
                || !mFullscreenManager.areBrowserControlsFullyVisible()
                || mSuggestionsSource == null || !mModel.hasSuggestions()) {
            return;
        }

        // Either we'll fail to show or we'll successfully show. Either way, we can't show IPH
        // after this attempt.
        mCanShowIph = false;
        Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
        if (!tracker.shouldTriggerHelpUI(FeatureConstants.CONTEXTUAL_SUGGESTIONS_FEATURE)) {
            return;
        }

        ViewRectProvider rectProvider = new ViewRectProvider(anchorView);
        rectProvider.setInsetPx(0, 0, 0,
                anchorView.getResources().getDimensionPixelOffset(
                        R.dimen.text_bubble_menu_anchor_y_inset));

        mHelpBubble = new ImageTextBubble(anchorView.getContext(), anchorView,
                R.string.contextual_suggestions_in_product_help,
                R.string.contextual_suggestions_in_product_help_accessibility, true, rectProvider,
                R.drawable.ic_logo_googleg_24dp);

        mHelpBubble.setDismissOnTouchInteraction(false);
        if (!sOverrideIPHTimeoutForTesting) {
            mHelpBubble.setAutoDismissTimeout(AccessibilityUtil.isAccessibilityEnabled()
                            ? IPH_AUTO_DISMISS_ACCESSIBILITY_TIMEOUT_MS
                            : IPH_AUTO_DISMISS_TIMEOUT_MS);
        }
        mHelpBubble.addOnDismissListener(() -> {
            tracker.dismissed(FeatureConstants.CONTEXTUAL_SUGGESTIONS_FEATURE);
            mFullscreenManager.getBrowserVisibilityDelegate().releasePersistentShowingToken(
                    mFullscreenToken);
            mHelpBubble = null;
        });
        mFullscreenToken =
                mFullscreenManager.getBrowserVisibilityDelegate().showControlsPersistent();
        mHelpBubble.show();
    }

    private void reportEvent(@ContextualSuggestionsEvent int event) {
        if (mTabModelSelector.getCurrentTab() == null
                || mTabModelSelector.getCurrentTab().getWebContents() == null) {
            // This method is not expected to be called if the current tab or webcontents are null.
            // If this assert is hit, please alert someone on the Chrome Explore on Content team.
            // See https://crbug.com/836672.
            assert false;
            return;
        }

        mSuggestionsSource.reportEvent(mTabModelSelector.getCurrentTab().getWebContents(), event);
    }

    private boolean isOverviewModeVisible() {
        return mOverviewModeBehavior != null && mOverviewModeBehavior.overviewVisible();
    }

    @VisibleForTesting
    void showContentInSheetForTesting(boolean disablePeekDelay) {
        if (disablePeekDelay) mHasPeekDelayPassed = true;
        maybeShowContentInSheet();
    }

    @VisibleForTesting
    TextBubble getHelpBubbleForTesting() {
        return mHelpBubble;
    }

    @VisibleForTesting
    static void setOverrideIPHTimeoutForTesting(boolean override) {
        sOverrideIPHTimeoutForTesting = override;
    }
}
