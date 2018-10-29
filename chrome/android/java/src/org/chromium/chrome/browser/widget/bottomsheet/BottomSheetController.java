// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.app.Activity;
import android.view.View;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.HintlessActivityTabObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.OverlayPanelManagerObserver;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.vr.VrModeObserver;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.ScrimView.ScrimObserver;
import org.chromium.chrome.browser.widget.ScrimView.ScrimParams;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.StateChangeReason;

import java.util.HashSet;
import java.util.PriorityQueue;
import java.util.Set;

/**
 * This class is responsible for managing the content shown by the {@link BottomSheet}. Features
 * wishing to show content in the {@link BottomSheet} UI must implement {@link BottomSheetContent}
 * and call {@link #requestShowContent(BottomSheetContent, boolean)} which will return true if the
 * content was actually shown (see full doc on method).
 */
public class BottomSheetController implements ApplicationStatus.ActivityStateListener {
    /** The initial capacity for the priority queue handling pending content show requests. */
    private static final int INITIAL_QUEUE_CAPACITY = 1;

    /** The parameters that controll how the scrim behaves while the sheet is open. */
    private final ScrimParams mScrimParams;

    /** A handle to the {@link BottomSheet} that this class controls. */
    private final BottomSheet mBottomSheet;

    /** A handle to the {@link SnackbarManager} that manages snackbars inside the bottom sheet. */
    private final SnackbarManager mSnackbarManager;

    /** A queue for content that is waiting to be shown in the {@link BottomSheet}. */
    private PriorityQueue<BottomSheetContent> mContentQueue;

    /** A set of contents that have requested to be shown, rather than just preloading. */
    private Set<BottomSheetContent> mFullShowRequestedSet;

    /** Whether the controller is already processing a hide request for the tab. */
    private boolean mIsProcessingHideRequest;

    /** Track whether the sheet was shown for the current tab. */
    private boolean mWasShownForCurrentTab;

    /** Whether the bottom sheet is temporarily suppressed. */
    private boolean mIsSuppressed;

    /** The manager for overlay panels to attach listeners to. */
    private OverlayPanelManager mOverlayPanelManager;

    /** Whether the bottom sheet should be suppressed when Contextual Search is showing. */
    private boolean mSuppressSheetForContextualSearch;

    /** A means for getting the activity's current tab and observing change events. */
    private ActivityTabProvider mTabProvider;

    /** The last known activity tab, if available. */
    private Tab mLastActivityTab;

    /**
     * Build a new controller of the bottom sheet.
     * @param activity An activity for context.
     * @param activityTabProvider The provider of the activity's current tab.
     * @param scrim The scrim that shows when the bottom sheet is opened.
     * @param bottomSheet The bottom sheet that this class will be controlling.
     * @param overlayManager The manager for overlay panels to attach listeners to.
     * @param suppressSheetForContextualSearch Whether the bottom sheet should be suppressed when
     *                                         Contextual Search is showing.
     */
    public BottomSheetController(final Activity activity,
            final ActivityTabProvider activityTabProvider, final ScrimView scrim,
            BottomSheet bottomSheet, OverlayPanelManager overlayManager,
            boolean suppressSheetForContextualSearch) {
        mBottomSheet = bottomSheet;
        mTabProvider = activityTabProvider;
        mOverlayPanelManager = overlayManager;
        mSuppressSheetForContextualSearch = suppressSheetForContextualSearch;
        mSnackbarManager = new SnackbarManager(
                activity, mBottomSheet.findViewById(R.id.bottom_sheet_snackbar_container));
        mSnackbarManager.onStart();
        ApplicationStatus.registerStateListenerForActivity(this, activity);
        mFullShowRequestedSet = new HashSet<>();

        // Initialize the queue with a comparator that checks content priority.
        mContentQueue = new PriorityQueue<>(INITIAL_QUEUE_CAPACITY,
                (content1, content2) -> content2.getPriority() - content1.getPriority());

        final TabObserver tabObserver = new EmptyTabObserver() {
            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                clearRequestsAndHide();
            }

            @Override
            public void onCrash(Tab tab) {
                clearRequestsAndHide();
            }

            @Override
            public void onDestroyed(Tab tab) {
                if (mLastActivityTab == tab) mLastActivityTab = null;
            }
        };

        VrModuleProvider.registerVrModeObserver(new VrModeObserver() {
            @Override
            public void onEnterVr() {
                suppressSheet(StateChangeReason.VR);
            }

            @Override
            public void onExitVr() {
                unsuppressSheet();
            }
        });

