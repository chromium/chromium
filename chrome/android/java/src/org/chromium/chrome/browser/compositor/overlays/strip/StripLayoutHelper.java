// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.ANIM_TAB_MOVE_MS;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.BUTTON_BACKGROUND_SIZE_DP;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.BUTTON_TOUCH_TARGET_SIZE_DP;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.INVALID_TIME;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.MAX_TAB_WIDTH_DP;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.MIN_TAB_WIDTH_DP;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.PINNED_TAB_WIDTH_DP;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.TAB_OVERLAP_WIDTH_DP;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.isTabPinningFromStripEnabled;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil.FOLIO_FOOT_LENGTH_DP;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.SystemClock;
import android.text.format.DateUtils;
import android.util.TypedValue;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.ListPopupWindow;

import androidx.annotation.ColorInt;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.view.ViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.MathUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.ButtonType;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.StackScroller;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle.StripLayoutGroupTitleDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnClickHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnKeyboardFocusHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.StripTabModelActionListener.ActionType;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator.AnchorInfo;
import org.chromium.chrome.browser.compositor.overlays.strip.TabLoadTracker.TabLoadTrackerCallback;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripIphController.IphType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.StripUpdateDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.TabStripDragHandler;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.MediaState;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabClosureParamsUtils;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorUtil;
import org.chromium.chrome.browser.tabmodel.TabGroupColorUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils.TabGroupCreationCallback;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.GroupSharedState;
import org.chromium.chrome.browser.tasks.tab_management.TabBubbler;
import org.chromium.chrome.browser.tasks.tab_management.TabCardLabelData;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinatorFactory;
import org.chromium.chrome.browser.tasks.tab_management.TabListNotificationHandler;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabShareUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiUtils;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TriggerSource;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.MotionEventUtils;
import org.chromium.ui.widget.RectProvider;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.function.Supplier;

/**
 * This class handles managing the positions and behavior of all tabs in a tab strip. It is
 * responsible for both responding to UI input events and model change notifications, adjusting and
 * animating the tab strip as required.
 *
 * <p>The stacking and visual behavior is driven by setting a {@link StripStacker}.
 */
