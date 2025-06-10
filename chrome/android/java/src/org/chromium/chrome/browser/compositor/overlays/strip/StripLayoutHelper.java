// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.FOLIO_ATTACHED_BOTTOM_MARGIN_DP;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.FOLIO_DETACHED_BOTTOM_MARGIN_DP;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.INVALID_TIME;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.MAX_TAB_WIDTH_DP;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.MIN_TAB_WIDTH_DP;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.TAB_OVERLAP_WIDTH_DP;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil.FOLIO_FOOT_LENGTH_DP;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
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
import android.view.MotionEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.ListPopupWindow;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
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
import org.chromium.chrome.browser.compositor.overlays.strip.TabLoadTracker.TabLoadTrackerCallback;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripIphController.IphType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.StripUpdateDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.TabDragSource;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabClosureParamsUtils;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupColorUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
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
import org.chromium.ui.util.XrUtils;
import org.chromium.ui.widget.RectProvider;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * This class handles managing the positions and behavior of all tabs in a tab strip. It is
 * responsible for both responding to UI input events and model change notifications, adjusting and
 * animating the tab strip as required.
 *
 * <p>The stacking and visual behavior is driven by setting a {@link StripStacker}.
 */
public class StripLayoutHelper
        implements StripLayoutGroupTitleDelegate,
                StripLayoutViewOnClickHandler,
                StripLayoutViewOnKeyboardFocusHandler,
                StripUpdateDelegate,
                AnimationHost,
                TabListNotificationHandler {
    private static final String TAG = "StripLayoutHelper";

    // Animation/Timer Constants
    private static final int RESIZE_DELAY_MS = 1500;
    private static final int SPINNER_UPDATE_DELAY_MS = 66;
    // Degrees per millisecond.
    private static final float SPINNER_DPMS = 0.33f;
    private static final int ANIM_TAB_CREATED_MS = 150;
    private static final int ANIM_TAB_CLOSED_MS = 150;
    private static final int ANIM_TAB_RESIZE_MS = 250;
    private static final int ANIM_TAB_DRAW_X_MS = 250;
    private static final int ANIM_BUTTONS_FADE_MS = 150;
    private static final int ANIM_HOVERED_TAB_CONTAINER_FADE_MS = 200;
    private static final int NEW_ANIM_TAB_RESIZE_MS = 200;

    // Visibility Constants
    private static final float TAB_WIDTH_MEDIUM = 156.f;
    private static final float NEW_TAB_BUTTON_BACKGROUND_Y_OFFSET_DP = 3.f;
    private static final float NEW_TAB_BUTTON_CLICK_SLOP_DP = 8.f;
    private static final float NEW_TAB_BUTTON_BACKGROUND_WIDTH_DP = 32.f;
    private static final float NEW_TAB_BUTTON_BACKGROUND_HEIGHT_DP = 32.f;
    private static final float BUTTON_DESIRED_TOUCH_TARGET_SIZE = 48.f;

    // Desired spacing between new tab button and tabs when tab strip is not full.
    private static final float NEW_TAB_BUTTON_X_OFFSET_TOWARDS_TABS = 4.f;
    private static final float DESIRED_PADDING_BETWEEN_NEW_TAB_BUTTON_AND_TABS = 2.f;
    private static final float NEW_TAB_BUTTON_DEFAULT_PRESSED_OPACITY = 0.2f;
    private static final float NEW_TAB_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY = 0.12f;
    private static final float NEW_TAB_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY = 0.08f;
    static final float TAB_OPACITY_HIDDEN = 0.f;
    static final float TAB_OPACITY_VISIBLE = 1.f;
    static final float FADE_FULL_OPACITY_THRESHOLD_DP = 24.f;
    private static final float NEW_TAB_BUTTON_WITH_MODEL_SELECTOR_BUTTON_PADDING = 8.f;

    private static final int MESSAGE_RESIZE = 1;
    private static final int MESSAGE_UPDATE_SPINNER = 2;
    private static final int MESSAGE_HOVER_CARD = 3;
    private static final float CLOSE_BTN_VISIBILITY_THRESHOLD_START = 96.f;
    private static final long TAB_SWITCH_METRICS_MAX_ALLOWED_SCROLL_INTERVAL =
            DateUtils.MINUTE_IN_MILLIS;

    // Reorder Drag Threshold Constants
    // TODO(crbug.com/382122020): Revisit and update if needed.
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
                int mSourceRootId = Tab.INVALID_TAB_ID;

                @Override
                public void willMoveTabGroup(int tabModelOldIndex, int tabModelNewIndex) {
                    mMovingGroup = true;
                }

                @Override
                public void didMoveTabGroup(
                        Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                    mMovingGroup = false;
                }

                @Override
                public void didMergeTabToGroup(Tab movedTab) {
                    // TODO(crbug.com/375047646): Investigate kicking off animations here.
                    int rootId = movedTab.getRootId();
                    updateGroupTextAndSharedState(rootId);
                    onTabMergeToOrMoveOutOfGroup();

                    // Tab merging should not automatically expand a collapsed tab group. If the
                    // target group is collapsed, the tab being merged should also be collapsed.
                    StripLayoutGroupTitle groupTitle = findGroupTitle(rootId);
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
                    mSourceRootId = movedTab.getRootId();
                }

                @Override
                public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
                    updateGroupTextAndSharedState(mSourceRootId);
                    int groupIdToHide = mGroupIdToHideSupplier.get();
                    boolean removedLastTabInGroup =
                            (groupIdToHide != Tab.INVALID_TAB_ID)
                                    && (movedTab.getRootId() == groupIdToHide);
                    // Skip if the rebuild will be handled elsewhere after reaching a "proper" tab
                    // state, such as confirming the group deletion.
                    if (!removedLastTabInGroup) onTabMergeToOrMoveOutOfGroup();

                    // Expand the tab if necessary.
                    StripLayoutTab tab = findTabById(movedTab.getId());
                    if (tab != null && tab.isCollapsed()) {
                        updateTabCollapsed(tab, false, false);
                        finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
                        computeAndUpdateTabWidth(
                                /* animate= */ true,
                                /* deferAnimations= */ false,
                                /* closedTab= */ null);
                    }
                }

                @Override
                public void didMoveWithinGroup(
                        Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                    updateGroupAccessibilityDescription(findGroupTitle(movedTab.getRootId()));
                }

                @Override
                public void didCreateNewGroup(Tab destinationTab, TabGroupModelFilter filter) {
                    rebuildStripViews();
                }

                @Override
                public void didChangeTabGroupTitle(int rootId, String newTitle) {
                    final StripLayoutGroupTitle groupTitle = findGroupTitle(rootId);
                    if (groupTitle == null) return;

                    updateGroupTextAndSharedState(groupTitle, newTitle);
                    mRenderHost.requestRender();
                }

                @Override
                public void didChangeTabGroupColor(int rootId, @TabGroupColorId int newColor) {
                    updateGroupTitleTint(findGroupTitle(rootId), newColor);
                }

                @Override
                public void didChangeTabGroupCollapsed(
                        int rootId, boolean isCollapsed, boolean animate) {
                    final StripLayoutGroupTitle groupTitle = findGroupTitle(rootId);
                    if (groupTitle == null) return;

                    if (!isCollapsed && groupTitle.getNotificationBubbleShown()) {
                        groupTitle.setNotificationBubbleShown(false);
                        updateGroupTextAndSharedState(rootId);
                    }
                    updateTabGroupCollapsed(groupTitle, isCollapsed, animate);
                }

                @Override
                public void didChangeGroupRootId(int oldRootId, int newRootId) {
                    StripLayoutGroupTitle groupTitle = findGroupTitle(oldRootId);
                    if (groupTitle != null) {
                        groupTitle.updateRootId(newRootId);
                        // Refresh properties since removing the root tab may have cleared the ones
                        // associated with the oldRootId before updating to the newRootId here.
                        updateGroupTextAndSharedState(groupTitle);
                        updateGroupTitleTint(groupTitle);
                    }

                    // Update LastSyncedGroupId to prevent the IPH from being dismissed when the
                    // synced rootId changes.
                    if (oldRootId == mLastSyncedGroupRootIdForIph) {
                        mLastSyncedGroupRootIdForIph = newRootId;
                    }

                    // Update sourceRootId for moving tabs out of a group. This handles the tab
                    // removal causing the rootId to change.
                    if (oldRootId == mSourceRootId) {
                        mSourceRootId = newRootId;
                    }
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
                    releaseResourcesForGroupTitle(oldTabGroupId);
                    if (mGroupIdToHideSupplier.get() == oldRootId) {
                        // Clear the hidden group ID if the group has been removed from the model.
                        mGroupIdToHideSupplier.set(Tab.INVALID_TAB_ID);
                    }

                    // dismiss the iph text bubble when the synced tab group is unsynced.
                    if (oldRootId == mLastSyncedGroupRootIdForIph) {
                        dismissTabStripSyncIph();
                    }
                    onWillCloseView(
                            StripLayoutUtils.findGroupTitle(mStripGroupTitles, oldTabGroupId));
                }
            };

    // External influences
    private final LayoutUpdateHost mUpdateHost;
    private final LayoutRenderHost mRenderHost;
    private final LayoutManagerHost mManagerHost;
    private final WindowAndroid mWindowAndroid;
    private TabModel mModel;
    private TabGroupModelFilter mTabGroupModelFilter;
    private TabCreator mTabCreator;
    private LayerTitleCache mLayerTitleCache;
    @NonNull private final BottomSheetController mBottomSheetController;
    @NonNull private final Supplier<ShareDelegate> mShareDelegateSupplier;

    @NonNull
    private final TabGroupListBottomSheetCoordinatorFactory
            mTabGroupListBottomSheetCoordinatorFactory;

    // Internal State
    private StripLayoutView[] mStripViews = new StripLayoutView[0];
    private StripLayoutTab[] mStripTabs = new StripLayoutTab[0];
    private StripLayoutTab[] mStripTabsToRender = new StripLayoutTab[0];
    private StripLayoutGroupTitle[] mStripGroupTitles = new StripLayoutGroupTitle[0];
    private StripLayoutGroupTitle[] mStripGroupTitlesToRender = new StripLayoutGroupTitle[0];
    private StripLayoutTab mTabAtPositionForTesting;
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
    private Animator mRunningAnimator;

    private final TintedCompositorButton mNewTabButton;
    @Nullable private final CompositorButton mModelSelectorButton;

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

    // Reorder State
    private boolean mMovingGroup;

    // Tab switch efficiency
    private Long mTabScrollStartTime;
    private Long mMostRecentTabScroll;

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

    // 3-dots menu button with tab strip end padding
    private final boolean mIncognito;
    private boolean mIsFirstLayoutPass;
    // Whether tab strip scrolling is in progress
    private boolean mIsStripScrollInProgress;

    // Tab menu item IDs
    public static final int ID_CLOSE_ALL_TABS = 0;

    private final Context mContext;

    // Animation states. True while the relevant animations are running, and false otherwise.
    private boolean mMultiStepTabCloseAnimRunning;
    private boolean mNewTabButtonAnimRunning;
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
    private StripLayoutView mDelayedReorderView;

    // X-position of the initial interaction with the view above. If the user drags a certain
    // distance away from this initial position, the context menu (if any) will be dismissed, and
    // we'll enter reorder mode.
    private float mDelayedReorderInitialX;

    // Tab Drag and Drop state to hold clicked tab being dragged.
    private final View mToolbarContainerView;
    @Nullable private final TabDragSource mTabDragSource;

    // Tab hover state.
    private StripLayoutTab mLastHoveredTab;
    private StripTabHoverCardView mTabHoverCardView;
    private long mLastHoverCardExitTime;

    // Tab Group Sync.
    private int mLastSyncedGroupRootIdForIph = Tab.INVALID_TAB_ID;
    private final Supplier<Boolean> mTabStripVisibleSupplier;

    // Tab group delete dialog.
    private final ObservableSupplierImpl<Integer> mGroupIdToHideSupplier =
            new ObservableSupplierImpl<>(Tab.INVALID_TAB_ID);

    // Tab group context menu.
    private TabGroupContextMenuCoordinator mTabGroupContextMenuCoordinator;

    // Tab context menu.
    @Nullable private TabContextMenuCoordinator mTabContextMenuCoordinator;
    @Nullable private TabGroupListBottomSheetCoordinator mTabGroupListBottomSheetCoordinator;
    @NonNull private final MultiInstanceManager mMultiInstanceManager;

    // Tab group share.
    @NonNull private DataSharingService mDataSharingService;
    @NonNull private CollaborationService mCollaborationService;
    @Nullable private final DataSharingTabManager mDataSharingTabManager;
    @Nullable private DataSharingService.Observer mDataSharingObserver;
    @Nullable private TabGroupSyncService mTabGroupSyncService;
    @Nullable private TabGroupSyncService.Observer mTabGroupSyncObserver;

    // IPH on tab strip.
    private TabStripIphController mTabStripIphController;
    private final List<QueuedIph> mQueuedIphList = new ArrayList<>();

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
     * @param tabDragSource The @{@link TabDragSource} instance to initiate drag and drop.
     * @param toolbarContainerView The @{link View} passed to @{link TabDragSource} for drag and
     *     drop.
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
            LayoutManagerHost managerHost,
            LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost,
            boolean incognito,
            CompositorButton modelSelectorButton,
            @Nullable TabDragSource tabDragSource,
            @NonNull View toolbarContainerView,
            @NonNull WindowAndroid windowAndroid,
            ActionConfirmationManager actionConfirmationManager,
            DataSharingTabManager dataSharingTabManager,
            Supplier<Boolean> tabStripVisibleSupplier,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull MultiInstanceManager multiInstanceManager,
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull
                    TabGroupListBottomSheetCoordinatorFactory
                            tabGroupListBottomSheetCoordinatorFactory) {
        mGroupTitleDrawXOffset = TAB_OVERLAP_WIDTH_DP - FOLIO_FOOT_LENGTH_DP;
        mGroupTitleOverlapWidth = FOLIO_FOOT_LENGTH_DP - mGroupTitleDrawXOffset;
        mNewTabButtonWidth = NEW_TAB_BUTTON_BACKGROUND_WIDTH_DP;
        mModelSelectorButton = modelSelectorButton;
        mToolbarContainerView = toolbarContainerView;
        mTabDragSource = tabDragSource;
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

        mManagerHost = managerHost;
        mUpdateHost = updateHost;
        mRenderHost = renderHost;

        // Set new tab button background resource.
        mNewTabButton =
                new TintedCompositorButton(
                        context,
                        ButtonType.NEW_TAB,
                        null,
                        NEW_TAB_BUTTON_BACKGROUND_WIDTH_DP,
                        NEW_TAB_BUTTON_BACKGROUND_HEIGHT_DP,
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
        int BackgroundDefaultTint = TabUiThemeProvider.getDefaultNtbContainerColor(context);

        // Primary @ 20% for default pressed bg color.
        int BackgroundPressedTint =
                ColorUtils.setAlphaComponentWithFloat(
                        SemanticColorUtils.getDefaultIconColorAccent1(context),
                        NEW_TAB_BUTTON_DEFAULT_PRESSED_OPACITY);

        // gm3_baseline_surface_container_dark for incognito bg color.
        int BackgroundIncognitoDefaultTint =
                context.getColor(R.color.tab_strip_bg_incognito_default_tint);

        // gm3_baseline_surface_container_highest_dark for incognito pressed bg color
        int BackgroundIncognitoPressedTint =
                context.getColor(R.color.tab_strip_bg_incognito_pressed_tint);

        // Tab strip redesign new tab button night mode bg color.
        if (ColorUtils.inNightMode(context)) {
            // colorSurfaceContainerLow for night mode bg color.
            BackgroundDefaultTint = SemanticColorUtils.getColorSurfaceContainerLow(context);

            // colorSurfaceContainerHighest for pressed night mode bg color.
            BackgroundPressedTint = SemanticColorUtils.getColorSurfaceContainerHighest(context);
        }
        mNewTabButton.setBackgroundTint(
                BackgroundDefaultTint,
                BackgroundPressedTint,
                BackgroundIncognitoDefaultTint,
                BackgroundIncognitoPressedTint,
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
        mCloseButtonMenu.setAdapter(
                new ArrayAdapter<String>(
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
                        if (position == ID_CLOSE_ALL_TABS) {
                            mTabGroupModelFilter
                                    .getTabModel()
                                    .getTabRemover()
                                    .closeTabs(
                                            TabClosureParams.closeAllTabs()
                                                    .hideTabGroups(true)
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
    }

    /** Cleans up internal state. An instance should not be used after this method is called. */
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
     * top-left point of the StripLayoutHelper. The area will include the tabs and new tab button.
     */
    RectF getTouchableRect() {
        return mTouchableRect;
    }

    /**
     * @return The visually ordered list of visible {@link StripLayoutTab}s.
     */
    public StripLayoutTab[] getStripLayoutTabsToRender() {
        return mStripTabsToRender;
    }

    /**
     * @return The visually ordered list of visible {@link StripLayoutGroupTitle}s.
     */
    public StripLayoutGroupTitle[] getStripLayoutGroupTitlesToRender() {
        return mStripGroupTitlesToRender;
    }

    /**
     * @return A {@link TintedCompositorButton} that represents the positioning of the new tab
     *         button.
     */
    public TintedCompositorButton getNewTabButton() {
        return mNewTabButton;
    }

    /**
     * @return The effective width of a tab (accounting for overlap).
     */
    private float getEffectiveTabWidth() {
        return mCachedTabWidthSupplier.get() - TAB_OVERLAP_WIDTH_DP;
    }

    /**
     * @return The visual offset to be applied to the new tab button.
     */
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
     * Check whether the tab strip is full by checking whether tab width has decreased to fit more
     * tabs.
     *
     * @return Whether the tab strip is full.
     */
    private boolean isTabStripFull() {
        return mCachedTabWidthSupplier.get() < MAX_TAB_WIDTH_DP;
    }

    /**
     * Determine How far to shift new tab button icon visually towards the tab in order to achieve
     * the desired spacing between new tab button and tabs when tab strip is not full.
     *
     * @return Visual offset of new tab button icon.
     */
    protected float getNtbVisualOffsetHorizontal() {
        return (BUTTON_DESIRED_TOUCH_TARGET_SIZE - mNewTabButtonWidth) / 2
                - DESIRED_PADDING_BETWEEN_NEW_TAB_BUTTON_AND_TABS;
    }

    /**
     * @return The opacity to use for the fade on the left side of the tab strip.
     */
    public float getLeftFadeOpacity() {
        return getFadeOpacity(true);
    }

    /**
     * @return The opacity to use for the fade on the right side of the tab strip.
     */
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
        float edgeOffset = mScrollDelegate.getEdgeOffset(isLeft);
        if (edgeOffset <= 0.f) {
            return 0.f;
        } else if (edgeOffset >= FADE_FULL_OPACITY_THRESHOLD_DP) {
            return 1.f;
        } else {
            return edgeOffset / FADE_FULL_OPACITY_THRESHOLD_DP;
        }
    }

    /**
     * @return The strip's current scroll offset. It's a 1-D vector on the X axis under the dynamic
     *     coordinate system used by {@link ScrollDelegate}.
     */
    float getScrollOffset() {
        return mScrollDelegate.getScrollOffset();
    }

    float getVisibleLeftBound() {
        return mLeftPadding;
    }

    float getVisibleRightBound() {
        return mWidth - mRightPadding;
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
            computeAndUpdateTabWidth(
                    /* animate= */ false, /* deferAnimations= */ false, /* closedTab= */ null);
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

            rebuildStripTabs(false, false);
            finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
            computeAndUpdateTabWidth(
                    /* animate= */ false, /* deferAnimations= */ false, /* closedTab= */ null);
        }
        if (getSelectedTabId() != Tab.INVALID_TAB_ID) {
            tabSelected(LayoutManagerImpl.time(), getSelectedTabId(), Tab.INVALID_TAB_ID);
        }
    }

    /** Called to notify that the tab state has been initialized. */
    protected void onTabStateInitialized() {
        mTabStateInitialized = true;

        if (mPlaceholderStripReady) {
            int numLeftoverPlaceholders = 0;
            for (int i = 0; i < mStripTabs.length; i++) {
                if (mStripTabs[i].getIsPlaceholder()) numLeftoverPlaceholders++;
            }

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
        rebuildStripTabs(false, false);
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
                mTabDragSource,
                mActionConfirmationManager,
                mCachedTabWidthSupplier,
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
            mDataSharingService = DataSharingServiceFactory.getForProfile(profile);
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
                            @Nullable
                            StripLayoutGroupTitle groupTitle =
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
                            updateOrClearSharedState(groupData);
                        }

                        @Override
                        public void onGroupAdded(GroupData groupData) {
                            updateOrClearSharedState(groupData);
                        }

                        @Override
                        public void onGroupRemoved(String collaborationId) {
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
        if (mTabStripIphController == null && !mIncognito) {
            UserEducationHelper userEducationHelper =
                    new UserEducationHelper(
                            mWindowAndroid.getActivity().get(),
                            mModel.getProfile(),
                            new Handler(Looper.getMainLooper()));
            Tracker tracker = TrackerFactory.getTrackerForProfile(mModel.getProfile());
            mTabStripIphController =
                    new TabStripIphController(
                            mContext.getResources(), userEducationHelper, tracker);
        }

        updateTitleCacheForInit();
        rebuildStripViews();
    }

    private boolean shouldEnableGroupSharing() {
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
            updateGroupTextAndSharedState(groupTitle, groupTitle.getTitle());
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

        final boolean doneAnimating = mRunningAnimator == null || !mRunningAnimator.isRunning();
        updateStrip();

        // If this is the first layout pass, scroll to the selected tab so that it is visible.
        // This is needed if the ScrollingStripStacker is being used because the selected tab is
        // not guaranteed to be visible.
        if (mIsFirstLayoutPass) {
            bringSelectedTabToVisibleArea(time, false);
            mIsFirstLayoutPass = false;
        }

        // Show IPH on the last synced tab group, so place it at the front of the queue.
        if (mLastSyncedGroupRootIdForIph != Tab.INVALID_TAB_ID
                && mTabStripIphController.wouldTriggerIph(IphType.TAB_GROUP_SYNC)) {
            final StripLayoutGroupTitle groupTitle = findGroupTitle(mLastSyncedGroupRootIdForIph);
            mQueuedIphList.add(
                    0,
                    () ->
                            attemptToShowTabStripIph(
                                    groupTitle, /* tab= */ null, IphType.TAB_GROUP_SYNC));
            mLastSyncedGroupRootIdForIph = Tab.INVALID_TAB_ID;
        }

        // 4. Attempt to show one iph text bubble at a time on tab strip.
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

    /**
     * Attempt to show IPH for a group title or a tab.
     *
     * @param groupTitle The group title or its related tab where the IPH should be shown.
     * @param tab The tab to show the IPH on. Pass in {@code null} if the IPH is not tied to a
     *     particular tab.
     * @param iphType The type of the IPH to be shown.
     * @return true if {@code showIphOnTabStrip} should be executed immediately; false to retry at a
     *     later time.
     */
    // TODO:(crbug.com/375271955) Ensure sync IPH doesn't show when joining a collaboration group.
    private boolean attemptToShowTabStripIph(
            StripLayoutGroupTitle groupTitle, @Nullable StripLayoutTab tab, @IphType int iphType) {
        // Remove the showTabStrip callback from the queue, as showing IPH is not applicable in
        // these cases.
        if (mModel.isIncognito()
                || mModel.getProfile() == null
                || groupTitle == null
                || !mTabStripIphController.wouldTriggerIph(iphType)) {
            return true;
        }
        // Return early if the tab strip is not visible on screen.
        if (Boolean.FALSE.equals(mTabStripVisibleSupplier.get())) {
            return false;
        }

        // Display iph only when the target view is fully visible.
        StripLayoutView view = tab == null ? (StripLayoutView) groupTitle : tab;
        if (!view.isVisible() || !isViewCompletelyVisible(view)) {
            return false;
        }

        mTabStripIphController.showIphOnTabStrip(
                groupTitle, tab, mToolbarContainerView, iphType, mHeight);
        return true;
    }

    void setLastSyncedGroupIdForTesting(int id) {
        mLastSyncedGroupRootIdForIph = id;
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
        if (selected) {
            bringSelectedTabToVisibleArea(0, false);
        } else {
            clearLastHoveredTab();
            mCloseButtonMenu.dismiss();
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
        } else {
            updateCloseButtons();

            Tab tab = getTabById(id);
            if (tab != null
                    && mTabGroupModelFilter != null
                    && mTabGroupModelFilter.getTabGroupCollapsed(tab.getRootId())) {
                mTabGroupModelFilter.deleteTabGroupCollapsed(tab.getRootId());
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
        StripLayoutTab previouslyFocusedTab = findTabById(prevId);
        if (previouslyFocusedTab != null) previouslyFocusedTab.setIsSelected(false);
        StripLayoutTab newFocusedTab = findTabById(id);
        if (newFocusedTab != null) newFocusedTab.setIsSelected(true);
    }

    /**
     * Called when a tab has been moved in the tabModel.
     *
     * @param time The current time of the app in ms.
     * @param id The id of the Tab.
     * @param oldIndex The old index of the tab in the {@link TabModel}.
     * @param newIndex The new index of the tab in the {@link TabModel}.
     */
    public void tabMoved(long time, int id, int oldIndex, int newIndex) {
        StripLayoutTab tab = findTabById(id);
        if (tab == null || oldIndex == newIndex) return;

        // 1. If the tab is already at the right spot, don't do anything.
        int index = findIndexForTab(id);
        if (index == newIndex || index + 1 == newIndex) return;

        // 2. Swap the tabs.
        StripLayoutUtils.moveElement(mStripTabs, index, newIndex);
        if (!mMovingGroup) {
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
    }

    /**
     * Called when a tab will be closed. When called, the closing tab will be part of the model.
     *
     * @param time The current time of the app in ms.
     * @param tab The tab that will be closed.
     */
    public void willCloseTab(long time, Tab tab) {
        if (tab == null) return;

        updateGroupTextAndSharedState(tab.getRootId());
        onWillCloseView(findTabById(tab.getId()));
    }

    /**
     * Called when a tab is being closed. When called, the closing tab will not be part of the
     * model.
     *
     * @param time The current time of the app in ms.
     * @param id The id of the tab being closed.
     */
    public void tabClosed(long time, int id) {
        if (findTabById(id) == null) return;

        // 1. Find out if we're closing the last tab.  This determines if we resize immediately.
        // We know mStripTabs.length >= 1 because findTabById did not return null.
        boolean closingLastTab = mStripTabs[mStripTabs.length - 1].getTabId() == id;

        // 2. Rebuild the strip.
        rebuildStripTabs(!closingLastTab, false);
    }

    /**
     * Called when a multiple tabs are being closed. When called, the closing tabs will not be part
     * of the model.
     *
     * @param tabs The list of tabs that are being closed.
     */
    public void multipleTabsClosed(List<Tab> tabs) {
        // 1. Find out if we're closing the last tab.  This determines if we resize immediately.
        // We know mStripTabs.length >= 1 because findTabById did not return null.
        boolean closingLastTab = false;
        for (Tab tab : tabs) {
            if (mStripTabs[mStripTabs.length - 1].getTabId() == tab.getId()) {
                closingLastTab = true;
                break;
            }
        }

        // 2. Rebuild the strip.
        rebuildStripTabs(!closingLastTab, false);
    }

    /** Called when all tabs are closed at once. */
    public void willCloseAllTabs() {
        rebuildStripTabs(true, false);
    }

    /**
     * Called when a tab close has been undone and the tab has been restored. This also re-selects
     * the last tab the user was on before the tab was closed.
     *
     * @param time The current time of the app in ms.
     * @param id The id of the Tab.
     */
    public void tabClosureCancelled(long time, int id) {
        final boolean selected = TabModelUtils.getCurrentTabId(mModel) == id;
        tabCreated(time, id, Tab.INVALID_TAB_ID, selected, true, false);
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
        List<Animator> animationList = rebuildStripTabs(false, !onStartup);
        Tab tab = getTabById(id);
        boolean collapsed = false;
        if (tab != null) {
            int rootId = tab.getRootId();
            updateGroupTextAndSharedState(rootId);
            if (mTabGroupModelFilter.getTabGroupCollapsed(rootId)) {
                if (selected) {
                    mTabGroupModelFilter.deleteTabGroupCollapsed(rootId);
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

        mUpdateHost.requestUpdate();
    }

    private void runTabAddedAnimator(
            @NonNull List<Animator> animationList, StripLayoutTab tab, boolean fromTabCreation) {
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
        computeAndUpdateTabWidth(false, false, null);

        // 3. Scroll the strip to bring the selected tab to view and ensure that the active tab
        // container is visible.
        if (mActiveTabIndexOnStartup != TabModel.INVALID_TAB_INDEX) {
            bringSelectedTabToVisibleArea(LayoutManagerImpl.time(), false);
            mStripTabs[mActiveTabIndexOnStartup].setContainerOpacity(TAB_OPACITY_VISIBLE);
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
        if (!mPlaceholderStripReady || mTabStateInitialized) return;

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
            if (replaceIndex != mModel.indexOf(getTabById(id))) return;
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

    /**
     * @return The expected tab count after tabs finish restoring.
     */
    protected int getTabCountOnStartupForTesting() {
        return mTabCountOnStartup;
    }

    /**
     * @return The expected active tab index after tabs finish restoring.
     */
    protected int getActiveTabIndexOnStartupForTesting() {
        return mActiveTabIndexOnStartup;
    }

    /**
     * @return Whether a non-restored tab was created during startup (e.g. through intent).
     */
    protected boolean getCreatedTabOnStartupForTesting() {
        return mCreatedTabOnStartup;
    }

    /**
     * Called to hide close tab buttons when tab width is <156dp when min tab width is 108dp or for
     * partially visible tabs at the edge of the tab strip when min tab width is set to >=156dp.
     */
    private void updateCloseButtons() {
        final int count = mStripTabs.length;
        int selectedIndex = getSelectedStripTabIndex();

        for (int i = 0; i < count; i++) {
            final StripLayoutTab tab = mStripTabs[i];
            boolean tabSelected = selectedIndex == i;
            boolean canShowCloseButton =
                    (tab.getWidth() >= TAB_WIDTH_MEDIUM
                            || (tabSelected && shouldShowCloseButton(tab, i)));
            // TODO(crbug.com/419843587): Await UX direction for close button appearance
            if (ChromeFeatureList.sTabletTabStripAnimation.isEnabled()
                    && tab.isDying()
                    && !tabSelected) {
                mStripTabs[i].setCanShowCloseButton(false, false);
            } else {
                mStripTabs[i].setCanShowCloseButton(canShowCloseButton, !mIsFirstLayoutPass);
            }
        }
    }

    private void setTabContainerVisible(StripLayoutTab tab, boolean selected) {
        // The container will be visible if the tab is selected or is a placeholder tab.
        float containerOpacity =
                selected || tab.getIsPlaceholder() ? TAB_OPACITY_VISIBLE : TAB_OPACITY_HIDDEN;
        tab.setContainerOpacity(containerOpacity);
    }

    private void updateTabContainersAndDividers() {
        int hoveredId = mLastHoveredTab != null ? mLastHoveredTab.getTabId() : Tab.INVALID_TAB_ID;

        StripLayoutView[] viewsOnStrip = StripLayoutUtils.getViewsOnStrip(mStripViews);
        for (int i = 0; i < viewsOnStrip.length; ++i) {
            if (!(viewsOnStrip[i] instanceof StripLayoutTab currTab)) continue;

            // 1. Set container visibility. Handled in a separate animation for hovered tabs.
            if (hoveredId != currTab.getTabId()) {
                setTabContainerVisible(currTab, isSelectedTab(currTab.getTabId()));
            }
            boolean currContainerHidden = currTab.getContainerOpacity() == TAB_OPACITY_HIDDEN;

            boolean hideDividerForDyingTab =
                    ChromeFeatureList.sTabletTabStripAnimation.isEnabled() && currTab.isDying();
            // 2. Set start divider visibility.
            if (i > 0 && viewsOnStrip[i - 1] instanceof StripLayoutTab prevTab) {
                boolean prevContainerHidden = prevTab.getContainerOpacity() == TAB_OPACITY_HIDDEN;
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
            mTouchableRect.set(getVisibleLeftBound(), 0, getVisibleRightBound(), mHeight);
            return;
        }

        // When the tab strip is not full and not in recording mode, NTB is always showing after
        // the last visible tab on strip.
        RectF touchableRect = new RectF(0, 0, 0, mHeight);
        RectF ntbTouchRect = new RectF();
        getNewTabButton().getTouchTarget(ntbTouchRect);
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        if (isRtl) {
            touchableRect.right = getVisibleRightBound();
            touchableRect.left = Math.max(ntbTouchRect.left, getVisibleLeftBound());
        } else {
            touchableRect.left = getVisibleLeftBound();
            touchableRect.right = Math.min(ntbTouchRect.right, getVisibleRightBound());
        }
        mTouchableRect.set(touchableRect);
    }

    /**
     * Checks whether a tab at the edge of the strip is partially hidden, in which case the close
     * button will be hidden to avoid accidental clicks.
     *
     * @param tab The tab to check.
     * @param index The index of the tab.
     * @return Whether the close button should be shown for this tab.
     */
    private boolean shouldShowCloseButton(StripLayoutTab tab, int index) {
        boolean tabStartHidden;
        boolean tabEndHidden;
        boolean isLastTab = index == mStripTabs.length - 1;
        if (LocalizationUtils.isLayoutRtl()) {
            if (isLastTab) {
                tabStartHidden =
                        tab.getDrawX() + TAB_OVERLAP_WIDTH_DP
                                < getVisibleLeftBound()
                                        + mNewTabButton.getDrawX()
                                        + mNewTabButton.getWidth();
            } else {
                tabStartHidden =
                        tab.getDrawX() + TAB_OVERLAP_WIDTH_DP
                                < getVisibleLeftBound() + getCloseBtnVisibilityThreshold(false);
            }
            tabEndHidden =
                    tab.getDrawX() > getVisibleRightBound() - getCloseBtnVisibilityThreshold(true);
        } else {
            tabStartHidden =
                    tab.getDrawX() + tab.getWidth()
                            < getVisibleLeftBound() + getCloseBtnVisibilityThreshold(true);
            if (isLastTab) {
                tabEndHidden =
                        tab.getDrawX() + tab.getWidth() - TAB_OVERLAP_WIDTH_DP
                                > getVisibleLeftBound() + mNewTabButton.getDrawX();
            } else {
                tabEndHidden =
                        (tab.getDrawX() + tab.getWidth() - TAB_OVERLAP_WIDTH_DP
                                > getVisibleRightBound() - getCloseBtnVisibilityThreshold(false));
            }
        }
        return !tabStartHidden && !tabEndHidden;
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
     * @param time The current time of the app in ms.
     * @param x The x coordinate of the end of the drag event.
     * @param y The y coordinate of the end of the drag event.
     * @param deltaX The number of pixels dragged in the x direction.
     */
    public void drag(long time, float x, float y, float deltaX) {
        resetResizeTimeout(false);
        deltaX = MathUtils.flipSignIf(deltaX, LocalizationUtils.isLayoutRtl());

        // 1. Reset the button state.
        mNewTabButton.drag(x, y);

        // 2.a. Enter reorder mode either if the view was initially clicked by a mouse OR the view
        // was long-pressed, but we suppressed reorder mode to instead show the view's context menu.
        // In the second case, dismiss the aforementioned context menu.
        boolean shouldTriggerReorder =
                mDelayedReorderView != null
                        && !mReorderDelegate.getInReorderMode()
                        && (Math.abs(x - mDelayedReorderInitialX) > INITIATE_REORDER_DRAG_THRESHOLD
                                || !isViewContextMenuShowing());
        boolean canReorderViewType =
                !(mDelayedReorderView instanceof StripLayoutGroupTitle)
                        || ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_STRIP_GROUP_REORDER);
        if (shouldTriggerReorder && canReorderViewType) {
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
     * @param time      The current time of the app in ms.
     * @param x         The y coordinate of the start of the fling event.
     * @param y         The y coordinate of the start of the fling event.
     * @param velocityX The amount of velocity in the x direction.
     * @param velocityY The amount of velocity in the y direction.
     */
    public void fling(long time, float x, float y, float velocityX, float velocityY) {
        resetResizeTimeout(false);

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
        resetResizeTimeout(false);

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
        resetResizeTimeout(false);
        StripLayoutView stripView = determineClickedView(x, y, /* buttons= */ 0);
        // If long-pressed on tab (not on close button) or group, mark for delayed reorder during
        // drag.
        if ((stripView instanceof StripLayoutTab clickedTab
                        && !clickedTab.checkCloseHitTest(x, y)
                        && ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_STRIP_CONTEXT_MENU))
                || stripView instanceof StripLayoutGroupTitle) {
            mDelayedReorderView = stripView;
            mDelayedReorderInitialX = x;
        } else if (stripView == null
                || (stripView instanceof StripLayoutTab
                        && !ChromeFeatureList.isEnabled(
                                ChromeFeatureList.TAB_STRIP_CONTEXT_MENU))) {
            startReorderMode(x, y, stripView, ReorderType.START_DRAG_DROP);
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

    private void dismissContextMenu() {
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
            StripLayoutGroupTitle groupTitle, boolean shouldWaitForUpdate) {
        if (mTabGroupContextMenuCoordinator == null) {
            mTabGroupContextMenuCoordinator =
                    TabGroupContextMenuCoordinator.createContextMenuCoordinator(
                            mModel,
                            mTabGroupModelFilter,
                            mMultiInstanceManager,
                            mWindowAndroid,
                            mDataSharingTabManager);
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
    private void showTabGroupContextMenuHelper(StripLayoutGroupTitle groupTitle) {
        // No-op if the tab group isn't found in sync (it might have been removed from another
        // device and will be cleaned up here soon).
        if (groupTitle.getTabGroupId() == null) return;
        // Popup menu requires screen coordinates for anchor view. Get absolute position for title.
        RectProvider anchorRectProvider = new RectProvider();
        getAnchorRect(groupTitle, anchorRectProvider);
        // If the menu is already showing (which may happen if the user does two long presses in
        // quick succession and showing the menu is slow), then abort.
        if (mTabGroupContextMenuCoordinator.isMenuShowing()) return;
        mTabGroupContextMenuCoordinator.showMenu(anchorRectProvider, groupTitle.getTabGroupId());
    }

    private void showTabContextMenu(StripLayoutTab tab) {
        if (mTabContextMenuCoordinator == null) {
            if (mTabGroupListBottomSheetCoordinator == null) {
                mTabGroupListBottomSheetCoordinator =
                        mTabGroupListBottomSheetCoordinatorFactory.create(
                                mContext,
                                mTabGroupModelFilter.getTabModel().getProfile(),
                                (newTabGroupId) -> {
                                    showTabGroupContextMenu(
                                            findGroupTitle(newTabGroupId),
                                            /* shouldWaitForUpdate= */ true);
                                },
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
                            mMultiInstanceManager,
                            mShareDelegateSupplier,
                            mWindowAndroid);
        }
        RectProvider anchorRectProvider = new RectProvider();
        getAnchorRect(tab, anchorRectProvider);
        StripLayoutUtils.performHapticFeedback(mToolbarContainerView);
        mTabContextMenuCoordinator.showMenu(anchorRectProvider, tab.getTabId());
    }

    /* package */ void showTabContextMenuForTesting(StripLayoutTab tab) {
        showTabContextMenu(tab);
    }

    /* package */ void destroyTabContextMenuForTesting() {
        if (mTabContextMenuCoordinator != null) mTabContextMenuCoordinator.destroyMenuForTesting();
    }

    /**
     * retrieves the corresponding group title using the group's collaboration ID then updates or
     * clears the shared state accordingly.
     *
     * @param groupData The shared group data.
     */
    private void updateOrClearSharedState(GroupData groupData) {
        String collaborationId = groupData.groupToken.collaborationId;
        StripLayoutGroupTitle groupTitle =
                StripLayoutUtils.findGroupTitleByCollaborationId(
                        mStripGroupTitles, collaborationId, mTabGroupSyncService);
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
    private void updateOrClearSharedState(GroupData groupData, StripLayoutGroupTitle groupTitle) {
        if (groupTitle == null) return;
        if (TabShareUtils.hasMultipleCollaborators(groupData)) {
            updateSharedTabGroup(groupData.groupToken.collaborationId, groupTitle);
        } else {
            clearSharedTabGroup(groupTitle);
        }
    }

    /**
     * Updates the tab group shared state if applicable.
     *
     * @param groupTitle The group title to update with the shared tab group state.
     */
    private void updateSharedTabGroupIfNeeded(@NonNull StripLayoutGroupTitle groupTitle) {
        Token tabGroupId = groupTitle.getTabGroupId();
        if (shouldEnableGroupSharing()) {
            SavedTabGroup savedTabGroup =
                    mTabGroupSyncService.getGroup(new LocalTabGroupId(tabGroupId));
            if (savedTabGroup == null || savedTabGroup.collaborationId == null) return;

            GroupData groupData = mCollaborationService.getGroupData(savedTabGroup.collaborationId);

            if (TabShareUtils.hasMultipleCollaborators(groupData)) {
                updateSharedTabGroup(savedTabGroup.collaborationId, groupTitle);
            }
        }
    }

    /**
     * Updates the shared state of a tab group, including the avatar face piles and setup
     * notification bubbler for the group title when the group is shared.
     *
     * @param collaborationId The sharing ID associated with the group.
     * @param groupTitle The group title to update with the shared tab group state.
     */
    private void updateSharedTabGroup(
            String collaborationId, @NonNull StripLayoutGroupTitle groupTitle) {
        // Setup tab bubbler used for showing notification bubbles for shared tab groups.
        if (groupTitle.getTabBubbler() == null) {
            TabBubbler tabBubbler =
                    new TabBubbler(
                            mTabGroupModelFilter.getTabModel().getProfile(),
                            this,
                            new ObservableSupplierImpl<>(groupTitle.getTabGroupId()));
            groupTitle.setTabBubbler(tabBubbler);
        }

        groupTitle.updateSharedTabGroup(
                collaborationId,
                mDataSharingService,
                mCollaborationService,
                (avatarRes) -> {
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
    private void clearSharedTabGroup(@NonNull StripLayoutGroupTitle groupTitle) {
        groupTitle.clearSharedTabGroup();
        mLayerTitleCache.removeSharedGroupAvatar(groupTitle.getTabGroupId());
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

    private void getAnchorRect(StripLayoutView stripLayoutView, RectProvider anchorRectProvider) {
        int[] toolbarCoordinates = new int[2];
        Rect backgroundPadding = new Rect();
        mToolbarContainerView.getLocationInWindow(toolbarCoordinates);
        Drawable background = TabOverflowMenuCoordinator.getMenuBackground(mContext, mIncognito);
        background.getPadding(backgroundPadding);
        stripLayoutView.getAnchorRect(anchorRectProvider.getRect());
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
        if (interactingView != null) {
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
        } else {
            // Broadcast to start moving the window instance as the user has long pressed on the
            // open space of the tab strip.
            // TODO(crbug.com/358191015): Decouple the move window broadcast from this method and
            // maybe move to #onLongPress when `stripView` is null.
            sendMoveWindowBroadcast(mToolbarContainerView, x, y);
        }
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
            hoveredTab.setCloseHovered(hoveredTab.checkCloseHitTest(x, y));
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
            mUpdateHost.requestUpdate();
            return;
        }

        // Hovered within the same tab that was last hovered into and close button hover state
        // remains unchanged.
        boolean isCloseHit = hoveredTab.checkCloseHitTest(x, y);
        if (hoveredTab == mLastHoveredTab && hoveredTab.isCloseHovered() == isCloseHit) {
            return;
        } else if (hoveredTab == mLastHoveredTab) {
            // Hovered within the same tab that was last hovered into, but close button hover state
            // has changed.
            hoveredTab.setCloseHovered(isCloseHit);
        } else {
            // Hovered from one tab to another tab on the strip.
            clearLastHoveredTab();
            updateLastHoveredTab(hoveredTab);
        }

        mUpdateHost.requestUpdate();
    }

    /** Called on hover exit event. */
    public void onHoverExit() {
        clearLastHoveredTab();

        // Clear tab strip button (NTB and MSB) hover state.
        clearCompositorButtonHoverStateIfNotClicked();

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

    @VisibleForTesting
    void setTabHoverCardView(StripTabHoverCardView tabHoverCardView) {
        mTabHoverCardView = tabHoverCardView;
    }

    StripTabHoverCardView getTabHoverCardViewForTesting() {
        return mTabHoverCardView;
    }

    void setLastHoveredTabForTesting(StripLayoutTab tab) {
        mLastHoveredTab = tab;
    }

    StripLayoutTab getLastHoveredTab() {
        return mLastHoveredTab;
    }

    void setTabGroupContextMenuCoordinatorForTesting(
            TabGroupContextMenuCoordinator tabGroupContextMenuCoordinator) {
        mTabGroupContextMenuCoordinator = tabGroupContextMenuCoordinator;
    }

    void setTabContextMenuCoordinatorForTesting(
            TabContextMenuCoordinator tabGroupContextMenuCoordinator) {
        mTabContextMenuCoordinator = tabGroupContextMenuCoordinator;
        ResettersForTesting.register(() -> mTabContextMenuCoordinator = null);
    }

    private void clearLastHoveredTab() {
        if (mLastHoveredTab == null) {
            return;
        }

        // Clear close button hover state.
        mLastHoveredTab.setCloseHovered(false);

        // Remove the highlight from the last hovered tab.
        updateHoveredTabAttachedState(mLastHoveredTab, false);
        mStripTabEventHandler.removeMessages(MESSAGE_HOVER_CARD);
        if (mTabHoverCardView.isShown()) {
            mLastHoverCardExitTime = SystemClock.uptimeMillis();
        }
        mTabHoverCardView.hide();
        mLastHoveredTab = null;
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
        CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        hoveredTab,
                        StripLayoutTab.OPACITY,
                        hoveredTab.getContainerOpacity(),
                        TAB_OPACITY_VISIBLE,
                        ANIM_HOVERED_TAB_CONTAINER_FADE_MS)
                .start();
        updateHoveredTabAttachedState(mLastHoveredTab, true);

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

    private void updateHoveredTabAttachedState(StripLayoutTab tab, boolean hovered) {
        if (tab == null) return;

        // Do not update the attached state of a selected tab that is hovered on.
        if (isSelectedTab(tab.getTabId())) return;

        // If a tab is hovered on, detach its container.
        tab.setFolioAttached(!hovered);
        tab.setBottomMargin(
                hovered ? FOLIO_DETACHED_BOTTOM_MARGIN_DP : FOLIO_ATTACHED_BOTTOM_MARGIN_DP);
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
    private void handleCloseTab(StripLayoutTab tab, boolean allowUndo) {
        mMultiStepTabCloseAnimRunning = false;
        finishAnimationsAndCloseDyingTabs(allowUndo);

        // When a tab is closed #resizeStripOnTabClose will run animations for the new tab offset
        // and tab x offsets. When there is only 1 tab remaining, we do not need to run those
        // animations, so #computeAndUpdateTabWidth() is used instead.
        boolean runImprovedTabAnimations = mStripTabs.length > 1;

        // 1. Set the dying state of the tab.
        tab.setIsDying(true);

        // 2. Start the tab closing animator with a listener to resize/move tabs after the closure.
        AnimatorListener listener =
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        // Removes all dying tabs from TabModel.
                        finishAnimationsAndCloseDyingTabs(allowUndo);

                        if (!ChromeFeatureList.sTabletTabStripAnimation.isEnabled()) {
                            if (runImprovedTabAnimations) {
                                resizeStripOnTabClose(getTabById(tab.getTabId()));
                            } else {
                                mMultiStepTabCloseAnimRunning = false;
                                mNewTabButtonAnimRunning = false;

                                // Resize the tabs appropriately.
                                computeAndUpdateTabWidth(
                                        /* animate= */ true,
                                        /* deferAnimations= */ false,
                                        /* closedTab= */ null);
                            }
                        }
                    }
                };
        runTabRemovalAnimation(tab, listener);

        // 3. If we're closing the selected tab, attempt to select the next expanded tab now. If
        // none exists, we'll default to the normal auto-selection behavior (i.e. selecting the
        // closest collapsed tab, or opening the GTS if none exist).
        if (getSelectedTabId() == tab.getTabId()) {
            int nextIndex = getNearbyExpandedTabIndex();
            if (nextIndex != TabModel.INVALID_TAB_INDEX) {
                TabModelUtils.setIndex(mModel, nextIndex);
            }
        }
    }

    private void runTabRemovalAnimation(StripLayoutTab tab, AnimatorListener listener) {
        // 1. Setup the close animation.
        List<Animator> tabClosingAnimators = new ArrayList<>();
        if (ChromeFeatureList.sTabletTabStripAnimation.isEnabled()) {
            // computeAndUpdateTabWidth handles animating a tab closing.
            tabClosingAnimators =
                    computeAndUpdateTabWidth(
                            /* animate= */ true,
                            /* deferAnimations= */ true,
                            /* closedTab= */ getTabById(tab.getTabId()));
        } else {
            tabClosingAnimators.add(
                    CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            tab,
                            StripLayoutTab.Y_OFFSET,
                            tab.getOffsetY(),
                            tab.getHeight(),
                            ANIM_TAB_CLOSED_MS));
            // 2. Start the animation.
            mNewTabButtonAnimRunning = true;
            mMultiStepTabCloseAnimRunning = true;
        }
        startAnimations(tabClosingAnimators, listener);
    }

    private void resizeStripOnTabClose(Tab closedTab) {
        List<Animator> tabStripAnimators = new ArrayList<>();

        // 1. Add tabs expanding animators to expand remaining tabs to fill scrollable area.
        List<Animator> tabExpandAnimators = computeAndUpdateTabWidth(true, true, closedTab);
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

        // 4. Add new tab button offset animation.
        tabStripAnimators.add(getLastTabClosedNtbAnimator());

        // 5. Add animation completion listener and start animations.
        startAnimations(
                tabStripAnimators,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mMultiStepTabCloseAnimRunning = false;
                        mNewTabButtonAnimRunning = false;
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
     */
    public void click(long time, float x, float y, int buttons) {
        resetResizeTimeout(false);
        StripLayoutView clickedView = determineClickedView(x, y, buttons);
        if (clickedView == null) return;
        clearLastHoveredTab();
        if (MotionEventUtils.isSecondaryClick(buttons)) {
            showContextMenu(clickedView);
        } else {
            clickedView.handleClick(time, buttons);
        }
    }

    /**
     * Called on up or cancel touch events. This is called after the click and fling event if any.
     *
     * @param time The current time of the app in ms.
     */
    public void onUpOrCancel(long time) {
        /* 1. Stop any reordering that is happening. For Android drag&drop, this method is invoked
         * immediately after View#startDrag to stop ongoing gesture events. Do not stop reorder in
         * this case.
         */
        if (!isViewDraggingInProgress()) stopReorderMode();

        // 2. Reset state
        if (mNewTabButton.onUpOrCancel() && mModel != null) {
            if (!mModel.isIncognito()) mModel.commitAllTabClosures();
            mTabCreator.launchNtp();
        }
        mIsStripScrollInProgress = false;
        resetDelayedReorderState();
    }

    @Override
    public void onClick(long time, StripLayoutView view, int motionEventButtonState) {
        if (view instanceof StripLayoutTab tab) {
            handleTabClick(tab);
        } else if (view instanceof StripLayoutGroupTitle groupTitle) {
            handleGroupTitleClick(groupTitle);
        } else if (view instanceof CompositorButton button) {
            if (button.getType() == ButtonType.NEW_TAB) {
                handleNewTabClick();
            } else if (button.getType() == ButtonType.TAB_CLOSE) {
                handleCloseButtonClick(
                        (StripLayoutTab) button.getParentView(), motionEventButtonState);
            }
        }
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

    private void showContextMenu(StripLayoutView clickedView) {
        if (clickedView instanceof StripLayoutTab clickedTab
                && ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_STRIP_CONTEXT_MENU)) {
            showTabContextMenu(clickedTab);
        } else if (clickedView instanceof CompositorButton button
                && button.getType() == ButtonType.TAB_CLOSE) {
            showCloseButtonMenu((StripLayoutTab) button.getParentView());
        } else if (clickedView instanceof StripLayoutGroupTitle groupTitle) {
            showTabGroupContextMenu(groupTitle, /* shouldWaitForUpdate= */ false);
        }
    }

    private void handleTabClick(StripLayoutTab tab) {
        if (tab == null || tab.isDying()) return;
        RecordUserAction.record("MobileTabSwitched.TabletTabStrip");
        recordTabSwitchTimeHistogram();

        int newIndex = TabModelUtils.getTabIndexById(mModel, tab.getTabId());

        // Early return, since placeholder tabs are known to not have tab ids.
        if (newIndex == Tab.INVALID_TAB_ID) return;

        TabModelUtils.setIndex(mModel, newIndex);
    }

    private void handleGroupTitleClick(StripLayoutGroupTitle groupTitle) {
        if (groupTitle == null) return;

        int rootId = groupTitle.getRootId();
        boolean isCollapsed = mTabGroupModelFilter.getTabGroupCollapsed(rootId);
        assert isCollapsed == groupTitle.isCollapsed();

        mTabGroupModelFilter.setTabGroupCollapsed(rootId, !isCollapsed, /* animate= */ true);
        RecordHistogram.recordBooleanHistogram("Android.TabStrip.TabGroupCollapsed", !isCollapsed);
    }

    private void handleNewTabClick() {
        if (mModel == null) return;

        RecordUserAction.record("MobileToolbarNewTab");
        if (!mModel.isIncognito()) mModel.commitAllTabClosures();
        mTabCreator.launchNtp();
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
    void handleCloseButtonClick(StripLayoutTab tab, int motionEventButtonState) {
        // Placeholder tabs are expected to have invalid tab ids.
        if (tab == null || tab.isDying() || tab.getTabId() == Tab.INVALID_TAB_ID) return;
        RecordUserAction.record("MobileToolbarCloseTab");
        int tabId = tab.getTabId();
        Tab realTab = getTabById(tabId);
        int rootId = realTab.getRootId();
        StripTabModelActionListener listener =
                new StripTabModelActionListener(
                        rootId,
                        ActionType.CLOSE,
                        mGroupIdToHideSupplier,
                        mToolbarContainerView,
                        /* beforeSyncDialogRunnable= */ null,
                        /* onSuccess= */ null);
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
                    assert tabClosureParams.tabs.size() == 1
                            && tabClosureParams.tabs.get(0) == realTab;
                    handleCloseTab(tab, tabClosureParams.allowUndo);
                };

        boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(motionEventButtonState);
        TabClosureParams params = TabClosureParams.closeTab(realTab).allowUndo(allowUndo).build();
        mTabGroupModelFilter
                .getTabModel()
                .getTabRemover()
                .prepareCloseTabs(params, /* allowDialog= */ true, listener, onPreparedCallback);
    }

    private StripLayoutView determineClickedView(float x, float y, int buttons) {
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

    /**
     * @return Whether or not the tabs are moving.
     */
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
        // Force any outstanding animations to finish. Need to recurse as some animations (like the
        // multi-step tab close animation) kick off another animation once the first ends.
        while (mRunningAnimator != null && mRunningAnimator.isRunning()) {
            mRunningAnimator.end();
        }
        mRunningAnimator = null;
    }

    @Override
    public void startAnimations(@Nullable List<Animator> animationList, AnimatorListener listener) {
        AnimatorSet set = new AnimatorSet();
        set.playTogether(animationList);
        if (listener != null) set.addListener(listener);

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
            if (tab.isDying()) tabsToRemove.add(tab);
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
                    for (StripLayoutTab stripTab : tabsToRemove) {
                        @Nullable Tab tab = mModel.getTabById(stripTab.getTabId());
                        if (tab == null) continue;
                        // Tab group closure related dialogs are handled elsewhere and any logic
                        // related to them can be bypassed.
                        mModel.getTabRemover()
                                .forceCloseTabs(
                                        TabClosureParams.closeTab(tab)
                                                .allowUndo(allowUndo)
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
                mCachedTabWidthSupplier.get(),
                TAB_OVERLAP_WIDTH_DP,
                mGroupTitleOverlapWidth);
    }

    /**
     * Rebuilds the list of {@link StripLayoutTab}s based on the {@link TabModel}. Reuses strip tabs
     * that still exist in the model. Sets tabs at their new position and animates any width
     * changes, unless a multi-step close is running. Requests a layout update.
     *
     * @param delayResize Whether or not the resultant width changes should be delayed (for the
     *     multi-step close animation.
     * @param deferAnimations Whether or not the resultant width changes should automatically run,
     *     or returned as a list to be kicked off simultaneously with other animations.
     * @return The list of width {@link Animator}s to run, if any.
     */
    private List<Animator> rebuildStripTabs(boolean delayResize, boolean deferAnimations) {
        final int count = mModel.getCount();
        StripLayoutTab[] tabs = new StripLayoutTab[count];

        for (int i = 0; i < count; i++) {
            final Tab tab = mModel.getTabAt(i);
            final int id = tab.getId();
            final StripLayoutTab oldTab = findTabById(id);
            tabs[i] = oldTab != null ? oldTab : createStripTab(id);
            setAccessibilityDescription(tabs[i], tab);
        }

        int oldTabsLength = mStripTabs.length;
        mStripTabs = tabs;
        // Update stripViews since tabs are updated.
        rebuildStripViews();

        List<Animator> animationList = null;
        // If multi-step animation is running, the resize will be handled elsewhere.
        if (mStripTabs.length != oldTabsLength && !mMultiStepTabCloseAnimRunning) {
            computeIdealViewPositions();
            if (delayResize) {
                resetResizeTimeout(/* postIfNotPresent= */ true);
            } else {
                finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
                animationList =
                        computeAndUpdateTabWidth(
                                /* animate= */ true,
                                /* deferAnimations= */ deferAnimations,
                                /* closedTab= */ null);
            }
        }

        return animationList;
    }

    private String buildGroupAccessibilityDescription(@NonNull StripLayoutGroupTitle groupTitle) {
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
        List<Tab> relatedTabs = mTabGroupModelFilter.getTabsInGroup(groupTitle.getTabGroupId());
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

    private void updateGroupAccessibilityDescription(StripLayoutGroupTitle groupTitle) {
        if (groupTitle == null) return;
        groupTitle.setAccessibilityDescription(buildGroupAccessibilityDescription(groupTitle));
    }

    @Override
    public void releaseResourcesForGroupTitle(Token groupId) {
        mLayerTitleCache.removeGroupTitle(groupId);
    }

    @Override
    public void rebuildResourcesForGroupTitle(StripLayoutGroupTitle groupTitle) {
        updateGroupTitleBitmapIfNeeded(groupTitle);
    }

    private AnimatorListener getCollapseAnimatorListener(
            StripLayoutGroupTitle collapsedGroupTitle) {
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

    private Animator updateTabCollapsed(StripLayoutTab tab, boolean isCollapsed, boolean animate) {
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

        return CompositorAnimator.ofFloatProperty(
                mUpdateHost.getAnimationHandler(),
                tab,
                StripLayoutTab.WIDTH,
                tab.getWidth(),
                TAB_OVERLAP_WIDTH_DP,
                ANIM_TAB_RESIZE_MS);
    }

    private void updateTabGroupCollapsed(
            StripLayoutGroupTitle groupTitle, boolean isCollapsed, boolean animate) {
        if (groupTitle.isCollapsed() == isCollapsed) return;

        List<Animator> collapseAnimationList = animate ? new ArrayList<>() : null;

        finishAnimations();
        groupTitle.setCollapsed(isCollapsed);
        for (StripLayoutTab tab :
                StripLayoutUtils.getGroupedTabs(mModel, mStripTabs, groupTitle.getRootId())) {
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
                computeAndUpdateTabWidth(
                        /* animate= */ animate,
                        /* deferAnimations= */ animate,
                        /* closedTab= */ null);
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
            if (selectedTab != null && selectedTab.getRootId() == groupTitle.getRootId()) {
                int nextIndex = getNearbyExpandedTabIndex();
                if (nextIndex != TabModel.INVALID_TAB_INDEX) {
                    TabModelUtils.setIndex(mModel, nextIndex);
                } else {
                    mTabCreator.launchNtp();
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
        StripLayoutTab selectedTab = getSelectedStripTab();
        if (selectedTab == null) {
            return;
        }
        Tab tab = getTabById(selectedTab.getTabId());
        if (tab != null && mTabGroupModelFilter.getTabGroupCollapsed(tab.getRootId())) {
            mTabGroupModelFilter.deleteTabGroupCollapsed(tab.getRootId());
        }
    }

    /**
     * @return The index of the nearby expanded tab to the selected tab. Prioritizes tabs before the
     *     selected tab. If none are found, return an invalid index.
     */
    private int getNearbyExpandedTabIndex() {
        int index = getSelectedStripTabIndex();

        for (int i = index - 1; i >= 0; --i) {
            if (!mStripTabs[i].isCollapsed()) return i;
        }

        for (int i = index + 1; i < mStripTabs.length; ++i) {
            if (!mStripTabs[i].isCollapsed()) return i;
        }

        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * Called when a tab has been merged into or removed from a group. Rebuilds the views and
     * re-computes ideal positions, since the order may have changed.
     */
    private void onTabMergeToOrMoveOutOfGroup() {
        finishAnimations();
        // Moving a tab into/out-of a group may cause the orders of views (i.e. the
        // group indicator) to change. The bottom indicator width may also change.
        // Rebuild views to address this.
        rebuildStripTabs(/* delayResize= */ false, /* deferAnimations= */ false);
        // Since views may have swapped, re-calculate ideal positions here.
        computeIdealViewPositions();
    }

    /**
     * Called to refresh the group title bitmap when it may have changed (text, color, or shared
     * group avatar).
     *
     * @param groupTitle The group title to refresh the bitmap for.
     */
    private void updateGroupTitleBitmapIfNeeded(@NonNull StripLayoutGroupTitle groupTitle) {
        if (groupTitle.isVisible()) {
            mLayerTitleCache.getUpdatedGroupTitle(
                    groupTitle.getTabGroupId(), groupTitle.getTitle(), mIncognito);
            mRenderHost.requestRender();
        }
    }

    private void updateGroupTitleTint(StripLayoutGroupTitle groupTitle) {
        int colorId = mTabGroupModelFilter.getTabGroupColor(groupTitle.getRootId());
        // If the color is invalid, temporarily assign a default placeholder color.
        if (colorId == TabGroupColorUtils.INVALID_COLOR_ID) colorId = TabGroupColorId.GREY;
        updateGroupTitleTint(groupTitle, colorId);
    }

    private void updateGroupTitleTint(
            StripLayoutGroupTitle groupTitle, @TabGroupColorId int newColor) {
        if (groupTitle == null) return;

        groupTitle.updateTint(newColor);
        updateGroupTitleBitmapIfNeeded(groupTitle);
    }

    @VisibleForTesting
    void updateGroupTextAndSharedState(int rootId) {
        updateGroupTextAndSharedState(findGroupTitle(rootId));
    }

    private void updateGroupTextAndSharedState(StripLayoutGroupTitle groupTitle) {
        if (groupTitle == null) return;
        updateGroupTextAndSharedState(
                groupTitle, mTabGroupModelFilter.getTabGroupTitle(groupTitle.getRootId()));
    }

    /**
     * Sets a non-empty title text for the given group indicator. Also updates the title text
     * bitmap, accessibility description, and tab/indicator sizes if necessary. If the group is
     * shared, it may also update user avatars and the notification bubble.
     *
     * @param groupTitle The {@link StripLayoutGroupTitle} that we're update the title text for.
     * @param titleText The title text to apply. If empty, use a default title text.
     */
    private void updateGroupTextAndSharedState(StripLayoutGroupTitle groupTitle, String titleText) {
        assert groupTitle != null;
        // Ignore updates for closing group indicators. This prevents assertion errors from using
        // stale group properties.
        if (groupTitle.willClose()) return;

        // 1. Update indicator text and width.
        titleText =
                StripLayoutUtils.getDefaultGroupTitleTextIfEmpty(
                        mContext, mTabGroupModelFilter, groupTitle.getTabGroupId(), titleText);
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
                computeAndUpdateTabWidth(
                        /* animate= */ false, /* deferAnimations= */ false, /* closedTab= */ null);
            } else {
                // If off-screen, request an update so we re-calculate tab initial positions and the
                // scroll offset limit.
                mUpdateHost.requestUpdate();
            }
        }
    }

    private StripLayoutGroupTitle findGroupTitle(int rootId) {
        return StripLayoutUtils.findGroupTitle(mStripGroupTitles, rootId);
    }

    private StripLayoutGroupTitle findGroupTitle(Token tabGroupId) {
        return StripLayoutUtils.findGroupTitle(mStripGroupTitles, tabGroupId);
    }

    private StripLayoutGroupTitle findOrCreateGroupTitle(int rootId, Token tabGroupId) {
        StripLayoutGroupTitle groupTitle = findGroupTitle(rootId);
        return groupTitle == null ? createGroupTitle(rootId, tabGroupId) : groupTitle;
    }

    private StripLayoutGroupTitle createGroupTitle(int rootId, Token tabGroupId) {
        // Delay setting the collapsed state, since mStripViews may not yet be up to date.
        StripLayoutGroupTitle groupTitle =
                new StripLayoutGroupTitle(
                        mContext,
                        /* delegate= */ this,
                        /* keyboardFocusHandler= */ this,
                        mIncognito,
                        rootId,
                        tabGroupId);
        pushPropertiesToGroupTitle(groupTitle);

        // Must pass in the group title instead of rootId, since the StripLayoutGroupTitle has not
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
        mUpdateHost.requestUpdate();
    }

    private int getTabGroupCount() {
        Set<Integer> groupRootIds = new HashSet<>();

        for (int i = 0; i < mStripTabs.length; ++i) {
            final StripLayoutTab stripTab = mStripTabs[i];
            final Tab tab = getTabById(stripTab.getTabId());
            if (mTabGroupModelFilter.isTabInTabGroup(tab)
                    && !groupRootIds.contains(tab.getRootId())) {
                groupRootIds.add(tab.getRootId());
            }
        }

        return groupRootIds.size();
    }

    private void buildBottomIndicator() {
        if (mStripTabs.length == 0 || mTabResizeAnimRunning) {
            return;
        }
        for (int i = 0; i < mStripGroupTitles.length; i++) {
            StripLayoutGroupTitle groupTitle = mStripGroupTitles[i];
            if (groupTitle == null
                    || groupTitle.isCollapsed()
                    || groupTitle.getRootId() == mGroupIdToHideSupplier.get()) {
                continue;
            }

            // Calculate the bottom indicator width.
            float bottomIndicatorWidth =
                    StripLayoutUtils.calculateBottomIndicatorWidth(
                            groupTitle,
                            StripLayoutUtils.getNumOfTabsInGroup(mTabGroupModelFilter, groupTitle),
                            getEffectiveTabWidth());

            // Update the bottom indicator width.
            if (groupTitle.getBottomIndicatorWidth() != bottomIndicatorWidth) {
                groupTitle.setBottomIndicatorWidth(bottomIndicatorWidth);
            }
        }
    }

    private void copyTabsWithGroupTitles() {
        if (mStripTabs.length == 0) return;

        int numGroups = getTabGroupCount();

        // If we have tab group to hide due to running tab group delete dialog, then skip the tab
        // group when rebuilding StripViews.
        if (mGroupIdToHideSupplier.get() != Tab.INVALID_TAB_ID && numGroups > 0) {
            numGroups -= 1;
        }

        int groupTitleIndex = 0;
        StripLayoutGroupTitle[] groupTitles = new StripLayoutGroupTitle[numGroups];

        int numViews = mStripTabs.length + numGroups;
        if (numViews != mStripViews.length) {
            mStripViews = new StripLayoutView[numViews];
        }

        int viewIndex = 0;
        // First view will be tab group title if first tab is grouped.
        Tab firstTab = getTabById(mStripTabs[0].getTabId());
        if (mTabGroupModelFilter.isTabInTabGroup(firstTab)) {
            int rootId = firstTab.getRootId();
            Token tabGroupId = firstTab.getTabGroupId();
            StripLayoutGroupTitle groupTitle = findOrCreateGroupTitle(rootId, tabGroupId);
            if (rootId != mGroupIdToHideSupplier.get()) {
                if (TabUiUtils.shouldShowIphForSync(mTabGroupSyncService, tabGroupId)) {
                    mLastSyncedGroupRootIdForIph = rootId;
                }
                groupTitles[groupTitleIndex++] = groupTitle;
                mStripViews[viewIndex++] = groupTitle;
            }
        }
        // Copy the StripLayoutTabs and create group titles where needed.
        for (int i = 0; i < mStripTabs.length - 1; i++) {
            final StripLayoutTab stripTab = mStripTabs[i];
            mStripViews[viewIndex++] = stripTab;

            Tab currTab = getTabById(stripTab.getTabId());
            Tab nextTab = getTabById(mStripTabs[i + 1].getTabId());
            int nextRootId = nextTab.getRootId();
            Token nextTabGroupId = nextTab.getTabGroupId();
            boolean nextTabInGroup = mTabGroupModelFilter.isTabInTabGroup(nextTab);
            boolean areRelatedTabs = currTab.getRootId() == nextRootId;
            if (nextTabInGroup && !areRelatedTabs) {
                StripLayoutGroupTitle groupTitle =
                        findOrCreateGroupTitle(nextRootId, nextTabGroupId);
                if (nextRootId != mGroupIdToHideSupplier.get()) {
                    if (TabUiUtils.shouldShowIphForSync(mTabGroupSyncService, nextTabGroupId)) {
                        mLastSyncedGroupRootIdForIph = nextRootId;
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
                        mTabGroupModelFilter.getTabGroupCollapsed(groupTitle.getRootId());
                updateTabGroupCollapsed(groupTitle, isCollapsed, false);
            }
            finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
            computeAndUpdateTabWidth(
                    /* animate= */ true, /* deferAnimations= */ false, /* closedTab= */ null);
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
            boolean animate, StripLayoutTab tabToAnimate, boolean tabAddedAnimation) {
        finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
        if (tabToAnimate != null) {
            assert animate;
            if (!tabAddedAnimation) {
                mMultiStepTabCloseAnimRunning = true;
                // Resize the tab strip accordingly.
                resizeStripOnTabClose(getTabById(tabToAnimate.getTabId()));
            } else {
                List<Animator> animationList =
                        computeAndUpdateTabWidth(
                                /* animate= */ true,
                                /* deferAnimations= */ true,
                                /* closedTab= */ null);
                if (animationList != null) {
                    runTabAddedAnimator(animationList, tabToAnimate, /* fromTabCreation= */ false);
                }
            }
        } else {
            computeAndUpdateTabWidth(
                    animate, /* deferAnimations= */ animate, /* closedTab= */ null);
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
                        mIncognito);

        tab.setIsPlaceholder(true);
        tab.setContainerOpacity(TAB_OPACITY_VISIBLE);

        // TODO(crbug.com/40942588): Added placeholder a11y descriptions to prevent crash due
        //  to invalid a11y node. Replace with official strings when available.
        String description = "Placeholder Tab";
        String title = "Placeholder";
        tab.setAccessibilityDescription(description, title, ResourcesCompat.ID_NULL);

        pushPropertiesToTab(tab);

        return tab;
    }

    @VisibleForTesting
    StripLayoutTab createStripTab(int id) {
        // TODO: Cache these
        StripLayoutTab tab =
                new StripLayoutTab(
                        mContext,
                        id,
                        /* clickHandler= */ this,
                        /* keyboardFocusHandler= */ this,
                        mTabLoadTrackerHost,
                        mUpdateHost,
                        mIncognito);

        if (isSelectedTab(id)) {
            tab.setContainerOpacity(TAB_OPACITY_VISIBLE);
        }

        pushPropertiesToTab(tab);

        return tab;
    }

    private void pushPropertiesToPlaceholder(StripLayoutTab placeholderTab, Tab tab) {
        placeholderTab.setTabId(tab.getId());
        placeholderTab.setIsPlaceholder(false);
        placeholderTab.setContainerOpacity(TAB_OPACITY_HIDDEN);

        setAccessibilityDescription(placeholderTab, tab);
    }

    private void pushPropertiesToTab(StripLayoutTab tab) {
        // The close button is visible by default. If it should be hidden on tab creation, do not
        // animate the fade-out. See (https://crbug.com/1342654).
        boolean shouldShowCloseButton = mCachedTabWidthSupplier.get() >= TAB_WIDTH_MEDIUM;
        tab.setCanShowCloseButton(shouldShowCloseButton, false);
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

    private int getNumLiveTabs() {
        int numLiveTabs = 0;

        for (int i = 0; i < mStripTabs.length; i++) {
            final StripLayoutTab tab = mStripTabs[i];
            if (tab.isDying() && !ChromeFeatureList.sTabletTabStripAnimation.isEnabled()) continue;
            if (!tab.isClosed() && !tab.isDraggedOffStrip() && !tab.isCollapsed()) numLiveTabs++;
        }

        return numLiveTabs;
    }

    /**
     * Computes and updates the tab width when resizing the tab strip.
     *
     * @param animate Whether to animate the update.
     * @param deferAnimations Whether to defer animations.
     * @param closedTab The tab that is closing. This value should be non-null, if the resize is
     *     caused by tab closing.
     * @return A list of animators for the tab width update.
     */
    private List<Animator> computeAndUpdateTabWidth(
            boolean animate, boolean deferAnimations, Tab closedTab) {
        // Skip updating the tab width when the tab strip width is unavailable.
        if (mWidth == 0) {
            return null;
        }

        // Remove any queued resize messages.
        mStripTabEventHandler.removeMessages(MESSAGE_RESIZE);

        int numTabs = Math.max(getNumLiveTabs(), 1);

        // 1. Compute the width of the available space for all tabs.
        float stripWidth = mWidth - mLeftMargin - mRightMargin;
        for (int i = 0; i < mStripGroupTitles.length; i++) {
            final StripLayoutGroupTitle groupTitle = mStripGroupTitles[i];
            stripWidth -= (groupTitle.getWidth() - mGroupTitleOverlapWidth);
        }

        // 2. Compute additional width we gain from overlapping the tabs.
        float overlapWidth = TAB_OVERLAP_WIDTH_DP * (numTabs - 1);

        // 3. Calculate the optimal tab width.
        float optimalTabWidth = (stripWidth + overlapWidth) / numTabs;

        // 4. Calculate the realistic tab width.
        mCachedTabWidthSupplier.set(
                MathUtils.clamp(optimalTabWidth, MIN_TAB_WIDTH_DP, MAX_TAB_WIDTH_DP));

        // 5. Prepare animations and propagate width to all tabs.
        ArrayList<Animator> resizeAnimationList = null;
        if (animate) resizeAnimationList = new ArrayList<>();
        for (int i = 0; i < mStripTabs.length; i++) {
            StripLayoutTab tab = mStripTabs[i];
            if (tab.isClosed()) tab.setWidth(TAB_OVERLAP_WIDTH_DP);
            if ((tab.isDying() && !ChromeFeatureList.sTabletTabStripAnimation.isEnabled())
                    || tab.isCollapsed()) {
                continue;
            }
            Float cachedTabWidth = mCachedTabWidthSupplier.get();
            if (resizeAnimationList != null) {
                CompositorAnimator animator;
                // Handle animating a tab being closed for TabletTabStripAnimation.
                if (tab.isDying()) {
                    animator =
                            CompositorAnimator.ofFloatProperty(
                                    mUpdateHost.getAnimationHandler(),
                                    tab,
                                    StripLayoutTab.WIDTH,
                                    tab.getWidth(),
                                    TAB_OVERLAP_WIDTH_DP,
                                    NEW_ANIM_TAB_RESIZE_MS);
                    resizeAnimationList.add(animator);
                    continue;
                }

                if (cachedTabWidth > 0f && tab.getWidth() == cachedTabWidth) {
                    // No need to create an animator to animate to the width we're already at.
                    continue;
                }

                int duration = ANIM_TAB_RESIZE_MS;

                if (ChromeFeatureList.sTabletTabStripAnimation.isEnabled()) {
                    duration = NEW_ANIM_TAB_RESIZE_MS;
                }
                animator =
                        CompositorAnimator.ofFloatProperty(
                                mUpdateHost.getAnimationHandler(),
                                tab,
                                StripLayoutTab.WIDTH,
                                tab.getWidth(),
                                cachedTabWidth,
                                duration);

                resizeAnimationList.add(animator);
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

        // 6. Animate bottom indicator when tab width change.
        for (int i = 0; i < mStripGroupTitles.length; i++) {
            StripLayoutGroupTitle groupTitle = mStripGroupTitles[i];
            if (groupTitle == null) {
                continue;
            }
            if (groupTitle.isCollapsed()) {
                continue;
            }
            float bottomIndicatorStartWidth = groupTitle.getBottomIndicatorWidth();
            float bottomIndicatorEndWidth;

            // When a grouped tab is closed, the bottom indicator end width needs to subtract the
            // width of the closed tab.
            if (closedTab != null && closedTab.getRootId() == groupTitle.getRootId()) {
                bottomIndicatorEndWidth =
                        StripLayoutUtils.calculateBottomIndicatorWidth(
                                groupTitle,
                                StripLayoutUtils.getNumOfTabsInGroup(
                                                mTabGroupModelFilter, groupTitle)
                                        - 1,
                                getEffectiveTabWidth());
            } else {
                bottomIndicatorEndWidth =
                        StripLayoutUtils.calculateBottomIndicatorWidth(
                                groupTitle,
                                StripLayoutUtils.getNumOfTabsInGroup(
                                        mTabGroupModelFilter, groupTitle),
                                getEffectiveTabWidth());
            }

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
        // TODO(dtrainor): Remove this once tabCreated() is refactored to be called even from
        // restore.
        if (mTabStateInitialized
                && (mStripTabs == null || mModel.getCount() != mStripTabs.length)) {
            rebuildStripTabs(false, false);
        }

        // 1. Update the scroll offset limits
        updateScrollOffsetLimits();

        // 2. Calculate the ideal view positions
        computeIdealViewPositions();

        // 3. Calculate view stacking - update view draw properties and visibility.
        float stripWidth = getVisibleRightBound() - getVisibleLeftBound();
        mStripStacker.pushDrawPropertiesToViews(
                mStripViews,
                getVisibleLeftBound(),
                stripWidth,
                mMultiStepTabCloseAnimRunning,
                mCachedTabWidthSupplier.get());

        // 4. Create render list.
        createRenderList();

        // 5. Figure out where to put the new tab button. If a tab is being closed, the new tab
        // button position will be updated with the tab resize and drawX animations.
        if (!mNewTabButtonAnimRunning) updateNewTabButtonState();

        // 6. Invalidate the accessibility provider in case the visible virtual views have changed.
        mRenderHost.invalidateAccessibilityProvider();

        // 7. Hide close buttons if tab width gets lower than 156dp.
        updateCloseButtons();

        // 8. Show dividers between inactive tabs.
        updateTabContainersAndDividers();

        // 9. Update the touchable rect.
        updateTouchableRect();

        // TODO(crbug.com/396213514): Move the show bubble logic somewhere less frequently called.
        // 10. Trigger show notification bubble for all shared tab groups that have recent updates.
        showNotificationBubblesForSharedTabGroups();
    }

    private float getStartPositionForStripViews() {
        // Shift all of the strip views over by the the left margin because we're
        // no longer base lined at 0
        if (!LocalizationUtils.isLayoutRtl()) {
            return mScrollDelegate.getScrollOffset()
                    + mLeftMargin
                    + mScrollDelegate.getReorderStartMargin();
        } else {
            return mWidth
                    - mCachedTabWidthSupplier.get()
                    - mScrollDelegate.getScrollOffset()
                    - mRightMargin
                    - mScrollDelegate.getReorderStartMargin();
        }
    }

    private void computeIdealViewPositions() {
        float startX = getStartPositionForStripViews();
        for (int i = 0; i < mStripViews.length; i++) {
            final StripLayoutView view = mStripViews[i];

            float delta;
            if (view instanceof StripLayoutTab tab) {
                if (tab.isClosed()) continue;
                // idealX represents where a tab should be placed in the tab strip.
                view.setIdealX(startX);
                if (ChromeFeatureList.sTabletTabStripAnimation.isEnabled()) {
                    delta = (tab.getWidth() - TAB_OVERLAP_WIDTH_DP) * tab.getWidthWeight();
                } else {
                    delta =
                            tab.isDying()
                                    ? getEffectiveTabWidth()
                                    : (tab.getWidth() - TAB_OVERLAP_WIDTH_DP)
                                            * tab.getWidthWeight();
                }

            } else {
                // Offset to "undo" the tab overlap width as that doesn't apply to non-tab views.
                // Also applies the desired overlap with the previous tab.
                float drawXOffset = mGroupTitleDrawXOffset;
                // Adjust for RTL.
                if (LocalizationUtils.isLayoutRtl()) {
                    drawXOffset = mCachedTabWidthSupplier.get() - view.getWidth() - drawXOffset;
                }

                view.setIdealX(startX + drawXOffset);
                delta = (view.getWidth() - mGroupTitleOverlapWidth) * view.getWidthWeight();
            }
            // Trailing margins will only be nonzero during reorder mode.
            delta += view.getTrailingMargin();
            delta = MathUtils.flipSignIf(delta, LocalizationUtils.isLayoutRtl());
            startX += delta;
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

    private float adjustNewTabButtonOffsetIfFull(float offset) {
        if (!isTabStripFull()) {
            // Move NTB close to tabs by 4 dp when tab strip is not full.
            boolean isLtr = !LocalizationUtils.isLayoutRtl();
            offset += MathUtils.flipSignIf(NEW_TAB_BUTTON_X_OFFSET_TOWARDS_TABS, isLtr);
        }
        return offset;
    }

    private CompositorAnimator getLastTabClosedNtbAnimator() {
        // TODO(crbug.com/338332428): Unify with the stacker methods.
        float viewsWidth = (getNumLiveTabs() * getEffectiveTabWidth()) + TAB_OVERLAP_WIDTH_DP;
        for (int i = 0; i < mStripViews.length; ++i) {
            final StripLayoutView view = mStripViews[i];
            if (!(view instanceof StripLayoutTab)) viewsWidth += view.getWidth();
        }

        boolean rtl = LocalizationUtils.isLayoutRtl();
        float offset = getStartPositionForStripViews() + MathUtils.flipSignIf(viewsWidth, rtl);
        if (rtl) offset += mCachedTabWidthSupplier.get() - mNewTabButtonWidth;
        offset = adjustNewTabButtonOffsetIfFull(offset);

        CompositorAnimator animator =
                CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        mNewTabButton,
                        StripLayoutView.DRAW_X,
                        mNewTabButton.getDrawX(),
                        offset,
                        ANIM_TAB_RESIZE_MS);
        return animator;
    }

    private void updateNewTabButtonState() {
        // 1. The NTB is faded out upon entering reorder mode and hidden when the model is empty.
        boolean isEmpty = mStripTabs.length == 0;
        mNewTabButton.setVisible(!isEmpty);
        if (isEmpty) return;

        // 2. Get offset from strip stacker.
        // Note: This method anchors the NTB to either a static position at the end of the strip OR
        // right next to the final tab in the strip. This only WAI if the final view in the strip is
        // guaranteed to be a tab. If this changes (e.g. we allow empty tab groups), then this will
        // need to be updated.
        float offset =
                mStripStacker.computeNewTabButtonOffset(
                        mStripTabs,
                        TAB_OVERLAP_WIDTH_DP,
                        mLeftMargin,
                        mRightMargin,
                        mWidth,
                        mNewTabButtonWidth);
        offset = adjustNewTabButtonOffsetIfFull(offset);

        // 3. Hide the new tab button if it's not visible on the screen.
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        if ((isRtl && offset + mNewTabButtonWidth < getVisibleLeftBound())
                || (!isRtl && offset > getVisibleRightBound())) {
            mNewTabButton.setVisible(false);
            return;
        }
        mNewTabButton.setVisible(true);

        // 4. Position the new tab button.
        mNewTabButton.setDrawX(offset);
    }

    /**
     * @param view The {@link StripLayoutView} to make fully visible.
     * @return a 1-D vector on the X axis of the window coordinate system that can make the tab
     *     fully visible.
     */
    private float calculateDeltaToMakeViewVisible(StripLayoutView view) {
        if (view == null) return 0.f;
        // These are always in view.
        if (view.equals(mNewTabButton) || view.equals(mModelSelectorButton)) return 0.f;

        // 1. Calculate offsets to fully show the view on the left/right side of the
        // strip. These offsets are scalars.
        // TODO(wenyufu): Account for offsetX{Left,Right} result too much offset. Is this expected?
        final float rightOffset = mRightFadeWidth + mRightMargin;
        final float leftOffset = mLeftFadeWidth + mLeftMargin;

        // 2. Calculate vectors from the view's ideal position to the farthest left/right point
        // where
        // the view can be visible.
        // These are 1-D vectors on the X axis of the window coordinate system.
        if (view instanceof TintedCompositorButton closeButton
                && closeButton.getParentView() instanceof StripLayoutTab stripTab) {
            view = stripTab;
        }
        final float deltaToFarLeft = leftOffset - view.getIdealX();
        final float deltaToFarRight =
                mWidth - rightOffset - mCachedTabWidthSupplier.get() - view.getIdealX();

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

    StripLayoutTab getTabAtPosition(float x) {
        return (StripLayoutTab) getViewAtPositionX(x, false);
    }

    StripLayoutView getViewAtPositionX(float x, boolean includeGroupTitles) {
        if (mTabAtPositionForTesting != null) {
            return mTabAtPositionForTesting;
        }
        return StripLayoutUtils.findViewAtPositionX(mStripViews, x, includeGroupTitles);
    }

    public boolean getInReorderModeForTesting() {
        return mReorderDelegate.getInReorderMode();
    }

    public float getStripStartMarginForReorderForTesting() {
        return mScrollDelegate.getReorderStartMargin();
    }

    public void startReorderModeAtIndexForTesting(int index) {
        StripLayoutTab tab = mStripTabs[index];
        updateStrip();
        float x = tab.getDrawX() + (tab.getWidth() / 2);
        startReorderMode(x, 0, getTabAtPosition(x), ReorderType.DRAG_WITHIN_STRIP);
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

        StripLayoutGroupTitle groupTitle = findGroupTitle(tab.getRootId());
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
                || !mTabGroupModelFilter.isTabInTabGroup(tab)
                || tab.getRootId() == mGroupIdToHideSupplier.get()) {
            return false;
        }

        // Show tab outline when tab is in group with folio attached and 1. tab is selected or 2.
        // tab is in foreground (e.g. the previously selected tab in destination strip).
        return stripLayoutTab.getFolioAttached()
                && (isSelectedTab(stripLayoutTab.getTabId())
                        || stripLayoutTab.getContainerOpacity() == TAB_OPACITY_VISIBLE);
    }

    private void handleReorderAutoScrolling(long time) {
        if (!mReorderDelegate.getInReorderMode()) return;
        mReorderDelegate.updateReorderPositionAutoScroll(
                mStripViews,
                mStripGroupTitles,
                mStripTabs,
                time,
                mWidth,
                mLeftMargin,
                mRightMargin);
    }

    private Tab getTabById(int tabId) {
        return mModel.getTabById(tabId);
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

    private StripLayoutTab getSelectedStripTab() {
        int index = getSelectedStripTabIndex();

        return index >= 0 && index < mStripTabs.length ? mStripTabs[index] : null;
    }

    private boolean isSelectedTab(int id) {
        return id != Tab.INVALID_TAB_ID && id == getSelectedTabId();
    }

    private void resetResizeTimeout(boolean postIfNotPresent) {
        final boolean present = mStripTabEventHandler.hasMessages(MESSAGE_RESIZE);

        if (present) mStripTabEventHandler.removeMessages(MESSAGE_RESIZE);

        if (present || postIfNotPresent) {
            mStripTabEventHandler.sendEmptyMessageAtTime(MESSAGE_RESIZE, RESIZE_DELAY_MS);
        }
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
            @NonNull Set<Integer> tabIdsToBeUpdated, boolean hasUpdate) {
        boolean updateForCollapsedGroup = false;
        boolean showIph =
                mTabStripIphController.wouldTriggerIph(IphType.GROUP_TITLE_NOTIFICATION_BUBBLE);

        for (int tabId : tabIdsToBeUpdated) {
            Tab tab = getTabById(tabId);
            final StripLayoutTab stripTab = findTabById(tabId);

            // Skip invalid tabs or selected tabs when showing updates.
            if (tab == null || stripTab == null || (isSelectedTab(tabId) && hasUpdate)) continue;

            int rootId = tab.getRootId();
            final StripLayoutGroupTitle groupTitle = findGroupTitle(rootId);

            // Show bubble and iph on group title if collapsed, otherwise show iph on the updated
            // tab.
            if (groupTitle != null && groupTitle.isCollapsed() && !updateForCollapsedGroup) {
                groupTitle.setNotificationBubbleShown(hasUpdate);
                updateGroupTextAndSharedState(rootId);
                if (hasUpdate && showIph) {
                    mQueuedIphList.add(
                            () ->
                                    attemptToShowTabStripIph(
                                            groupTitle,
                                            /* tab= */ null,
                                            IphType.GROUP_TITLE_NOTIFICATION_BUBBLE));
                }
                updateForCollapsedGroup = true;
            } else if (groupTitle != null && !groupTitle.isCollapsed()) {
                if (hasUpdate && showIph) {
                    mQueuedIphList.add(
                            () ->
                                    attemptToShowTabStripIph(
                                            groupTitle, stripTab, IphType.TAB_NOTIFICATION_BUBBLE));
                }
            }
            // Update tab bubble and the related accessibility description.
            stripTab.setNotificationBubbleShown(hasUpdate);
            setAccessibilityDescription(stripTab, tab);
            mLayerTitleCache.updateTabBubble(tabId, hasUpdate);
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
                case MESSAGE_RESIZE:
                    finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
                    computeAndUpdateTabWidth(
                            /* animate= */ true,
                            /* deferAnimations= */ false,
                            /* closedTab= */ null);
                    mUpdateHost.requestUpdate();
                    break;
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
        // 1. Bring the anchor tab to the foreground.
        int tabIndex = TabModelUtils.getTabIndexById(mModel, anchorTab.getTabId());
        TabModelUtils.setIndex(mModel, tabIndex);

        // 2. Anchor the popupMenu to the view associated with the tab
        View tabView = TabModelUtils.getCurrentTab(mModel).getView();
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
    private void bringViewToVisibleArea(StripLayoutView view, long time, boolean animate) {
        if (mWidth == 0) return;
        float delta = calculateDeltaToMakeViewVisible(view);
        setScrollForScrollingTabStacker(delta, /* isDeltaHorizontal= */ true, animate, time);
    }

    private boolean isViewCompletelyVisible(StripLayoutView view) {
        float leftBound = getVisibleLeftBound() + mLeftFadeWidth;
        float rightBound = getVisibleRightBound() - mRightFadeWidth;
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
        return !view.isVisible() || isViewCompletelyHiddenAt(view.getDrawX(), view.getWidth());
    }

    /**
     * Determines whether a view will be completely outside of the visible area of the tab strip
     * once it reaches its ideal position.
     *
     * @param view The {@link StripLayoutView} whose visibility will be determined.
     * @return {@code true} if the view will be completely hidden, {@code false} otherwise.
     */
    private boolean willViewBeCompletelyHidden(StripLayoutView view) {
        return isViewCompletelyHiddenAt(view.getIdealX(), view.getWidth());
    }

    private boolean isViewCompletelyHiddenAt(float viewX, float viewWidth) {
        // Check if the tab is outside the visible bounds to the left...
        return viewX + viewWidth <= getVisibleLeftBound() + mLeftFadeWidth
                // ... or to the right.
                || viewX >= getVisibleRightBound() - mRightFadeWidth;
    }

    /**
     * To prevent accidental tab closures, when the close button of a tab is very close to the edge
     * of the tab strip, we hide the close button. The threshold for hiding is different based on
     * the length of the fade at the end of the strip.
     *
     * @param start Whether its the start of the tab strip.
     * @return The distance threshold from the edge of the tab strip to hide the close button.
     */
    private float getCloseBtnVisibilityThreshold(boolean start) {
        if (start) {
            // TODO(zheliooo): Add unit tests to cover start tab cases for testTabSelected in
            // StripLayoutHelperTest.
            return CLOSE_BTN_VISIBILITY_THRESHOLD_START;
        } else {
            return LocalizationUtils.isLayoutRtl() ? mLeftFadeWidth : mRightFadeWidth;
        }
    }

    /**
     * @return true if the close button menu is showing
     */
    public boolean isCloseButtonMenuShowingForTesting() {
        return mCloseButtonMenu.isShowing();
    }

    /**
     * @param menuItemId The id of the menu item to click
     */
    public void clickCloseButtonMenuItemForTesting(int menuItemId) {
        mCloseButtonMenu.performItemClick(menuItemId);
    }

    /**
     * @return The width of the tab strip.
     */
    float getWidthForTesting() {
        return mWidth;
    }

    /**
     * @return The width of a tab.
     */
    float getCachedTabWidthForTesting() {
        return mCachedTabWidthSupplier.get();
    }

    /**
     * @return The strip's scroll offset limit (a 1-D vector along the X axis, under the dynamic
     *     coordinate system used by {@link ScrollDelegate}).
     */
    float getScrollOffsetLimitForTesting() {
        return mScrollDelegate.getScrollOffsetLimitForTesting(); // IN-TEST
    }

    /**
     * @return The scroller.
     */
    StackScroller getScrollerForTesting() {
        return mScrollDelegate.getScrollerForTesting(); // IN-TEST
    }

    /**
     * @return An array containing the StripLayoutTabs.
     */
    StripLayoutTab[] getStripLayoutTabsForTesting() {
        return mStripTabs;
    }

    /** Set the value of mStripTabs for testing */
    void setStripLayoutTabsForTesting(StripLayoutTab[] stripTabs) {
        this.mStripTabs = stripTabs;
    }

    /**
     * @return An array containing the StripLayoutViews.
     */
    StripLayoutView[] getStripLayoutViewsForTesting() {
        return mStripViews;
    }

    /**
     * @return The currently interacting tab.
     */
    StripLayoutTab getInteractingTabForTesting() {
        return mReorderDelegate.getInteractingTabForTesting(); // IN-TEST
    }

    /**
     * @return The view that we'll delay enter reorder mode for.
     */
    StripLayoutView getDelayedReorderViewForTesting() {
        return mDelayedReorderView;
    }

    Animator getRunningAnimatorForTesting() {
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

    private void setAccessibilityDescription(StripLayoutTab stripTab, Tab tab) {
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
            StripLayoutTab stripTab, @Nullable String title, boolean isHidden) {
        if (stripTab == null) return;

        @StringRes int resId;
        if (mIncognito) {
            resId =
                    isHidden
                            ? R.string.accessibility_tabstrip_tab_incognito
                            : R.string.accessibility_tabstrip_tab_incognito_selected;
        } else if (isHidden) {
            resId =
                    stripTab.getNotificationBubbleShown()
                            ? R.string.accessibility_tabstrip_tab_notification
                            : R.string.accessibility_tabstrip_tab;
        } else {
            resId = R.string.accessibility_tabstrip_tab_selected;
        }

        if (!stripTab.needsAccessibilityDescriptionUpdate(title, resId)) {
            // The resulting accessibility description would be the same as the current description,
            // so skip updating it to avoid having to read resources unnecessarily.
            return;
        }

        final String description = mContext.getString(resId, title);
        stripTab.setAccessibilityDescription(description, title, resId);
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
            float startX = StripLayoutUtils.adjustXForTabDrop(currX, mCachedTabWidthSupplier);

            // 3. Mark the "interacting" view. This is not the DnD dragged view, but rather the view
            // in the strip that is currently being hovered by the DnD drag.
            StripLayoutView hoveredView =
                    getViewAtPositionX(startX, /* includeGroupTitles= */ true);
            if (hoveredView == null) hoveredView = mStripViews[mStripViews.length - 1];

            // 4. Start reorder - prepare strip to indicate drop target.
            startReorderMode(startX, /* y= */ 0.f, hoveredView, ReorderType.DRAG_ONTO_STRIP);
        }
    }

    public void handleDragWithin(
            long time, float x, float y, float deltaX, boolean draggedTabIncognito) {
        if (mIncognito == draggedTabIncognito) {
            drag(time, x, y, deltaX);
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
        if (mergeToGroup && isCollapsed) {
            // Selects the first tab in the collapsed group. For expanded groups, the correct tab
            // should be selected during tab creation.
            TabModelUtils.setIndex(mModel, index);
            finishAnimationsAndCloseDyingTabs(/* allowUndo= */ true);
            computeAndUpdateTabWidth(
                    /* animate= */ true, /* deferAnimations= */ false, /* closedTab= */ null);
        }
    }

    public void stopReorderMode() {
        if (mReorderDelegate.getInReorderMode()) {
            mReorderDelegate.stopReorderMode(mStripViews, mStripGroupTitles);
        }
    }

    public int getTabIndexForTabDrop(float x) {
        for (int i = 0; i < mStripViews.length; i++) {
            final StripLayoutView stripView = mStripViews[i];
            final float leftEdge;
            final float rightEdge;
            boolean rtl = LocalizationUtils.isLayoutRtl();
            if (stripView instanceof StripLayoutTab tab) {
                if (tab.isCollapsed()) continue;
                final float halfTabWidth = mCachedTabWidthSupplier.get() / 2;
                leftEdge = tab.getTouchTargetLeft();
                rightEdge = tab.getTouchTargetRight();

                boolean hasReachedThreshold =
                        rtl ? x > rightEdge - halfTabWidth : x < leftEdge + halfTabWidth;
                if (hasReachedThreshold) {
                    return StripLayoutUtils.findIndexForTab(mStripTabs, tab.getTabId());
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
                    return StripLayoutUtils.findIndexForTab(
                            mStripTabs, ((StripLayoutTab) mStripViews[i + 1]).getTabId());
                }
            }
        }
        return mStripTabs.length;
    }

    private boolean isViewDraggingInProgress() {
        return mTabDragSource != null && mTabDragSource.isViewDraggingInProgress();
    }

    private void onWillCloseView(StripLayoutView view) {
        if (view == null) return;

        view.setWillClose();
        if (view == mDelayedReorderView) resetDelayedReorderState();
        if (view == mReorderDelegate.getInteractingView()) stopReorderMode();
    }

    private void resetDelayedReorderState() {
        mDelayedReorderView = null;
        mDelayedReorderInitialX = 0.f;
    }

    private void sendMoveWindowBroadcast(View view, float startXInView, float startYInView) {
        if (!XrUtils.isXrDevice()) return;
        if (mWindowAndroid.getActivity().get() == null) return;

        // The start position is in the view coordinate system and related to the top left position
        // of the toolbar container view. Convert it to the screen coordinate system for window drag
        // start position.
        int[] topLeftLocation = new int[2];
        view.getLocationOnScreen(topLeftLocation);
        float startXInScreen = topLeftLocation[0] + startXInView;
        float startYInScreen = topLeftLocation[1] + startYInView;

        int taskId = ApplicationStatus.getTaskId(mWindowAndroid.getActivity().get());

        // Prepare the move window intent for the Android system to initiate move and take over the
        // user input events. The intent is ignored when not handled with no impact to existing
        // Android platforms.
        Intent intent = new Intent();
        intent.setPackage(view.getContext().getPackageName());
        intent.setAction("com.android.systemui.MOVE_WINDOW");
        intent.putExtra("MOVE_WINDOW_TASK_ID", taskId);
        intent.putExtra("MOVE_WINDOW_START_X", startXInScreen);
        intent.putExtra("MOVE_WINDOW_START_Y", startYInScreen);
        mWindowAndroid.sendBroadcast(intent);
    }

    void startDragAndDropTabForTesting(
            @NonNull StripLayoutTab clickedTab, @NonNull PointF dragStartPointF) {
        startReorderMode(
                dragStartPointF.x, dragStartPointF.y, clickedTab, ReorderType.START_DRAG_DROP);
    }
}