        mTabProvider.addObserverAndTrigger(new HintlessActivityTabObserver() {
            @Override
            public void onActivityTabChanged(Tab tab) {
                // Temporarily suppress the sheet if entering a state where there is no activity
                // tab.
                if (tab == null) {
                    suppressSheet(StateChangeReason.COMPOSITED_UI);
                    return;
                }

                // If refocusing the same tab, simply unsupress the sheet.
                if (mLastActivityTab == tab) {
                    unsuppressSheet();
                    return;
                }

                // Move the observer to the new activity tab and clear the sheet.
                if (mLastActivityTab != null) mLastActivityTab.removeObserver(tabObserver);
                mLastActivityTab = tab;
                mLastActivityTab.addObserver(tabObserver);
                clearRequestsAndHide();
            }
        });

        ScrimObserver scrimObserver = new ScrimObserver() {
            @Override
            public void onScrimClick() {
                if (!mBottomSheet.isSheetOpen()) return;
                mBottomSheet.setSheetState(
                        mBottomSheet.getCurrentSheetContent().isPeekStateEnabled()
                                ? BottomSheet.SheetState.PEEK
                                : BottomSheet.SheetState.HIDDEN,
                        true, StateChangeReason.TAP_SCRIM);
            }

            @Override
            public void onScrimVisibilityChanged(boolean visible) {}
        };
        mScrimParams = new ScrimParams(mBottomSheet, false, true, 0, scrimObserver);