@NullMarked
public class StripLayoutHelper
        implements StripLayoutGroupTitleDelegate,
                StripLayoutViewOnClickHandler,
                StripLayoutViewOnKeyboardFocusHandler,
                StripUpdateDelegate,
                AnimationHost,
                TabListNotificationHandler {
    // Animation/Timer Constants
    private static final int SPINNER_UPDATE_DELAY_MS = 66;
    // Degrees per millisecond.
    private static final float SPINNER_DPMS = 0.33f;
    private static final int ANIM_TAB_CREATED_MS = 150;
    private static final int ANIM_TAB_CLOSED_MS = 150;
    private static final int ANIM_TAB_RESIZE_MS = 250;
    private static final int ANIM_TAB_DRAW_X_MS = 250;
    private static final int ANIM_BUTTONS_FADE_MS = 150;
    private static final int NEW_ANIM_TAB_RESIZE_MS = 200;

    // Visibility Constants
    private static final float NEW_TAB_BUTTON_BACKGROUND_Y_OFFSET_DP = 3.f;

    // Desired spacing between new tab button and tabs when tab strip is not full.
    private static final float DESIRED_PADDING_BETWEEN_NEW_TAB_BUTTON_AND_TABS = 2.f;
    private static final float NEW_TAB_BUTTON_DEFAULT_PRESSED_OPACITY = 0.2f;
    private static final float NEW_TAB_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY = 0.12f;
    private static final float NEW_TAB_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY = 0.08f;
    static final float FADE_FULL_OPACITY_THRESHOLD_DP = 24.f;

    // Values adapt based on whether the device is desktop or tablet.
    private static final boolean IS_DESKTOP_DENSITY = StripLayoutUtils.shouldApplyMoreDensity();
    private static final float NEW_TAB_BUTTON_CLICK_SLOP_DP =
            (BUTTON_TOUCH_TARGET_SIZE_DP - BUTTON_BACKGROUND_SIZE_DP) / 2;
    private static final float NEW_TAB_BUTTON_WITH_MODEL_SELECTOR_BUTTON_PADDING =
            IS_DESKTOP_DENSITY ? 24.f : 8.f;

    private static final int MESSAGE_UPDATE_SPINNER = 1;
    private static final int MESSAGE_HOVER_CARD = 2;
    private static final long TAB_SWITCH_METRICS_MAX_ALLOWED_SCROLL_INTERVAL =
            DateUtils.MINUTE_IN_MILLIS;

    // Reorder Drag Threshold Constants
    private static final float INITIATE_REORDER_DRAG_THRESHOLD = 30.f;

    // Scrolling constants.
    @VisibleForTesting static final int SCROLL_SPEED_FACTOR = 40;

    // Histogram Constants
    private static final String PLACEHOLDER_LEFTOVER_TABS_HISTOGRAM_NAME =
            "Android.TabStrip.PlaceholderStripLeftoverTabsCount";
    private static final String PLACEHOLDER_TABS_CREATED_DURING_RESTORE_HISTOGRAM_NAME =
            "Android.TabStrip.PlaceholderStripTabsCreatedDuringRestoreCount";
    private static final String PLACEHOLDER_TABS_NEEDED_DURING_RESTORE_HISTOGRAM_NAME =
            "Android.TabStrip.PlaceholderStripTabsNeededDuringRestoreCount";
    private static final String PLACEHOLDER_VISIBLE_DURATION_HISTOGRAM_NAME =
            "Android.TabStrip.PlaceholderStripVisibleDuration";

    @VisibleForTesting
    static final String NULL_TAB_HOVER_CARD_VIEW_SHOW_DELAYED_HISTOGRAM_NAME =
            "Android.TabStrip.NullTabHoverCardView.ShowDelayed";

    // Hover card constants
    @VisibleForTesting static final int MAX_HOVER_CARD_DELAY_MS = 800;
    @VisibleForTesting static final int MIN_HOVER_CARD_DELAY_MS = 300;
    private static final int SHOW_HOVER_CARD_WITHOUT_DELAY_TIME_BUFFER = 300;

    // An observer that is notified of changes to a {@link TabGroupModelFilter} object.
    private final TabGroupModelFilterObserver mTabGroupModelFilterObserver =
            new TabGroupModelFilterObserver() {
                @Nullable Token mSourceTabGroupId;

                @Override
                public void willMoveTabGroup(Token tabGroupId, int currentIndex) {
                    mMovingGroup = true;
                }

                @Override
                public void didMoveTabGroup(
                        Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                    mMovingGroup = false;
                    // The sequencing of #didMoveTabGroup and #didMoveTab is different with and
                    // without Tab Collections. With Tab Collections enabled, the final event is
                    // #didMoveTabGroup, meaning we need to trigger a rebuild here. With it
                    // disabled, we instead need to trigger a rebuild after the final #didMoveTab
                    // event, which is handled in #tabMoved.
                    if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_COLLECTION_ANDROID)) {
                        // Additionally rebuild the StripLayoutTabs here as well. This was
                        // previously maintained by #tabMoved, but the old/new indices that are
                        // provided are different with Tab Collections enabled.
                        rebuildStripTabs(/* deferAnimations= */ true);
                        rebuildStripViewsAfterMove();
                    }
                }

                @Override
                public void didMergeTabToGroup(Tab movedTab, boolean isDestinationTab) {
                    // TODO(crbug.com/375047646): Investigate kicking off animations here.
                    Token tabGroupId = movedTab.getTabGroupId();
                    assumeNonNull(tabGroupId);
                    updateGroupTextAndSharedState(tabGroupId);
                    onTabMergeToOrMoveOutOfGroup();

                    // Tab merging should not automatically expand a collapsed tab group. If the
                    // target group is collapsed, the tab being merged should also be collapsed.
                    StripLayoutGroupTitle groupTitle = findGroupTitle(movedTab.getTabGroupId());
                    if (groupTitle != null) {
                        updateTabCollapsed(
                                findTabById(movedTab.getId()), groupTitle.isCollapsed(), false);
                    }
                }

                @Override
                public void willMoveTabOutOfGroup(
                        Tab movedTab, @Nullable Token destinationTabGroupId) {
                    // TODO(crbug.com/326494015): Refactor #didMoveTabOutOfGroup to pass in previous
                    //  root ID.
                    mSourceTabGroupId = movedTab.getTabGroupId();
                }

                @Override
                public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
                    updateGroupTextAndSharedState(mSourceTabGroupId);
                    Token groupIdToHide = mGroupIdToHideSupplier.get();
                    // TODO(crbug.com/430514194): There is a strong possibility this is never true
                    // as didRemoveTabGroup is invoked before this and would make groupIdToHide
                    // null.
                    boolean removedHiddenLastTabInGroup =
                            groupIdToHide != null
                                    && groupIdToHide.equals(mSourceTabGroupId)
                                    && mTabGroupModelFilter != null
                                    && !mTabGroupModelFilter.tabGroupExists(mSourceTabGroupId);

                    // Skip if the rebuild will be handled elsewhere after reaching a "proper" tab
                    // state, such as confirming the group deletion.
                    if (!removedHiddenLastTabInGroup) {
                        onTabMergeToOrMoveOutOfGroup();
                    }

                    // Expand the tab if necessary.
                    StripLayoutTab tab = findTabById(movedTab.getId());
                    if (tab != null && tab.isCollapsed()) {
                        updateTabCollapsed(tab, false, false);
                        finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
                        computeAndUpdateTabWidth(/* animate= */ true, /* deferAnimations= */ false);
                    }
                }

                @Override
                public void didMoveWithinGroup(
                        Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                    updateGroupAccessibilityDescription(findGroupTitle(movedTab.getTabGroupId()));
                }

                @Override
                public void didCreateNewGroup(Tab destinationTab, TabGroupModelFilter filter) {
                    rebuildStripViews();
                }

                @Override
                public void didChangeTabGroupTitle(Token tabGroupId, @Nullable String newTitle) {
                    updateGroupTextAndSharedState(tabGroupId);
                    mRenderHost.requestRender();
                }

                @Override
                public void didChangeTabGroupColor(
                        Token tabGroupId, @TabGroupColorId int newColor) {
                    updateGroupTitleTint(findGroupTitle(tabGroupId), newColor);
                }

                @Override
                public void didChangeTabGroupCollapsed(
                        Token tabGroupId, boolean isCollapsed, boolean animate) {
                    final StripLayoutGroupTitle groupTitle = findGroupTitle(tabGroupId);
                    if (groupTitle == null) return;

                    if (!isCollapsed && groupTitle.getNotificationBubbleShown()) {
                        groupTitle.setNotificationBubbleShown(false);
                        updateGroupTextAndSharedState(tabGroupId);
                    }
                    updateTabGroupCollapsed(groupTitle, isCollapsed, animate);
                }

                @Override
                public void willCloseTabGroup(Token tabGroupId, boolean isHiding) {
                    onWillCloseView(StripLayoutUtils.findGroupTitle(mStripGroupTitles, tabGroupId));
                }

                @Override
                public void didRemoveTabGroup(
                        int oldRootId,
                        @Nullable Token oldTabGroupId,
                        @DidRemoveTabGroupReason int removalReason) {
                    if (oldTabGroupId == null) return;

                    StripLayoutGroupTitle groupTitle = findGroupTitle(oldTabGroupId);
                    // TODO(crbug.com/443337907) If we're closing for a close button click, we don't
                    // want to clobber the existing animations. This check can be removed once we
                    // update the tab strip close button clicks to immediately remove from the
                    // model.
                    if (!mCloseAnimationsRequested || groupTitle == null || groupTitle.isDying()) {
                        clearClosingGroupTitleState(oldTabGroupId);
                    } else {
                        mClosingGroupTitles.add(groupTitle);
                        requestCloseAnimations();
                    }

                    // Dismiss the iph text bubble when the synced tab group is unsynced.
                    if (oldTabGroupId.equals(mLastSyncedGroupIdForIph)) dismissTabStripSyncIph();
                    onWillCloseView(
                            StripLayoutUtils.findGroupTitle(mStripGroupTitles, oldTabGroupId));
                }
            };

    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void onTabsSelectionChanged() {
                    if (mModel == null || mStripTabs.length == 0) return;
                    for (StripLayoutTab stripTab : mStripTabs) {
                        mTabDelegate.setIsTabMultiSelected(
                                stripTab, mModel.isTabMultiSelected(stripTab.getTabId()));
                        setAccessibilityDescription(stripTab, getTabById(stripTab.getTabId()));
                        if (stripTab.isKeyboardFocused()) {
                            ViewCompat.setAccessibilityPaneTitle(
                                    mToolbarContainerView, stripTab.getAccessibilityDescription());
                        }
                    }
                    mUpdateHost.requestUpdate();
                }

                @Override
                public void didChangePinState(Tab tab) {
                    boolean isPinned = tab.getIsPinned();
                    StripLayoutTab stripTab = findTabById(tab.getId());
                    assumeNonNull(stripTab);
                    stripTab.setIsPinned(isPinned);
                    mPinnedTabsBoundarySupplier.set(getPinnedTabsBoundary());
                    setAccessibilityDescription(stripTab, tab);

                    // Compute each view's ideal position to get ready for the tab move animation
                    // below.
                    computeIdealViewPositions();

                    // Foreground the pinned/unpinned tab to start animation.
                    stripTab.setIsForegrounded(/* isForegrounded= */ true);
                    mTabDelegate.setIsTabNonDragReordering(
                            stripTab,
                            /* isNonDragReordering= */ !stripTab.getIsSelected()
                                    && !stripTab.getIsMultiSelected());
                    List<Animator> pinnedAnimations =
                            computeAndUpdateTabWidth(
                                    /* animate= */ true, /* deferAnimations= */ true);
                    assumeNonNull(pinnedAnimations);

                    // Animate the moving tab to the pinned boundary. Normally we animate from drawX
                    // to idealX, but if the first unpinned slot is off-screen, a newly unpinned tab
                    // would “fly” toward the strip start. To make it appear to fly into the
                    // unpinned section instead, animate offsetX toward the absolute pinned
                    // boundary.
                    float startOffsetX = stripTab.getDrawX() - stripTab.getIdealX();
                    float endOffsetX =
                            isPinned ? 0f : getPinnedTabsBoundary() - stripTab.getIdealX();
                    pinnedAnimations.add(
                            CompositorAnimator.ofFloatProperty(
                                    mUpdateHost.getAnimationHandler(),
                                    stripTab,
                                    StripLayoutView.X_OFFSET,
                                    startOffsetX,
                                    endOffsetX,
                                    ANIM_TAB_MOVE_MS));

                    queueAnimations(
                            pinnedAnimations,
                            new AnimatorListenerAdapter() {
                                @Override
                                public void onAnimationEnd(Animator animation) {
                                    stripTab.setIsForegrounded(/* isForegrounded= */ false);
                                    mTabDelegate.setIsTabNonDragReordering(
                                            stripTab, /* isNonDragReordering= */ false);
                                    stripTab.setOffsetX(0f);
                                }
                            });

                    if (isPinned) {
                        recordPinnedOnlyTabStripUserAction();
                    }
                }

                @Override
                public void willUndoTabClosure(List<Tab> tabs, boolean isAllTabs) {
                    finishAnimations();
                }
            };

    // External influences
    private final SceneOverlay mSceneOverlay;
    private final LayoutUpdateHost mUpdateHost;
    private final LayoutRenderHost mRenderHost;
    private final LayoutManagerHost mManagerHost;
    private final WindowAndroid mWindowAndroid;

    // Set after native initialization
    private @MonotonicNonNull TabModel mModel;
    private @MonotonicNonNull TabGroupModelFilter mTabGroupModelFilter;
    private @MonotonicNonNull TabCreator mTabCreator;
    private @MonotonicNonNull TabStripIphController mTabStripIphController; // IPH on tab strip.

    // Set when StripLayoutHelperManager's mLayerTitleCacheSupplier gets a value
    private @MonotonicNonNull LayerTitleCache mLayerTitleCache;

    private final BottomSheetController mBottomSheetController;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;

    private final TabGroupListBottomSheetCoordinatorFactory
            mTabGroupListBottomSheetCoordinatorFactory;

    // Internal State
    private StripLayoutView[] mStripViews = new StripLayoutView[0];
    private StripLayoutTab[] mStripTabs = new StripLayoutTab[0];
    private StripLayoutTab[] mStripTabsToRender = new StripLayoutTab[0];
    private StripLayoutGroupTitle[] mStripGroupTitles = new StripLayoutGroupTitle[0];
    private StripLayoutGroupTitle[] mStripGroupTitlesToRender = new StripLayoutGroupTitle[0];
    private @Nullable StripLayoutTab mTabAtPositionForTesting;
    private final StripTabEventHandler mStripTabEventHandler = new StripTabEventHandler();
    private final TabLoadTrackerCallback mTabLoadTrackerHost = new TabLoadTrackerCallbackImpl();
    private final RectF mTouchableRect = new RectF();

    // Delegates that manage different functions for the tab strip.
    private final ActionConfirmationManager mActionConfirmationManager;
    private final StripStacker mStripStacker = new ScrollingStripStacker();
    private final ScrollDelegate mScrollDelegate;
    private ReorderDelegate mReorderDelegate = new ReorderDelegate();

    // Common state used for animations on the strip triggered by independent actions including and
    // not limited to tab closure, tab creation/selection, and tab reordering. Not intended to be
    // used for hover actions. Consider using setAndStartRunningAnimator() to set and start this
    // animator.
    private @Nullable Animator mRunningAnimator;
    private final List<Animator> mQueuedAnimators = new ArrayList<>();

    private boolean mCloseAnimationsRequested;
    private final Set<StripLayoutTab> mClosingTabs = new HashSet<>();
    private final Set<StripLayoutGroupTitle> mClosingGroupTitles = new HashSet<>();

    private final TintedCompositorButton mNewTabButton;
    private final @Nullable CompositorButton mModelSelectorButton;

    // Layout Constants
    private final float mNewTabButtonWidth;
    private final ListPopupWindow mCloseButtonMenu;

    // All views are overlapped by TAB_OVERLAP_WIDTH_DP. Group titles do not need to be overlapped
    // by this much, so we offset the drawX.
    private final float mGroupTitleDrawXOffset;
    // The effective overlap width for group titles. This is the "true" overlap width, but adjusted
    // to account for the start offset above.
    private final float mGroupTitleOverlapWidth;

    // Strip State
    /**
     * The {@link Supplier} for the width of a tab based on the number of tabs and the available
     * space on the tab strip. Constricted by MIN_TAB_WIDTH_DP and MAX_TAB_WIDTH_DP.
     */
    private final ObservableSupplierImpl<Float> mCachedTabWidthSupplier =
            new ObservableSupplierImpl<>(0f);

    private final ObservableSupplierImpl<Float> mPinnedTabsBoundarySupplier =
            new ObservableSupplierImpl<>(0f);

    // Reorder State
    private boolean mMovingGroup;

    // Close State
    private boolean mPendingMouseTabClosure;
    // If closing the end-most tab via precision pointer, that tab's properties are stored here.
    // These properties are used to determine how to resize the strip such that the preceding tab's
    // close button moves to where the closing tab's close button was, if possible.
    private @Nullable Float mClosingEndMostTabDrawX;
    private @Nullable Float mClosingEndMostTabWidth;

    // Multi-selection state
    private int mAnchorTabId = Tab.INVALID_TAB_ID;

    // Tab switch efficiency
    private @Nullable Long mTabScrollStartTime;
    private @Nullable Long mMostRecentTabScroll;

    // UI State
    private float mWidth;
    private float mHeight;
    private long mLastSpinnerUpdate;
    // The margins on the tab strip used when positioning tabs. Tabs within these margins are not
    // touchable, but other strip widgets (e.g new tab button) could be.
    private float mLeftMargin;
    private float mRightMargin;
    private float mLeftFadeWidth;
    private float mRightFadeWidth;
    // Padding regions on the edges of the strip where strip touch events are blocked. Different
    // from margins, no strip widgets should be drawn within the padding regions.
    private float mLeftPadding;
    private float mRightPadding;
    private float mTopPadding;

    // New tab button with tab strip end padding
    private final float mFixedEndPadding;
    private float mReservedEndMargin;

    private final boolean mIncognito;
    private boolean mSelected;
    private boolean mIsFirstLayoutPass;
    // Whether tab strip scrolling is in progress
    private boolean mIsStripScrollInProgress;

    // Tab menu item IDs
    public static final int ID_CLOSE_ALL_TABS = 0;

    private final Context mContext;

    // Animation states. True while the relevant animations are running, and false otherwise.
    private boolean mMultiStepTabCloseAnimRunning;
    private boolean mTabResizeAnimRunning;

    // TabModel info available before the tab state is actually initialized. Determined from frozen
    // tab metadata.
    private boolean mTabStateInitialized;
    private boolean mPlaceholderStripReady;
    private boolean mSelectedOnStartup;
    private boolean mCreatedTabOnStartup;
    private boolean mActiveTabReplaced;
    private int mTabCountOnStartup;
    private int mActiveTabIndexOnStartup;
    private int mCurrentPlaceholderIndex;

    private long mPlaceholderCreationTime;
    private int mTabsCreatedDuringRestore;
    private int mPlaceholdersNeededDuringRestore;

    // View initially interacted with at the start of a motion event that does not immediately, but
    // may eventually, trigger reorder mode. The first case is dragging a tab with the mouse,
    // without first long-pressing. The second case is a long-press triggering a view's context
    // menu, where we'll delay reorder (and dismissing the context menu) until a drag threshold has
    // been reached.
    private @Nullable StripLayoutView mDelayedReorderView;

    // X-position of the initial interaction with the view above. If the user drags a certain
    // distance away from this initial position, the context menu (if any) will be dismissed, and
    // we'll enter reorder mode.
    private float mDelayedReorderInitialX;

    // Tab Drag and Drop state to hold clicked tab being dragged.
    private final View mToolbarContainerView;
    private final @Nullable TabStripDragHandler mTabStripDragHandler;

    // Tab hover state.
    private @Nullable StripLayoutTab mLastHoveredTab;
    private @Nullable StripTabHoverCardView mTabHoverCardView;
    private long mLastHoverCardExitTime;

    // Tab Group Sync.
    private @Nullable Token mLastSyncedGroupIdForIph;
    private final Supplier<Boolean> mTabStripVisibleSupplier;

    // Tab group delete dialog.
    private final ObservableSupplierImpl<@Nullable Token> mGroupIdToHideSupplier =
            new ObservableSupplierImpl<>();

    // Tab group context menu.
    // Set when showTabGroupContextMenu is called for the first time.
    private @MonotonicNonNull TabGroupContextMenuCoordinator mTabGroupContextMenuCoordinator;

    // Tab context menu.
    private final MultiInstanceManager mMultiInstanceManager;
    // Set when showTabContextMenu is called for the first time.
    private @MonotonicNonNull TabContextMenuCoordinator mTabContextMenuCoordinator;
    private @MonotonicNonNull TabGroupListBottomSheetCoordinator
            mTabGroupListBottomSheetCoordinator;
    // Set when the context menu triggered by a gesture on empty strip space is shown for the first
    // time.
    private @MonotonicNonNull TabStripContextMenuCoordinator mTabStripContextMenuCoordinator;

    // Tab group share.
    // These are set if shouldEnableGroupSharing() is true.
    private @MonotonicNonNull DataSharingService mDataSharingService;
    private @MonotonicNonNull CollaborationService mCollaborationService;

    private final DataSharingTabManager mDataSharingTabManager;
    private DataSharingService.@Nullable Observer mDataSharingObserver;
    private @Nullable TabGroupSyncService mTabGroupSyncService;
    private TabGroupSyncService.@Nullable Observer mTabGroupSyncObserver;

    private final List<QueuedIph> mQueuedIphList = new ArrayList<>();

    private final StripLayoutTabDelegate mTabDelegate;

    // Pinned tabs.
    private boolean mIsPinnedOnlyStripRecorded;

    @FunctionalInterface
    interface QueuedIph {
        boolean attemptToShow();
    }

    /**
     * Creates an instance of the {@link StripLayoutHelper}.
     *
     * @param context The current Android {@link Context}.
     * @param managerHost The parent {@link LayoutManagerHost}.
     * @param updateHost The parent {@link LayoutUpdateHost}.
     * @param renderHost The {@link LayoutRenderHost}.
     * @param incognito Whether or not this tab strip is incognito.
     * @param modelSelectorButton The {@link CompositorButton} used to toggle between regular and
     *     incognito models.
     * @param tabStripDragHandler The @{@link TabStripDragHandler} instance to initiate drag and
     *     drop.
     * @param toolbarContainerView The @{link View} passed to @{link TabStripDragHandler} for drag
     *     and drop.
     * @param windowAndroid The @{@link WindowAndroid} instance to access Activity.
     * @param actionConfirmationManager The {@link ActionConfirmationManager} for group actions.
     * @param dataSharingTabManager The {@link DataSharingTabManager} for shared groups.
     * @param tabStripVisibleSupplier Supplier of the boolean indicating whether the tab strip is
     *     visible. The tab strip can be hidden due to the tab switcher being displayed or the
     *     window width is less than 600dp.
     * @param bottomSheetController The {@link BottomSheetController} used to show bottom sheets.
     * @param multiInstanceManager The {@link MultiInstanceManager} used to move tabs to other
     *     windows.
     * @param shareDelegateSupplier Supplies {@link ShareDelegate} to share tab URLs.
     */
    public StripLayoutHelper(
            Context context,
            StripLayoutHelperManager manager,
            LayoutManagerHost managerHost,
            LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost,
            boolean incognito,
            @Nullable CompositorButton modelSelectorButton,
            @Nullable TabStripDragHandler tabStripDragHandler,
            View toolbarContainerView,
            WindowAndroid windowAndroid,
            ActionConfirmationManager actionConfirmationManager,
            DataSharingTabManager dataSharingTabManager,
            Supplier<Boolean> tabStripVisibleSupplier,
            BottomSheetController bottomSheetController,
            MultiInstanceManager multiInstanceManager,
            Supplier<ShareDelegate> shareDelegateSupplier,
            TabGroupListBottomSheetCoordinatorFactory tabGroupListBottomSheetCoordinatorFactory) {
        mGroupTitleDrawXOffset = TAB_OVERLAP_WIDTH_DP - FOLIO_FOOT_LENGTH_DP;
        mGroupTitleOverlapWidth = FOLIO_FOOT_LENGTH_DP - mGroupTitleDrawXOffset;
        mNewTabButtonWidth = BUTTON_BACKGROUND_SIZE_DP;
        mModelSelectorButton = modelSelectorButton;
        mToolbarContainerView = toolbarContainerView;
        mTabStripDragHandler = tabStripDragHandler;
        mWindowAndroid = windowAndroid;
        mLastHoverCardExitTime = INVALID_TIME;
        mTabStripVisibleSupplier = tabStripVisibleSupplier;
        mDataSharingTabManager = dataSharingTabManager;
        mBottomSheetController = bottomSheetController;
        mMultiInstanceManager = multiInstanceManager;
        mShareDelegateSupplier = shareDelegateSupplier;
        mTabGroupListBottomSheetCoordinatorFactory = tabGroupListBottomSheetCoordinatorFactory;
        mScrollDelegate = new ScrollDelegate(context);

        // Use toolbar menu button padding to align NTB with menu button.
        mFixedEndPadding =
                context.getResources().getDimension(R.dimen.button_end_padding)
                        / context.getResources().getDisplayMetrics().density;
        mReservedEndMargin = mFixedEndPadding + mNewTabButtonWidth;
        updateMargins(false);

        mSceneOverlay = manager;
        mManagerHost = managerHost;
        mUpdateHost = updateHost;
        mRenderHost = renderHost;

        // Set new tab button background resource.
        mNewTabButton =
                new TintedCompositorButton(
                        context,
                        ButtonType.NEW_TAB,
                        null,
                        BUTTON_BACKGROUND_SIZE_DP,
                        BUTTON_BACKGROUND_SIZE_DP,
                        mToolbarContainerView::setTooltipText,
                        /* clickHandler= */ this,
                        /* keyboardFocusHandler= */ this,
                        R.drawable.ic_new_tab_button,
                        NEW_TAB_BUTTON_CLICK_SLOP_DP);
        mNewTabButton.setBackgroundResourceId(R.drawable.bg_circle_tab_strip_button);

        int apsBackgroundHoveredTint =
                ColorUtils.setAlphaComponentWithFloat(
                        SemanticColorUtils.getDefaultTextColor(context),
                        NEW_TAB_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY);
        int apsBackgroundPressedTint =
                ColorUtils.setAlphaComponentWithFloat(
                        SemanticColorUtils.getDefaultTextColor(context),
                        NEW_TAB_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY);

        int apsBackgroundIncognitoHoveredTint =
                ColorUtils.setAlphaComponentWithFloat(
                        context.getColor(R.color.tab_strip_button_hover_bg_color),
                        NEW_TAB_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY);
        int apsBackgroundIncognitoPressedTint =
                ColorUtils.setAlphaComponentWithFloat(
                        context.getColor(R.color.tab_strip_button_hover_bg_color),
                        NEW_TAB_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY);

        // Primary container for default bg color.
        int backgroundDefaultTint = TabUiThemeProvider.getDefaultNtbContainerColor(context);

        // Primary @ 20% for default pressed bg color.
        int backgroundPressedTint =
                ColorUtils.setAlphaComponentWithFloat(
                        SemanticColorUtils.getDefaultIconColorAccent1(context),
                        NEW_TAB_BUTTON_DEFAULT_PRESSED_OPACITY);

        // gm3_baseline_surface_container_dark for incognito bg color.
        int backgroundIncognitoDefaultTint =
                context.getColor(R.color.tab_strip_bg_incognito_default_tint);

        // gm3_baseline_surface_container_highest_dark for incognito pressed bg color
        int backgroundIncognitoPressedTint =
                context.getColor(R.color.tab_strip_bg_incognito_pressed_tint);

        // Tab strip redesign new tab button night mode bg color.
        if (ColorUtils.inNightMode(context)) {
            // colorSurfaceContainerLow for night mode bg color.
            backgroundDefaultTint = SemanticColorUtils.getColorSurfaceContainerLow(context);

            // colorSurfaceContainerHighest for pressed night mode bg color.
            backgroundPressedTint = SemanticColorUtils.getColorSurfaceContainerHighest(context);
        }
        mNewTabButton.setBackgroundTint(
                backgroundDefaultTint,
                backgroundPressedTint,
                backgroundIncognitoDefaultTint,
                backgroundIncognitoPressedTint,
                apsBackgroundHoveredTint,
                apsBackgroundPressedTint,
                apsBackgroundIncognitoHoveredTint,
                apsBackgroundIncognitoPressedTint);

        // No pressed state color change for new tab button icon.
        mNewTabButton.setTintResources(
                R.color.default_icon_color_tint_list,
                R.color.default_icon_color_tint_list,
                R.color.modern_white,
                R.color.modern_white);

        // y-offset  = lowered tab container + (tab container size - bg size)/2 -
        // Tab title y-offset = 2 + (38 - 32)/2 - 2 = 3dp
        mNewTabButton.setDrawY(NEW_TAB_BUTTON_BACKGROUND_Y_OFFSET_DP);

        mNewTabButton.setIncognito(incognito);
        Resources res = context.getResources();
        mNewTabButton.setAccessibilityDescription(
                res.getString(R.string.accessibility_toolbar_btn_new_tab),
                res.getString(R.string.accessibility_toolbar_btn_new_incognito_tab));
        mContext = context;
        mIncognito = incognito;

        // Create tab menu
        mCloseButtonMenu = new ListPopupWindow(mContext);

        mCloseButtonMenu.setBackgroundDrawable(
                AppCompatResources.getDrawable(
                        mContext, R.drawable.tablet_tab_strip_close_all_tabs_context_menu));

        mCloseButtonMenu.setAdapter(
                new ArrayAdapter<>(
                        mContext,
                        R.layout.one_line_list_item,
                        new String[] {
                            mContext.getString(
                                    !mIncognito
                                            ? R.string.menu_close_all_tabs
                                            : R.string.menu_close_all_incognito_tabs)
                        }));
        mCloseButtonMenu.setOnItemClickListener(
                new OnItemClickListener() {
                    @Override
                    public void onItemClick(
                            AdapterView<?> parent, View view, int position, long id) {
                        mCloseButtonMenu.dismiss();
                        if (mTabGroupModelFilter == null) return;
                        if (position == ID_CLOSE_ALL_TABS) {
                            mTabGroupModelFilter
                                    .getTabModel()
                                    .getTabRemover()
                                    .closeTabs(
                                            TabClosureParams.closeAllTabs()
                                                    .hideTabGroups(true)
                                                    .tabClosingSource(
                                                            TabClosingSource.TABLET_TAB_STRIP)
                                                    .build(),
                                            /* allowDialog= */ true);
                            RecordUserAction.record("MobileToolbarCloseAllTabs");
                        }
                    }
                });

        int menuWidth = mContext.getResources().getDimensionPixelSize(R.dimen.menu_width);
        mCloseButtonMenu.setWidth(menuWidth);
        mCloseButtonMenu.setModal(true);

        mActionConfirmationManager = actionConfirmationManager;
        mGroupIdToHideSupplier.addObserver((newIdToHide) -> rebuildStripViews());

        mIsFirstLayoutPass = true;

        mTabDelegate = new StripLayoutTabDelegate(mUpdateHost);
    }

    /** Cleans up internal state. An instance should not be used after this method is called. */
    @SuppressWarnings("NullAway")
    public void destroy() {
        mStripTabEventHandler.removeCallbacksAndMessages(null);
        mLastHoveredTab = null;
        if (mTabHoverCardView != null) {
            mTabHoverCardView.destroy();
            mTabHoverCardView = null;
        }
        if (mDataSharingService != null) {
            mDataSharingService.removeObserver(mDataSharingObserver);
            mDataSharingService = null;
        }
        if (mTabGroupModelFilter != null) {
            mTabGroupModelFilter.removeTabGroupObserver(mTabGroupModelFilterObserver);
            mTabGroupModelFilter = null;
        }
        if (mTabGroupContextMenuCoordinator != null) {
            mTabGroupContextMenuCoordinator.destroy();
            mTabGroupContextMenuCoordinator = null;
        }
        if (mTabGroupSyncService != null) {
            mTabGroupSyncService.removeObserver(mTabGroupSyncObserver);
            mTabGroupSyncService = null;
        }
        if (mTabContextMenuCoordinator != null) {
            mTabContextMenuCoordinator.dismiss();
            mTabContextMenuCoordinator = null;
        }
        if (mTabGroupListBottomSheetCoordinator != null) {
            mTabGroupListBottomSheetCoordinator.destroy();
            mTabGroupListBottomSheetCoordinator = null;
        }
        if (mModel != null) {
            mModel.removeObserver(mTabModelObserver);
            mModel = null;
        }
    }

    /**
     * Get a list of virtual views for accessibility.
     *
     * @param views A List to populate with virtual views.
     */
    public void getVirtualViews(List<VirtualView> views) {
        for (int i = 0; i < mStripViews.length; i++) {
            final StripLayoutView view = mStripViews[i];
            view.getVirtualViews(views);
        }
        if (mNewTabButton.isVisible()) mNewTabButton.getVirtualViews(views);
    }

    /**
     * Get the touchable area within the strip, presented as a {@link RectF}, where (0,0) is the
     * top-left point of the StripLayoutHelper. The area will include the tabs, but not the new tab
     * button.
     */
    RectF getTouchableRect() {
        return mTouchableRect;
    }

    /** Returns the visually ordered list of visible {@link StripLayoutTab}s. */
    public StripLayoutTab[] getStripLayoutTabsToRender() {
        return mStripTabsToRender;
    }

    /** Returns the visually ordered list of visible {@link StripLayoutGroupTitle}s. */
    public StripLayoutGroupTitle[] getStripLayoutGroupTitlesToRender() {
        return mStripGroupTitlesToRender;
    }

    /**
     * Returns a {@link TintedCompositorButton} that represents the positioning of the new tab
     * button.
     */
    public TintedCompositorButton getNewTabButton() {
        return mNewTabButton;
    }

    /**
     * @param isPinned Whether the tab has been pinned.
     * @return The effective width of a tab (accounting for overlap).
     */
    private float getEffectiveTabWidth(boolean isPinned) {
        return getCachedTabWidth(isPinned) - TAB_OVERLAP_WIDTH_DP;
    }

    /** Returns the total effective width of pinned tabs. */
    protected float getTotalPinnedTabsWidth() {
        float width = 0.f;
        for (StripLayoutTab tab : mStripTabs) {
            if (!tab.getIsPinned()) break;
            if (isLiveTab(tab)) {
                width += getEffectiveTabWidth(/* isPinned= */ true) + tab.getTrailingMargin();
            }
        }
        return width == 0.f ? 0.f : width + mScrollDelegate.getReorderStartMargin();
    }

    /** Returns the pinned tabs boundary on tab strip. */
    private float getPinnedTabsBoundary() {
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        return getStartPositionForStripViews()
                + (isRtl ? -getTotalPinnedTabsWidth() : getTotalPinnedTabsWidth());
    }

    /** Returns the visual offset to be applied to the new tab button. */
    protected float getNewTabButtonVisualOffset() {
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        float newTabButtonTouchTargetOffset;
        if (isTabStripFull()) {
            newTabButtonTouchTargetOffset = 0;
        } else {
            newTabButtonTouchTargetOffset = getNtbVisualOffsetHorizontal();
        }
        return isRtl ? newTabButtonTouchTargetOffset : -newTabButtonTouchTargetOffset;
    }

    /**
     * Returns whether the tab strip is full by checking whether tab width has decreased to fit more
     * tabs.
     */
    private boolean isTabStripFull() {
        return getCachedTabWidth(/* isPinned= */ false) < MAX_TAB_WIDTH_DP;
    }

    /**
     * Determine how far to shift new tab button icon visually towards the tab in order to achieve
     * the desired spacing between new tab button and tabs when tab strip is not full.
     *
     * @return Visual offset of new tab button icon.
     */
    protected float getNtbVisualOffsetHorizontal() {
        return Math.max(
                (BUTTON_TOUCH_TARGET_SIZE_DP - mNewTabButtonWidth) / 2
                        - DESIRED_PADDING_BETWEEN_NEW_TAB_BUTTON_AND_TABS,
                0);
    }

    /** Returns the opacity to use for the fade on the left side of the tab strip. */
    public float getLeftFadeOpacity() {
        return getFadeOpacity(true);
    }

    /** Returns the opacity to use for the fade on the right side of the tab strip. */
    public float getRightFadeOpacity() {
        return getFadeOpacity(false);
    }

    /**
     * When the {@link ScrollingStripStacker} is being used, a fade is shown at the left and right
     * edges to indicate there is tab strip content off screen. As the scroll position approaches
     * the edge of the screen, the fade opacity is lowered.
     *
     * @param isLeft Whether the opacity for the left or right side should be returned.
     * @return The opacity to use for the fade.
     */
    private float getFadeOpacity(boolean isLeft) {
        // If there isn’t enough room to show even a single unpinned tab (pinned-only strip), force
        // show the end edge fade, because the end fade is used to mask the cut-off tab area so it
        // doesn't appear under the NTB.
        boolean rtl = LocalizationUtils.isLayoutRtl();
        boolean isEndFade = rtl ? isLeft : !isLeft;
        if (doPinnedTabsOccupyEntireVisibleArea() && isEndFade) return 1.f;

        float edgeOffset = mScrollDelegate.getEdgeOffset(isLeft);

        // Force start fade to be fully opaque when pinned tabs exist so the first unpinned divider
        // doesn't show through during scrolling.
        boolean shouldForceStartFadeOpacity = !isEndFade && getTotalPinnedTabsWidth() > 0;
        if (edgeOffset <= 0.f) {
            return 0.f;
        } else if (edgeOffset >= FADE_FULL_OPACITY_THRESHOLD_DP || shouldForceStartFadeOpacity) {
            return 1.f;
        } else {
            return edgeOffset / FADE_FULL_OPACITY_THRESHOLD_DP;
        }
    }

    private boolean doPinnedTabsOccupyEntireVisibleArea() {
        // Return false if tab strip is still initializing and `mWidth` is 0.
        if (mWidth == 0) return false;
        return getStripWidthForResizing() - getTotalPinnedTabsWidth()
                < mCachedTabWidthSupplier.get();
    }

    /**
     * Returns the strip's current scroll offset. It's a 1-D vector on the X axis under the dynamic
     * coordinate system used by {@link ScrollDelegate}.
     */
    float getScrollOffset() {
        return mScrollDelegate.getScrollOffset();
    }

    /**
     * Returns the visible left bound of the tab strip for pinned or unpinned views. Pinned views
     * begin at {@code mLeftPadding} and remain fixed (do not scroll), while unpinned views scroll
     * and are positioned after the pinned tabs. Pass {@code false} for the entire tab strip bound,
     * or {@code true} for the scrolling portion.
     *
     * @param clampToUnpinnedViews true to return the bound for unpinned views; false for pinned
     *     views.
     * @return the tab strip's visible left bound.
     */
    float getVisibleLeftBound(boolean clampToUnpinnedViews) {
        if (!clampToUnpinnedViews) {
            return mLeftPadding;
        }
        return mLeftPadding + (LocalizationUtils.isLayoutRtl() ? 0.f : getTotalPinnedTabsWidth());
    }

    /**
     * See {@link #getVisibleLeftBound(boolean)} for details on difference between pinned and
     * unpinned bounds.
     *
     * @param clampToUnpinnedViews true to return the bound for unpinned views; false for pinned
     *     views.
     * @return the tab strip's visible right bound.
     */
    float getVisibleRightBound(boolean clampToUnpinnedViews) {
        float baseRightBound = mWidth - mRightPadding;
        if (!clampToUnpinnedViews) {
            return baseRightBound;
        }
        return baseRightBound - (LocalizationUtils.isLayoutRtl() ? getTotalPinnedTabsWidth() : 0.f);
    }

    /** Returns tab strip's visible left padding accounting for pinned tab background. */
    protected float getLeftPaddingToDraw() {
        return mLeftPadding + (LocalizationUtils.isLayoutRtl() ? 0.f : getTotalPinnedTabsWidth());
    }

    /** Returns tab strip's visible right padding accounting for pinned tab background. */
    protected float getRightPaddingToDraw() {
        return mRightPadding + (LocalizationUtils.isLayoutRtl() ? getTotalPinnedTabsWidth() : 0.f);
    }

    /**
     * @param msbTouchTargetSize The touch target size for the model selector button.
     */
    public void updateEndMarginForStripButtons(float msbTouchTargetSize) {
        // When MSB is not visible we add strip end padding here. When MSB is visible strip end
        // padding will be included in MSB margin, so just add padding between NTB and MSB here.
        mReservedEndMargin =
                msbTouchTargetSize
                        + mNewTabButtonWidth
                        + (mModelSelectorButton != null && mModelSelectorButton.isVisible()
                                ? NEW_TAB_BUTTON_WITH_MODEL_SELECTOR_BUTTON_PADDING
                                : mFixedEndPadding);
        updateMargins(true);
    }

    private void updateMargins(boolean recalculateTabWidth) {
        if (LocalizationUtils.isLayoutRtl()) {
            mLeftMargin = mReservedEndMargin + mLeftPadding;
            mRightMargin = mRightPadding;
        } else {
            mLeftMargin = mLeftPadding;
            mRightMargin = mReservedEndMargin + mRightPadding;
        }
        if (recalculateTabWidth) {
            finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
            computeAndUpdateTabWidth(/* animate= */ false, /* deferAnimations= */ false);
        }
    }

    /**
     * Sets the left fade width based on which fade is showing.
     *
     * @param fadeWidth The width of the left fade.
     */
    public void setLeftFadeWidth(float fadeWidth) {
        if (mLeftFadeWidth != fadeWidth) {
            mLeftFadeWidth = fadeWidth;
            bringSelectedTabToVisibleArea(LayoutManagerImpl.time(), false);
        }
    }

    /**
     * Sets the right fade width based on which fade is showing.
     *
     * @param fadeWidth The width of the right fade.
     */
    public void setRightFadeWidth(float fadeWidth) {
        if (mRightFadeWidth != fadeWidth) {
            mRightFadeWidth = fadeWidth;
            bringSelectedTabToVisibleArea(LayoutManagerImpl.time(), false);
        }
    }

    /**
     * Updates the size of the virtual tab strip, making the tabs resize and move accordingly.
     *
     * @param width The new available width.
     * @param height The new height this stack should be.
     * @param orientationChanged Whether the screen orientation was changed.
     * @param time The current time of the app in ms.
     * @param leftPadding The new left padding.
     * @param rightPadding The new right padding.
     * @param topPadding The new top padding.
     */
    public void onSizeChanged(
            float width,
            float height,
            boolean orientationChanged,
            long time,
            float leftPadding,
            float rightPadding,
            float topPadding) {
        if (mWidth == width
                && mHeight == height
                && leftPadding == mLeftPadding
                && rightPadding == mRightPadding
                && topPadding == mTopPadding) {
            return;
        }

        StripLayoutTab selectedTab = getSelectedStripTab();
        boolean wasSelectedTabVisible = selectedTab != null && selectedTab.isVisible();
        boolean recalculateTabWidth =
                mWidth != width || mLeftPadding != leftPadding || mRightPadding != rightPadding;

        mWidth = width;
        mHeight = height;
        mLeftPadding = leftPadding;
        mRightPadding = rightPadding;
        boolean topPaddingChanged = topPadding != mTopPadding;
        mTopPadding = topPadding;

        for (int i = 0; i < mStripViews.length; i++) {
            final StripLayoutView view = mStripViews[i];
            if (topPaddingChanged) {
                view.setTouchTargetInsets(null, mTopPadding, null, -mTopPadding);
            }
            view.setHeight(mHeight);
        }

        if (topPaddingChanged) {
            mNewTabButton.setTouchTargetInsets(null, mTopPadding, null, -mTopPadding);
        }

        updateMargins(recalculateTabWidth);
        if (mStripViews.length > 0) mUpdateHost.requestUpdate();

        // Dismiss tab menu, similar to how the app menu is dismissed on orientation change
        mCloseButtonMenu.dismiss();

        // Dismiss iph on orientation change, as its position might become incorrect.
        dismissTabStripSyncIph();

        if ((orientationChanged && wasSelectedTabVisible) || !mTabStateInitialized) {
            bringSelectedTabToVisibleArea(time, mTabStateInitialized);
        }

        recordPinnedOnlyTabStripUserAction();
    }

    /**
     * Notify the a title has changed.
     *
     * @param tabId The id of the tab that has changed.
     * @param title The new title.
     */
    public void tabTitleChanged(int tabId, String title) {
        Tab tab = getTabById(tabId);
        if (tab != null) setAccessibilityDescription(findTabById(tabId), title, tab.isHidden());
    }

    /**
     * Sets the {@link TabModel} that this {@link StripLayoutHelper} will visually represent.
     *
     * @param model The {@link TabModel} to visually represent.
     * @param tabCreator The {@link TabCreator}, used to create new tabs.
     * @param tabStateInitialized Whether the tab model's tab state is fully initialized after
     *     startup or not.
     */
    public void setTabModel(TabModel model, TabCreator tabCreator, boolean tabStateInitialized) {
        if (mModel == model) return;
        mModel = model;
        mTabCreator = tabCreator;
        mTabStateInitialized = tabStateInitialized;
        // If the tabs are still restoring and the refactoring experiment is enabled, we'll create a
        // placeholder strip. This means we don't need to call rebuildStripTabs() to generate "real"
        // strip tabs.
        if (!mTabStateInitialized) {
            // If the placeholder strip is ready, replace the matching placeholders for the tabs
            // that have already been restored.
            mSelectedOnStartup = mModel.isActiveModel();
            if (mPlaceholderStripReady) replacePlaceholdersForRestoredTabs();
        } else {
            RecordHistogram.deprecatedRecordMediumTimesHistogram(
                    PLACEHOLDER_VISIBLE_DURATION_HISTOGRAM_NAME, 0L);

            rebuildStripTabs(/* deferAnimations= */ false);
            finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
            computeAndUpdateTabWidth(/* animate= */ false, /* deferAnimations= */ false);
        }
        if (getSelectedTabId() != Tab.INVALID_TAB_ID) {
            tabSelected(LayoutManagerImpl.time(), getSelectedTabId(), Tab.INVALID_TAB_ID);
        }
        mModel.addObserver(mTabModelObserver);
    }

    /** Called to notify that the tab state has been initialized. */
    protected void onTabStateInitialized() {
        mTabStateInitialized = true;

        if (mPlaceholderStripReady) {
            int numLeftoverPlaceholders = 0;
            for (int i = 0; i < mStripTabs.length; i++) {
                StripLayoutTab stripTab = mStripTabs[i];
                if (stripTab.getIsPlaceholder()) numLeftoverPlaceholders++;
                assumeNonNull(mModel);
                Tab tab = mModel.getTabById(stripTab.getTabId());
                if (tab != null) {
                    stripTab.setIsPinned(tab.getIsPinned());
                }
            }
            mPinnedTabsBoundarySupplier.set(getPinnedTabsBoundary());

            RecordHistogram.recordCount1000Histogram(
                    PLACEHOLDER_LEFTOVER_TABS_HISTOGRAM_NAME, numLeftoverPlaceholders);
            RecordHistogram.recordCount1000Histogram(
                    PLACEHOLDER_TABS_CREATED_DURING_RESTORE_HISTOGRAM_NAME,
                    mTabsCreatedDuringRestore);
            RecordHistogram.recordCount1000Histogram(
                    PLACEHOLDER_TABS_NEEDED_DURING_RESTORE_HISTOGRAM_NAME,
                    mPlaceholdersNeededDuringRestore);
            RecordHistogram.deprecatedRecordMediumTimesHistogram(
                    PLACEHOLDER_VISIBLE_DURATION_HISTOGRAM_NAME,
                    SystemClock.uptimeMillis() - mPlaceholderCreationTime);
        }

        // Recreate the StripLayoutTabs from the TabModel, now that all of the real Tabs have been
        // restored. This will reuse valid tabs, discard invalid tabs, and correct tab orders.
        rebuildStripTabs(/* deferAnimations= */ false);
        if (getSelectedTabId() != Tab.INVALID_TAB_ID) {
            tabSelected(LayoutManagerImpl.time(), getSelectedTabId(), Tab.INVALID_TAB_ID);
        }
    }

    /**
     * Sets the {@link TabGroupModelFilter} that will access the internal tab group state.
     *
     * @param tabGroupModelFilter The {@link TabGroupModelFilter}.
     */
    public void setTabGroupModelFilter(TabGroupModelFilter tabGroupModelFilter) {
        if (mTabGroupModelFilter != null) {
            mTabGroupModelFilter.removeTabGroupObserver(mTabGroupModelFilterObserver);
        }

        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupModelFilter.addTabGroupObserver(mTabGroupModelFilterObserver);

        Profile profile = tabGroupModelFilter.getTabModel().getProfile();
        mReorderDelegate.initialize(
                /* animationHost= */ this,
                /* stripUpdateDelegate= */ this,
                mTabGroupModelFilter,
                mScrollDelegate,
                mTabStripDragHandler,
                mActionConfirmationManager,
                mCachedTabWidthSupplier,
                mPinnedTabsBoundarySupplier,
                mGroupIdToHideSupplier,
                mToolbarContainerView);

        if (profile != null && !profile.isOffTheRecord()) {
            mTabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        }

        // Note that profile could be null for incognito if there are no incognito tabs. The
        // DataSharingObserver is added before tabs and groups are created on tab strip, so we can
        // listen to collaboration change as soon as the tab strip is initialized.
        // TODO(crbug.com/380511640) Use SharedGroupObserver instead of DataSharingObserver.
        if (shouldEnableGroupSharing()) {
            mDataSharingService = DataSharingServiceFactory.getForProfile(assumeNonNull(profile));
            mCollaborationService = CollaborationServiceFactory.getForProfile(profile);
            mTabGroupSyncObserver =
                    new TabGroupSyncService.Observer() {
                        @Override
                        public void onTabGroupLocalIdChanged(
                                String syncTabGroupId, @Nullable LocalTabGroupId localTabGroupId) {
                            if (localTabGroupId == null) return;

                            // The group title can be null in split-screen since the sync service
                            // notifies both screens of a group localId change, but only one screen
                            // contains the group.

                            @Nullable StripLayoutGroupTitle groupTitle =
                                    findGroupTitle(localTabGroupId.tabGroupId);
                            if (groupTitle == null) return;

                            updateSharedTabGroupIfNeeded(groupTitle);
                        }

                        @Override
                        public void onTabGroupUpdated(
                                SavedTabGroup group, @TriggerSource int source) {
                            if (group == null || group.localId == null) return;
                            GroupData groupData =
                                    mCollaborationService.getGroupData(group.collaborationId);
                            StripLayoutGroupTitle groupTitle =
                                    StripLayoutUtils.findGroupTitle(
                                            mStripGroupTitles, group.localId.tabGroupId);
                            updateOrClearSharedState(groupData, groupTitle);
                        }
                    };
            mTabGroupSyncService.addObserver(mTabGroupSyncObserver);
            mDataSharingObserver =
                    new DataSharingService.Observer() {
                        @Override
                        public void onGroupChanged(GroupData groupData) {
                            if (mTabGroupSyncService == null) return;
                            updateOrClearSharedState(groupData, mTabGroupSyncService);
                        }

                        @Override
                        public void onGroupAdded(GroupData groupData) {
                            if (mTabGroupSyncService == null) return;
                            updateOrClearSharedState(groupData, mTabGroupSyncService);
                        }

                        @Override
                        public void onGroupRemoved(String collaborationId) {
                            if (mTabGroupSyncService == null) return;
                            StripLayoutGroupTitle groupTitle =
                                    StripLayoutUtils.findGroupTitleByCollaborationId(
                                            mStripGroupTitles,
                                            collaborationId,
                                            mTabGroupSyncService);
                            if (groupTitle == null) return;
                            clearSharedTabGroup(groupTitle);
                        }
                    };
            mDataSharingService.addObserver(mDataSharingObserver);
        }

        // Prepare to show tab strip IPH for tab group sync and share notification bubble. Skip
        // initialization if testing value has been set.
        if (mTabStripIphController == null
                && !mIncognito
                && mModel != null
                && mModel.getProfile() != null) {
            Profile modelProfile = mModel.getProfile();
            UserEducationHelper userEducationHelper =
                    new UserEducationHelper(
                            assumeNonNull(mWindowAndroid.getActivity().get()),
                            modelProfile,
                            new Handler(Looper.getMainLooper()));
            Tracker tracker = TrackerFactory.getTrackerForProfile(modelProfile);
            mTabStripIphController =
                    new TabStripIphController(
                            mContext.getResources(), userEducationHelper, tracker);
        }

        updateTitleCacheForInit();
        rebuildStripViews();
    }

    @EnsuresNonNullIf("mTabGroupSyncService")
    private boolean shouldEnableGroupSharing() {
        if (mTabGroupModelFilter == null) return false;
        Profile profile = mTabGroupModelFilter.getTabModel().getProfile();
        if (profile == null || profile.isOffTheRecord() || mTabGroupSyncService == null) {
            return false;
        }
        CollaborationService collaborationService =
                CollaborationServiceFactory.getForProfile(profile);
        return collaborationService.getServiceStatus().isAllowedToJoin();
    }

    TabGroupModelFilterObserver getTabGroupModelFilterObserverForTesting() {
        return mTabGroupModelFilterObserver;
    }

    /**
     * Sets the {@link LayerTitleCache} for the tab strip bitmaps.
     *
     * @param layerTitleCache The {@link LayerTitleCache}.
     */
    public void setLayerTitleCache(LayerTitleCache layerTitleCache) {
        mLayerTitleCache = layerTitleCache;
        updateTitleCacheForInit();
        mRenderHost.requestRender();
    }

    private void updateTitleCacheForInit() {
        if (mTabGroupModelFilter == null || mLayerTitleCache == null) return;

        for (int i = 0; i < mStripGroupTitles.length; ++i) {
            final StripLayoutGroupTitle groupTitle = mStripGroupTitles[i];
            updateGroupTextAndSharedStateUnchecked(groupTitle);
        }
    }

    /** Dismiss iph on the tab strip. */
    private void dismissTabStripSyncIph() {
        if (mTabStripIphController != null) {
            mTabStripIphController.dismissTextBubble();
        }
    }

    /**
     * Helper-specific updates. Cascades the values updated by the animations and flings.
     *
     * @param time The current time of the app in ms.
     * @return Whether or not animations are done.
     */
    public boolean updateLayout(long time) {
        // 1.a. Handle any Scroller movements (flings).
        if (mScrollDelegate.updateScrollInProgress(time)) {
            // 1.b. Scroll still in progress, so request update.
            mUpdateHost.requestUpdate();
        }

        // 2. Handle reordering automatically scrolling the tab strip.
        handleReorderAutoScrolling(time);

        // 3. Update tab spinners.
        updateSpinners(time);

        queueCloseAnimationsIfAny();
        startQueuedAnimationsIfAny();
        updateStrip();

        // If this is the first layout pass, scroll to the selected tab so that it is visible.
        // This is needed if the ScrollingStripStacker is being used because the selected tab is
        // not guaranteed to be visible.
        if (mIsFirstLayoutPass) {
            bringSelectedTabToVisibleArea(time, false);
            mIsFirstLayoutPass = false;
        }

        // Show IPH on the last synced tab group, so place it at the front of the queue.
        if (mLastSyncedGroupIdForIph != null
                && (mTabStripIphController != null
                        && mTabStripIphController.wouldTriggerIph(IphType.TAB_GROUP_SYNC))) {
            final StripLayoutGroupTitle groupTitle = findGroupTitle(mLastSyncedGroupIdForIph);
            mQueuedIphList.add(
                    0,
                    () ->
                            attemptToShowTabStripIph(
                                    groupTitle,
                                    /* tab= */ null,
                                    IphType.TAB_GROUP_SYNC,
                                    /* enableSnoozeMode= */ false));
            mLastSyncedGroupIdForIph = null;
        }

        // 4. Attempt to show one iph text bubble at a time on tab strip.
        final boolean doneAnimating = mRunningAnimator == null || !mRunningAnimator.isRunning();
        if (doneAnimating && mScrollDelegate.isFinished()) {
            Iterator<QueuedIph> iterator = mQueuedIphList.iterator();
            while (iterator.hasNext()) {
                QueuedIph iphCallback = iterator.next();
                // Remove iph callback from list when the run is successful.
                if (iphCallback.attemptToShow()) {
                    iterator.remove();
                    break;
                }
            }
        }
        return doneAnimating;
    }

    private void recordPinnedOnlyTabStripUserAction() {
        if (isTabPinningFromStripEnabled()
                && !mIsPinnedOnlyStripRecorded
                && doPinnedTabsOccupyEntireVisibleArea()) {
            mIsPinnedOnlyStripRecorded = true;
            RecordUserAction.record("MobileToolbarPinnedOnlyTabStripSkipInit");
        }
    }

    /**
     * Attempt to show IPH for a group title or a tab.
     *
     * @param groupTitle The group title or its related tab where the IPH should be shown.
     * @param tab The tab to show the IPH on. Pass in {@code null} if the IPH is not tied to a
     *     particular tab.
     * @param iphType The type of the IPH to be shown.
     * @param enableSnoozeMode Whether to enable snooze mode on the IPH.
     * @return true if {@code showIphOnTabStrip} should be executed immediately; false to retry at a
     *     later time.
     */
    // TODO:(crbug.com/375271955) Ensure sync IPH doesn't show when joining a collaboration group.
    private boolean attemptToShowTabStripIph(
            @Nullable StripLayoutGroupTitle groupTitle,
            @Nullable StripLayoutTab tab,
            @IphType int iphType,
            boolean enableSnoozeMode) {
        if (mModel == null || mTabStripIphController == null) return false;
        // Remove the showTabStrip callback from the queue, as showing IPH is not applicable in
        // these cases.
        if (mModel.isIncognito()
                || mModel.getProfile() == null
                || (tab == null && groupTitle == null)
                || !mTabStripIphController.wouldTriggerIph(iphType)) {
            return true;
        }
        // Return early if the tab strip is not visible on screen.
        if (Boolean.FALSE.equals(mTabStripVisibleSupplier.get())) {
            return false;
        }

        // Display iph only when the target view is fully visible.
        StripLayoutView view = assumeNonNull(tab == null ? (StripLayoutView) groupTitle : tab);
        if (!view.isVisible() || !isViewCompletelyVisible(view)) {
            return false;
        }

        mTabStripIphController.showIphOnTabStrip(
                groupTitle, tab, mToolbarContainerView, iphType, mHeight, enableSnoozeMode);
        return true;
    }

    void setLastSyncedGroupIdForTesting(@Nullable Token tabGroupId) {
        mLastSyncedGroupIdForIph = tabGroupId;
    }

    void setTabStripIphControllerForTesting(TabStripIphController tabStripIphController) {
        mTabStripIphController = tabStripIphController;
    }

    void setIsFirstLayoutPassForTesting(boolean isFirstLayoutPass) {
        mIsFirstLayoutPass = isFirstLayoutPass;
    }

    /**
     * Called when a new tab model is selected.
     *
     * @param selected If the new tab model selected is the model that this strip helper associated
     *     with.
     */
    public void tabModelSelected(boolean selected) {
        mSelected = selected;
        if (selected) {
            bringSelectedTabToVisibleArea(0, false);
        } else {
            clearLastHoveredTab();
            finishAnimations();
            mCloseButtonMenu.dismiss();
        }
    }

    /** Marks the helper as selected. */
    public void setSelected(boolean selected) {
        mSelected = selected;
    }

    /** Updates the current selected tab. */
    private void updateSelectedTab(int newFocusedTabId, int previouslyFocusedTabId) {
        StripLayoutTab previouslyFocusedTab = findTabById(previouslyFocusedTabId);
        if (previouslyFocusedTab != null) {
            mTabDelegate.setIsTabSelected(previouslyFocusedTab, false);
        }
        StripLayoutTab newFocusedTab = findTabById(newFocusedTabId);
        if (newFocusedTab != null) {
            mTabDelegate.setIsTabSelected(newFocusedTab, true);
        }
    }

    /**
     * Called when a tab get selected.
     *
     * @param time The current time of the app in ms.
     * @param id The id of the selected tab.
     * @param prevId The id of the previously selected tab.
     */
    public void tabSelected(long time, int id, int prevId) {
        StripLayoutTab stripTab = findTabById(id);
        if (stripTab == null) {
            tabCreated(time, id, prevId, true, false, false);
            updateSelectedTab(id, prevId);
        } else {
            updateSelectedTab(id, prevId);
            updateCloseButtons();

            Tab tab = getTabById(id);
            if (tab != null
                    && mTabGroupModelFilter != null
                    && tab.getTabGroupId() != null
                    && mTabGroupModelFilter.getTabGroupCollapsed(tab.getTabGroupId())) {
                mTabGroupModelFilter.deleteTabGroupCollapsed(tab.getTabGroupId());
            }

            if (!mReorderDelegate.getInReorderMode()) {
                // If the tab was selected through a method other than the user tapping on the
                // strip, it may not be currently visible. Scroll if necessary.
                bringSelectedTabToVisibleArea(time, true);
            }

            mUpdateHost.requestUpdate();

            setAccessibilityDescription(stripTab, getTabById(id));
            if (prevId != Tab.INVALID_TAB_ID) {
                setAccessibilityDescription(findTabById(prevId), getTabById(prevId));
            }
        }
    }

    /**
     * Called when a tab has been moved in the tabModel.
     *
     * @param id The id of the Tab.
     * @param oldIndex The old index of the tab in the {@link TabModel}.
     * @param newIndex The new index of the tab in the {@link TabModel}.
     */
    public void tabMoved(int id, int oldIndex, int newIndex) {
        // See crbug.com/455498650. When re-parenting a tab group, the sequence of events is such
        // that we move a tab (out of the group), then attempt to close it. The delayed close
        // animations need to be completed between each event to avoid interacting with stale state.
        if (mCloseAnimationsRequested) finishAnimations();
        StripLayoutTab tab = findTabById(id);
        if (tab == null || oldIndex == newIndex) return;

        // 1. If the tab is already at the right spot, don't do anything.
        int index = findIndexForTab(id);
        if (index == newIndex || index + 1 == newIndex) return;

        // 2. Swap the tabs.
        StripLayoutUtils.moveElement(mStripTabs, index, newIndex);
        if (!mMovingGroup) rebuildStripViewsAfterMove();
    }

    /**
     * Called when a tab will be closed. When called, the closing tab will be part of the model.
     *
     * @param tab The tab that will be closed.
     */
    public void willCloseTab(Tab tab) {
        if (tab == null) return;
        updateGroupTextAndSharedState(tab.getTabGroupId());
        onWillCloseView(findTabById(tab.getId()));
    }

    /**
     * Called when a tab is being closed. When called, the closing tab will not be part of the
     * model.
     *
     * @param tab The {@link Tab} being closed.
     */
    public void tabClosed(Tab tab) {
        if (findTabById(tab.getId()) == null) return;
        multipleTabsClosed(Collections.singletonList(tab));
    }

    /**
     * Called when multiple tabs are being closed. When called, the closing tabs will not be part of
     * the model.
     *
     * @param tabs The list of tabs that are being closed.
     */
    public void multipleTabsClosed(List<Tab> tabs) {
        for (Tab tab : tabs) {
            StripLayoutTab stripTab = findTabById(tab.getId());
            if (stripTab != null && !stripTab.isDying()) {
                mClosingTabs.add(stripTab);
                stripTab.setSkipAsyncClosure(/* skipAsyncClosure= */ true);
            }
        }
        if (!mClosingTabs.isEmpty()) {
            requestCloseAnimations();
        } else {
            // TODO(crbug.com/443337907): Can be removed once we update the tab strip close buttons
            //  to this new animation method.
            rebuildStripTabs(/* deferAnimations= */ false);
            clearPendingMouseTabClosureState();
        }
    }

    /** Called when all tabs are closed at once. */
    public void willCloseAllTabs() {
        rebuildStripTabs(/* deferAnimations= */ false);
    }

    /**
     * Called when a tab close has been undone and the tab has been restored. This also re-selects
     * the last tab the user was on before the tab was closed.
     *
     * @param time The current time of the app in ms.
     * @param id The id of the Tab.
     */
    public void tabClosureCancelled(long time, int id) {
        if (mModel == null) return;
        finishAnimations();
        final boolean selected = TabModelUtils.getCurrentTabId(mModel) == id;
        tabCreated(time, id, Tab.INVALID_TAB_ID, selected, true, false);
        updateGroupTextAndSharedState(mModel.getTabByIdChecked(id).getTabGroupId());
    }

    /**
     * Called when a tab is created from the top left button.
     * @param time             The current time of the app in ms.
     * @param id               The id of the newly created tab.
     * @param prevId           The id of the source tab.
     * @param selected         Whether the tab will be selected.
     * @param closureCancelled Whether the tab was restored by a tab closure cancellation.
     * @param onStartup        Whether the tab is being unfrozen during startup.
     */
    public void tabCreated(
            long time,
            int id,
            int prevId,
            boolean selected,
            boolean closureCancelled,
            boolean onStartup) {
        if (findTabById(id) != null) return;

        // 1. If tab state is still initializing, replace the matching placeholder tab.
        if (!mTabStateInitialized) {
            replaceNextPlaceholder(id, selected, onStartup);

            return;
        }

        // Otherwise, 2. Build any tabs that are missing. Determine if it will be collapsed.
        finishAnimationsAndPushTabUpdates();
        List<Animator> animationList = rebuildStripTabs(!onStartup);
        Tab tab = getTabById(id);
        boolean collapsed = false;
        if (tab != null) {
            Token tabGroupId = tab.getTabGroupId();
            updateGroupTextAndSharedState(tabGroupId);
            if (tabGroupId != null
                    && mTabGroupModelFilter != null
                    && mTabGroupModelFilter.getTabGroupCollapsed(tabGroupId)) {
                if (selected) {
                    mTabGroupModelFilter.deleteTabGroupCollapsed(tabGroupId);
                } else {
                    collapsed = true;
                }
            }
        }

        // 3. Start an animation for the newly created tab, unless it is collapsed.
        if (animationList == null) animationList = new ArrayList<>();
        StripLayoutTab stripTab = findTabById(id);
        if (stripTab != null) {
            updateTabCollapsed(stripTab, collapsed, false);
            if (!onStartup && !collapsed) {
                runTabAddedAnimator(animationList, stripTab, /* fromTabCreation= */ true);
            }
        }

        // 4. If the new tab will be selected, scroll it to view. If the new tab will not be
        // selected, scroll the currently selected tab to view. Skip auto-scrolling if the tab is
        // being created due to a tab closure being undone.
        if (stripTab != null && !closureCancelled && !collapsed) {
            boolean animate = !onStartup;
            if (selected) {
                float delta = calculateDeltaToMakeViewVisible(stripTab);
                setScrollForScrollingTabStacker(
                        delta, /* isDeltaHorizontal= */ true, animate, time);
            } else {
                bringSelectedTabToVisibleArea(time, animate);
            }
        }

        // 5. Trigger IPH for tab tearing on XR if applicable.
        if (DeviceInfo.isXr()
                && mStripTabs.length > 1
                && !onStartup
                && !closureCancelled
                && stripTab != null) {
            mQueuedIphList.add(
                    () ->
                            attemptToShowTabStripIph(
                                    /* groupTitle */ null,
                                    stripTab,
                                    IphType.TAB_TEARING_XR,
                                    /* enableSnoozeMode= */ true));
        }

        mUpdateHost.requestUpdate();
    }

    private void runTabAddedAnimator(
            List<Animator> animationList, StripLayoutTab tab, boolean fromTabCreation) {
        if (!ChromeFeatureList.sTabletTabStripAnimation.isEnabled() || !fromTabCreation) {
            animationList.add(
                    CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            tab,
                            StripLayoutTab.Y_OFFSET,
                            tab.getHeight(),
                            0f,
                            ANIM_TAB_CREATED_MS));
        }

        startAnimations(animationList);
    }

    /**
     * Set the relevant tab model metadata prior to the tab state initialization.
     *
     * @param activeTabIndexOnStartup What the active tab index should be after tabs finish
     *     restoring.
     * @param tabCountOnStartup What the tab count should be after tabs finish restoring.
     * @param createdTabOnStartup If an additional tab was created on startup (e.g. through intent).
     */
    protected void setTabModelStartupInfo(
            int tabCountOnStartup, int activeTabIndexOnStartup, boolean createdTabOnStartup) {
        mTabCountOnStartup = tabCountOnStartup;
        mActiveTabIndexOnStartup = activeTabIndexOnStartup;
        mCreatedTabOnStartup = createdTabOnStartup;

        // Avoid creating the placeholder strip if we have an invalid active tab index.
        if (mActiveTabIndexOnStartup < 0 || mActiveTabIndexOnStartup >= mTabCountOnStartup) return;

        // If tabs are still being restored on startup, create placeholder tabs to mitigate jank.
        if (!mTabStateInitialized) {
            prepareEmptyPlaceholderStripLayout();

            // If the TabModel has already been set, then replace placeholders for restored tabs.
            if (mModel != null) replacePlaceholdersForRestoredTabs();
        }
    }

    /**
     * Creates the placeholder tabs that will be shown on startup before the tab state is
     * initialized.
     */
    private void prepareEmptyPlaceholderStripLayout() {
        // TODO(crbug.com/41497111): Investigate if we can update for tab group indicators.
        if (mPlaceholderStripReady || mTabStateInitialized) return;

        // 1. Fill with placeholder tabs.
        mStripTabs = new StripLayoutTab[mTabCountOnStartup];
        for (int i = 0; i < mStripTabs.length; i++) {
            mStripTabs[i] = createPlaceholderStripTab();
        }
        rebuildStripViews();

        // 2. Initialize the draw parameters.
        finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
        computeAndUpdateTabWidth(false, false);

        // 3. Scroll the strip to bring the selected tab to view and ensure that the active tab
        // container is visible.
        if (mActiveTabIndexOnStartup != TabModel.INVALID_TAB_INDEX) {
            bringSelectedTabToVisibleArea(LayoutManagerImpl.time(), false);
            StripLayoutTabDelegate.setTabVisibility(
                    mStripTabs[mActiveTabIndexOnStartup], /* isVisible= */ true);
        }

        // 4. Mark that the placeholder strip layout is ready and request a visual update.
        mPlaceholderStripReady = true;
        mPlaceholderCreationTime = SystemClock.uptimeMillis();
        mUpdateHost.requestUpdate();
    }

    /**
     * Replace placeholders for all tabs that have already been restored. Do so by updating all
     * relevant properties in the StripLayoutTab (id).
     */
    private void replacePlaceholdersForRestoredTabs() {
        if (!mPlaceholderStripReady || mTabStateInitialized || mModel == null) return;

        // If the number of tabs is less than the expected active tab index, it means that there
        // will need to be placeholders before the active tab. If this is the case, replace the
        // active tab later to ensure it's at the correct index.
        int numTabsToCopy = mModel.getCount();
        if (mCreatedTabOnStartup) numTabsToCopy--;
        boolean needPlaceholdersBeforeActiveTab =
                numTabsToCopy <= mActiveTabIndexOnStartup && mSelectedOnStartup;
        if (needPlaceholdersBeforeActiveTab && numTabsToCopy > 0) numTabsToCopy--;
        mCurrentPlaceholderIndex = numTabsToCopy;

        // There should not be more restored tabs than the allotted placeholder tabs.
        assert numTabsToCopy <= mStripTabs.length;

        // 1. Replace the placeholder tabs by updating the relevant properties.
        for (int i = 0; i < numTabsToCopy; i++) {
            final StripLayoutTab stripTab = mStripTabs[i];
            final Tab tab = mModel.getTabAt(i);

            pushPropertiesToPlaceholder(stripTab, tab);
        }
        if (!needPlaceholdersBeforeActiveTab) mActiveTabReplaced = true;

        // 2. If a new tab was created on startup (e.g. through intent), copy it over now.
        if (mCreatedTabOnStartup) {
            final StripLayoutTab stripTab = mStripTabs[mStripTabs.length - 1];
            final Tab tab = mModel.getTabAt(mModel.getCount() - 1);

            pushPropertiesToPlaceholder(stripTab, tab);
        }

        // 3. If the active tab could not be copied earlier, copy it over now at the correct index.
        if (needPlaceholdersBeforeActiveTab) {
            int prevActiveIndex = mModel.getCount() - 1;
            if (mCreatedTabOnStartup) prevActiveIndex--;

            if (prevActiveIndex >= 0) {
                final StripLayoutTab stripTab = mStripTabs[mActiveTabIndexOnStartup];
                final Tab tab = mModel.getTabAt(prevActiveIndex);

                pushPropertiesToPlaceholder(stripTab, tab);

                mActiveTabReplaced = true;
            }
        }

        // 4. Request new frame.
        mRenderHost.requestRender();
    }

    private void replaceNextPlaceholder(int id, boolean selected, boolean onStartup) {
        assert !mTabStateInitialized;

        // Placeholders are not yet ready. This strip tab will instead be created when we
        // prepare the placeholder strip.
        if (!mPlaceholderStripReady) return;

        // The active tab is handled separately.
        if (mCurrentPlaceholderIndex == mActiveTabIndexOnStartup && mSelectedOnStartup) {
            mCurrentPlaceholderIndex++;
        }

        // Tab manually created while tabs were still restoring on startup.
        if (!onStartup) {
            mTabsCreatedDuringRestore++;
            return;
        }

        // Unexpectedly ran out of placeholders.
        if (mCurrentPlaceholderIndex >= mStripTabs.length && !selected) {
            mPlaceholdersNeededDuringRestore++;
            return;
        }

        // Replace the matching placeholder.
        int replaceIndex;
        if (selected || !mActiveTabReplaced) {
            replaceIndex = mActiveTabIndexOnStartup;
            mActiveTabReplaced = true;
        } else {
            // Should match the index in the model. Though there are some mechanisms to return us to
            // a "valid" state that may break this, such as ensuring that grouped tabs are
            // contiguous. See https://crbug.com/329191924 for details.
            replaceIndex = mCurrentPlaceholderIndex++;
            if (mModel == null || replaceIndex != mModel.indexOf(getTabById(id))) return;
        }

        if (replaceIndex >= 0 && replaceIndex < mStripTabs.length) {
            final StripLayoutTab placeholderTab = mStripTabs[replaceIndex];
            final Tab tab = getTabById(id);

            pushPropertiesToPlaceholder(placeholderTab, tab);

            if (placeholderTab.isVisible()) {
                mRenderHost.requestRender();
            }
        }
    }

    /** Returns The expected tab count after tabs finish restoring. */
    protected int getTabCountOnStartupForTesting() {
        return mTabCountOnStartup;
    }

    /** Returns The expected active tab index after tabs finish restoring. */
    protected int getActiveTabIndexOnStartupForTesting() {
        return mActiveTabIndexOnStartup;
    }

    /** Returns Whether a non-restored tab was created during startup (e.g. through intent). */
    protected boolean getCreatedTabOnStartupForTesting() {
        return mCreatedTabOnStartup;
    }

    /**
     * Called to hide close tab buttons when tab width is <156dp when min tab width is 108dp or for
     * partially visible tabs at the edge of the tab strip when min tab width is set to >=156dp.
     */
    private void updateCloseButtons() {
        boolean anyVisibilityChange = false;

        final int count = mStripTabs.length;

        for (int i = 0; i < count; i++) {
            final StripLayoutTab tab = mStripTabs[i];
            boolean isLastTab = i == mStripTabs.length - 1;
            anyVisibilityChange |=
                    mTabDelegate.updateTabCloseButtonVisibility(
                            tab,
                            isLastTab,
                            mLeftFadeWidth,
                            mRightFadeWidth,
                            getVisibleLeftBound(/* clampToUnpinnedViews= */ true),
                            getVisibleRightBound(/* clampToUnpinnedViews= */ true),
                            mNewTabButton,
                            mIsFirstLayoutPass);
        }
        if (anyVisibilityChange) {
            // If close buttons appear / disappear, the CompositorView's keyboard focus index will
            // be wrong; fix it.
            // Note that we guard this in an if(anyVisibilityChange) block to avoid requesting
            // updates unnecessarily (which would hurt performance).
            mUpdateHost.requestUpdate(
                    () -> {
                        @Nullable StripLayoutView keyboardFocusedView = getKeyboardFocusedView();
                        if (keyboardFocusedView != null) {
                            mManagerHost.requestKeyboardFocus(mSceneOverlay, keyboardFocusedView);
                        }
                    });
        }
    }

    private void updateTabContainersAndDividers() {
        int hoveredId = mLastHoveredTab != null ? mLastHoveredTab.getTabId() : Tab.INVALID_TAB_ID;

        StripLayoutView[] viewsOnStrip = StripLayoutUtils.getViewsOnStrip(mStripViews);
        for (int i = 0; i < viewsOnStrip.length; ++i) {
            if (!(viewsOnStrip[i] instanceof StripLayoutTab currTab)) continue;

            // 1. Set container visibility. Handled in a separate animation for hovered tabs.
            if (hoveredId != currTab.getTabId()) {
                StripLayoutTabDelegate.updateTabVisibility(currTab);
            }
            boolean currContainerHidden = StripLayoutTabDelegate.isTabHidden(currTab);

            boolean hideDividerForDyingTab =
                    ChromeFeatureList.sTabletTabStripAnimation.isEnabled() && currTab.isDying();
            // 2. Set start divider visibility.
            if (i > 0 && viewsOnStrip[i - 1] instanceof StripLayoutTab prevTab) {
                boolean prevContainerHidden = StripLayoutTabDelegate.isTabHidden(prevTab);
                boolean prevTabHasMargin = prevTab.getTrailingMargin() > 0;
                boolean startDividerVisible =
                        !hideDividerForDyingTab
                                && currContainerHidden
                                && (prevContainerHidden || prevTabHasMargin);
                currTab.setStartDividerVisible(startDividerVisible);
            } else {
                currTab.setStartDividerVisible(/* visible= */ false);
            }

            // 3. Set end divider visibility. May be forced hidden for group reorder.
            if (currTab.shouldForceHideEndDivider()) {
                currTab.setEndDividerVisible(/* visible= */ false);
            } else {
                boolean isLastTab = i == (viewsOnStrip.length - 1);
                boolean endDividerVisible =
                        (isLastTab || viewsOnStrip[i + 1] instanceof StripLayoutGroupTitle)
                                && currContainerHidden
                                && !hideDividerForDyingTab;
                currTab.setEndDividerVisible(endDividerVisible);
            }
        }
    }

    private void updateTouchableRect() {
        // Make the entire strip touchable when during dragging / reordering mode.
        boolean isTabDraggingInProgress = isViewDraggingInProgress();
        if (isTabStripFull() || mReorderDelegate.getInReorderMode() || isTabDraggingInProgress) {
            mTouchableRect.set(
                    getVisibleLeftBound(/* clampToUnpinnedViews= */ false),
                    0,
                    getVisibleRightBound(/* clampToUnpinnedViews= */ false),
                    mHeight);
            return;
        }

        if (mStripViews.length == 0) {
            mTouchableRect.setEmpty();
            return;
        }

        // Get the bounding box of all strip views (excludes new tab and model selector buttons).
        StripLayoutView firstStripView = mStripViews[0];
        StripLayoutView lastStripView = mStripViews[mStripViews.length - 1];

        float leftBound = firstStripView.getDrawX();
        float rightBound = lastStripView.getDrawX() + lastStripView.getWidth();

        if (LocalizationUtils.isLayoutRtl()) {
            leftBound = lastStripView.getDrawX();
            rightBound = firstStripView.getDrawX() + firstStripView.getWidth();
        }

        // Clamp the bounding box to the visible area.
        float left = Math.max(leftBound, getVisibleLeftBound(/* clampToUnpinnedViews= */ false));
        float right = Math.min(rightBound, getVisibleRightBound(/* clampToUnpinnedViews= */ false));

        // Ensure left is not greater than right, which can happen if all tabs are off-screen.
        if (left > right) {
            mTouchableRect.setEmpty();
        } else {
            mTouchableRect.set(left, 0, right, mHeight);
        }
    }

    /**
     * Called when a tab has started loading resources.
     *
     * @param id The id of the Tab.
     */
    public void tabLoadStarted(int id) {
        StripLayoutTab tab = findTabById(id);
        if (tab != null) tab.loadingStarted();
    }

    /**
     * Called when a tab has stopped loading resources.
     * @param id The id of the Tab.
     */
    public void tabLoadFinished(int id) {
        StripLayoutTab tab = findTabById(id);
        if (tab != null) tab.loadingFinished();
    }

    /**
     * Called on touch drag event.
     *
     * @param x The x coordinate of the end of the drag event.
     * @param y The y coordinate of the end of the drag event.
     * @param deltaX The number of pixels dragged in the x direction.
     */
    public void drag(float x, float y, float deltaX) {
        deltaX = MathUtils.flipSignIf(deltaX, LocalizationUtils.isLayoutRtl());

        // 1. Reset the button state.
        mNewTabButton.drag(x, y);

        // 2.a. Enter reorder mode either if the view was initially clicked by a mouse OR the view
        // was long-pressed, but we suppressed reorder mode to instead show the view's context menu.
        // In the second case, dismiss the aforementioned context menu.
        if (mDelayedReorderView != null
                && !mReorderDelegate.getInReorderMode()
                && (Math.abs(x - mDelayedReorderInitialX) > INITIATE_REORDER_DRAG_THRESHOLD
                        || !isViewContextMenuShowing())) {
            if (isViewContextMenuShowing()) dismissContextMenu();
            // Intentionally start the reorder at the initial long-press x. The difference from the
            // current event (accumulatedDeltaX in step 3) will then "snap" the interacting view to
            // its expected position.
            startReorderMode(
                    mDelayedReorderInitialX, y, mDelayedReorderView, ReorderType.START_DRAG_DROP);
            resetDelayedReorderState();
        } else if (mReorderDelegate.getInReorderMode()) {
            // 2.b. If already reordering, instead update the in-progress reorder.
            mReorderDelegate.updateReorderPosition(
                    mStripViews,
                    mStripGroupTitles,
                    mStripTabs,
                    x,
                    deltaX,
                    ReorderType.DRAG_WITHIN_STRIP);
        } else if (!isViewContextMenuShowing()) {
            // 2.c. Otherwise, if the context menu is not showing, scroll the tab strip.
            if (!mIsStripScrollInProgress) {
                mIsStripScrollInProgress = true;
                RecordUserAction.record("MobileToolbarSlideTabs");
                onStripScrollStart();
            }
            mScrollDelegate.setScrollOffset(mScrollDelegate.getScrollOffset() + deltaX);
        }

        mUpdateHost.requestUpdate();
    }

    private void onStripScrollStart() {
        long currentTime = SystemClock.elapsedRealtime();

        // If last scroll is within the max allowed interval, do not reset start time.
        if (mMostRecentTabScroll != null
                && currentTime - mMostRecentTabScroll
                        <= TAB_SWITCH_METRICS_MAX_ALLOWED_SCROLL_INTERVAL) {
            mMostRecentTabScroll = currentTime;
            return;
        }

        mTabScrollStartTime = currentTime;
        mMostRecentTabScroll = currentTime;
    }

    /**
     * Called on touch fling event. This is called before the onUpOrCancel event.
     *
     * @param time The current time of the app in ms.
     * @param velocityX The amount of velocity in the x direction.
     */
    public void fling(long time, float velocityX) {
        // 1. If we're currently in reorder mode or the context menu is showing, don't allow the
        // user to fling.
        if (mReorderDelegate.getInReorderMode() || isViewContextMenuShowing()) return;

        // 2. Begin scrolling.
        mScrollDelegate.fling(
                time, MathUtils.flipSignIf(velocityX, LocalizationUtils.isLayoutRtl()));
        mUpdateHost.requestUpdate();
    }

    /**
     * Called on onDown event.
     *
     * @param x The x position of the event.
     * @param y The y position of the event.
     * @param buttons State of all buttons that are pressed.
     */
    public void onDown(float x, float y, int buttons) {
        if (mNewTabButton.onDown(x, y, buttons)) {
            mRenderHost.requestRender();
            return;
        }

        StripLayoutView clickedView = getViewAtPositionX(x, /* includeGroupTitles= */ true);
        if (clickedView instanceof StripLayoutTab clickedTab
                && clickedTab.checkCloseHitTest(x, y)) {
            clickedTab.setClosePressed(/* closePressed= */ true, buttons);
            mRenderHost.requestRender();
        } else if (MotionEventUtils.isPrimaryButton(buttons)) {
            mDelayedReorderView = clickedView;
            mDelayedReorderInitialX = x;
        }

        if (!mScrollDelegate.isFinished()) mScrollDelegate.stopScroll();
    }

    /**
     * Called on long press touch event.
     *
     * @param x The x coordinate of the position of the press event.
     * @param y The y coordinate of the position of the press event.
     */
    public void onLongPress(float x, float y) {
        StripLayoutView stripView = determineClickedView(x, y, /* buttons= */ 0);

        if (stripView == null) {
            // Show the tab strip context menu at the long-press position on the empty space.
            showTabStripContextMenu(x, y);
            return;
        }

        // If long-pressed on tab (not on close button) or group, mark for delayed reorder during
        // drag.
        if ((stripView instanceof StripLayoutTab clickedTab && !clickedTab.checkCloseHitTest(x, y))
                || stripView instanceof StripLayoutGroupTitle) {
            mDelayedReorderView = stripView;
            mDelayedReorderInitialX = x;
        }
        showContextMenu(stripView);
    }

    /** Returns {@code true} if a context menu triggered from long-pressing a view is showing. */
    private boolean isViewContextMenuShowing() {
        return (mTabGroupContextMenuCoordinator != null
                        && mTabGroupContextMenuCoordinator.isMenuShowing())
                || (mTabContextMenuCoordinator != null
                        && mTabContextMenuCoordinator.isMenuShowing())
                || (mCloseButtonMenu != null && mCloseButtonMenu.isShowing());
    }

    @VisibleForTesting
    void dismissContextMenu() {
        if (mTabGroupContextMenuCoordinator != null) mTabGroupContextMenuCoordinator.dismiss();
        if (mTabContextMenuCoordinator != null) mTabContextMenuCoordinator.dismiss();
        if (mCloseButtonMenu != null) mCloseButtonMenu.dismiss();
    }

    /**
     * Shows the tab group context menu for group with title {@code groupTitle}. {@code
     * shouldWaitForUpdate} should be true when we expect that the tab strip is actually changing,
     * but it should be false otherwise; if it is incorrectly true, the context menu will not be
     * shown until after the tab strip changes in some way.
     *
     * @param groupTitle The title of the group to open.
     * @param shouldWaitForUpdate Whether we expect that the tab strip needs to change before the
     *     tab group context menu can be shown.
     */
    private void showTabGroupContextMenu(
            @Nullable StripLayoutGroupTitle groupTitle, boolean shouldWaitForUpdate) {
        if (mModel == null || mTabGroupModelFilter == null) return;
        if (mTabGroupContextMenuCoordinator == null) {
            mTabGroupContextMenuCoordinator =
                    TabGroupContextMenuCoordinator.createContextMenuCoordinator(
                            mModel,
                            mTabGroupModelFilter,
                            mMultiInstanceManager,
                            mWindowAndroid,
                            mDataSharingTabManager,
                            (groupId, toLeft) -> {
                                // Don't use anchorTab here, since that will be the anchor of the
                                // first-opened tab context menu (it won't change when a new context
                                // menu is opened).
                                mReorderDelegate.reorderViewInDirection(
                                        mTabDelegate,
                                        mStripViews,
                                        mStripGroupTitles,
                                        mStripTabs,
                                        findGroupTitle(groupId),
                                        toLeft);
                            });
        }
        StripLayoutUtils.performHapticFeedback(mToolbarContainerView);

        if (shouldWaitForUpdate) {
            // We do this after a requestUpdate so that the view will have the correct position for
            // the tab group title.
            // We may need to do something different after adding animations.
            // TODO(crbug.com/354983679): Investigate adding animations.
            mUpdateHost.requestUpdate(() -> showTabGroupContextMenuHelper(groupTitle));
        } else {
            showTabGroupContextMenuHelper(groupTitle);
        }
    }

    /**
     * Shows {@code mTabGroupContextMenuCoordinator} given a group title. This method assumes that
     * {@code mTabGroupContextMenuCoordinator} has been correctly set. See {@link
     * this#showTabGroupContextMenu(StripLayoutGroupTitle, boolean)}.
     *
     * @param groupTitle The title of the group.
     */
    private void showTabGroupContextMenuHelper(@Nullable StripLayoutGroupTitle groupTitle) {
        // No-op if the tab group isn't found in sync (it might have been removed from another
        // device and will be cleaned up here soon).
        if (groupTitle == null || groupTitle.getTabGroupId() == null) return;
        // Popup menu requires screen coordinates for anchor view. Get absolute position for title.
        RectProvider anchorRectProvider = new RectProvider();
        groupTitle.getAnchorRect(anchorRectProvider.getRect());
        getAdjustedAnchorRect(anchorRectProvider);
        // If the menu is already showing (which may happen if the user does two long presses in
        // quick succession and showing the menu is slow), then abort.
        // Also note that we can assume mTabGroupContextMenuCoordinator is non-null since this
        // method should only be called after mTabGroupContextMenuCoordinator is initialized.
        if (assumeNonNull(mTabGroupContextMenuCoordinator).isMenuShowing()) return;
        mTabGroupContextMenuCoordinator.showMenu(anchorRectProvider, groupTitle.getTabGroupId());
    }

    private void showTabContextMenu(List<Integer> tabIds, StripLayoutTab anchorTab) {
        if (mModel == null || mTabGroupModelFilter == null) return;
        if (mTabContextMenuCoordinator == null) {
            TabGroupCreationCallback tabGroupCreationCallback =
                    (newTabGroupId) ->
                            showTabGroupContextMenu(
                                    findGroupTitle(newTabGroupId), /* shouldWaitForUpdate= */ true);
            if (mTabGroupListBottomSheetCoordinator == null) {
                mTabGroupListBottomSheetCoordinator =
                        mTabGroupListBottomSheetCoordinatorFactory.create(
                                mContext,
                                assumeNonNull(mTabGroupModelFilter.getTabModel().getProfile()),
                                tabGroupCreationCallback,
                                /* tabMovedCallback= */ null,
                                mTabGroupModelFilter,
                                mBottomSheetController,
                                /* supportsShowNewGroup= */ true,
                                /* destroyOnHide= */ false);
            }
            mTabContextMenuCoordinator =
                    TabContextMenuCoordinator.createContextMenuCoordinator(
                            () -> mModel,
                            mTabGroupModelFilter,
                            mTabGroupListBottomSheetCoordinator,
                            tabGroupCreationCallback,
                            mMultiInstanceManager,
                            mShareDelegateSupplier,
                            mWindowAndroid,
                            assertNonNull(mWindowAndroid.getActivity().get()),
                            (ids, toLeft) -> {
                                // Don't use anchorTab here, since that will be the anchor of the
                                // first-opened tab context menu (it won't change when a new context
                                // menu is opened).
                                mReorderDelegate.reorderViewInDirection(
                                        mTabDelegate,
                                        mStripViews,
                                        mStripGroupTitles,
                                        mStripTabs,
                                        assumeNonNull(findTabById(ids.getAnchorTabId())),
                                        toLeft);
                            });
        }
        RectProvider anchorRectProvider = new RectProvider();
        anchorTab.getAnchorRect(anchorRectProvider.getRect());
        getAdjustedAnchorRect(anchorRectProvider);
        StripLayoutUtils.performHapticFeedback(mToolbarContainerView);
        mTabContextMenuCoordinator.showMenu(
                anchorRectProvider, new AnchorInfo(anchorTab.getTabId(), tabIds));
    }

    /**
     * Opens the context menu for the keyboard-focused view, if applicable.
     * @return Whether the context menu was successfully opened.
     */
    public boolean openKeyboardFocusedContextMenu() {
        @Nullable StripLayoutView focusedView = getKeyboardFocusedView();
        if (focusedView == null) return false;
        return showContextMenu(focusedView);
    }

    /**
     * Moves the currently keyboard-selected strip view to the left or right by one position.
     *
     * @param toLeft Whether to move towards the left (note: this is left even in RTL).
     * @return Whether the item was successfully reordered.
     */
    public boolean moveSelectedStripView(boolean toLeft) {
        @Nullable StripLayoutView focusedView = getKeyboardFocusedView();
        if (focusedView == null) return false;
        mReorderDelegate.reorderViewInDirection(
                mTabDelegate, mStripViews, mStripGroupTitles, mStripTabs, focusedView, toLeft);
        return true;
    }

    /* package */ void showTabContextMenuForTesting(
            List<Integer> tabIds, StripLayoutTab anchorTab) {
        showTabContextMenu(tabIds, anchorTab);
    }

    /* package */ void destroyTabContextMenuForTesting() {
        if (mTabContextMenuCoordinator != null) mTabContextMenuCoordinator.destroyMenuForTesting();
    }

    /**
     * retrieves the corresponding group title using the group's collaboration ID then updates or
     * clears the shared state accordingly.
     *
     * @param groupData The shared group data.
     * @param tabGroupSyncService The {@link TabGroupSyncService}
     */
    private void updateOrClearSharedState(
            GroupData groupData, TabGroupSyncService tabGroupSyncService) {
        String collaborationId = groupData.groupToken.collaborationId;
        StripLayoutGroupTitle groupTitle =
                StripLayoutUtils.findGroupTitleByCollaborationId(
                        mStripGroupTitles, collaborationId, tabGroupSyncService);
        updateOrClearSharedState(groupData, groupTitle);
    }

    /**
     * Updates the shared state and avatar face piles for a tab group if it has multiple
     * collaborators. If the group no longer qualifies as a shared group, clears the shared state
     * and removes the avatar.
     *
     * @param groupData The shared group data.
     * @param groupTitle The group title to update or clear shared state.
     */
    private void updateOrClearSharedState(
            @Nullable GroupData groupData, @Nullable StripLayoutGroupTitle groupTitle) {
        if (groupTitle == null) return;
        @GroupSharedState int groupSharedState = TabShareUtils.discernSharedGroupState(groupData);
        if (groupSharedState == GroupSharedState.NOT_SHARED || groupData == null) {
            clearSharedTabGroup(groupTitle);
        } else {
            updateSharedTabGroup(groupData.groupToken.collaborationId, groupTitle);
        }
    }

    /**
     * Updates the tab group shared state if applicable.
     *
     * @param groupTitle The group title to update with the shared tab group state.
     */
    private void updateSharedTabGroupIfNeeded(StripLayoutGroupTitle groupTitle) {
        Token tabGroupId = groupTitle.getTabGroupId();
        if (shouldEnableGroupSharing()) {
            SavedTabGroup savedTabGroup =
                    mTabGroupSyncService.getGroup(new LocalTabGroupId(tabGroupId));
            if (savedTabGroup == null
                    || savedTabGroup.collaborationId == null
                    || mCollaborationService == null) {
                return;
            }

            GroupData groupData = mCollaborationService.getGroupData(savedTabGroup.collaborationId);
            updateOrClearSharedState(groupData, groupTitle);
        }
    }

    /**
     * Updates the shared state of a tab group, including the avatar face piles and setup
     * notification bubbler for the group title when the group is shared.
     *
     * @param collaborationId The sharing ID associated with the group.
     * @param groupTitle The group title to update with the shared tab group state.
     */
    private void updateSharedTabGroup(String collaborationId, StripLayoutGroupTitle groupTitle) {
        if (mTabGroupModelFilter == null) return;
        // Setup tab bubbler used for showing notification bubbles for shared tab groups.
        if (groupTitle.getTabBubbler() == null) {
            TabBubbler tabBubbler =
                    new TabBubbler(
                            assumeNonNull(mTabGroupModelFilter.getTabModel().getProfile()),
                            this,
                            new ObservableSupplierImpl<>(groupTitle.getTabGroupId()));
            groupTitle.setTabBubbler(tabBubbler);
        }

        if (mDataSharingService == null || mCollaborationService == null) return;
        groupTitle.updateSharedTabGroup(
                collaborationId,
                mDataSharingService,
                mCollaborationService,
                (avatarRes) -> {
                    if (mLayerTitleCache == null) return;
                    mLayerTitleCache.registerSharedGroupAvatar(
                            groupTitle.getTabGroupId(), avatarRes);
                },
                () -> updateGroupTextAndSharedState(groupTitle));
    }

    /**
     * Clear group avatar face piles displayed on group title and other share related group title
     * data.
     *
     * @param groupTitle The groupTitle to clear shared state.
     */
    private void clearSharedTabGroup(StripLayoutGroupTitle groupTitle) {
        groupTitle.clearSharedTabGroup();
        if (mLayerTitleCache != null) {
            mLayerTitleCache.removeSharedGroupAvatar(groupTitle.getTabGroupId());
        }
        updateGroupTextAndSharedState(groupTitle);
    }

    /**
     * Displays notification bubbles for all shared tab groups with recent updates from other
     * collaborators (e.g. tab additions, removals, or changes).
     */
    private void showNotificationBubblesForSharedTabGroups() {
        for (StripLayoutGroupTitle groupTitle : mStripGroupTitles) {
            TabBubbler tabBubbler = groupTitle.getTabBubbler();
            if (tabBubbler != null) {
                tabBubbler.showAll();
            }
        }
    }

    private void getAdjustedAnchorRect(RectProvider anchorRectProvider) {
        int[] toolbarCoordinates = new int[2];
        Rect backgroundPadding = new Rect();
        mToolbarContainerView.getLocationInWindow(toolbarCoordinates);
        Drawable background = TabOverflowMenuCoordinator.getMenuBackground(mContext, mIncognito);
        background.getPadding(backgroundPadding);

        // Use parent toolbar view coordinates to offset title rect.
        // Also shift the anchor left by menu padding to align the menu exactly with title x.
        int xOffset =
                MathUtils.flipSignIf(
                        toolbarCoordinates[0] - backgroundPadding.left,
                        LocalizationUtils.isLayoutRtl());
        int topPaddingPx =
                Math.round(mTopPadding * mContext.getResources().getDisplayMetrics().density);
        anchorRectProvider.getRect().offset(xOffset, toolbarCoordinates[1] + topPaddingPx);
    }

    private void startReorderMode(
            float x, float y, StripLayoutView interactingView, @ReorderType int reorderType) {
        // Allow the user to drag the selected tab out of the tab strip.
        if (mReorderDelegate.getInReorderMode()) return;
        // Attempt to start reordering. If the interacting view is a StripLayoutTab,
        // only continue if it is valid (non-null, non-dying, non-placeholder) and
        // the tab state is initialized.
        if (interactingView instanceof StripLayoutTab interactingTab
                && (interactingTab.isDying()
                        || interactingTab.getTabId() == Tab.INVALID_TAB_ID
                        || !mTabStateInitialized)) {
            return;
        }

        mReorderDelegate.startReorderMode(
                mStripViews,
                mStripTabs,
                mStripGroupTitles,
                interactingView,
                new PointF(x, y),
                reorderType);
    }

    /**
     * Called on hover enter event.
     *
     * @param x The x coordinate of the position of the hover enter event.
     */
    public void onHoverEnter(float x, float y) {
        StripLayoutTab hoveredTab = getTabAtPosition(x);

        // Hovered into a tab on the strip.
        if (hoveredTab != null) {
            updateLastHoveredTab(hoveredTab);
            // Check whether the close button on the hovered tab is being hovered on.
            StripLayoutTabDelegate.updateTabCloseHoverState(hoveredTab, x, y);
        } else {
            // Check whether new tab button or model selector button is being hovered.
            updateCompositorButtonHoverState(x, y);
        }
        mUpdateHost.requestUpdate();
    }

    /**
     * Called on hover move event.
     *
     * @param x The x coordinate of the position of the hover move event.
     */
    public void onHoverMove(float x, float y) {
        // Check whether new tab button or model selector button is being hovered.
        updateCompositorButtonHoverState(x, y);

        StripLayoutTab hoveredTab = getTabAtPosition(x);
        // Hovered into a non-tab region within the strip.
        if (hoveredTab == null) {
            clearLastHoveredTab();
        } else if (hoveredTab == mLastHoveredTab) {
            if (!StripLayoutTabDelegate.updateTabCloseHoverState(hoveredTab, x, y)) return;
        } else {
            // Hovered from one tab to another tab on the strip.
            clearLastHoveredTab();
            updateLastHoveredTab(hoveredTab);
        }
        mUpdateHost.requestUpdate();
    }

    /** Called on hover exit event. */
    public void onHoverExit(boolean inTabStrip) {
        // TODO(crbug.com/419015257): Use inTabStrip to delay resize on tab close from mouse.
        clearLastHoveredTab();

        // Clear tab strip button (NTB and MSB) hover state.
        clearCompositorButtonHoverStateIfNotClicked();

        // Trigger a resize, as the pointer has left the strip, and we no longer need to suppress.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_STRIP_MOUSE_CLOSE_RESIZE_DELAY)
                && !inTabStrip) {
            clearPendingMouseTabClosureState();
            computeAndUpdateTabWidth(/* animate= */ true, /* deferAnimations= */ false);
        }

        mUpdateHost.requestUpdate();
    }

    /**
     * Called when the user performs a scrolling action by a peripheral such as a mouse or trackpad.
     *
     * @param horizontalAxisScroll The horizontal/x value of the scroll, higher means harder scroll.
     * @param verticalAxisScroll The vertical/y value of the scroll, higher means harder scroll.
     */
    public void onScroll(float horizontalAxisScroll, float verticalAxisScroll) {
        // We want mouse scrolls and trackpad scrolls, both vertical and horizontal, to map to
        // scrolling the tab strip. If the user scrolls diagonally, presenting both a
        // horizontal and vertical scroll component, we will defer to the horizontal value. We will
        // scroll by a set amount of dp, regardless of how much 'force' the user scrolls with such
        // as flinging a mouse wheel, i.e. all that matters is the sign of the scroll vector.
        boolean useHorizontalAxisScroll = Math.abs(horizontalAxisScroll) > MathUtils.EPSILON;
        float userScrollDelta = useHorizontalAxisScroll ? horizontalAxisScroll : verticalAxisScroll;
        float tabScrollDelta =
                TypedValue.applyDimension(
                        TypedValue.COMPLEX_UNIT_DIP,
                        Math.signum(userScrollDelta) * SCROLL_SPEED_FACTOR,
                        mContext.getResources().getDisplayMetrics());

        setScrollForScrollingTabStacker(
                tabScrollDelta,
                useHorizontalAxisScroll,
                /* shouldAnimate= */ true,
                LayoutManagerImpl.time());
        mUpdateHost.requestUpdate();
    }

    /** Called in post delay task in q#onDown to clear tab hover state. */
    protected void clearTabHoverState() {
        clearLastHoveredTab();
        mUpdateHost.requestUpdate();
    }

    /** Check whether model selector button or new tab button is being hovered. */
    private void updateCompositorButtonHoverState(float x, float y) {
        boolean isModelSelectorHovered = false;
        if (mModelSelectorButton != null) {
            // Model selector button is being hovered.
            isModelSelectorHovered = mModelSelectorButton.checkClickedOrHovered(x, y);
            mModelSelectorButton.setHovered(isModelSelectorHovered);
        }
        // There's a delay in updating NTB's position/touch target when MSB initially appears on the
        // strip, taking over NTB's position and moving NTB closer to the tabs. Consequently, hover
        // highlights are observed on both NTB and MSB. To address this, this check is added to
        // ensure only one button can be hovered at a time.
        if (!isModelSelectorHovered) {
            mNewTabButton.setHovered(mNewTabButton.checkClickedOrHovered(x, y));
        } else {
            mNewTabButton.setHovered(false);
        }
    }

    /** Clear button hover state */
    private void clearCompositorButtonHoverStateIfNotClicked() {
        mNewTabButton.setHovered(false);
        if (mModelSelectorButton != null) {
            mModelSelectorButton.setHovered(false);
        }
    }

    void setTabHoverCardView(StripTabHoverCardView tabHoverCardView) {
        mTabHoverCardView = tabHoverCardView;
        // If onHoverEnter was already processed before this method call, show card now.
        if (mLastHoveredTab != null && !mTabHoverCardView.isShown()) {
            showTabHoverCardView(/* isDelayedCall= */ false);
        }
    }

    @Nullable StripTabHoverCardView getTabHoverCardViewForTesting() {
        return mTabHoverCardView;
    }

    void setLastHoveredTabForTesting(StripLayoutTab tab) {
        mLastHoveredTab = tab;
        mTabDelegate.setIsTabHovered(tab, true);
    }

    @Nullable StripLayoutTab getLastHoveredTab() {
        return mLastHoveredTab;
    }

    void setTabGroupContextMenuCoordinatorForTesting(
            TabGroupContextMenuCoordinator tabGroupContextMenuCoordinator) {
        mTabGroupContextMenuCoordinator = tabGroupContextMenuCoordinator;
    }

    @SuppressWarnings("NullAway")
    void setTabContextMenuCoordinatorForTesting(
            TabContextMenuCoordinator tabGroupContextMenuCoordinator) {
        mTabContextMenuCoordinator = tabGroupContextMenuCoordinator;
        ResettersForTesting.register(() -> mTabContextMenuCoordinator = null);
    }

    @SuppressWarnings("NullAway")
    void setTabStripContextMenuCoordinatorForTesting(
            TabStripContextMenuCoordinator tabStripContextMenuCoordinator) {
        mTabStripContextMenuCoordinator = tabStripContextMenuCoordinator;
        ResettersForTesting.register(() -> mTabStripContextMenuCoordinator = null);
    }

    private void clearLastHoveredTab() {
        if (mLastHoveredTab == null) {
            return;
        }
        mTabDelegate.setIsTabHovered(mLastHoveredTab, false);
        mLastHoveredTab = null;
        // Hide hover card view.
        mStripTabEventHandler.removeMessages(MESSAGE_HOVER_CARD);
        // Hover card view can be null if hover event was processed before view inflation completes.
        if (mTabHoverCardView != null) {
            if (mTabHoverCardView.isShown()) {
                mLastHoverCardExitTime = SystemClock.uptimeMillis();
            }
            mTabHoverCardView.hide();
        }
    }

    @VisibleForTesting
    void updateLastHoveredTab(StripLayoutTab hoveredTab) {
        if (hoveredTab == null) return;

        // Do nothing if attempting to update the hover state of a tab while a tab strip animation
        // is running. This is to avoid applying the tab hover state during animations triggered for
        // some actions on the strip, for example, resizing the strip after tab closure, that might
        // cause the hover state to show / stick undesirably.
        if (mRunningAnimator != null && mRunningAnimator.isRunning()) return;

        // Do nothing if hovering into a drawn tab that is for example, hidden behind the model
        // selector button.
        if (isViewCompletelyHidden(hoveredTab)) return;

        mLastHoveredTab = hoveredTab;
        mTabDelegate.setIsTabHovered(hoveredTab, true);

        // Show the tab hover card.
        // Just in case, cancel the previous delayed hover card event.
        mStripTabEventHandler.removeMessages(MESSAGE_HOVER_CARD);
        if (shouldShowHoverCardImmediately()) {
            showTabHoverCardView(/* isDelayedCall= */ false);
        } else {
            mStripTabEventHandler.sendEmptyMessageDelayed(
                    MESSAGE_HOVER_CARD, getHoverCardDelay(mLastHoveredTab.getWidth()));
        }
    }

    private boolean shouldShowHoverCardImmediately() {
        if (CompositorAnimationHandler.isInTestingMode()) {
            return true;
        }
        if (mLastHoverCardExitTime == INVALID_TIME) {
            return false;
        }

        long elapsedTime = SystemClock.uptimeMillis() - mLastHoverCardExitTime;
        return elapsedTime <= SHOW_HOVER_CARD_WITHOUT_DELAY_TIME_BUFFER;
    }

    @VisibleForTesting
    int getHoverCardDelay(float tabWidth) {
        // Delay is calculated as a logarithmic scale and bounded by a minimum width
        // based on the width of a pinned tab and a maximum of the standard width.
        //
        //  delay (ms)
        //           |
        // max delay-|                                    *
        //           |                          *
        //           |                    *
        //           |                *
        //           |            *
        //           |         *
        //           |       *
        //           |     *
        //           |    *
        // min delay-|****
        //           |___________________________________________ tab width
        //               |                                |
        //       pinned tab width               standard tab width

        tabWidth = MathUtils.clamp(tabWidth, MIN_TAB_WIDTH_DP, MAX_TAB_WIDTH_DP);
        double logarithmicFraction =
                Math.log(tabWidth - MIN_TAB_WIDTH_DP + 1.f)
                        / Math.log(MAX_TAB_WIDTH_DP - MIN_TAB_WIDTH_DP + 1.f);
        int scalingFactor = MAX_HOVER_CARD_DELAY_MS - MIN_HOVER_CARD_DELAY_MS;
        int delay = (int) (logarithmicFraction * scalingFactor) + MIN_HOVER_CARD_DELAY_MS;

        return delay;
    }

    private void showTabHoverCardView(boolean isDelayedCall) {
        if (mLastHoveredTab == null) {
            return;
        }
        // TODO(crbug.com/396683827): If there are no calls with unexpectedly null
        // mTabHoverCardView, the early null-check return and histogram should be removed.
        if (mTabHoverCardView == null) {
            RecordHistogram.recordBooleanHistogram(
                    NULL_TAB_HOVER_CARD_VIEW_SHOW_DELAYED_HISTOGRAM_NAME, isDelayedCall);
            return;
        }

        if (mModel == null) return;

        // Don't allow the hovercard to show when any context menu is already showing.
        if (isViewContextMenuShowing()) return;

        int hoveredTabIndex = findIndexForTab(mLastHoveredTab.getTabId());
        mTabHoverCardView.show(
                mModel.getTabAt(hoveredTabIndex),
                isSelectedTab(mLastHoveredTab.getTabId()),
                mLastHoveredTab.getDrawX(),
                mLastHoveredTab.getWidth(),
                mHeight,
                mTopPadding);
    }

    /**
     * Closes the given {@link StripLayoutTab} with animation.
     *
     * <p>Sequence of events:
     *
     * <ol>
     *   <li>Finish ongoing tab removal animation if there is any. This is done by {@link
     *       #finishAnimationsAndCloseDyingTabs(boolean)}.
     *   <li>Mark the given tab as "dying".
     *   <li>Start the tab removal animation for the given tab.
     *   <li>When the animation ends, remove the tab from {@link TabModel}. This is also done by
     *       {@link #finishAnimationsAndCloseDyingTabs(boolean)}. We intentionally delay removing
     *       the tab until the animation ends, since when the tab is removed from {@link TabModel},
     *       we'll also remove the corresponding {@link StripLayoutTab} from {@link #mStripTabs}. If
     *       we don't delay the tab removal, we won't be able to animate the matching tab container
     *       translating off of the strip.
     * </ol>
     *
     * @param tab the {@link StripLayoutTab} to close.
     * @param allowUndo whether to allow undo of tab closure, such as showing the "undo" snackbar.
     * @see #finishAnimationsAndCloseDyingTabs(boolean)
     */
    @VisibleForTesting
    void handleCloseTab(StripLayoutTab tab, boolean allowUndo) {
        mMultiStepTabCloseAnimRunning = false;
        finishAnimationsAndCloseDyingTabs(allowUndo);

        // When a tab is closed #resizeStripOnTabClose will run animations for the new tab offset
        // and tab x offsets. When there is only 1 tab remaining, we do not need to run those
        // animations, so #computeAndUpdateTabWidth() is used instead.
        boolean runImprovedTabAnimations = mStripTabs.length > 1;

        // 1. Set the dying state of the tab.
        tab.setIsDying(true);

        // 2. Start the tab closing animator with a listener to resize/move tabs after the closure.
        // If closing the end-most tab, set an offset to prevent the tab from "jumping" to align
        // with the new end-most tab. This will be cleared when the resize is animated.
        if (!ChromeFeatureList.sTabletTabStripAnimation.isEnabled()
                && isEndMostTab(tab.getTabId())) {
            mNewTabButton.setOffsetX(
                    MathUtils.flipSignIf(
                            getEffectiveTabWidth(/* isPinned= */ false),
                            LocalizationUtils.isLayoutRtl()));
        }
        AnimatorListener listener =
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        // Removes all dying tabs from TabModel.
                        finishAnimationsAndCloseDyingTabs(allowUndo);

                        if (!ChromeFeatureList.sTabletTabStripAnimation.isEnabled()) {
                            resizeStripOnTabClose(runImprovedTabAnimations);
                        }
                    }
                };
        runTabRemovalAnimation(tab, listener);

        // 3. If we're closing the selected tab, attempt to select the next expanded tab now. If
        // none exists, we'll default to the normal auto-selection behavior (i.e. selecting the
        // closest collapsed tab, or opening the GTS if none exist).
        if (mModel != null && getSelectedTabId() == tab.getTabId()) {
            int nextIndex = getNextIndexAfterClose(Collections.singleton(tab));
            if (nextIndex != TabModel.INVALID_TAB_INDEX) TabModelUtils.setIndex(mModel, nextIndex);
        }
    }

    private Animator getLegacyTabClosedAnimator(StripLayoutTab tab) {
        return CompositorAnimator.ofFloatProperty(
                mUpdateHost.getAnimationHandler(),
                tab,
                StripLayoutTab.Y_OFFSET,
                tab.getOffsetY(),
                tab.getHeight(),
                ANIM_TAB_CLOSED_MS);
    }

    private Animator getViewWidthAnimator(StripLayoutView view, float targetWidth, int duration) {
        return CompositorAnimator.ofFloatProperty(
                mUpdateHost.getAnimationHandler(),
                view,
                StripLayoutView.WIDTH,
                view.getWidth(),
                targetWidth,
                duration);
    }

    private List<Animator> getTabClosingAnimators(Collection<StripLayoutTab> tabs) {
        if (ChromeFeatureList.sTabletTabStripAnimation.isEnabled()) {
            // computeAndUpdateTabWidth handles animating a tab closing.
            List<Animator> tabClosingAnimators =
                    computeAndUpdateTabWidth(/* animate= */ true, /* deferAnimations= */ true);
            if (tabClosingAnimators != null) return tabClosingAnimators;
            return new ArrayList<>();
        } else {
            mMultiStepTabCloseAnimRunning = true;
            List<Animator> tabClosingAnimators = new ArrayList<>();
            for (StripLayoutTab tab : tabs) {
                tabClosingAnimators.add(getLegacyTabClosedAnimator(tab));
            }
            return tabClosingAnimators;
        }
    }

    private void runTabRemovalAnimation(StripLayoutTab tab, AnimatorListener listener) {
        startAnimations(getTabClosingAnimators(Collections.singletonList(tab)), listener);
    }

    private void resizeStripOnTabClose(boolean runImprovedTabAnimations) {
        if (runImprovedTabAnimations) {
            resizeStripOnTabClose();
        } else {
            mNewTabButton.setOffsetX(/* offsetX= */ 0.f);
            mMultiStepTabCloseAnimRunning = false;
            // Resize the tabs appropriately.
            computeAndUpdateTabWidth(/* animate= */ true, /* deferAnimations= */ false);
        }
    }

    private void resizeStripOnTabClose() {
        List<Animator> tabStripAnimators = new ArrayList<>();

        // 1. Add tabs expanding animators to expand remaining tabs to fill scrollable area.
        List<Animator> tabExpandAnimators = computeAndUpdateTabWidth(true, true);
        if (tabExpandAnimators != null) tabStripAnimators.addAll(tabExpandAnimators);

        // 2. Calculate new scroll offset and idealX for tab offset animation.
        updateScrollOffsetLimits();
        computeIdealViewPositions();

        // 3. Animate the tabs sliding to their idealX.
        for (int i = 0; i < mStripViews.length; ++i) {
            final StripLayoutView view = mStripViews[i];
            if (view.getDrawX() == view.getIdealX()) {
                // Don't animate views that won't change location.
                continue;
            } else if (isViewCompletelyHidden(view) && willViewBeCompletelyHidden(view)) {
                // Don't animate views that won't be seen by the user (i.e. not currently visible
                // and won't be visible after moving) - just set the draw X immediately.
                view.setDrawX(view.getIdealX());
                continue;
            }

            CompositorAnimator drawXAnimator =
                    CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            view,
                            StripLayoutView.X_OFFSET,
                            view.getDrawX() - view.getIdealX(),
                            0.f,
                            ANIM_TAB_DRAW_X_MS);
            tabStripAnimators.add(drawXAnimator);
        }

        // 4. Add new tab button offset animation if needed.
        if (mNewTabButton.getOffsetX() != 0.f) {
            tabStripAnimators.add(
                    CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            mNewTabButton,
                            StripLayoutView.X_OFFSET,
                            mNewTabButton.getOffsetX(),
                            /* endValue= */ 0.f,
                            ANIM_TAB_RESIZE_MS));
        }

        // 5. Add animation completion listener and start animations.
        startAnimations(
                tabStripAnimators,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mMultiStepTabCloseAnimRunning = false;
                    }
                });
    }

    /**
     * Called on click. This is called before the onUpOrCancel event.
     *
     * @param time The current time of the app in ms.
     * @param x The x coordinate of the position of the click.
     * @param y The y coordinate of the position of the click.
     * @param buttons State of all buttons that were pressed when onDown was invoked.
     * @param modifiers State of all Meta/Modifier keys that are pressed.
     */
    public void click(long time, float x, float y, int buttons, int modifiers) {
        StripLayoutView clickedView = determineClickedView(x, y, buttons);
        clearLastHoveredTab();
        if (clickedView == null) {
            if (MotionEventUtils.isSecondaryClick(buttons)) {
                // A right click on empty strip space should trigger the strip context menu.
                showTabStripContextMenu(x, y);
            }
            return;
        }
        if (MotionEventUtils.isSecondaryClick(buttons)) {
            showContextMenu(clickedView);
        } else {
            clickedView.handleClick(time, buttons, modifiers);
        }
    }

    /**
     * Called on up or cancel touch events. This is called after the click and fling event if any.
     */
    public void onUpOrCancel() {
        /* 1. Stop any reordering that is happening. For Android drag&drop, this method is invoked
         * immediately after View#startDrag to stop ongoing gesture events. Do not stop reorder in
         * this case.
         */
        if (!isViewDraggingInProgress()) stopReorderMode();

        // 2. Reset state
        if (mNewTabButton.onUpOrCancel() && mModel != null) {
            if (!mModel.isIncognito()) mModel.commitAllTabClosures();
            if (mTabCreator != null) TabCreatorUtil.launchNtp(mTabCreator);
        }
        mIsStripScrollInProgress = false;
        resetDelayedReorderState();
    }

    @Override
    public void onClick(
            long time, StripLayoutView view, int motionEventButtonState, int modifiers) {
        if (view instanceof StripLayoutTab tab) {
            handleTabClick(
                    tab, modifiers, MotionEventUtils.isPrimaryButton(motionEventButtonState));
            return;
        } else if (view instanceof StripLayoutGroupTitle groupTitle) {
            handleGroupTitleClick(groupTitle);
        } else if (view instanceof CompositorButton button) {
            if (button.getType() == ButtonType.NEW_TAB) {
                handleNewTabClick();
            } else if (button.getType() == ButtonType.TAB_CLOSE) {
                handleCloseButtonClick(
                        (StripLayoutTab) button.getParentView(), motionEventButtonState);
                return;
            }
        }
        // If multi-selection is active, any click on the tab strip that is not a tab should clear
        // the selection.
        clearMultiSelection(/* clearAnchor= */ true, /* notifyObservers= */ true);
    }

    @Override
    public void onKeyboardFocus(boolean isFocused, StripLayoutView view) {
        if (!isFocused) return;
        bringViewToVisibleArea(
                view,
                LayoutManagerImpl.time(),
                /* animate= */ !AccessibilityState.prefersReducedMotion());
        mUpdateHost.requestUpdate();
    }

    /**
     * Show the context menu originating at {@param clickedView}, and returns true if a context menu
     * was shown. (Note: this will return false if there is no context menu to be shown at {@param
     * clickedView}.
     *
     * @param clickedView The view for which to show a context menu.
     * @return Whether a context menu was shown.
     */
    private boolean showContextMenu(StripLayoutView clickedView) {
        if (clickedView == null) return false;
        if (clickedView instanceof StripLayoutTab clickedTab) {
            // The current tab is always multi selected. So we need to check if there are more tabs
            // in multi-selection.
            if (mModel != null
                    && mModel.isTabMultiSelected(clickedTab.getTabId())
                    && mModel.getMultiSelectedTabsCount() > 1) {
                showTabContextMenu(getMultiSelectedTabIds(), clickedTab);
            } else {
                if (mModel != null) {
                    mModel.clearMultiSelection(/* notifyObservers= */ true);
                }
                showTabContextMenu(Collections.singletonList(clickedTab.getTabId()), clickedTab);
            }
            return true;
        } else if (clickedView instanceof CompositorButton button
                && button.getType() == ButtonType.TAB_CLOSE) {
            if (mModel != null) {
                mModel.clearMultiSelection(/* notifyObservers= */ true);
            }
            showCloseButtonMenu(assumeNonNull((StripLayoutTab) button.getParentView()));
            return true;
        } else if (clickedView instanceof StripLayoutGroupTitle groupTitle) {
            if (mModel != null) {
                mModel.clearMultiSelection(/* notifyObservers= */ true);
            }
            showTabGroupContextMenu(groupTitle, /* shouldWaitForUpdate= */ false);
            return true;
        }
        return false;
    }

    /**
     * Shows the context menu originating at strip coordinates ({@code xDp}, {@code yDp}) to handle
     * a long-press or right-click event at this position. The coordinates are assumed to lie on the
     * empty space of the tab strip. For context menus associated with gestures on a {@link
     * StripLayoutView}, see {@link #showContextMenu(StripLayoutView)}.
     *
     * @param xDp The x coordinate of the position of the gesture event.
     * @param yDp The y coordinate of the position of the gesture event.
     */
    private void showTabStripContextMenu(float xDp, float yDp) {
        if (mTabStripContextMenuCoordinator == null) {
            mTabStripContextMenuCoordinator =
                    new TabStripContextMenuCoordinator(mContext, mMultiInstanceManager);
        }

        // Determine the anchor view rect to position the menu.
        float dpToPx = mContext.getResources().getDisplayMetrics().density;
        RectProvider anchorRectProvider = new RectProvider();
        anchorRectProvider.setRect(
                new Rect(
                        Math.round(xDp * dpToPx),
                        Math.round(yDp * dpToPx),
                        Math.round(xDp * dpToPx),
                        Math.round(yDp * dpToPx)));
        getAdjustedAnchorRect(anchorRectProvider);

        var activity = assertNonNull(mWindowAndroid.getActivity().get());
        mTabStripContextMenuCoordinator.showMenu(
                anchorRectProvider, assumeNonNull(mModel).isIncognito(), activity);
    }

    private List<Integer> getMultiSelectedTabIds() {
        StripLayoutUtils.recordTabMultiSelectionTabCount(mModel);
        List<Integer> multiSelectedTabs = new ArrayList<>();
        if (mModel == null) return multiSelectedTabs;
        for (StripLayoutTab stripTab : mStripTabs) {
            if (mModel.isTabMultiSelected(stripTab.getTabId())) {
                multiSelectedTabs.add(stripTab.getTabId());
            }
        }
        assert multiSelectedTabs.size() == mModel.getMultiSelectedTabsCount()
                : "Count of multi selected tabs don't match.";
        assert multiSelectedTabs.size() >= 2 : "Too few tabs in multi selection";
        return multiSelectedTabs;
    }

    /**
     * Processes a click event on a tab, dispatching to the appropriate multi-select handler based
     * on the keyboard modifier state. This serves as the main entry point for all multi-selection
     * click logic.
     *
     * @param tab The {@link StripLayoutTab} that was clicked by the user.
     * @param modifiers The active keyboard modifiers from the {@link MotionEvent#getMetaState()}.
     * @param isMouseClick True if the click event originated from a mouse.
     */
    private void handleTabClick(StripLayoutTab tab, int modifiers, boolean isMouseClick) {
        if (tab == null || tab.isDying() || mModel == null) return;

        // Restrict modified clicks to mouse input only for a predictable experience.
        // If feature disabled, return to legacy behaviour.
        if (!ChromeFeatureList.sAndroidTabHighlighting.isEnabled()
                    || (!isMouseClick && !StripLayoutUtils.isTabHighlightingTestingEnabled())) {
            selectTab(tab);
            clearMultiSelection(/* clearAnchor= */ true, /* notifyObservers= */ true);
            mRenderHost.requestRender();
            return;
        }

        // Force flags are required for testing on an emulator, as key presses don't seem to be
        // propagated to the app.
        boolean isShiftPressed = (modifiers & KeyEvent.META_SHIFT_ON) != 0
                || StripLayoutUtils.isTabHighlightingForceShiftClick();
        boolean isCtrlPressed = (modifiers & KeyEvent.META_CTRL_ON) != 0
                || StripLayoutUtils.isTabHighlightingForceCtrlClick();

        if (isShiftPressed && isCtrlPressed) {
            handleShiftClick(tab, /* isDestructive= */ false);
        } else if (isShiftPressed) {
            handleShiftClick(tab, /* isDestructive= */ true);
        } else if (isCtrlPressed) {
            handleCtrlClick(tab);
        } else {
            selectTab(tab);
            clearMultiSelection(/* clearAnchor= */ true, /* notifyObservers= */ true);
        }

        mRenderHost.requestRender();
    }

    /**
     * Handles a Ctrl+Click event, which toggles the selection state of a single tab. If the tab is
     * already in the multi-selection set, it is removed; otherwise, it is added. If the tab is
     * added to the selection, it is also set as the active tab.
     *
     * @param clickedTab The tab that was clicked.
     */
    private void handleCtrlClick(StripLayoutTab clickedTab) {
        if (clickedTab == null || clickedTab.isDying() || mModel == null) return;
        int tabId = clickedTab.getTabId();
        // If the tab is already multi-selected, ctrl click should unselect it.
        if (mModel.isTabMultiSelected(tabId)) {
            if (tabId == getSelectedTabId()) {
                handleSelectedTabCtrlClicked(tabId);
                return;
            }
            mModel.setTabsMultiSelected(Collections.singleton(tabId), false);
        } else {
            int oldSelectedTabId = getSelectedTabId();
            // select clicked tab.
            selectTab(clickedTab);
            // When Ctrl clicked, even the previous tab gets selected.
            mModel.setTabsMultiSelected(Set.of(tabId, oldSelectedTabId), true);
            // Clear anchor tab.
            mAnchorTabId = Tab.INVALID_TAB_ID;
        }
    }

    /**
     * Handles a Shift+Click event, which selects a range of tabs from an anchor tab to the clicked
     * tab.
     *
     * @param clickedTab The tab that was clicked, representing the endpoint of the range.
     * @param isDestructive If true, any existing multi-selection is cleared before the new range is
     *     selected. If false, the new range is added to the existing selection.
     */
    private void handleShiftClick(StripLayoutTab clickedTab, boolean isDestructive) {
        if (clickedTab == null || clickedTab.isDying() || mModel == null) return;
        if (isDestructive) {
            clearMultiSelection(/* clearAnchor= */ false, /* notifyObservers= */ false);
        }
        if (mAnchorTabId == Tab.INVALID_TAB_ID) {
            // If there's no anchor, treat the previously selected tab as anchor.
            mAnchorTabId = getSelectedTabId();
        }

        int anchorIndex = mModel.indexOf(getTabById(mAnchorTabId));
        int clickedIndex = mModel.indexOf(getTabById(clickedTab.getTabId()));

        int startIndex = Math.min(anchorIndex, clickedIndex);
        int endIndex = Math.max(anchorIndex, clickedIndex);

        Set<Integer> selectedTabIds = new HashSet<>();
        Set<Token> tabGroupIds = new HashSet<>();
        if (startIndex != -1 && endIndex != -1) {
            for (int i = startIndex; i <= endIndex; i++) {
                int tabId = mStripTabs[i].getTabId();
                selectedTabIds.add(tabId);
                Tab tab = mModel.getTabById(tabId);
                // If part of a tab group, expand the tab group.
                if (tab == null) return;
                Token tabGroupId = tab.getTabGroupId();
                if (tabGroupId != null
                        && !tabGroupIds.contains(tabGroupId)
                        && mTabGroupModelFilter != null) {
                    mTabGroupModelFilter.setTabGroupCollapsed(
                            tabGroupId, false, /* animate= */ true);
                    tabGroupIds.add(tabGroupId);
                }
            }
        }
        selectTab(clickedTab);
        mModel.setTabsMultiSelected(/* tabIds= */ selectedTabIds, /* isSelected= */ true);
    }

    /**
     * Sets the given tab as the currently active tab in the TabModel and records relevant usage
     * metrics.
     *
     * @param tab The tab to select.
     */
    private void selectTab(StripLayoutTab tab) {
        if (tab == null || tab.isDying() || mModel == null) return;
        RecordUserAction.record("MobileTabSwitched.TabletTabStrip");
        recordTabSwitchTimeHistogram();

        int newIndex = TabModelUtils.getTabIndexById(mModel, tab.getTabId());
        // Early return, since placeholder tabs are known to not have tab ids.
        if (newIndex == Tab.INVALID_TAB_ID) return;
        TabModelUtils.setIndex(mModel, newIndex);
    }

    /**
     * Clears the entire set of multi-selected tabs.
     *
     * @param clearAnchor If true, the anchor tab for Shift+Click range selection is also reset.
     */
    private void clearMultiSelection(boolean clearAnchor, boolean notifyObservers) {
        if (clearAnchor) {
            // Clear anchor tab.
            mAnchorTabId = Tab.INVALID_TAB_ID;
        }
        if (mModel == null) return;
        mModel.clearMultiSelection(notifyObservers);
    }

    /**
     * Handles the specific user action of Ctrl+clicking the currently active tab. This action
     * deselects the active tab and transfers the active status to another tab within the existing
     * multi-selection. The new active tab will be the leftmost tab in the current selection. if the
     * clicked tab is the only one selected, this method does nothing to prevent a state with no
     * active tab.
     *
     * @param tabId The ID of the currently active tab that was clicked.
     */
    private void handleSelectedTabCtrlClicked(int tabId) {
        if (mModel == null || mModel.getMultiSelectedTabsCount() <= 1 || mStripTabs.length <= 1) {
            // Can't deselect the only tab.
            return;
        }

        // Find and select the new active tab, which will be the leftmost tab
        // in the selection that isn't the one being deselected.
        for (StripLayoutTab stripTab : mStripTabs) {
            int id = stripTab.getTabId();
            if (id != tabId && mModel.isTabMultiSelected(id)) {
                selectTab(stripTab);
                mModel.setTabsMultiSelected(Collections.singleton(tabId), false);
                break;
            }
        }
    }

    /**
     * Toggles multiselection on the keyboard focused tab.
     *
     * @return Whether the multiselect action was successfully performed.
     */
    public boolean multiselectKeyboardFocusedItem() {
        @Nullable StripLayoutView focusedView = getKeyboardFocusedView();
        if (focusedView instanceof StripLayoutTab) {
            multiselectKeyboardFocusedItem((StripLayoutTab) focusedView);
            return true;
        }
        return false;
    }

    private void multiselectKeyboardFocusedItem(StripLayoutTab tab) {
        if (tab == null || tab.isDying() || mModel == null) return;
        int tabId = tab.getTabId();
        // If the tab is already multi-selected, unselect it.
        if (mModel.isTabMultiSelected(tabId)) {
            mModel.setTabsMultiSelected(Collections.singleton(tabId), false);
        } else {
            int activeTabId = getSelectedTabId();
            // When toggling multiselect, we need to add the active tab to the multi-selection set.
            // This is an additive operation, and does not reset the selection set.
            mModel.setTabsMultiSelected(Set.of(tabId, activeTabId), true);
        }
    }

    public int getAnchorTabIdForTesting() {
        return mAnchorTabId;
    }

    private void handleGroupTitleClick(StripLayoutGroupTitle groupTitle) {
        if (groupTitle == null || mTabGroupModelFilter == null) return;

        Token tabGroupId = groupTitle.getTabGroupId();
        boolean isCollapsed = mTabGroupModelFilter.getTabGroupCollapsed(tabGroupId);
        assert isCollapsed == groupTitle.isCollapsed();

        mTabGroupModelFilter.setTabGroupCollapsed(tabGroupId, !isCollapsed, /* animate= */ true);
        RecordHistogram.recordBooleanHistogram("Android.TabStrip.TabGroupCollapsed", !isCollapsed);
    }

    private void handleNewTabClick() {
        if (mModel == null || mTabCreator == null) return;

        RecordUserAction.record("MobileToolbarNewTab");
        if (!mModel.isIncognito()) mModel.commitAllTabClosures();
        TabCreatorUtil.launchNtp(mTabCreator);
    }

    /**
     * Closes the given {@link StripLayoutTab}.
     *
     * <p>Sequence of events:
     *
     * <ol>
     *   <li>Call {@code TabRemover#prepareCloseTabs()}. We don't call {@code
     *       TabRemover#forceCloseTabs()} since the {@link TabModel} or other systems might do some
     *       "pre-work", such as showing the "delete group confirmation dialog" if the last tab in a
     *       group is being closed.
     *   <li>Call {@link #handleCloseTab(StripLayoutTab, boolean)} to start the tab removal
     *       animation and remove the tab when the animation ends.
     * </ol>
     *
     * @param tab the {@link StripLayoutTab} to close.
     * @param motionEventButtonState {@link MotionEvent#getButtonState()} at the moment of the click
     *     if the click is detected via motion events; otherwise, this parameter is {@link
     *     MotionEventUtils#MOTION_EVENT_BUTTON_NONE}.
     * @see #handleCloseTab(StripLayoutTab, boolean)
     */
    @VisibleForTesting
    void handleCloseButtonClick(@Nullable StripLayoutTab tab, int motionEventButtonState) {
        // Placeholder tabs are expected to have invalid tab ids.
        if (tab == null || tab.isDying() || tab.getTabId() == Tab.INVALID_TAB_ID) return;
        RecordUserAction.record("MobileToolbarCloseTab");

        int tabId = tab.getTabId();
        Tab realTab = assumeNonNull(getTabById(tabId));
        Token tabGroupId = realTab.getTabGroupId();

        // Set mouse tab closure state.
        if (MotionEventUtils.isPrimaryButton(motionEventButtonState)) {
            mPendingMouseTabClosure = true;
            boolean pendingClosureIsEndMostTab = isEndMostTab(tabId);
            if (pendingClosureIsEndMostTab) {
                // Store the properties since the tab may be removed by the time we're resizing. We
                // can't infer the width from #getCachedTabWidth when we're resizing, since we might
                // call #computeAndUpdateTabWidth multiple times, changing the cached width.
                mClosingEndMostTabDrawX = tab.getDrawX();
                mClosingEndMostTabWidth = tab.getWidth();
            }
        }

        // Prepare close params.
        StripTabModelActionListener listener = null;
        if (tabGroupId != null) {
            listener =
                    new StripTabModelActionListener(
                            tabGroupId,
                            ActionType.CLOSE,
                            mGroupIdToHideSupplier,
                            mToolbarContainerView,
                            /* beforeSyncDialogRunnable= */ null,
                            /* onSuccess= */ null);
        }
        Callback<TabClosureParams> onPreparedCallback =
                (tabClosureParams) -> {
                    // Note: the documentation of TabRemover#prepareCloseTabs() says we should use
                    // the TabClosureParams here to close the tab, but historically this class
                    // ignores this TabClosureParams and creates a new one later, in
                    // finishAnimationsAndCloseDyingTabs().
                    //
                    // For now, we follow the status quo by passing parameters such as "allowUndo"
                    // so that the new TabClosureParams created in
                    // finishAnimationsAndCloseDyingTabs() can get these parameters.
                    //
                    // TODO(crbug.com/415079634): check if passing TabClosureParams to
                    //  finishAnimationsAndCloseDyingTabs() is the right thing to do since this
                    //  indicates only closing the tab that was clicked instead of all dying tabs.
                    List<Tab> tabs = tabClosureParams.tabs;
                    assert tabs != null && tabs.size() == 1 && tabs.get(0) == realTab;
                    handleCloseTab(tab, tabClosureParams.allowUndo);
                };

        boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(motionEventButtonState);
        TabClosureParams params =
                TabClosureParams.closeTab(realTab)
                        .allowUndo(allowUndo)
                        .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                        .build();
        if (mTabGroupModelFilter == null) return;
        mTabGroupModelFilter
                .getTabModel()
                .getTabRemover()
                .prepareCloseTabs(params, /* allowDialog= */ true, listener, onPreparedCallback);
    }

    private void clearPendingMouseTabClosureState() {
        mPendingMouseTabClosure = false;
        mClosingEndMostTabDrawX = null;
        mClosingEndMostTabWidth = null;
    }

    private void clearClosingGroupTitleState(Token tabGroupId) {
        releaseResourcesForGroupTitle(tabGroupId);
        if (Objects.equals(tabGroupId, mGroupIdToHideSupplier.get())) {
            // Clear the hidden group ID if the group has been removed from the model.
            mGroupIdToHideSupplier.set(null);
        }
    }

    private @Nullable StripLayoutView determineClickedView(float x, float y, int buttons) {
        if (mNewTabButton.click(x, y, buttons)) return mNewTabButton;
        StripLayoutView view = getViewAtPositionX(x, true);
        if (view instanceof StripLayoutTab clickedTab) {
            if (clickedTab.checkCloseHitTest(x, y) || MotionEventUtils.isTertiaryButton(buttons)) {
                return clickedTab.getCloseButton();
            }
            return clickedTab;
        }
        return view;
    }

    private void recordTabSwitchTimeHistogram() {
        if (mTabScrollStartTime == null || mMostRecentTabScroll == null) return;

        long endTime = SystemClock.elapsedRealtime();
        long duration = endTime - mTabScrollStartTime;
        long timeFromLastInteraction = endTime - mMostRecentTabScroll;

        // Discard sample if last scroll was over the max allowed interval.
        if (timeFromLastInteraction <= TAB_SWITCH_METRICS_MAX_ALLOWED_SCROLL_INTERVAL) {
            RecordHistogram.deprecatedRecordMediumTimesHistogram(
                    "Android.TabStrip.TimeToSwitchTab", duration);
        }

        mTabScrollStartTime = null;
        mMostRecentTabScroll = null;
    }

    /** Returns Whether or not the tabs are moving. */
    public boolean isAnimatingForTesting() {
        return (mRunningAnimator != null && mRunningAnimator.isRunning())
                || !mScrollDelegate.isFinished();
    }

    @Override
    public CompositorAnimationHandler getAnimationHandler() {
        return mUpdateHost.getAnimationHandler();
    }

    @Override
    public void finishAnimations() {
        finishAnimations(/* startQueuedCloseAnimations= */ true);
    }

    /**
     * Finishes any outstanding animations.
     *
     * @param startQueuedCloseAnimations True iff we should try to queue any close animations.
     *     Should only be false when calling from the closure flow. Most, if not all, other
     *     call-sites want this to be true to avoid interacting with stale StripLayoutTab state.
     */
    private void finishAnimations(boolean startQueuedCloseAnimations) {
        // Start any queued animations. This will ensure that their end state is reached, and any
        // listeners are triggered accordingly.
        if (startQueuedCloseAnimations) queueCloseAnimationsIfAny();
        startQueuedAnimationsIfAny();
        // Force any outstanding animations to finish. Need to recurse as some animations (like the
        // multi-step tab close animation) kick off another animation once the first ends.
        while (mRunningAnimator != null && mRunningAnimator.isRunning()) {
            mRunningAnimator.end();
        }
        mRunningAnimator = null;
    }

    @Override
    public void startAnimations(
            @Nullable List<Animator> animationList, @Nullable AnimatorListener listener) {
        AnimatorSet set = getAnimatorSet(animationList, listener);
        finishAnimations();
        setAndStartRunningAnimator(set);
    }

    private void startAnimations(List<Animator> animationList) {
        startAnimations(animationList, /* listener= */ null);
    }

    private void setAndStartRunningAnimator(Animator animator) {
        mRunningAnimator = animator;
        mRunningAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        // Clear any persisting tab hover state after tab strip animations have
                        // ended. This is to prevent the hover state from sticking after an action
                        // on the strip, including and not limited to tab closure and tab
                        // reordering.
                        clearTabHoverState();
                    }
                });
        mRunningAnimator.start();
    }

    @Override
    public void queueAnimations(
            @Nullable List<Animator> animationList, @Nullable AnimatorListener listener) {
        AnimatorSet set = getAnimatorSet(animationList, listener);
        mQueuedAnimators.add(set);
        // The queued animators get started in the next #updateLayout call. Request an update here
        // to ensure that we get one of these calls.
        mUpdateHost.requestUpdate();
    }

    private void requestCloseAnimations() {
        mCloseAnimationsRequested = true;
        if (!mSelected) {
            // Intentionally called after mCloseAnimationsRequested is set to true. This is so the
            // #queueCloseAnimationsIfAny call in #finishAnimations will succeed in queueing the
            // close animations. They are then immediately finished in the same #finishAnimations
            // call. This resets mCloseAnimationsRequested back to false as expected.
            finishAnimations();
        } else {
            // Passes startQueuedCloseAnimations as false to prevent clobbering the close animations
            // for any other simultaneously removed views.
            finishAnimations(/* startQueuedCloseAnimations= */ false);
            mUpdateHost.requestUpdate();
        }
    }

    private void queueCloseAnimationsIfAny() {
        if (!mCloseAnimationsRequested) return;
        mCloseAnimationsRequested = false;

        // TODO(crbug.com/450076798): Unify closing tabs + closing group titles logic.
        // Set initial state.
        for (StripLayoutView view : mClosingTabs) view.setIsDying(true);
        for (StripLayoutView view : mClosingGroupTitles) view.setIsDying(true);

        // Create animators.
        List<Animator> animationList = getTabClosingAnimators(mClosingTabs);
        if (ChromeFeatureList.sTabletTabStripAnimation.isEnabled()) {
            for (StripLayoutGroupTitle groupTitle : mClosingGroupTitles) {
                animationList.add(
                        getViewWidthAnimator(
                                groupTitle, mGroupTitleOverlapWidth, ANIM_TAB_CLOSED_MS));
            }
        }

        // Queue the animations.
        queueAnimations(
                animationList,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        boolean runImprovedTabAnimations = mStripTabs.length > 1;
                        rebuildStripTabs(/* deferAnimations= */ false);
                        if (!ChromeFeatureList.sTabletTabStripAnimation.isEnabled()) {
                            resizeStripOnTabClose(runImprovedTabAnimations);
                        }
                        clearStateOnCloseAnimationsEnd();
                    }
                });
    }

    private void clearStateOnCloseAnimationsEnd() {
        for (StripLayoutGroupTitle groupTitle : mClosingGroupTitles) {
            clearClosingGroupTitleState(groupTitle.getTabGroupId());
        }
        clearPendingMouseTabClosureState();
        mClosingTabs.clear();
        mClosingGroupTitles.clear();
    }

    private AnimatorSet getAnimatorSet(
            @Nullable List<Animator> animationList, @Nullable AnimatorListener listener) {
        AnimatorSet set = new AnimatorSet();
        set.playTogether(animationList);
        if (listener != null) set.addListener(listener);
        return set;
    }

    private void startQueuedAnimationsIfAny() {
        if (!mQueuedAnimators.isEmpty()) {
            // We need to clone and clear mQueuedAnimators to prevent an infinite-loop when we try
            // to #finishAnimations in #startAnimations below.
            List<Animator> queuedAnimators = new ArrayList<>(mQueuedAnimators);
            mQueuedAnimators.clear();
            startAnimations(queuedAnimators);
        }
    }

    @Override
    public void finishAnimationsAndPushTabUpdates() {
        finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
    }

    /**
     * Finishes any outstanding animations and closes tabs that are dying.
     *
     * <p>Note that this method creates new {@link TabClosureParams} using the parameters passed in.
     *
     * @param allowUndo whether to allow undo of tab closure, such as showing the "undo" snackbar.
     */
    private void finishAnimationsAndCloseDyingTabs(boolean allowUndo) {
        if (mRunningAnimator == null) return;

        // 1. Finish animations.
        finishAnimations();

        // 2. Figure out which tabs need to be closed.
        ArrayList<StripLayoutTab> tabsToRemove = new ArrayList<>();
        for (StripLayoutTab tab : mStripTabs) {
            if (tab.isDying() && !tab.shouldSkipAsyncClosure()) tabsToRemove.add(tab);
        }

        if (tabsToRemove.isEmpty()) return;

        // 3. Mark all StripLayoutTabs to remove as "closed".
        for (StripLayoutTab tab : tabsToRemove) {
            tab.setIsClosed(true);
        }

        // 4. Remove tabs from the TabModel.
        //    Between when the close button was clicked and when the tab removal animation ended,
        //    the tab may have already been removed from the TabModel.
        //    So we call TabRemover#forceCloseTabs() in an async task to avoid
        //    ConcurrentModificationException.
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (mModel == null) return;
                    for (StripLayoutTab stripTab : tabsToRemove) {
                        @Nullable Tab tab = mModel.getTabById(stripTab.getTabId());
                        if (tab == null) continue;
                        // Tab group closure related dialogs are handled elsewhere and any logic
                        // related to them can be bypassed.
                        mModel.getTabRemover()
                                .forceCloseTabs(
                                        TabClosureParams.closeTab(tab)
                                                .allowUndo(allowUndo)
                                                .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                                                .build());
                    }

                    if (!tabsToRemove.isEmpty()) mUpdateHost.requestUpdate();
                });
    }

    private void updateSpinners(long time) {
        long diff = time - mLastSpinnerUpdate;
        float degrees = diff * SPINNER_DPMS;
        boolean tabsToLoad = false;
        for (int i = 0; i < mStripTabs.length; i++) {
            StripLayoutTab tab = mStripTabs[i];
            // TODO(clholgat): Only update if the tab is visible.
            if (tab.isLoading()) {
                tab.addLoadingSpinnerRotation(degrees);
                tabsToLoad = true;
            }
        }
        mLastSpinnerUpdate = time;
        if (tabsToLoad) {
            mStripTabEventHandler.removeMessages(MESSAGE_UPDATE_SPINNER);
            mStripTabEventHandler.sendEmptyMessageDelayed(
                    MESSAGE_UPDATE_SPINNER, SPINNER_UPDATE_DELAY_MS);
        }
    }

    @VisibleForTesting
    void updateScrollOffsetLimits() {
        mScrollDelegate.updateScrollOffsetLimits(
                mStripViews,
                mWidth,
                mLeftMargin,
                mRightMargin,
                getCachedTabWidth(/* isPinned= */ false),
                TAB_OVERLAP_WIDTH_DP,
                mGroupTitleOverlapWidth);
    }

    /**
     * Rebuilds the list of {@link StripLayoutTab}s based on the {@link TabModel}. Reuses strip tabs
     * that still exist in the model. Sets tabs at their new position and animates any width
     * changes, unless a multi-step close is running. Requests a layout update.
     *
     * @param deferAnimations Whether or not the resultant width changes should automatically run,
     *     or returned as a list to be kicked off simultaneously with other animations.
     * @return The list of width {@link Animator}s to run, if any.
     */
    private @Nullable List<Animator> rebuildStripTabs(boolean deferAnimations) {
        if (mModel == null) return List.of();
        final int count = mModel.getCount();
        StripLayoutTab[] tabs = new StripLayoutTab[count];

        for (int i = 0; i < count; i++) {
            final Tab tab = assumeNonNull(mModel.getTabAt(i));
            final int id = tab.getId();
            final StripLayoutTab oldTab = findTabById(id);
            boolean isPinned = isTabPinningFromStripEnabled() && tab.getIsPinned();
            tabs[i] = oldTab != null ? oldTab : createStripTab(id, isPinned);
            setAccessibilityDescription(tabs[i], tab);
        }

        mStripTabs = tabs;
        // Update stripViews since tabs are updated.
        rebuildStripViews();

        // If a tab close is animating, the resize may be handled elsewhere.
        if (mMultiStepTabCloseAnimRunning || mPendingMouseTabClosure) return null;
        recordPinnedOnlyTabStripUserAction();

        // Otherwise, animate the required width changes.
        computeIdealViewPositions();
        finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
        return computeAndUpdateTabWidth(
                /* animate= */ true, /* deferAnimations= */ deferAnimations);
    }

    private String buildGroupAccessibilityDescription(StripLayoutGroupTitle groupTitle) {
        Resources res = mContext.getResources();
        StringBuilder builder = new StringBuilder();

        // 1. Determine and append the correct a11y string for the group depending on its shared
        // state.
        @StringRes int resId = R.string.accessibility_tabstrip_group;
        if (groupTitle.isGroupShared()) {
            resId =
                    groupTitle.getNotificationBubbleShown()
                            ? R.string.accessibility_tabstrip_shared_group_with_new_activity
                            : R.string.accessibility_tabstrip_shared_group;
        }
        String groupDescription = res.getString(resId, groupTitle.getTitle());
        builder.append(groupDescription);

        // 2. Retrieve the grouped tabs and append the tab titles.
        List<Tab> relatedTabs =
                mTabGroupModelFilter == null
                        ? List.of()
                        : mTabGroupModelFilter.getTabsInGroup(groupTitle.getTabGroupId());
        int relatedTabsCount = relatedTabs.size();
        if (relatedTabsCount > 0) {
            final String contentDescriptionSeparator = " - ";
            builder.append(contentDescriptionSeparator);

            String firstTitle = relatedTabs.get(0).getTitle();
            String tabsDescription;
            if (relatedTabsCount == 1) {
                // <title>
                tabsDescription = firstTitle;
            } else {
                // <title> and <num> other tabs
                int descriptionRes = R.string.accessibility_tabstrip_group_multiple_tabs;
                tabsDescription = res.getString(descriptionRes, firstTitle, relatedTabsCount - 1);
            }
            builder.append(tabsDescription);
        }

        return builder.toString();
    }

    private void updateGroupAccessibilityDescription(@Nullable StripLayoutGroupTitle groupTitle) {
        if (groupTitle == null) return;
        groupTitle.setAccessibilityDescription(buildGroupAccessibilityDescription(groupTitle));
    }

    @Override
    public void releaseResourcesForGroupTitle(@Nullable Token groupId) {
        if (mLayerTitleCache != null) mLayerTitleCache.removeGroupTitle(groupId);
    }

    @Override
    public void rebuildResourcesForGroupTitle(StripLayoutGroupTitle groupTitle) {
        updateGroupTitleBitmapIfNeeded(groupTitle);
    }

    private AnimatorListener getCollapseAnimatorListener(
            @Nullable StripLayoutGroupTitle collapsedGroupTitle) {
        return new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                if (collapsedGroupTitle != null) collapsedGroupTitle.setBottomIndicatorWidth(0.f);
            }
        };
    }

    void collapseTabGroupForTesting(StripLayoutGroupTitle groupTitle, boolean isCollapsed) {
        updateTabGroupCollapsed(groupTitle, isCollapsed, true);
        // End animator set to invoke all pending listeners on set.
        finishAnimations();
    }

    private @Nullable Animator updateTabCollapsed(
            @Nullable StripLayoutTab tab, boolean isCollapsed, boolean animate) {
        if (tab == null) return null;
        tab.setCollapsed(isCollapsed);

        // The tab expand will be handled when the tab strip resizes, since we'll need to first
        // update mCachedTabWidth.
        if (!isCollapsed) return null;

        // Set to the tab overlap width so the tab effectively takes up no space. If we instead
        // animate to 0, the following tabs will unexpectedly be shifted as this tab takes up
        // "negative" space.
        if (!animate) {
            tab.setWidth(TAB_OVERLAP_WIDTH_DP);
            return null;
        }

        return getViewWidthAnimator(tab, TAB_OVERLAP_WIDTH_DP, ANIM_TAB_RESIZE_MS);
    }

    private void updateTabGroupCollapsed(
            StripLayoutGroupTitle groupTitle, boolean isCollapsed, boolean animate) {
        if (mModel == null) return;
        if (groupTitle.isCollapsed() == isCollapsed) return;

        List<Animator> collapseAnimationList = animate ? new ArrayList<>() : null;

        finishAnimations();
        groupTitle.setCollapsed(isCollapsed);
        List<StripLayoutTab> groupedTabs =
                StripLayoutUtils.getGroupedTabs(mModel, mStripTabs, groupTitle.getTabGroupId());
        for (StripLayoutTab tab : groupedTabs) {
            if (collapseAnimationList != null) {
                Animator animator = updateTabCollapsed(tab, isCollapsed, true);
                if (animator != null) collapseAnimationList.add(animator);
            } else {
                updateTabCollapsed(tab, isCollapsed, false);
            }
        }

        // Similar to bottom indicator collapse animation, the expansion animation should also begin
        // from the padded width of the group title.
        if (!isCollapsed) {
            groupTitle.setBottomIndicatorWidth(groupTitle.getPaddedWidth());
        }

        finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
        List<Animator> resizeAnimationList =
                computeAndUpdateTabWidth(/* animate= */ animate, /* deferAnimations= */ animate);
        if (collapseAnimationList != null) {
            StripLayoutGroupTitle collapsedGroupTitle = null;
            if (isCollapsed) {
                // Animate bottom indicator down to the group title padded width when collapsing,
                // and then hide the remaining portion under the group title.
                collapsedGroupTitle = groupTitle;
                collapseAnimationList.add(
                        CompositorAnimator.ofFloatProperty(
                                mUpdateHost.getAnimationHandler(),
                                groupTitle,
                                StripLayoutGroupTitle.BOTTOM_INDICATOR_WIDTH,
                                groupTitle.getBottomIndicatorWidth(),
                                groupTitle.getPaddedWidth(),
                                ANIM_TAB_RESIZE_MS));
            }

            if (resizeAnimationList != null) collapseAnimationList.addAll(resizeAnimationList);
            startAnimations(
                    collapseAnimationList, getCollapseAnimatorListener(collapsedGroupTitle));
        } else {
            if (isCollapsed) {
                groupTitle.setBottomIndicatorWidth(0.f);
            }
        }

        // Select an adjacent expanded tab if the current selected tab is being collapsed, If all
        // tabs are collapsed, open a ntp.
        if (isCollapsed) {
            Tab selectedTab = getTabById(getSelectedTabId());
            if (selectedTab != null
                    && groupTitle.getTabGroupId().equals(selectedTab.getTabGroupId())) {
                int nextIndex = getNearbyTabIndex(groupedTabs);
                if (nextIndex != TabModel.INVALID_TAB_INDEX && mModel != null) {
                    TabModelUtils.setIndex(mModel, nextIndex);
                } else if (mTabCreator != null) {
                    TabCreatorUtil.launchNtp(mTabCreator);
                }
            }
        }
    }

    /**
     * Handles edge cases such as merging a selected tab into a collapsed tab group through GTS,
     * followed by exiting GTS with a back gesture. The tab group containing the selected tab should
     * be expanded.
     */
    protected void expandGroupOnGtsExit() {
        if (mTabGroupModelFilter == null) return;
        StripLayoutTab selectedTab = getSelectedStripTab();
        if (selectedTab == null) {
            return;
        }
        Tab tab = getTabById(selectedTab.getTabId());
        Token tabGroupId = tab != null ? tab.getTabGroupId() : null;
        if (tabGroupId != null && mTabGroupModelFilter.getTabGroupCollapsed(tabGroupId)) {
            mTabGroupModelFilter.deleteTabGroupCollapsed(tabGroupId);
        }
    }

    @Override
    public int getNextIndexAfterClose(Collection<StripLayoutTab> closingTabs) {
        // Intentionally kept separate from #getNearbyNotClosingTabIndex, to have more specific
        // javadocs for each method.
        return getNearbyTabIndex(closingTabs);
    }

    /**
     * Wrapper for {@link #getNearbyTabIndex(Collection, Collection)}. Prioritizes tabs in a certain
     * direction based on {@link ChromeFeatureList#TAB_STRIP_AUTO_SELECT_ON_CLOSE_CHANGE}.
     */
    private int getNearbyTabIndex(Collection<StripLayoutTab> excludedTabs) {
        // Have to create a copy of the list, so that the reverse doesn't affect the original array.
        List<StripLayoutTab> allTabs = new ArrayList<>(Arrays.asList(mStripTabs));
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_STRIP_AUTO_SELECT_ON_CLOSE_CHANGE)) {
            // If the flag is enabled, reverse the order of tabs to prefer picking a nearby tab
            // after (as opposed to before) the excluded tabs.
            Collections.reverse(allTabs);
        }
        return getNearbyTabIndex(allTabs, excludedTabs);
    }

    /**
     * Wrapper for {@link #getNearbyTabIndex(Collection, Collection, boolean)}. Prioritizes expanded
     * tabs, if possible.
     */
    private int getNearbyTabIndex(
            Collection<StripLayoutTab> allTabs, Collection<StripLayoutTab> excludedTabs) {
        int nearbyIndex = getNearbyTabIndex(allTabs, excludedTabs, /* ignoreCollapsedTabs= */ true);
        if (nearbyIndex != TabModel.INVALID_TAB_INDEX) return nearbyIndex;
        return getNearbyTabIndex(allTabs, excludedTabs, /* ignoreCollapsedTabs= */ false);
    }

    /**
     * Returns The index of a tab nearest to the excluded tabs. Can include or ignore collapsed
     * tabs. Prioritizes tabs before the {@code excludedTabs} in {@code allTabs}, though the
     * ordering of {@param allTabs} does not necessarily reflect that of the {@link TabModel}.
     *
     * @param allTabs All of the {@link StripLayoutTab}s. Ordered to either prefer picking tabs
     *     before or after the excluded tabs, in the {@link TabModel}.
     * @param excludedTabs The excluded {@link StripLayoutTab}s.
     * @param ignoreCollapsedTabs Whether we should include collapsed tabs or not.
     */
    private int getNearbyTabIndex(
            Collection<StripLayoutTab> allTabs,
            Collection<StripLayoutTab> excludedTabs,
            boolean ignoreCollapsedTabs) {
        StripLayoutTab nearbyTab = null;
        boolean seenExcludedTab = false;
        for (StripLayoutTab tab : allTabs) {
            // 1. If we encounter an excluded tab and already have a nearby tab, return its index.
            if (excludedTabs.contains(tab)) {
                if (nearbyTab != null) return findIndexForTab(nearbyTab.getTabId());
                seenExcludedTab = true;
                continue;
            }
            // 2. Potentially ignore collapsed tabs.
            if (tab.isCollapsed() && ignoreCollapsedTabs) continue;
            // 3. Process the current tab as a candidate nearby tab.
            if (seenExcludedTab) {
                // If we didn't return a tab when we saw the first closing tab, it means there's
                // none before the excluded tabs. Return the closest tab after them instead.
                return findIndexForTab(tab.getTabId());
            } else {
                nearbyTab = tab;
            }
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * Called when a tab has been merged into or removed from a group. Rebuilds the views and
     * re-computes ideal positions, since the order may have changed.
     */
    private void onTabMergeToOrMoveOutOfGroup() {
        finishAnimations();
        // Moving a tab into/out-of a group may cause the orders of views (i.e. the group indicator)
        // to change. The bottom indicator width may also change. Rebuild views to address this. We
        // need to rebuild tabs as well, since we get this signal before we get the #didMoveTab
        // event, meaning mStripViews is still stale.
        rebuildStripTabs(/* deferAnimations= */ false);
        // Since views may have swapped, re-calculate ideal positions here.
        computeIdealViewPositions();
    }

    /**
     * Called to refresh the group title bitmap when it may have changed (text, color, or shared
     * group avatar).
     *
     * @param groupTitle The group title to refresh the bitmap for.
     */
    private void updateGroupTitleBitmapIfNeeded(StripLayoutGroupTitle groupTitle) {
        if (mLayerTitleCache == null) return;
        if (groupTitle.isVisible()) {
            mLayerTitleCache.getUpdatedGroupTitle(
                    groupTitle.getTabGroupId(), groupTitle.getTitle(), mIncognito);
            mRenderHost.requestRender();
        }
    }

    private void updateGroupTitleTint(StripLayoutGroupTitle groupTitle) {
        if (mTabGroupModelFilter == null) return;
        int colorId = mTabGroupModelFilter.getTabGroupColor(groupTitle.getTabGroupId());
        // If the color is invalid, temporarily assign a default placeholder color.
        if (colorId == TabGroupColorUtils.INVALID_COLOR_ID) colorId = TabGroupColorId.GREY;
        updateGroupTitleTint(groupTitle, colorId);
    }

    private void updateGroupTitleTint(
            @Nullable StripLayoutGroupTitle groupTitle, @TabGroupColorId int newColor) {
        if (groupTitle == null) return;

        groupTitle.updateTint(newColor);
        updateGroupTitleBitmapIfNeeded(groupTitle);
    }

    @VisibleForTesting
    void updateGroupTextAndSharedState(@Nullable Token tabGroupId) {
        updateGroupTextAndSharedState(findGroupTitle(tabGroupId));
    }

    private void updateGroupTextAndSharedState(@Nullable StripLayoutGroupTitle groupTitle) {
        if (groupTitle == null
                || mTabGroupModelFilter == null
                || !mTabGroupModelFilter.tabGroupExists(groupTitle.getTabGroupId())) {
            return;
        }
        updateGroupTextAndSharedStateUnchecked(groupTitle);
    }

    /**
     * Sets a non-empty title text for the given group indicator. Also updates the title text
     * bitmap, accessibility description, and tab/indicator sizes if necessary. If the group is
     * shared, it may also update user avatars and the notification bubble. This method does not
     * check if the group exists as during initialization it may not yet exist, but should be drawn.
     *
     * @param groupTitle The {@link StripLayoutGroupTitle} that we're update the title text for.
     */
    private void updateGroupTextAndSharedStateUnchecked(StripLayoutGroupTitle groupTitle) {
        if (mLayerTitleCache == null || mTabGroupModelFilter == null) return;

        // Ignore updates for closing group indicators. This prevents assertion errors from using
        // stale group properties.
        if (groupTitle.willClose()) return;

        // 1. Update indicator text and width.
        String titleText =
                TabGroupTitleUtils.getDisplayableTitle(
                        mContext, mTabGroupModelFilter, groupTitle.getTabGroupId());
        int widthPx = mLayerTitleCache.getGroupTitleWidth(mIncognito, titleText);
        float widthDp = widthPx / mContext.getResources().getDisplayMetrics().density;
        float oldWidth = groupTitle.getWidth();

        // Recalculate width, including avatar width for shared groups if applicable.
        groupTitle.updateTitle(titleText, widthDp);
        updateGroupAccessibilityDescription(groupTitle);

        // 2. Update title text and avatar bitmap if needed.
        updateGroupTitleBitmapIfNeeded(groupTitle);

        // 3. Handle indicator size change if needed.
        if (groupTitle.getWidth() != oldWidth) {
            if (groupTitle.isVisible()) {
                // If on-screen, this may result in the ideal tab width changing.
                finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
                computeAndUpdateTabWidth(/* animate= */ false, /* deferAnimations= */ false);
            } else {
                // If off-screen, request an update so we re-calculate tab initial positions and the
                // scroll offset limit.
                mUpdateHost.requestUpdate();
            }
        }
    }

    @Contract("!null -> !null")
    private @Nullable StripLayoutGroupTitle findGroupTitle(@Nullable Token tabGroupId) {
        return StripLayoutUtils.findGroupTitle(mStripGroupTitles, tabGroupId);
    }

    private StripLayoutGroupTitle findOrCreateGroupTitle(Token tabGroupId) {
        StripLayoutGroupTitle groupTitle = findGroupTitle(tabGroupId);
        return groupTitle == null ? createGroupTitle(tabGroupId) : groupTitle;
    }

    private StripLayoutGroupTitle createGroupTitle(Token tabGroupId) {
        // Delay setting the collapsed state, since mStripViews may not yet be up to date.
        StripLayoutGroupTitle groupTitle =
                new StripLayoutGroupTitle(
                        mContext,
                        /* delegate= */ this,
                        /* keyboardFocusHandler= */ this,
                        mIncognito,
                        tabGroupId);
        pushPropertiesToGroupTitle(groupTitle);

        // Must pass in the group title instead of group id, since the StripLayoutGroupTitle has not
        // been added to mStripViews yet.
        updateGroupTitleTint(groupTitle);
        updateGroupTextAndSharedState(groupTitle);

        // Update tab group share avatars if necessary. The data sharing observer should already be
        // in place by this point.
        updateSharedTabGroupIfNeeded(groupTitle);
        return groupTitle;
    }

    private void pushPropertiesToGroupTitle(StripLayoutGroupTitle groupTitle) {
        groupTitle.setDrawY(0);
        groupTitle.setHeight(mHeight);
        groupTitle.setTouchTargetInsets(null, mTopPadding, null, -mTopPadding);
    }

    @VisibleForTesting
    void rebuildStripViews() {
        if (mTabGroupModelFilter != null && mTabStateInitialized) {
            copyTabsWithGroupTitles();
            buildBottomIndicator();
        } else {
            copyTabs();
        }
        // If views are reordered, the CompositorView's keyboard focus index will be wrong; fix it.
        mUpdateHost.requestUpdate(
                () -> {
                    @Nullable StripLayoutView keyboardFocusedView = getKeyboardFocusedView();
                    if (keyboardFocusedView != null) {
                        mManagerHost.requestKeyboardFocus(mSceneOverlay, keyboardFocusedView);
                    }
                });
    }

    private void rebuildStripViewsAfterMove() {
        if (mReorderDelegate.isReorderingTab()) {
            // Update strip start and end margins to create more space for first tab or last tab
            // to drag out of group.
            mReorderDelegate.setEdgeMarginsForReorder(mStripTabs);
        }
        // When tab groups are moved, each tab is moved one-by-one. During this process, the
        // invariant that tab groups must be contiguous is temporarily broken, so we suppress
        // rebuilding until the entire group is moved. See https://crbug.com/329318567.
        // TODO(crbug.com/329335086): Investigate reordering (with #moveElement) instead of
        // rebuilding here.
        rebuildStripViews();
        computeIdealViewPositions();
    }

    /**
     * Returns the tab group count. Should only be called when rebuilding the tab strip, as we've
     * previously seen {@link TabGroupModelFilter#getTabGroupCount()} be incorrect on startup. That
     * said, other call-sites should use the aforementioned TabGroupModelFilter method if possible.
     * TODO(crbug.com/454960178): Investigate if this is still needed.
     */
    private int getTabGroupCount() {
        if (mModel == null || mTabGroupModelFilter == null) return 0;

        Set<Token> groupIds = new HashSet<>();

        for (int i = 0; i < mStripTabs.length; ++i) {
            final StripLayoutTab stripTab = mStripTabs[i];
            final Tab tab = mModel.getTabByIdChecked(stripTab.getTabId());
            if (mTabGroupModelFilter.isTabInTabGroup(tab)) groupIds.add(tab.getTabGroupId());
        }

        Token tabGroupIdToHide = mGroupIdToHideSupplier.get();
        if (tabGroupIdToHide != null && !groupIds.contains(tabGroupIdToHide)) {
            // Rebuilding tab strip with a group ID to hide without a matching Tab.
            // TODO(crbug.com/443337907): When we migrate the close button flow to the new closure
            //  flow, we should be able to assert that this should be unreachable, rather than just
            //  clearing the invalid state. This is currently possible because of the sequence of
            //  events we get when confirming a group deletion: we rebuild on the tab closure event,
            //  but clear the state on the group removal event.
            mGroupIdToHideSupplier.set(null);
        }

        return groupIds.size();
    }

    private void buildBottomIndicator() {
        if (mStripTabs.length == 0 || mTabResizeAnimRunning || mTabGroupModelFilter == null) {
            return;
        }
        for (int i = 0; i < mStripGroupTitles.length; i++) {
            StripLayoutGroupTitle groupTitle = mStripGroupTitles[i];
            if (groupTitle == null
                    || groupTitle.isCollapsed()
                    || groupTitle.getTabGroupId().equals(mGroupIdToHideSupplier.get())) {
                continue;
            }

            // Calculate the bottom indicator width.
            float bottomIndicatorWidth =
                    StripLayoutUtils.calculateBottomIndicatorWidth(
                            groupTitle,
                            StripLayoutUtils.getNumOfTabsInGroup(mTabGroupModelFilter, groupTitle),
                            getEffectiveTabWidth(/* isPinned= */ false));

            // Update the bottom indicator width.
            if (groupTitle.getBottomIndicatorWidth() != bottomIndicatorWidth) {
                groupTitle.setBottomIndicatorWidth(bottomIndicatorWidth);
            }
        }
    }

    private void copyTabsWithGroupTitles() {
        if (mStripTabs.length == 0) return;
        if (mTabGroupModelFilter == null) return;

        int numGroups = getTabGroupCount();

        // If we have tab group to hide due to running tab group delete dialog, then skip the tab
        // group when rebuilding StripViews.
        if (mGroupIdToHideSupplier.get() != null) {
            if (mTabGroupModelFilter.tabGroupExists(mGroupIdToHideSupplier.get())) {
                if (numGroups > 0) numGroups--;
            } else {
                assert false : "Rebuilding tab strip with a nonexistent group ID to hide.";
                mGroupIdToHideSupplier.set(null);
            }
        }

        int groupTitleIndex = 0;
        StripLayoutGroupTitle[] groupTitles = new StripLayoutGroupTitle[numGroups];
        Set<Token> seenGroupIds = new HashSet<>();

        int numViews = mStripTabs.length + numGroups;
        if (numViews != mStripViews.length) {
            mStripViews = new StripLayoutView[numViews];
        }

        int viewIndex = 0;
        // First view will be tab group title if first tab is grouped.
        Tab firstTab = assumeNonNull(getTabById(mStripTabs[0].getTabId()));
        if (mTabGroupModelFilter.isTabInTabGroup(firstTab)) {
            Token tabGroupId = firstTab.getTabGroupId();
            assert tabGroupId != null;
            StripLayoutGroupTitle groupTitle = findOrCreateGroupTitle(tabGroupId);
            seenGroupIds.add(tabGroupId);
            if (!tabGroupId.equals(mGroupIdToHideSupplier.get())) {
                if (TabUiUtils.shouldShowIphForSync(mTabGroupSyncService, tabGroupId)) {
                    mLastSyncedGroupIdForIph = tabGroupId;
                }
                groupTitles[groupTitleIndex++] = groupTitle;
                mStripViews[viewIndex++] = groupTitle;
            }
        }
        // Copy the StripLayoutTabs and create group titles where needed.
        for (int i = 0; i < mStripTabs.length - 1; i++) {
            final StripLayoutTab stripTab = mStripTabs[i];
            mStripViews[viewIndex++] = stripTab;

            Tab currTab = assumeNonNull(getTabById(stripTab.getTabId()));
            Tab nextTab = assumeNonNull(getTabById(mStripTabs[i + 1].getTabId()));
            Token nextTabGroupId = nextTab.getTabGroupId();
            boolean nextTabInGroup = mTabGroupModelFilter.isTabInTabGroup(nextTab);
            boolean areRelatedTabs =
                    nextTabInGroup
                            ? assumeNonNull(nextTabGroupId).equals(currTab.getTabGroupId())
                            : false;
            if (nextTabInGroup && !areRelatedTabs) {
                assumeNonNull(nextTabGroupId);
                if (seenGroupIds.contains(nextTabGroupId)) {
                    assert false : "Rebuilding tab strip with a non-contiguous tab group.";
                    continue;
                }
                seenGroupIds.add(nextTabGroupId);
                StripLayoutGroupTitle groupTitle = findOrCreateGroupTitle(nextTabGroupId);
                if (!nextTabGroupId.equals(mGroupIdToHideSupplier.get())) {
                    if (TabUiUtils.shouldShowIphForSync(mTabGroupSyncService, nextTabGroupId)) {
                        mLastSyncedGroupIdForIph = nextTabGroupId;
                    }
                    groupTitles[groupTitleIndex++] = groupTitle;
                    mStripViews[viewIndex++] = groupTitle;
                }
            }
        }
        // Final view will be the last tab.
        assert viewIndex == mStripViews.length - 1 : "Did not find all tab groups.";
        mStripViews[viewIndex] = mStripTabs[mStripTabs.length - 1];

        // Destroy TabBubbler for removed group titles.
        List<StripLayoutGroupTitle> newGroupTitles = new ArrayList<>(Arrays.asList(groupTitles));
        for (StripLayoutGroupTitle groupTitle : mStripGroupTitles) {
            if (!newGroupTitles.contains(groupTitle)) {
                groupTitle.setTabBubbler(null);
            }
        }

        int oldGroupCount = mStripGroupTitles.length;
        mStripGroupTitles = groupTitles;
        if (mStripGroupTitles.length != oldGroupCount) {
            for (int i = 0; i < mStripGroupTitles.length; ++i) {
                final StripLayoutGroupTitle groupTitle = mStripGroupTitles[i];
                boolean isCollapsed =
                        mTabGroupModelFilter.getTabGroupCollapsed(groupTitle.getTabGroupId());
                updateTabGroupCollapsed(groupTitle, isCollapsed, false);
            }
            finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
            computeAndUpdateTabWidth(/* animate= */ true, /* deferAnimations= */ false);
        }
    }

    private void copyTabs() {
        int numViews = mStripTabs.length;
        if (numViews != mStripViews.length) {
            mStripViews = new StripLayoutView[numViews];
        }
        for (int i = 0; i < mStripViews.length; i++) {
            mStripViews[i] = mStripTabs[i];
        }
    }

    @Override
    public void refresh() {
        mUpdateHost.requestUpdate();
    }

    @Override
    public void resizeTabStrip(
            boolean animate, @Nullable StripLayoutTab tabToAnimate, boolean tabAddedAnimation) {
        finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
        if (tabToAnimate != null) {
            assert animate;
            if (!tabAddedAnimation) {
                mMultiStepTabCloseAnimRunning = true;
                // Resize the tab strip accordingly.
                resizeStripOnTabClose();
            } else {
                List<Animator> animationList =
                        computeAndUpdateTabWidth(/* animate= */ true, /* deferAnimations= */ true);
                if (animationList != null) {
                    runTabAddedAnimator(animationList, tabToAnimate, /* fromTabCreation= */ false);
                }
            }
        } else {
            computeAndUpdateTabWidth(animate, /* deferAnimations= */ animate);
        }

        // Update the ideal view positions, since these are needed for reorder offset calculations.
        computeIdealViewPositions();
    }

    private StripLayoutTab createPlaceholderStripTab() {
        StripLayoutTab tab =
                new StripLayoutTab(
                        mContext,
                        Tab.INVALID_TAB_ID,
                        /* clickHandler= */ this,
                        /* keyboardFocusHandler= */ this,
                        mTabLoadTrackerHost,
                        mUpdateHost,
                        mIncognito,
                        /* isPinned= */ false);
        mTabDelegate.setIsTabPlaceholder(tab, true);

        // TODO(crbug.com/40942588): Added placeholder a11y descriptions to prevent crash due
        //  to invalid a11y node. Replace with official strings when available.
        String description = "Placeholder Tab";
        String title = "Placeholder";
        tab.setAccessibilityDescription(description, title, Resources.ID_NULL);

        pushPropertiesToTab(tab);

        return tab;
    }

    @VisibleForTesting
    StripLayoutTab createStripTab(int id, boolean isPinned) {
        // TODO: Cache these
        StripLayoutTab tab =
                new StripLayoutTab(
                        mContext,
                        id,
                        /* clickHandler= */ this,
                        /* keyboardFocusHandler= */ this,
                        mTabLoadTrackerHost,
                        mUpdateHost,
                        mIncognito,
                        isPinned);

        if (isSelectedTab(id)) {
            StripLayoutTabDelegate.setTabVisibility(tab, /* isVisible= */ true);
        }

        pushPropertiesToTab(tab);

        return tab;
    }

    private void pushPropertiesToPlaceholder(StripLayoutTab placeholderTab, @Nullable Tab tab) {
        if (tab == null) return;
        placeholderTab.setTabId(tab.getId());
        mTabDelegate.setIsTabPlaceholder(placeholderTab, false);
        setAccessibilityDescription(placeholderTab, tab);
    }

    private void pushPropertiesToTab(StripLayoutTab tab) {
        // The close button is visible by default except for pinned tabs. If it should be hidden on
        // tab creation, do not animate the fade-out. See (https://crbug.com/1342654).
        boolean shouldShowCloseButton =
                !tab.getIsPinned()
                        && getCachedTabWidth(/* isPinned= */ false)
                                >= StripLayoutTabDelegate.TAB_WIDTH_MEDIUM;
        tab.setCanShowCloseButton(shouldShowCloseButton, false);

        // This is an effective width of 0 due to how we overlap tabs.
        tab.setWidth(TAB_OVERLAP_WIDTH_DP);
        tab.setHeight(mHeight);
        tab.setTouchTargetInsets(null, mTopPadding, null, -mTopPadding);
    }

    /**
     * @param id The Tab id.
     * @return The StripLayoutTab that corresponds to that tabid.
     */
    @VisibleForTesting
    public @Nullable StripLayoutTab findTabById(int id) {
        return StripLayoutUtils.findTabById(mStripTabs, id);
    }

    private int findIndexForTab(int id) {
        return StripLayoutUtils.findIndexForTab(mStripTabs, id);
    }

    private boolean isEndMostTab(int tabId) {
        return mStripTabs.length > 0 && tabId == mStripTabs[mStripTabs.length - 1].getTabId();
    }

    private boolean isLiveTab(StripLayoutTab tab) {
        return !tab.isClosed()
                && !tab.isDraggedOffStrip()
                && !tab.isCollapsed()
                && !(tab.isDying() && ChromeFeatureList.sTabletTabStripAnimation.isEnabled());
    }

    /** Returns the total number of unpinned tabs that are live. */
    private int getNumLiveUnpinnedTabs() {
        int numLiveTabs = 0;

        for (int i = mStripTabs.length - 1; i >= 0; i--) {
            final StripLayoutTab tab = mStripTabs[i];
            if (tab.getIsPinned()) break;
            if (isLiveTab(tab)) numLiveTabs++;
        }

        return numLiveTabs;
    }

    /** Returns the total number of pinned tabs that are live. */
    private int getNumLivePinnedTabs() {
        int numPinnedTabs = 0;

        for (StripLayoutTab tab : mStripTabs) {
            if (!tab.getIsPinned()) break;
            if (isLiveTab(tab)) numPinnedTabs++;
        }

        return numPinnedTabs;
    }

    /**
     * Calculates the strip width such that it aligns with the end-most (closing) tab's far edge.
     * This is so that the next tab's far edge will (at most) slide to this position. This will
     * guarantee that either a) the next tab's close button will slide to where the cursor clicked
     * or b) the tabs will expand to their max width, but won't reach the cursor. Should only be
     * called from {@link #getStripWidthForResizing}.
     *
     * @param closingEndMostTabDrawX The drawX of the end-most (closing) {@link StripLayoutTab}.
     * @param closingEndMostTabWidth The width of the end-most (closing) {@link StripLayoutTab}.
     * @return The target available width, such that the next tab's close button will align with the
     *     cursor, as it just clicked to close the end-most tab.
     */
    private float getStripWidthForEndMostTabMouseClosure(
            float closingEndMostTabDrawX, float closingEndMostTabWidth) {
        if (LocalizationUtils.isLayoutRtl()) {
            return mWidth - mRightMargin - closingEndMostTabDrawX;
        } else {
            return closingEndMostTabDrawX + closingEndMostTabWidth - mLeftMargin;
        }
    }

    /**
     * Returns the width of the tab strip when resizing the strip. Accounts for the restricted
     * resizing when closing tabs with the mouse. Should only be called from {@link
     * #getAvailableTabWidthForResizing}.
     */
    private float getStripWidthForResizing() {
        // If we're resizing in response to a mouse click closing the end-most tab, we may restrict
        // the resize to align the next tab's close button with the cursor.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_STRIP_MOUSE_CLOSE_RESIZE_DELAY)
                && mClosingEndMostTabDrawX != null
                && mClosingEndMostTabWidth != null) {
            return getStripWidthForEndMostTabMouseClosure(
                    mClosingEndMostTabDrawX, mClosingEndMostTabWidth);
        }

        // Otherwise, the entirety of the tab strip width is available.
        return mWidth - mLeftMargin - mRightMargin;
    }

    /**
     * Returns the available width for tabs when resizing the strip. Accounts for both tab group
     * indicators and restricted resizing when closing tabs with the mouse.
     */
    private float getAvailableTabWidthForResizing() {
        // TODO(crbug.com/419015257): Move to separate file/delegate and add tests.
        float stripWidth = getStripWidthForResizing();
        for (int i = 0; i < mStripGroupTitles.length; i++) {
            final StripLayoutGroupTitle groupTitle = mStripGroupTitles[i];
            if (groupTitle.isDying()) continue;
            stripWidth -= (groupTitle.getWidth() - mGroupTitleOverlapWidth);
        }
        return stripWidth - getTotalPinnedTabsWidth();
    }

    private void computeCachedTabWidth() {
        // 1. Compute the number of unpinned tabs and the available width for them.
        int numTabs = Math.max(getNumLiveUnpinnedTabs(), 1);
        float stripWidth = getAvailableTabWidthForResizing();

        // 2. Compute additional width we gain from overlapping the tabs.
        float overlapWidth = TAB_OVERLAP_WIDTH_DP * (numTabs - 1);

        // 3. Calculate the optimal tab width.
        float optimalTabWidth = (stripWidth + overlapWidth) / numTabs;

        // 4. Calculate the realistic tab width.
        mCachedTabWidthSupplier.set(
                MathUtils.clamp(optimalTabWidth, MIN_TAB_WIDTH_DP, MAX_TAB_WIDTH_DP));
    }

    /**
     * Computes and updates the tab width when resizing the tab strip.
     *
     * @param animate Whether to animate the update.
     * @param deferAnimations Whether to defer animations.
     * @return A list of animators for the tab width update.
     */
    private @Nullable List<Animator> computeAndUpdateTabWidth(
            boolean animate, boolean deferAnimations) {
        // Skip updating the tab width when the tab strip width is unavailable.
        if (mWidth == 0) {
            return null;
        }

        // Suppress resizes from tab closures from mouse. We may still need to animate the closing
        // tab shrinking, though. If closing the end-most tab, we may need to partially resize to
        // align the next tab's close button with the cursor (if possible).
        boolean delayingResizeForMouseClose =
                ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_STRIP_MOUSE_CLOSE_RESIZE_DELAY)
                        && mPendingMouseTabClosure
                        && mClosingEndMostTabDrawX == null
                        && mClosingEndMostTabWidth == null;

        // 1. Recompute cached tab width.
        if (!delayingResizeForMouseClose) {
            computeCachedTabWidth();
        }

        // 2. Prepare animations and propagate width to all tabs.
        ArrayList<Animator> resizeAnimationList = null;
        if (animate) resizeAnimationList = new ArrayList<>();
        for (int i = 0; i < mStripTabs.length; i++) {
            StripLayoutTab tab = mStripTabs[i];
            if ((tab.isDying() && !ChromeFeatureList.sTabletTabStripAnimation.isEnabled())
                    || tab.isCollapsed()) {
                continue;
            }
            float cachedTabWidth = getCachedTabWidth(tab.getIsPinned());
            if (resizeAnimationList != null) {
                // Handle animating a tab being closed for TabletTabStripAnimation.
                if (tab.isDying()) {
                    resizeAnimationList.add(
                            getViewWidthAnimator(
                                    tab, TAB_OVERLAP_WIDTH_DP, NEW_ANIM_TAB_RESIZE_MS));
                    continue;
                }

                if (cachedTabWidth > 0f && tab.getWidth() == cachedTabWidth) {
                    // No need to create an animator to animate to the width we're already at.
                    continue;
                }

                int duration =
                        ChromeFeatureList.sTabletTabStripAnimation.isEnabled()
                                ? NEW_ANIM_TAB_RESIZE_MS
                                : ANIM_TAB_RESIZE_MS;
                resizeAnimationList.add(getViewWidthAnimator(tab, cachedTabWidth, duration));
            } else {
                mStripTabs[i].setWidth(cachedTabWidth);
            }
        }

        // Return early if there is no animation to run.
        if (resizeAnimationList == null) {
            buildBottomIndicator();
            mUpdateHost.requestUpdate();
            return null;
        }

        // 3. Animate bottom indicator when tab width change.
        for (int i = 0; i < mStripGroupTitles.length; i++) {
            StripLayoutGroupTitle groupTitle = mStripGroupTitles[i];
            if (groupTitle == null) {
                continue;
            }
            if (groupTitle.isCollapsed()) {
                continue;
            }
            float bottomIndicatorStartWidth = groupTitle.getBottomIndicatorWidth();
            float bottomIndicatorEndWidth =
                    StripLayoutUtils.calculateBottomIndicatorWidth(
                            groupTitle,
                            StripLayoutUtils.getNumLiveGroupedTabs(
                                    assumeNonNull(mModel), mStripTabs, groupTitle.getTabGroupId()),
                            getEffectiveTabWidth(/* isPinned= */ false));

            if (bottomIndicatorEndWidth > 0f
                    && bottomIndicatorStartWidth == bottomIndicatorEndWidth) {
                // No need to create an animator to animate to the width we're already at.
                continue;
            }

            resizeAnimationList.add(
                    CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            groupTitle,
                            StripLayoutGroupTitle.BOTTOM_INDICATOR_WIDTH,
                            bottomIndicatorStartWidth,
                            bottomIndicatorEndWidth,
                            ANIM_TAB_RESIZE_MS));
        }

        if (deferAnimations) return resizeAnimationList;
        startAnimations(resizeAnimationList, getTabResizeAnimatorListener());

        return null;
    }

    private AnimatorListener getTabResizeAnimatorListener() {
        return new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mTabResizeAnimRunning = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mTabResizeAnimRunning = false;
            }
        };
    }

    private void updateStrip() {
        // 1. Update the scroll offset limits
        updateScrollOffsetLimits();

        // 2. Calculate the ideal view positions
        computeIdealViewPositions();

        // 3. Calculate view stacking - update view draw properties and visibility.
        mStripStacker.pushDrawPropertiesToViews(
                mStripViews,
                getVisibleLeftBound(/* clampToUnpinnedViews= */ false),
                getVisibleRightBound(/* clampToUnpinnedViews= */ false));
        mStripStacker.pushDrawPropertiesToButtons(
                mNewTabButton,
                mStripTabs,
                mLeftMargin,
                mRightMargin,
                mWidth,
                mNewTabButtonWidth,
                isTabStripFull());

        // 4. Create render list.
        createRenderList();

        // 5. Invalidate the accessibility provider in case the visible virtual views have changed.
        mRenderHost.invalidateAccessibilityProvider();

        // 6. Hide close buttons if tab width gets lower than 156dp.
        updateCloseButtons();

        // 7. Show dividers between inactive tabs.
        updateTabContainersAndDividers();

        // 8. Update the touchable rect.
        updateTouchableRect();

        // TODO(crbug.com/396213514): Move the show bubble logic somewhere less frequently called.
        // 9. Trigger show notification bubble for all shared tab groups that have recent updates.
        showNotificationBubblesForSharedTabGroups();
    }

    private float getStartPositionForStripViews() {
        // Shift all of the strip views over by the the left margin because we're no longer base
        // lined at 0.
        if (!LocalizationUtils.isLayoutRtl()) {
            return mLeftMargin + mScrollDelegate.getReorderStartMargin();
        } else {
            return mWidth
                    - TAB_OVERLAP_WIDTH_DP
                    - mRightMargin
                    - mScrollDelegate.getReorderStartMargin();
        }
    }

    private void computeIdealViewPositions() {
        float startX = getStartPositionForStripViews();
        boolean scrollOffsetAdded = false;
        boolean rtl = LocalizationUtils.isLayoutRtl();
        for (int i = 0; i < mStripViews.length; i++) {
            final StripLayoutView view = mStripViews[i];

            if (!scrollOffsetAdded
                    && (!(view instanceof StripLayoutTab)
                            || !((StripLayoutTab) view).getIsPinned())) {
                startX += MathUtils.flipSignIf(mScrollDelegate.getScrollOffset(), rtl);
                scrollOffsetAdded = true;
            }

            float delta;
            if (view instanceof StripLayoutTab tab) {
                if (tab.isClosed()) continue;
                // idealX represents where a tab should be placed in the tab strip.
                setTabIdealX(tab, startX);

                if (ChromeFeatureList.sTabletTabStripAnimation.isEnabled() || !tab.isDying()) {
                    delta = (tab.getWidth() - TAB_OVERLAP_WIDTH_DP) * tab.getWidthWeight();
                } else {
                    delta = getEffectiveTabWidth(tab.getIsPinned());
                }
            } else if (view instanceof StripLayoutGroupTitle groupTitle) {
                // Offset to "undo" the tab overlap width as that doesn't apply to non-tab views.
                // Also applies the desired overlap with the previous tab.
                float drawXOffset = MathUtils.flipSignIf(mGroupTitleDrawXOffset, rtl);
                setGroupTitleIdealX(groupTitle, startX + drawXOffset);

                delta = (view.getWidth() - mGroupTitleOverlapWidth) * view.getWidthWeight();
            } else {
                assert false : "Unexpected view type in tab strip views.";
                delta = 0;
            }
            // Trailing margins will only be nonzero during reorder mode.
            delta += view.getTrailingMargin();
            delta = MathUtils.flipSignIf(delta, rtl);
            startX += delta;
        }
    }

    /**
     * Sets the idealX for the given {@link StripLayoutTab}. For LTR, this is the same as the
     * startX. For RTL, however, draw coordinates are still always anchored at the top-left of the
     * screen. This means we need to set the idealX to the left-side of the tab. This is the startX
     * minus the effective width of the tab.
     *
     * @param stripTab The {@link StripLayoutTab} to set the idealX for.
     * @param startX The idealX of the tab's starting side. This is the left side in LTR and the
     *     right side in RTL.
     */
    private void setTabIdealX(StripLayoutTab stripTab, float startX) {
        if (LocalizationUtils.isLayoutRtl()) {
            stripTab.setIdealX(startX - (stripTab.getWidth() - TAB_OVERLAP_WIDTH_DP));
        } else {
            stripTab.setIdealX(startX);
        }
    }

    /** See {@link #setTabIdealX}. Same concept, but for group titles. */
    private void setGroupTitleIdealX(StripLayoutGroupTitle groupTitle, float startX) {
        if (LocalizationUtils.isLayoutRtl()) {
            // Use the tab overlap width here. The group title's true width accounts for how much it
            // overlaps previous tab (i.e. the tab overlap width). Note that this is different from
            // the visual size, which can be found through StripLayoutGroupTitle#getPaddedWidth.
            groupTitle.setIdealX(startX - (groupTitle.getWidth() - TAB_OVERLAP_WIDTH_DP));
        } else {
            groupTitle.setIdealX(startX);
        }
    }

    private boolean shouldRenderView(StripLayoutView view) {
        return view.isVisible() && !view.isDraggedOffStrip();
    }

    private int getVisibleViewCount(StripLayoutView[] views) {
        int renderCount = 0;
        for (int i = 0; i < views.length; ++i) {
            if (shouldRenderView(views[i])) renderCount++;
        }
        return renderCount;
    }

    private void populateVisibleViews(StripLayoutView[] allViews, StripLayoutView[] viewsToRender) {
        int renderIndex = 0;
        for (int i = 0; i < allViews.length; ++i) {
            final StripLayoutView view = allViews[i];
            if (shouldRenderView(view)) viewsToRender[renderIndex++] = view;
        }
    }

    private void createRenderList() {
        // 1. Figure out how many tabs will need to be rendered.
        int tabRenderCount = getVisibleViewCount(mStripTabs);
        int groupTitleRenderCount = getVisibleViewCount(mStripGroupTitles);

        // 2. Reallocate the render list if necessary.
        if (mStripTabsToRender.length != tabRenderCount) {
            mStripTabsToRender = new StripLayoutTab[tabRenderCount];
        }
        if (mStripGroupTitlesToRender.length != groupTitleRenderCount) {
            mStripGroupTitlesToRender = new StripLayoutGroupTitle[groupTitleRenderCount];
        }

        // 3. Populate it with the visible tabs.
        populateVisibleViews(mStripTabs, mStripTabsToRender);
        populateVisibleViews(mStripGroupTitles, mStripGroupTitlesToRender);
    }

    /**
     * @param view The {@link StripLayoutView} to make fully visible.
     * @return a 1-D vector on the X axis of the window coordinate system that can make the tab
     *     fully visible.
     */
    private float calculateDeltaToMakeViewVisible(@Nullable StripLayoutView view) {
        if (view == null) return 0.f;
        // These are always in view.
        if (view.equals(mNewTabButton) || view.equals(mModelSelectorButton)) return 0.f;
        if (view instanceof StripLayoutTab tab && tab.getIsPinned()) return 0.f;

        // 1. Calculate the bounds to fully show the regular view on the left/right side of the
        // strip.
        // TODO(wenyufu): Account for offsetX{Left,Right} result too much offset. Is this expected?
        boolean rtl = LocalizationUtils.isLayoutRtl();
        final float rightBound =
                getVisibleRightBound(/* clampToUnpinnedViews= */ true)
                        - mRightFadeWidth
                        - (rtl ? 0f : mReservedEndMargin);
        final float leftBound =
                getVisibleLeftBound(/* clampToUnpinnedViews= */ true)
                        + mLeftFadeWidth
                        - (rtl ? mReservedEndMargin : 0f);

        // 2. Calculate vectors from the view's ideal position to the farthest left/right point
        // where the view can be visible.
        // These are 1-D vectors on the X axis of the window coordinate system.
        if (view instanceof TintedCompositorButton closeButton
                && closeButton.getParentView() instanceof StripLayoutTab stripTab) {
            view = stripTab;
        }
        final float deltaToFarLeft = leftBound - view.getIdealX();
        final float deltaToFarRight =
                rightBound - getCachedTabWidth(/* isPinned= */ false) - view.getIdealX();

        // 3. The following case means the view is already completely in the visible area of the
        // strip, i.e., it needs to be:
        // moved left to reach the far left point, and
        // moved right to reach the far right point.
        if (deltaToFarLeft < 0 && deltaToFarRight > 0) return 0.f;

        // 4. Return the vector with less distance for the view to travel.
        return Math.abs(deltaToFarLeft) < Math.abs(deltaToFarRight)
                ? deltaToFarLeft
                : deltaToFarRight;
    }

    void setTabAtPositionForTesting(StripLayoutTab tab) {
        mTabAtPositionForTesting = tab;
    }

    @Nullable StripLayoutTab getTabAtPosition(float x) {
        return (StripLayoutTab) getViewAtPositionX(x, false);
    }

    @Nullable StripLayoutView getViewAtPositionX(float x, boolean includeGroupTitles) {
        if (mTabAtPositionForTesting != null) {
            return mTabAtPositionForTesting;
        }

        // Views are only hidden once they're completely out of the visible region. Since tabs are
        // wider than the NTB (which is roughly where the end-fade begins), it is possible for a tab
        // to be visible while having its touch target partially past the NTB. Visible tabs are
        // considered clickable, so manually suppress clicks beyond the NTB to prevent this.
        if (LocalizationUtils.isLayoutRtl()) {
            if (x <= mNewTabButton.getDrawX() + mNewTabButton.getWidth()) return null;
        } else {
            if (x >= mNewTabButton.getDrawX()) return null;
        }

        return StripLayoutUtils.findViewAtPositionX(mStripViews, x, includeGroupTitles);
    }

    public boolean getInReorderModeForTesting() {
        return mReorderDelegate.getInReorderMode();
    }

    public void startReorderModeAtIndexForTesting(int index) {
        StripLayoutTab tab = mStripTabs[index];
        updateStrip();
        float x = tab.getDrawX() + (tab.getWidth() / 2);
        startReorderMode(x, 0, tab, ReorderType.DRAG_WITHIN_STRIP);
    }

    @Override
    public void setCompositorButtonsVisible(boolean visible) {
        float endOpacity = visible ? 1.f : 0.f;

        CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        mNewTabButton,
                        CompositorButton.OPACITY,
                        mNewTabButton.getOpacity(),
                        endOpacity,
                        ANIM_BUTTONS_FADE_MS)
                .start();
        if (mModelSelectorButton != null) {
            CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            mModelSelectorButton,
                            CompositorButton.OPACITY,
                            mModelSelectorButton.getOpacity(),
                            endOpacity,
                            ANIM_BUTTONS_FADE_MS)
                    .start();
        }
    }

    /**
     * @param id The id of the selected tab.
     * @return The outline color if the selected tab will show its Tab Group Indicator outline.
     *     {@code Color.TRANSPARENT} otherwise.
     */
    protected @ColorInt int getSelectedOutlineGroupTint(int id, boolean shouldShowOutline) {
        if (!shouldShowOutline) return Color.TRANSPARENT;

        Tab tab = getTabById(id);
        if (tab == null) return Color.TRANSPARENT;

        StripLayoutGroupTitle groupTitle = findGroupTitle(tab.getTabGroupId());
        if (groupTitle == null) return Color.TRANSPARENT;

        return groupTitle.getTint();
    }

    /**
     * @param stripLayoutTab The current {@link StripLayoutTab}.
     * @return Whether the tab outline should be shown. True if 1. the tab is grouped and selected
     *     and 2. the folio container is attached. False otherwise.
     */
    protected boolean shouldShowTabOutline(StripLayoutTab stripLayoutTab) {
        // Return false if null tab (e.g. placeholders have invalid tab IDs), ungrouped, or
        // temporarily hidden while delete dialog is showing.
        Tab tab = getTabById(stripLayoutTab.getTabId());
        if (tab == null
                || mTabGroupModelFilter == null
                || !mTabGroupModelFilter.isTabInTabGroup(tab)
                || assumeNonNull(tab.getTabGroupId()).equals(mGroupIdToHideSupplier.get())) {
            return false;
        }

        // Show tab outline when tab is in group with folio attached and 1. tab is selected or 2.
        // tab is in foreground (e.g. the previously selected tab in destination strip).
        return stripLayoutTab.getFolioAttached()
                && (isSelectedTab(stripLayoutTab.getTabId())
                        || StripLayoutTabDelegate.isTabVisible(stripLayoutTab));
    }

    private void handleReorderAutoScrolling(long time) {
        if (!mReorderDelegate.getInReorderMode()) return;
        boolean rtl = LocalizationUtils.isLayoutRtl();
        float leftBound =
                getVisibleLeftBound(/* clampToUnpinnedViews= */ true)
                        + (rtl ? mReservedEndMargin : 0f);
        float rightBound =
                getVisibleRightBound(/* clampToUnpinnedViews= */ true)
                        + (rtl ? 0f : mReservedEndMargin);
        mReorderDelegate.updateReorderPositionAutoScroll(
                mStripViews, mStripGroupTitles, mStripTabs, time, leftBound, rightBound);
    }

    private @Nullable Tab getTabById(int tabId) {
        return mModel == null ? null : mModel.getTabById(tabId);
    }

    private int getSelectedTabId() {
        if (mModel == null) return Tab.INVALID_TAB_ID;

        int index = mModel.index();
        if (index == TabModel.INVALID_TAB_INDEX) return Tab.INVALID_TAB_ID;

        Tab tab = mModel.getTabAt(index);
        if (tab == null) return Tab.INVALID_TAB_ID;

        return tab.getId();
    }

    @VisibleForTesting
    int getSelectedStripTabIndex() {
        return mTabStateInitialized
                ? findIndexForTab(getSelectedTabId())
                : mActiveTabIndexOnStartup;
    }

    private @Nullable StripLayoutTab getSelectedStripTab() {
        int index = getSelectedStripTabIndex();

        return index >= 0 && index < mStripTabs.length ? mStripTabs[index] : null;
    }

    private boolean isSelectedTab(int id) {
        return id != Tab.INVALID_TAB_ID && id == getSelectedTabId();
    }

    protected void scrollTabToView(long time, boolean requestUpdate) {
        bringSelectedTabToVisibleArea(time, true);
        if (requestUpdate) mUpdateHost.requestUpdate();
    }

    /**
     * Updates the notification bubble for a set of tabs and group title if collapsed. Tabs passed
     * in this call all belong to same group.
     *
     * @param tabIdsToBeUpdated The set of tab IDs to update the notification bubble for.
     * @param hasUpdate Whether there is an update to the notification bubble.
     */
    @Override
    public void updateTabStripNotificationBubble(
            Set<Integer> tabIdsToBeUpdated, boolean hasUpdate) {
        boolean updateForCollapsedGroup = false;
        boolean showIph =
                assumeNonNull(mTabStripIphController)
                        .wouldTriggerIph(IphType.GROUP_TITLE_NOTIFICATION_BUBBLE);

        for (int tabId : tabIdsToBeUpdated) {
            Tab tab = getTabById(tabId);
            final StripLayoutTab stripTab = findTabById(tabId);

            // Skip invalid tabs or selected tabs when showing updates.
            if (tab == null || stripTab == null || (isSelectedTab(tabId) && hasUpdate)) continue;

            Token tabGroupId = tab.getTabGroupId();
            final StripLayoutGroupTitle groupTitle = findGroupTitle(tabGroupId);

            // Show bubble and iph on group title if collapsed, otherwise show iph on the updated
            // tab.
            if (groupTitle != null && groupTitle.isCollapsed() && !updateForCollapsedGroup) {
                groupTitle.setNotificationBubbleShown(hasUpdate);
                updateGroupTextAndSharedState(tabGroupId);
                if (hasUpdate && showIph) {
                    mQueuedIphList.add(
                            () ->
                                    attemptToShowTabStripIph(
                                            groupTitle,
                                            /* tab= */ null,
                                            IphType.GROUP_TITLE_NOTIFICATION_BUBBLE,
                                            /* enableSnoozeMode= */ false));
                }
                updateForCollapsedGroup = true;
            } else if (groupTitle != null && !groupTitle.isCollapsed()) {
                if (hasUpdate && showIph) {
                    mQueuedIphList.add(
                            () ->
                                    attemptToShowTabStripIph(
                                            groupTitle,
                                            stripTab,
                                            IphType.TAB_NOTIFICATION_BUBBLE,
                                            /* enableSnoozeMode= */ false));
                }
            }
            // Update tab bubble and the related accessibility description.
            stripTab.setNotificationBubbleShown(hasUpdate);
            setAccessibilityDescription(stripTab, tab);
            if (mLayerTitleCache != null) mLayerTitleCache.updateTabBubble(tabId, hasUpdate);
        }
        mUpdateHost.requestUpdate();
    }

    @Override
    public void updateTabCardLabels(Map<Integer, TabCardLabelData> labelData) {
        // Not implemented for tablet tab strip.
    }

    @SuppressLint("HandlerLeak")
    private class StripTabEventHandler extends Handler {
        @Override
        public void handleMessage(Message m) {
            switch (m.what) {
                case MESSAGE_UPDATE_SPINNER:
                    mUpdateHost.requestUpdate();
                    break;
                case MESSAGE_HOVER_CARD:
                    showTabHoverCardView(/* isDelayedCall= */ true);
                    mUpdateHost.requestUpdate();
                    break;
                default:
                    assert false : "StripTabEventHandler got unknown message " + m.what;
            }
        }
    }

    private class TabLoadTrackerCallbackImpl implements TabLoadTrackerCallback {
        @Override
        public void loadStateChanged(int id) {
            mUpdateHost.requestUpdate();
        }
    }

    /**
     * Sets the current scroll offset of the TabStrip.
     *
     * @param offset The offset to set the TabStrip's scroll position to; it's a 1-D vector on the X
     *     axis under the dynamic coordinate system used by {@link ScrollDelegate}.
     */
    public void setScrollOffsetForTesting(float offset) {
        mScrollDelegate.setNonClampedScrollOffsetForTesting(offset); // IN-TEST
        updateStrip();
    }

    /**
     * Displays the tab menu below the anchor tab.
     *
     * @param anchorTab The tab the menu will be anchored to
     */
    @VisibleForTesting
    void showCloseButtonMenu(StripLayoutTab anchorTab) {
        if (mModel == null) return;

        // 1. Bring the anchor tab to the foreground.
        int tabIndex = TabModelUtils.getTabIndexById(mModel, anchorTab.getTabId());
        TabModelUtils.setIndex(mModel, tabIndex);

        // 2. Anchor the popupMenu to the view associated with the tab
        @Nullable Tab tab = TabModelUtils.getCurrentTab(mModel);
        if (tab == null) return;
        View tabView = tab.getView();
        mCloseButtonMenu.setAnchorView(tabView);
        // 3. Set the vertical offset to align the close button menu with bottom of the tab strip
        int tabHeight = mManagerHost.getHeight();
        int verticalOffset =
                -(tabHeight - (int) mContext.getResources().getDimension(R.dimen.tab_strip_height));
        mCloseButtonMenu.setVerticalOffset(verticalOffset);

        // 4. Set the horizontal offset to align the close button menu with the right side of the
        // tab
        int horizontalOffset =
                Math.round(
                                (anchorTab.getDrawX() + anchorTab.getWidth())
                                        * mContext.getResources().getDisplayMetrics().density)
                        - mCloseButtonMenu.getWidth();
        // Cap the horizontal offset so that the close button menu doesn't get drawn off screen.
        horizontalOffset = Math.max(horizontalOffset, 0);
        mCloseButtonMenu.setHorizontalOffset(horizontalOffset);

        mCloseButtonMenu.show();
    }

    /**
     * Sets the direction and distance for scrolling the tab strip.
     *
     * @param delta a 1-D vector under the window coordinate system; it can be on the X axis or the
     *     Y axis, depending on {@code isDeltaHorizontal}
     * @param isDeltaHorizontal whether {@code delta} is on the X axis
     * @param shouldAnimate whether to animate the scrolling
     * @param time the current time of the app, in ms
     */
    private void setScrollForScrollingTabStacker(
            float delta, boolean isDeltaHorizontal, boolean shouldAnimate, long time) {
        if (delta == 0.f) return;

        // The "delta" parameter is a 1-D vector under the window coordinate system.
        // Before passing it to ScrollDelegate, we must transform it in accordance with
        // ScrollDelegate's dynamic coordinate system.
        // Please see ScrollDelegate's class doc for details on the two coordinate systems.
        float deltaForScrollDelegate =
                isDeltaHorizontal
                        ? MathUtils.flipSignIf(delta, LocalizationUtils.isLayoutRtl())
                        : delta;

        mScrollDelegate.startScroll(time, deltaForScrollDelegate, shouldAnimate);
    }

    /** Scrolls to the selected tab if it's not fully visible. */
    private void bringSelectedTabToVisibleArea(long time, boolean animate) {
        bringViewToVisibleArea(getSelectedStripTab(), time, animate);
    }

    /** Scrolls to {@param view} if it's not fully visible. */
    private void bringViewToVisibleArea(
            @Nullable StripLayoutView view, long time, boolean animate) {
        if (mWidth == 0) return;
        float delta = calculateDeltaToMakeViewVisible(view);
        setScrollForScrollingTabStacker(delta, /* isDeltaHorizontal= */ true, animate, time);
    }

    private boolean isViewCompletelyVisible(StripLayoutView view) {
        boolean isPinned = (view instanceof StripLayoutTab tab) && tab.getIsPinned();
        float leftBound =
                getVisibleLeftBound(/* clampToUnpinnedViews= */ !isPinned) + mLeftFadeWidth;
        float rightBound =
                getVisibleRightBound(/* clampToUnpinnedViews= */ !isPinned) - mRightFadeWidth;
        float viewStart = 0f;
        float viewEnd = 0f;
        if (view instanceof StripLayoutTab tab) {
            viewStart = tab.getDrawX();
            viewEnd = tab.getDrawX() + tab.getWidth();
        } else {
            StripLayoutGroupTitle groupTitle = (StripLayoutGroupTitle) view;
            viewStart = groupTitle.getPaddedX();
            viewEnd = groupTitle.getPaddedX() + groupTitle.getPaddedWidth();
        }
        return view.isVisible() && viewStart > leftBound && viewEnd < rightBound;
    }

    /**
     * Determines whether a drawn view is completely outside of the visible area of the tab strip.
     *
     * @param view The {@link StripLayoutView} whose visibility is determined.
     * @return {@code true} if the view is completely hidden, {@code false} otherwise.
     */
    @VisibleForTesting
    boolean isViewCompletelyHidden(StripLayoutView view) {
        boolean isPinned = (view instanceof StripLayoutTab tab) && tab.getIsPinned();
        return !view.isVisible()
                || isViewCompletelyHiddenAt(view.getDrawX(), view.getWidth(), isPinned);
    }

    /**
     * Determines whether a view will be completely outside of the visible area of the tab strip
     * once it reaches its ideal position.
     *
     * @param view The {@link StripLayoutView} whose visibility will be determined.
     * @return {@code true} if the view will be completely hidden, {@code false} otherwise.
     */
    private boolean willViewBeCompletelyHidden(StripLayoutView view) {
        boolean isPinned = (view instanceof StripLayoutTab tab) && tab.getIsPinned();
        return isViewCompletelyHiddenAt(view.getIdealX(), view.getWidth(), isPinned);
    }

    private boolean isViewCompletelyHiddenAt(float viewX, float viewWidth, boolean isPinned) {
        float leftBound =
                getVisibleLeftBound(/* clampToUnpinnedViews= */ !isPinned) + mLeftFadeWidth;
        float rightBound =
                getVisibleRightBound(/* clampToUnpinnedViews= */ !isPinned) - mRightFadeWidth;
        // Check if the tab is outside the visible bounds to the left...
        return viewX + viewWidth <= leftBound
                // ... or to the right.
                || viewX >= rightBound;
    }

    /** Returns true if the close button menu is showing */
    public boolean isCloseButtonMenuShowingForTesting() {
        return mCloseButtonMenu.isShowing();
    }

    /**
     * @param menuItemId The id of the menu item to click
     */
    public void clickCloseButtonMenuItemForTesting(int menuItemId) {
        mCloseButtonMenu.performItemClick(menuItemId);
    }

    /** Returns The width of the tab strip. */
    float getWidthForTesting() {
        return mWidth;
    }

    /**
     * @param isPinned Whether the tab has been pinned.
     * @return The width of a tab.
     */
    private float getCachedTabWidth(boolean isPinned) {
        return isPinned ? PINNED_TAB_WIDTH_DP : assumeNonNull(mCachedTabWidthSupplier.get());
    }

    /** Returns The width of a tab. */
    float getUnpinnedTabWidthForTesting() {
        return getCachedTabWidth(/* isPinned= */ false);
    }

    /**
     * Returns The strip's scroll offset limit (a 1-D vector along the X axis, under the dynamic
     * coordinate system used by {@link ScrollDelegate}).
     */
    float getScrollOffsetLimitForTesting() {
        return mScrollDelegate.getScrollOffsetLimitForTesting(); // IN-TEST
    }

    /** Returns The scroller. */
    StackScroller getScrollerForTesting() {
        return mScrollDelegate.getScrollerForTesting(); // IN-TEST
    }

    /** Returns An array containing the StripLayoutTabs. */
    StripLayoutTab[] getStripLayoutTabsForTesting() {
        return mStripTabs;
    }

    /** Set the value of mStripTabs for testing */
    void setStripLayoutTabsForTesting(StripLayoutTab[] stripTabs) {
        this.mStripTabs = stripTabs;
    }

    /** Returns An array containing the StripLayoutViews. */
    StripLayoutView[] getStripLayoutViewsForTesting() {
        return mStripViews;
    }

    /** Returns The currently interacting tab. */
    @Nullable StripLayoutTab getInteractingTabForTesting() {
        return mReorderDelegate.getInteractingTabForTesting(); // IN-TEST
    }

    /** Returns The view that we'll delay enter reorder mode for. */
    @Nullable StripLayoutView getDelayedReorderViewForTesting() {
        return mDelayedReorderView;
    }

    Set<StripLayoutTab> getClosingTabsForTesting() {
        return mClosingTabs;
    }

    Set<StripLayoutGroupTitle> getClosingGroupTitlesForTesting() {
        return mClosingGroupTitles;
    }

    @Nullable Animator getRunningAnimatorForTesting() {
        return mRunningAnimator;
    }

    void setRunningAnimatorForTesting(Animator animator) {
        mRunningAnimator = animator;
    }

    protected boolean isMultiStepCloseAnimationsRunningForTesting() {
        return mMultiStepTabCloseAnimRunning;
    }

    protected float getLastReorderXForTesting() {
        return mReorderDelegate.getLastReorderXForTesting();
    }

    protected void setInReorderModeForTesting(boolean inReorderMode) {
        mReorderDelegate.setInReorderModeForTesting(inReorderMode);
    }

    void setReorderDelegateForTesting(ReorderDelegate delegate) {
        mReorderDelegate = delegate;
    }

    ReorderDelegate getReorderDelegateForTesting() {
        return mReorderDelegate;
    }

    void finishScrollForTesting() {
        mScrollDelegate.finishScrollForTesting();
    }

    boolean getIsStripScrollInProgressForTesting() {
        return mIsStripScrollInProgress;
    }

    void setPendingMouseTabClosureForTesting(boolean pendingMouseTabClosure) {
        mPendingMouseTabClosure = pendingMouseTabClosure;
    }

    boolean getPendingMouseTabClosureForTesting() {
        return mPendingMouseTabClosure;
    }

    ObservableSupplierImpl<@Nullable Token> getGroupIdToHideSupplierForTesting() {
        return mGroupIdToHideSupplier;
    }

    private void setAccessibilityDescription(@Nullable StripLayoutTab stripTab, @Nullable Tab tab) {
        if (tab != null) setAccessibilityDescription(stripTab, tab.getTitle(), tab.isHidden());
    }

    /**
     * Set the accessibility description of a {@link StripLayoutTab}.
     *
     * @param stripTab The StripLayoutTab to set the accessibility description.
     * @param title The title of the tab.
     * @param isHidden Current visibility state of the Tab.
     */
    private void setAccessibilityDescription(
            @Nullable StripLayoutTab stripTab, @Nullable String title, boolean isHidden) {
        if (stripTab == null) return;

        @StringRes
        int resId =
                getTabAccessibilityLabelRes(
                        stripTab.getIsPinned(),
                        stripTab.getNotificationBubbleShown(),
                        isHidden,
                        stripTab.getIsMultiSelected(),
                        stripTab.getMediaState());

        if (!stripTab.needsAccessibilityDescriptionUpdate(title, resId)) {
            // The resulting accessibility description would be the same as the current description,
            // so skip updating it to avoid having to read resources unnecessarily.
            return;
        }

        final String description = mContext.getString(resId, title);
        stripTab.setAccessibilityDescription(description, title, resId);
    }

    /**
     * Get the accessibility description string resource of a {@link StripLayoutTab}.
     *
     * @param isPinned Whether the tab is pinned.
     * @param notificationShown Whether the tab has notification shown.
     * @param isHidden Current visibility state of the Tab.
     * @param isMultiSelected Whether the tab is multi-selected.
     * @param mediaState The {@link MediaState} state of the tab.
     */
    private @StringRes int getTabAccessibilityLabelRes(
            boolean isPinned,
            boolean notificationShown,
            boolean isHidden,
            boolean isMultiSelected,
            @MediaState int mediaState) {
        if (notificationShown) {
            return R.string.accessibility_tabstrip_tab_notification;
        }

        final boolean selected = !isHidden;
        if (mediaState != MediaState.NONE) {
            if (isMultiSelected) {
                if (isPinned) {
                    return getMediaAccessibilityString(
                            mediaState,
                            mIncognito,
                            R.string.accessibility_tabstrip_tab_multiselected_pinned_audible,
                            R.string.accessibility_tabstrip_tab_multiselected_pinned_muted,
                            R.string.accessibility_tabstrip_tab_multiselected_pinned_recording,
                            R.string.accessibility_tabstrip_tab_multiselected_pinned_sharing,
                            R.string
                                    .accessibility_tabstrip_tab_multiselected_pinned_audible_incognito,
                            R.string
                                    .accessibility_tabstrip_tab_multiselected_pinned_muted_incognito,
                            R.string
                                    .accessibility_tabstrip_tab_multiselected_pinned_recording_incognito,
                            R.string
                                    .accessibility_tabstrip_tab_multiselected_pinned_sharing_incognito);
                } else {
                    return getMediaAccessibilityString(
                            mediaState,
                            mIncognito,
                            R.string.accessibility_tabstrip_tab_multiselected_audible,
                            R.string.accessibility_tabstrip_tab_multiselected_muted,
                            R.string.accessibility_tabstrip_tab_multiselected_recording,
                            R.string.accessibility_tabstrip_tab_multiselected_sharing,
                            R.string.accessibility_tabstrip_tab_multiselected_audible_incognito,
                            R.string.accessibility_tabstrip_tab_multiselected_muted_incognito,
                            R.string.accessibility_tabstrip_tab_multiselected_recording_incognito,
                            R.string.accessibility_tabstrip_tab_multiselected_sharing_incognito);
                }
            }

            if (selected) {
                if (isPinned) {
                    return getMediaAccessibilityString(
                            mediaState,
                            mIncognito,
                            R.string.accessibility_tabstrip_tab_selected_pinned_audible,
                            R.string.accessibility_tabstrip_tab_selected_pinned_muted,
                            R.string.accessibility_tabstrip_tab_selected_pinned_recording,
                            R.string.accessibility_tabstrip_tab_selected_pinned_sharing,
                            R.string.accessibility_tabstrip_tab_selected_pinned_audible_incognito,
                            R.string.accessibility_tabstrip_tab_selected_pinned_muted_incognito,
                            R.string.accessibility_tabstrip_tab_selected_pinned_recording_incognito,
                            R.string.accessibility_tabstrip_tab_selected_pinned_sharing_incognito);
                } else {
                    return getMediaAccessibilityString(
                            mediaState,
                            mIncognito,
                            R.string.accessibility_tabstrip_tab_selected_audible,
                            R.string.accessibility_tabstrip_tab_selected_muted,
                            R.string.accessibility_tabstrip_tab_selected_recording,
                            R.string.accessibility_tabstrip_tab_selected_sharing,
                            R.string.accessibility_tabstrip_tab_selected_audible_incognito,
                            R.string.accessibility_tabstrip_tab_selected_muted_incognito,
                            R.string.accessibility_tabstrip_tab_selected_recording_incognito,
                            R.string.accessibility_tabstrip_tab_selected_sharing_incognito);
                }
            } else { // not selected and not multiselected
                if (isPinned) {
                    return getMediaAccessibilityString(
                            mediaState,
                            mIncognito,
                            R.string.accessibility_tabstrip_tab_pinned_audible,
                            R.string.accessibility_tabstrip_tab_pinned_muted,
                            R.string.accessibility_tabstrip_tab_pinned_recording,
                            R.string.accessibility_tabstrip_tab_pinned_sharing,
                            R.string.accessibility_tabstrip_tab_pinned_audible_incognito,
                            R.string.accessibility_tabstrip_tab_pinned_muted_incognito,
                            R.string.accessibility_tabstrip_tab_pinned_recording_incognito,
                            R.string.accessibility_tabstrip_tab_pinned_sharing_incognito);
                } else {
                    return getMediaAccessibilityString(
                            mediaState,
                            mIncognito,
                            R.string.accessibility_tabstrip_tab_audible,
                            R.string.accessibility_tabstrip_tab_muted,
                            R.string.accessibility_tabstrip_tab_recording,
                            R.string.accessibility_tabstrip_tab_sharing,
                            R.string.accessibility_tabstrip_tab_audible_incognito,
                            R.string.accessibility_tabstrip_tab_muted_incognito,
                            R.string.accessibility_tabstrip_tab_recording_incognito,
                            R.string.accessibility_tabstrip_tab_sharing_incognito);
                }
            }
        }

        @StringRes
        int pinnedUnselected =
                mIncognito
                        ? R.string.accessibility_tabstrip_tab_pinned_incognito
                        : R.string.accessibility_tabstrip_tab_pinned;
        @StringRes
        int pinnedSelected =
                mIncognito
                        ? R.string.accessibility_tabstrip_tab_pinned_selected_incognito
                        : R.string.accessibility_tabstrip_tab_pinned_selected;
        @StringRes
        int unpinnedUnselected =
                mIncognito
                        ? R.string.accessibility_tabstrip_tab_incognito
                        : R.string.accessibility_tabstrip_tab;
        @StringRes
        int unpinnedSelected =
                mIncognito
                        ? R.string.accessibility_tabstrip_tab_selected_incognito
                        : R.string.accessibility_tabstrip_tab_selected;

        @StringRes
        int multiselected =
                mIncognito
                        ? R.string.accessibility_tabstrip_tab_multiselected_incognito
                        : R.string.accessibility_tabstrip_tab_multiselected;

        @StringRes
        int multiselectedPinned =
                mIncognito
                        ? R.string.accessibility_tabstrip_tab_multiselected_pinned_incognito
                        : R.string.accessibility_tabstrip_tab_multiselected_pinned;

        // A selected tab is always considered part of multi-selection. and so does not need to have
        // a separate string.
        if (isHidden) {
            if (isPinned) {
                if (isMultiSelected) {
                    return multiselectedPinned;
                } else {
                    return pinnedUnselected;
                }
            } else {
                if (isMultiSelected) {
                    return multiselected;
                } else {
                    return unpinnedUnselected;
                }
            }
        } else {
            if (isPinned) {
                return pinnedSelected;
            } else {
                return unpinnedSelected;
            }
        }
    }

    private @StringRes int getMediaAccessibilityString(
            @MediaState int mediaState,
            boolean isIncognito,
            @StringRes int audible,
            @StringRes int muted,
            @StringRes int recording,
            @StringRes int sharing,
            @StringRes int audibleIncognito,
            @StringRes int mutedIncognito,
            @StringRes int recordingIncognito,
            @StringRes int sharingIncognito) {
        if (isIncognito) {
            switch (mediaState) {
                case MediaState.AUDIBLE:
                    return audibleIncognito;
                case MediaState.MUTED:
                    return mutedIncognito;
                case MediaState.RECORDING:
                    return recordingIncognito;
                case MediaState.SHARING:
                    return sharingIncognito;
                default:
                    assert false : "Invalid media state: " + mediaState;
                    return 0;
            }
        } else {
            switch (mediaState) {
                case MediaState.AUDIBLE:
                    return audible;
                case MediaState.MUTED:
                    return muted;
                case MediaState.RECORDING:
                    return recording;
                case MediaState.SHARING:
                    return sharing;
                default:
                    assert false : "Invalid media state: " + mediaState;
                    return 0;
            }
        }
    }

    // ============================================================================================
    // Drag and Drop View Delegate.
    // ============================================================================================

    public void handleDragEnter(
            float currX, float lastX, boolean isSourceStrip, boolean draggedTabIncognito) {
        if (isSourceStrip) {
            // Drag enter event after reorder was stopped. no-op.
            if (!mReorderDelegate.getInReorderMode()) return;
            // Tab drag started reorder - update reorder to handle drag onto strip.
            mReorderDelegate.updateReorderPosition(
                    mStripViews,
                    mStripGroupTitles,
                    mStripTabs,
                    lastX,
                    /* deltaX= */ 0f,
                    ReorderType.DRAG_ONTO_STRIP);
        } else {
            // Destination strip (ie: view dragged onto another strip)
            // 1. If strip model does not match dragged view's, no-op.
            if (mIncognito != draggedTabIncognito) return;

            // 2. StartX indicates where the external drag enters the  tab strip.
            // Adjust by a half tab-width so that we target the nearest tab gap.
            float startX =
                    StripLayoutUtils.adjustXForTabDrop(
                            currX,
                            mCachedTabWidthSupplier,
                            TabStripDragHandler.isDraggingPinnedItem());

            // 3. Mark the "interacting" view. This is not the DnD dragged view, but rather the view
            // in the strip that is currently being hovered by the DnD drag.
            StripLayoutView hoveredView =
                    getViewAtPositionX(startX, /* includeGroupTitles= */ true);
            if (hoveredView == null) hoveredView = mStripViews[mStripViews.length - 1];

            // 4. Start reorder - prepare strip to indicate drop target.
            startReorderMode(startX, /* y= */ 0.f, hoveredView, ReorderType.DRAG_ONTO_STRIP);
        }
    }

    public void handleDragWithin(float x, float y, float deltaX, boolean draggedTabIncognito) {
        if (mIncognito == draggedTabIncognito) {
            drag(x, y, deltaX);
        }
    }

    public void handleDragExit(boolean isSourceStrip, boolean draggedTabIncognito) {
        if (isSourceStrip) {
            // Drag exit event after reorder was stopped. no-op.
            if (!mReorderDelegate.getInReorderMode()) return;
            // Tab drag started reorder - update reorder to handle drag out of strip.
            // endX is inaccurate but unused.
            mReorderDelegate.updateReorderPosition(
                    mStripViews,
                    mStripGroupTitles,
                    mStripTabs,
                    /* endX= */ 0f,
                    /* deltaX= */ 0f,
                    ReorderType.DRAG_OUT_OF_STRIP);
        } else if (mIncognito == draggedTabIncognito) {
            stopReorderMode();
        }
    }

    /**
     * Handles merging a group of tabs into an existing tab group on drop and expands them if the
     * dropped group was collapsed.
     *
     * @param tabIds The list of tab IDs to merge into an existing group.
     * @param index The index to insert the tabs.
     * @param isCollapsed Whether the dropped group was collapsed before the drop.
     */
    public void maybeMergeToGroupOnDrop(List<Integer> tabIds, int index, boolean isCollapsed) {
        boolean mergeToGroup =
                mReorderDelegate.handleDropForExternalView(mStripGroupTitles, tabIds, index);

        // Expand strip tabs if needed.
        if (mergeToGroup && isCollapsed && mModel != null) {
            // Selects the first tab in the collapsed group. For expanded groups, the correct tab
            // should be selected during tab creation.
            TabModelUtils.setIndex(mModel, index);
            finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
            computeAndUpdateTabWidth(/* animate= */ true, /* deferAnimations= */ false);
        }
    }

    public void stopReorderMode() {
        if (mReorderDelegate.getInReorderMode()) {
            mReorderDelegate.stopReorderMode(mStripViews, mStripGroupTitles);
        }
    }

    public int getTabIndexForTabDrop(float x, boolean isPinned) {
        for (int i = 0; i < mStripViews.length; i++) {
            final StripLayoutView stripView = mStripViews[i];
            final float leftEdge;
            final float rightEdge;
            boolean rtl = LocalizationUtils.isLayoutRtl();
            if (stripView instanceof StripLayoutTab tab) {
                if (tab.isCollapsed()) continue;
                final float halfTabWidth = getCachedTabWidth(tab.getIsPinned()) / 2;
                leftEdge = tab.getTouchTargetLeft();
                rightEdge = tab.getTouchTargetRight();

                boolean hasReachedThreshold =
                        rtl ? x > rightEdge - halfTabWidth : x < leftEdge + halfTabWidth;
                if (hasReachedThreshold) {
                    int tabIndex = StripLayoutUtils.findIndexForTab(mStripTabs, tab.getTabId());
                    return isPinned == tab.getIsPinned() ? tabIndex : getNumLivePinnedTabs();
                }
            } else {
                final StripLayoutGroupTitle groupTitle = (StripLayoutGroupTitle) stripView;
                final float halfGroupTitleWidth = groupTitle.getWidth() / 2;
                leftEdge = groupTitle.getDrawX();
                rightEdge = leftEdge + groupTitle.getWidth();

                boolean hasReachedThreshold =
                        rtl
                                ? x > rightEdge - halfGroupTitleWidth
                                : x < leftEdge + halfGroupTitleWidth;
                if (hasReachedThreshold) {
                    int tabIndex =
                            StripLayoutUtils.findIndexForTab(
                                    mStripTabs, ((StripLayoutTab) mStripViews[i + 1]).getTabId());
                    return isPinned ? getNumLivePinnedTabs() : tabIndex;
                }
            }
        }
        return isPinned ? getNumLivePinnedTabs() : mStripTabs.length;
    }

    public void onMediaStateChanged(Tab tab, @MediaState int mediaState) {
        StripLayoutTab stripLayoutTab = findTabById(tab.getId());
        // This state may get reset after the tab has already closed, so ignore if null.
        if (stripLayoutTab == null) return;
        stripLayoutTab.setMediaState(mediaState);
        setAccessibilityDescription(stripLayoutTab, tab);
    }

    private boolean isViewDraggingInProgress() {
        return mTabStripDragHandler != null && mTabStripDragHandler.isViewDraggingInProgress();
    }

    private void onWillCloseView(@Nullable StripLayoutView view) {
        if (view == null) return;

        view.setWillClose(/* willClose= */ true);
        if (view == mDelayedReorderView) resetDelayedReorderState();
        if (view == mReorderDelegate.getInteractingView()) stopReorderMode();
    }

    private void resetDelayedReorderState() {
        mDelayedReorderView = null;
        mDelayedReorderInitialX = 0.f;
    }

    /** Returns the keyboard-focused view, or null if there is none. */
    private @Nullable StripLayoutView getKeyboardFocusedView() {
        List<VirtualView> virtualViews = new ArrayList<>();
        getVirtualViews(virtualViews);
        for (VirtualView view : virtualViews) {
            if (view.isKeyboardFocused()) return (StripLayoutView) view;
        }
        return null;
    }

    void startDragAndDropTabForTesting(StripLayoutTab clickedTab, PointF dragStartPointF) {
        startReorderMode(
                dragStartPointF.x, dragStartPointF.y, clickedTab, ReorderType.START_DRAG_DROP);
    }
}