        mBottomSheet.addObserver(new EmptyBottomSheetObserver() {
            @Override
            public void onSheetOpened(@StateChangeReason int reason) {
                scrim.showScrim(mScrimParams);
                scrim.setViewAlpha(0);
            }

            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                scrim.hideScrim(false);
            }

            @Override
            public void onTransitionPeekToHalf(float transitionFraction) {
                // TODO(mdjones): This event should not occur after the bottom sheet is closed.
                if (scrim.getVisibility() == View.VISIBLE) {
                    scrim.setViewAlpha(transitionFraction);
                }
            }

            @Override
            public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
                mSnackbarManager.dismissAllSnackbars();
            }
        });

        if (mSuppressSheetForContextualSearch) {
            mOverlayPanelManager.addObserver(new OverlayPanelManagerObserver() {
                @Override
                public void onOverlayPanelShown() {
                    suppressSheet(StateChangeReason.COMPOSITED_UI);
                }

                @Override
                public void onOverlayPanelHidden() {
                    unsuppressSheet();
                }
            });
        }
    }

    /**
     * Temporarily suppress the bottom sheet while other UI is showing. This will not itself change
     * the content displayed by the sheet.
     * @param reason The reason the sheet was suppressed.
     */
    private void suppressSheet(@StateChangeReason int reason) {
        mIsSuppressed = true;
        mBottomSheet.setSheetState(BottomSheet.SheetState.HIDDEN, false, reason);
    }

    /**
     * Unsuppress the bottom sheet. This may or may not affect the sheet depending on the state of
     * the browser (i.e. the tab switcher may be showing).
     */
    private void unsuppressSheet() {
        if (!mIsSuppressed || mTabProvider.getActivityTab() == null || !mWasShownForCurrentTab
                || isOtherUIObscuring() || VrModuleProvider.getDelegate().isInVr()) {
            return;
        }
        mIsSuppressed = false;

        if (mBottomSheet.getCurrentSheetContent() != null) {
            mBottomSheet.setSheetState(BottomSheet.SheetState.PEEK, true);
        } else {
            // In the event the previous content was hidden, try to show the next one.
            showNextContent();
        }
    }

    /**
     * @return The {@link BottomSheet} controlled by this class.
     */
    public BottomSheet getBottomSheet() {
        return mBottomSheet;
    }

    /**
     * @return The {@link SnackbarManager} that manages snackbars inside the bottom sheet.
     */
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    /**
     * Request that some content be shown in the bottom sheet.
     * @param content The content to be shown in the bottom sheet.
     * @param animate Whether the appearance of the bottom sheet should be animated.
     * @return True if the content was shown, false if it was suppressed. Content is suppressed if
     *         higher priority content is in the sheet, the sheet is expanded beyond the peeking
     *         state, or the browser is in a mode that does not support showing the sheet.
     */
    public boolean requestShowContent(BottomSheetContent content, boolean animate) {
        // If pre-load failed, do nothing. The content will automatically be queued.
        mFullShowRequestedSet.add(content);
        if (!loadInternal(content)) return false;
        if (!mBottomSheet.isSheetOpen() && !isOtherUIObscuring()) {
            mBottomSheet.setSheetState(BottomSheet.SheetState.PEEK, animate);
        }
        mWasShownForCurrentTab = true;
        return true;
    }

    /**
     * Handles loading or suppressing of content based on priority.
     * @param content The content to load.
     * @return True if the content started loading.
     */
    private boolean loadInternal(BottomSheetContent content) {
        if (content == mBottomSheet.getCurrentSheetContent()) return true;
        if (mTabProvider.getActivityTab() == null) return false;

        BottomSheetContent shownContent = mBottomSheet.getCurrentSheetContent();
        boolean shouldSuppressExistingContent = shownContent != null
                && content.getPriority() < shownContent.getPriority()
                && canBottomSheetSwitchContent();

        if (shouldSuppressExistingContent) {
            mContentQueue.add(mBottomSheet.getCurrentSheetContent());
            shownContent = content;
        } else if (mBottomSheet.getCurrentSheetContent() == null) {
            shownContent = content;
        } else {
            mContentQueue.add(content);
        }

        assert shownContent != null;
        mBottomSheet.showContent(shownContent);

        return shownContent == content;
    }

    /**
     * Hide content shown in the bottom sheet. If the content is not showing, this call retracts the
     * request to show it.
     * @param content The content to be hidden.
     * @param animate Whether the sheet should animate when hiding.
     */
    public void hideContent(BottomSheetContent content, boolean animate) {
        mFullShowRequestedSet.remove(content);

        if (content != mBottomSheet.getCurrentSheetContent()) {
            mContentQueue.remove(content);
            return;
        }

        // If the sheet is already processing a request to hide visible content, do nothing.
        if (mIsProcessingHideRequest) return;

        // Handle showing the next content if it exists.
        if (mBottomSheet.getSheetState() == BottomSheet.SheetState.HIDDEN) {
            // If the sheet is already hidden, simply show the next content.
            showNextContent();
        } else {
            // If the sheet wasn't hidden, wait for it to be before showing the next content.
            BottomSheetObserver hiddenSheetObserver = new EmptyBottomSheetObserver() {
                @Override
                public void onSheetStateChanged(int currentState) {
                    // Don't do anything until the sheet is completely hidden.
                    if (currentState != BottomSheet.SheetState.HIDDEN) return;

                    showNextContent();
                    mBottomSheet.removeObserver(this);
                    mIsProcessingHideRequest = false;
                }
            };

            mIsProcessingHideRequest = true;
            mBottomSheet.addObserver(hiddenSheetObserver);
            mBottomSheet.setSheetState(BottomSheet.SheetState.HIDDEN, animate);
        }
    }

    /**
     * Expand the {@link BottomSheet}. If there is no content in the sheet, this is a noop.
     */
    public void expandSheet() {
        if (mBottomSheet.getCurrentSheetContent() == null) return;
        mBottomSheet.setSheetState(BottomSheet.SheetState.HALF, true);
        if (mOverlayPanelManager.getActivePanel() != null) {
            // TODO(mdjones): This should only apply to contextual search, but contextual search is
            //                the only implementation. Fix this to only apply to contextual search.
            mOverlayPanelManager.getActivePanel().closePanel(
                    OverlayPanel.StateChangeReason.UNKNOWN, true);
        }
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.STARTED) {
            mSnackbarManager.onStart();
        } else if (newState == ActivityState.STOPPED) {
            mSnackbarManager.onStop();
        } else if (newState == ActivityState.DESTROYED) {
            ApplicationStatus.unregisterActivityStateListener(this);
        }
    }

    /**
     * Show the next {@link BottomSheetContent} if it is available and peek the sheet. If no content
     * is available the sheet's content is set to null.
     */
    private void showNextContent() {
        if (mContentQueue.isEmpty()) {
            mBottomSheet.showContent(null);
            return;
        }

        BottomSheetContent nextContent = mContentQueue.poll();
        mBottomSheet.showContent(nextContent);
        if (mFullShowRequestedSet.contains(nextContent)) {
            mBottomSheet.setSheetState(BottomSheet.SheetState.PEEK, true);
        }
    }

    /**
     * Clear all the content show requests and hide the current content.
     */
    private void clearRequestsAndHide() {
        mContentQueue.clear();
        mFullShowRequestedSet.clear();
        // TODO(mdjones): Replace usages of bottom sheet with a model in line with MVC.
        // TODO(mdjones): It would probably be useful to expose an observer method that notifies
        //                objects when all content requests are cleared.
        hideContent(mBottomSheet.getCurrentSheetContent(), true);
        mWasShownForCurrentTab = false;
        mIsSuppressed = false;
    }

    /**
     * @return Whether some other UI is preventing the sheet from showing.
     */
    protected boolean isOtherUIObscuring() {
        return mOverlayPanelManager.getActivePanel() != null;
    }

    /**
     * @return Whether the sheet currently supports switching its content.
     */
    protected boolean canBottomSheetSwitchContent() {
        return !mBottomSheet.isSheetOpen();
    }
}
