// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.PointF;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.FloatProperty;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.ListPopupWindow;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.CompositorOnClickHandler;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.StackScroller;
import org.chromium.chrome.browser.compositor.overlays.strip.TabLoadTracker.TabLoadTrackerCallback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.util.ColorUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * This class handles managing the positions and behavior of all tabs in a tab strip.  It is
 * responsible for both responding to UI input events and model change notifications, adjusting and
 * animating the tab strip as required.
 *
 * <p>
 * The stacking and visual behavior is driven by setting a {@link StripStacker}.
 */
public class StripLayoutHelper implements StripLayoutTab.StripLayoutTabDelegate {
    /** A property for animations to use for changing the X offset of the tab. */
    public static final FloatProperty<StripLayoutHelper> SCROLL_OFFSET =
            new FloatProperty<StripLayoutHelper>("scrollOffset") {
                @Override
                public void setValue(StripLayoutHelper object, float value) {
                    object.setScrollOffset(value);
                }

                @Override
                public Float get(StripLayoutHelper object) {
                    return object.getScrollOffset();
                }
            };

    // Drag Constants
    private static final int REORDER_SCROLL_NONE = 0;
    private static final int REORDER_SCROLL_LEFT = 1;
    private static final int REORDER_SCROLL_RIGHT = 2;

    // Behavior Constants
    private static final float EPSILON = 0.001f;
    private static final float REORDER_OVERLAP_SWITCH_PERCENTAGE = 0.53f;
    private static final float DROP_INTO_GROUP_MAX_OFFSET = 36.f;

    // Animation/Timer Constants
    private static final int RESIZE_DELAY_MS = 1500;
    private static final int SPINNER_UPDATE_DELAY_MS = 66;
    // Degrees per millisecond.
    private static final float SPINNER_DPMS = 0.33f;
    private static final int SCROLL_DURATION_MS = 250;
    private static final int SCROLL_DURATION_MS_MEDIUM = 350;
    private static final int SCROLL_DURATION_MS_LONG = 450;
    private static final int ANIM_FOLIO_DETACH_MS = 75;
    private static final int ANIM_TAB_CREATED_MS = 150;
    private static final int ANIM_TAB_CLOSED_MS = 150;
    private static final int ANIM_TAB_RESIZE_MS = 250;
    private static final int ANIM_TAB_DRAW_X_MS = 250;
    private static final int ANIM_TAB_SELECTION_DELAY = 150;
    private static final int ANIM_TAB_MOVE_MS = 125;
    private static final int ANIM_TAB_SLIDE_OUT_MS = 250;
    private static final int ANIM_BUTTONS_FADE_MS = 150;
    private static final int NEW_TAB_BUTTON_OFFSET_MOVE_MS = 250;
    private static final int SCROLL_DISTANCE_SHORT = 960;
    private static final int SCROLL_DISTANCE_MEDIUM = 1920;
    private static final long INVALID_TIME = 0L;
    private static final int ANIM_HOVERED_TAB_CONTAINER_FADE_MS = 200;
    static final long DROP_INTO_GROUP_MS = 300L;

    // Visibility Constants
    private static final float TAB_OVERLAP_WIDTH_LARGE_DP = 28.f;
    private static final float TAB_WIDTH_MEDIUM = 156.f;
    private static final float REORDER_EDGE_SCROLL_MAX_SPEED_DP = 1000.f;
    private static final float REORDER_EDGE_SCROLL_START_MIN_DP = 87.4f;
    private static final float REORDER_EDGE_SCROLL_START_MAX_DP = 18.4f;
    private static final float NEW_TAB_BUTTON_BACKGROUND_Y_OFFSET_DP = 3.f;
    private static final float NEW_TAB_BUTTON_CLICK_SLOP_DP = 8.f;
    private static final float NEW_TAB_BUTTON_BACKGROUND_WIDTH_DP = 32.f;
    private static final float NEW_TAB_BUTTON_BACKGROUND_HEIGHT_DP = 32.f;
    @VisibleForTesting static final float FOLIO_ATTACHED_BOTTOM_MARGIN_DP = 0.f;
    private static final float FOLIO_ANIM_INTERMEDIATE_MARGIN_DP = -12.f;
    @VisibleForTesting static final float FOLIO_DETACHED_BOTTOM_MARGIN_DP = 4.f;
    private static final float BUTTON_DESIRED_TOUCH_TARGET_SIZE = 48.f;

    // Desired spacing between new tab button and tabs when tab strip is not full.
    private static final float NEW_TAB_BUTTON_X_OFFSET_TOWARDS_TABS = 4.f;
    private static final float DESIRED_PADDING_BETWEEN_NEW_TAB_BUTTON_AND_TABS = 2.f;

    private static final float NEW_TAB_BUTTON_DEFAULT_PRESSED_OPACITY = 0.2f;
    private static final float NEW_TAB_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY = 0.12f;
    private static final float NEW_TAB_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY = 0.08f;
    static final float TAB_OPACITY_HIDDEN = 0.f;
    static final float TAB_OPACITY_VISIBLE_BACKGROUND = 0.55f;
    static final float TAB_OPACITY_VISIBLE_FOREGROUND = 1.f;
    static final float FADE_FULL_OPACITY_THRESHOLD_DP = 24.f;
    private static final float TAB_STRIP_TAB_WIDTH = 108.f;
    private static final float NEW_TAB_BUTTON_WITH_MODEL_SELECTOR_BUTTON_PADDING = 8.f;

    private static final int MESSAGE_RESIZE = 1;
    private static final int MESSAGE_UPDATE_SPINNER = 2;
    private static final float CLOSE_BTN_VISIBILITY_THRESHOLD_START = 96.f;
    private static final long TAB_SWITCH_METRICS_MAX_ALLOWED_SCROLL_INTERVAL =
            DateUtils.MINUTE_IN_MILLIS;

    // Histogram Constants
    private static final String PLACEHOLDER_LEFTOVER_TABS_HISTOGRAM_NAME =
            "Android.TabStrip.PlaceholderStripLeftoverTabsCount";
    private static final String PLACEHOLDER_TABS_CREATED_DURING_RESTORE_HISTOGRAM_NAME =
            "Android.TabStrip.PlaceholderStripTabsCreatedDuringRestoreCount";
    private static final String PLACEHOLDER_TABS_NEEDED_DURING_RESTORE_HISTOGRAM_NAME =
            "Android.TabStrip.PlaceholderStripTabsNeededDuringRestoreCount";
    private static final String PLACEHOLDER_VISIBLE_DURATION_HISTOGRAM_NAME =
            "Android.TabStrip.PlaceholderStripVisibleDuration";

    // External influences
    private final LayoutUpdateHost mUpdateHost;
    private final LayoutRenderHost mRenderHost;
    private final LayoutManagerHost mManagerHost;
    private final WindowAndroid mWindowAndroid;
    private TabModel mModel;
    private TabGroupModelFilter mTabGroupModelFilter;
    private TabCreator mTabCreator;
    private StripStacker mStripStacker = new ScrollingStripStacker();

    // Internal State
    private StripLayoutTab[] mStripTabs = new StripLayoutTab[0];
    private StripLayoutTab[] mStripTabsVisuallyOrdered = new StripLayoutTab[0];
    private StripLayoutTab[] mStripTabsToRender = new StripLayoutTab[0];
    private StripLayoutTab mTabAtPositionForTesting;
    private final StripTabEventHandler mStripTabEventHandler = new StripTabEventHandler();
    private final TabLoadTrackerCallback mTabLoadTrackerHost = new TabLoadTrackerCallbackImpl();

    // Common state used for animations on the strip triggered by independent actions including and
    // not limited to tab closure, tab creation/selection, and tab reordering. Not intended to be
    // used for hover actions. Consider using setAndStartRunningAnimator() to set and start this
    // animator.
    private Animator mRunningAnimator;

    private final TintedCompositorButton mNewTabButton;
    private final CompositorButton mModelSelectorButton;

    // Layout Constants
    private final float mTabOverlapWidth;
    private final float mNewTabButtonWidth;
    private final float mMinTabWidth;
    private final float mMaxTabWidth;
    private final ListPopupWindow mTabMenu;

    // Strip State
    private StackScroller mScroller;
    private float mScrollOffset;
    private float mMinScrollOffset;
    private float mCachedTabWidth;

    // Reorder State
    private int mReorderState = REORDER_SCROLL_NONE;
    private boolean mInReorderMode;
    private float mLastReorderX;
    private float mTabMarginWidth;
    private float mStripStartMarginForReorder;
    private float mReorderExtraMinScrollOffset;
    private long mLastReorderScrollTime;
    private long mLastUpdateTime;
    private long mHoverStartTime;
    private float mHoverStartOffset;
    private boolean mHoveringOverGroup;

    // Tab switch efficiency
    private Long mTabScrollStartTime;
    private Long mMostRecentTabScroll;

    // UI State
    private StripLayoutTab mInteractingTab;
    private CompositorButton mLastPressedCloseButton;
    private float mWidth;
    private float mHeight;
    private long mLastSpinnerUpdate;
    private float mLeftMargin;
    private float mRightMargin;
    private float mLeftFadeWidth;
    private float mRightFadeWidth;

    // New tab button with tab strip end padding
    private float mTabStripEndPadding;
    // 3-dots menu button with tab strip end padding
    private final boolean mIncognito;
    private boolean mIsFirstLayoutPass;
    private boolean mAnimationsDisabledForTesting;
    // Whether tab strip scrolling is in progress
    private boolean mIsStripScrollInProgress;

    // Tab menu item IDs
    public static final int ID_CLOSE_ALL_TABS = 0;

    private Context mContext;

    // Animation states. True while the relevant animations are running, and false otherwise.
    private boolean mMultiStepTabCloseAnimRunning;
    private boolean mTabGroupMarginAnimRunning;
    private boolean mTabCreating;

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

    // Tab Drag and Drop state to hold clicked tab being dragged.
    private View mToolbarContainerView;
    @Nullable private final TabDragSource mTabDragSource;
    private StripLayoutTab mActiveClickedTab;

    // Tab Drag and Drop state to track if the dragged tab has been "torn" off of the tab strip.
    private boolean mDraggedTabOffStrip;
    private float mLastOffsetX;

    // Tab hover state.
    private StripLayoutTab mLastHoveredTab;
    private StripTabHoverCardView mTabHoverCardView;

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
            @NonNull WindowAndroid windowAndroid) {
        mTabOverlapWidth = TAB_OVERLAP_WIDTH_LARGE_DP;
        mNewTabButtonWidth = NEW_TAB_BUTTON_BACKGROUND_WIDTH_DP;
        mModelSelectorButton = modelSelectorButton;
        mToolbarContainerView = toolbarContainerView;
        mTabDragSource = tabDragSource;
        mWindowAndroid = windowAndroid;

        // Use toolbar menu button padding to align NTB with menu button.
        mTabStripEndPadding =
                context.getResources().getDimension(R.dimen.button_end_padding)
                        / context.getResources().getDisplayMetrics().density;

        mRightMargin =
                LocalizationUtils.isLayoutRtl() ? 0 : mTabStripEndPadding + mNewTabButtonWidth;
        mLeftMargin =
                LocalizationUtils.isLayoutRtl() ? mTabStripEndPadding + mNewTabButtonWidth : 0;

        mMinTabWidth = TAB_STRIP_TAB_WIDTH;

        mMaxTabWidth = TabUiThemeUtil.getMaxTabStripTabWidthDp();
        mManagerHost = managerHost;
        mUpdateHost = updateHost;
        mRenderHost = renderHost;
        CompositorOnClickHandler newTabClickHandler =
                new CompositorOnClickHandler() {
                    @Override
                    public void onClick(long time) {
                        handleNewTabClick();
                    }
                };

        // Set new tab button background resource.
        mNewTabButton =
                new TintedCompositorButton(
                        context,
                        NEW_TAB_BUTTON_BACKGROUND_WIDTH_DP,
                        NEW_TAB_BUTTON_BACKGROUND_HEIGHT_DP,
                        newTabClickHandler,
                        R.drawable.ic_new_tab_button);
        mNewTabButton.setBackgroundResourceId(R.drawable.bg_circle_tab_strip_button);

        int apsBackgroundHoveredTint =
                ColorUtils.setAlphaComponent(
                        SemanticColorUtils.getDefaultTextColor(context),
                        (int) (NEW_TAB_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY * 255));
        int apsBackgroundPressedTint =
                ColorUtils.setAlphaComponent(
                        SemanticColorUtils.getDefaultTextColor(context),
                        (int) (NEW_TAB_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY * 255));

        int apsBackgroundIncognitoHoveredTint =
                ColorUtils.setAlphaComponent(
                        context.getResources().getColor(R.color.tab_strip_button_hover_bg_color),
                        (int) (NEW_TAB_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY * 255));
        int apsBackgroundIncognitoPressedTint =
                ColorUtils.setAlphaComponent(
                        context.getResources().getColor(R.color.tab_strip_button_hover_bg_color),
                        (int) (NEW_TAB_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY * 255));

        // Primary container for default bg color.
        int BackgroundDefaultTint = TabUiThemeProvider.getDefaultNTBContainerColor(context);

        // Primary @ 20% for default pressed bg color.
        int BackgroundPressedTint =
                ColorUtils.setAlphaComponent(
                        SemanticColorUtils.getDefaultIconColorAccent1(context),
                        (int) (NEW_TAB_BUTTON_DEFAULT_PRESSED_OPACITY * 255));

        // Surface-2 baseline for incognito bg color.
        int BackgroundIncognitoDefaultTint =
                context.getResources().getColor(R.color.default_bg_color_dark_elev_2_baseline);

        // Surface-5 baseline for incognito pressed bg color
        int BackgroundIncognitoPressedTint =
                context.getResources().getColor(R.color.default_bg_color_dark_elev_5_baseline);

        // Tab strip redesign new tab button night mode bg color.
        if (ColorUtils.inNightMode(context)) {
            // Surface-1 for night mode bg color.
            BackgroundDefaultTint =
                    ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_1);

            // Surface 5 for pressed night mode bg color.
            BackgroundPressedTint =
                    ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_5);
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
        mNewTabButton.setY(NEW_TAB_BUTTON_BACKGROUND_Y_OFFSET_DP);

        mNewTabButton.setIncognito(incognito);
        mNewTabButton.setClickSlop(NEW_TAB_BUTTON_CLICK_SLOP_DP);
        Resources res = context.getResources();
        mNewTabButton.setAccessibilityDescription(
                res.getString(R.string.accessibility_toolbar_btn_new_tab),
                res.getString(R.string.accessibility_toolbar_btn_new_incognito_tab));
        mContext = context;
        mIncognito = incognito;

        // Create tab menu
        mTabMenu = new ListPopupWindow(mContext);
        mTabMenu.setAdapter(
                new ArrayAdapter<String>(
                        mContext,
                        android.R.layout.simple_list_item_1,
                        new String[] {
                            mContext.getString(
                                    !mIncognito
                                            ? R.string.menu_close_all_tabs
                                            : R.string.menu_close_all_incognito_tabs)
                        }));
        mTabMenu.setOnItemClickListener(
                new OnItemClickListener() {
                    @Override
                    public void onItemClick(
                            AdapterView<?> parent, View view, int position, long id) {
                        mTabMenu.dismiss();
                        if (position == ID_CLOSE_ALL_TABS) {
                            mModel.closeAllTabs(false);
                            RecordUserAction.record("MobileToolbarCloseAllTabs");
                        }
                    }
                });

        int menuWidth = mContext.getResources().getDimensionPixelSize(R.dimen.menu_width);
        mTabMenu.setWidth(menuWidth);
        mTabMenu.setModal(true);
        mIsFirstLayoutPass = true;
    }

    /** Cleans up internal state. */
    public void destroy() {
        mStripTabEventHandler.removeCallbacksAndMessages(null);
        if (mTabHoverCardView != null) {
            mTabHoverCardView.destroy();
            mTabHoverCardView = null;
        }
    }

    /**
     * Get a list of virtual views for accessibility.
     *
     * @param views A List to populate with virtual views.
     */
    public void getVirtualViews(List<VirtualView> views) {
        for (int i = 0; i < mStripTabs.length; i++) {
            StripLayoutTab tab = mStripTabs[i];
            tab.getVirtualViews(views);
        }
        if (mNewTabButton.isVisible()) views.add(mNewTabButton);
    }

    /**
     * @return The visually ordered list of visible {@link StripLayoutTab}s.
     */
    public StripLayoutTab[] getStripLayoutTabsToRender() {
        return mStripTabsToRender;
    }

    /**
     * @return A {@link TintedCompositorButton} that represents the positioning of the new tab
     *         button.
     */
    public TintedCompositorButton getNewTabButton() {
        return mNewTabButton;
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
    protected boolean isTabStripFull() {
        return mCachedTabWidth < TabUiThemeUtil.getMaxTabStripTabWidthDp();
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
     * When the {@link ScrollingStripStacker} is being used, a fade is shown at the left and
     * right edges to indicate there is tab strip content off screen. As the scroll position
     * approaches the edge of the screen, the fade opacity is lowered.
     *
     * @param isLeft Whether the opacity for the left or right side should be returned.
     * @return The opacity to use for the fade.
     */
    private float getFadeOpacity(boolean isLeft) {
        // In RTL, scroll position 0 is on the right side of the screen, whereas in LTR scroll
        // position 0 is on the left. Account for that in the offset calculation.
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        boolean useUnadjustedScrollOffset = isRtl != isLeft;
        float offset =
                -(useUnadjustedScrollOffset ? mScrollOffset : (mMinScrollOffset - mScrollOffset));

        if (offset <= 0.f) {
            return 0.f;
        } else if (offset >= FADE_FULL_OPACITY_THRESHOLD_DP) {
            return 1.f;
        } else {
            return offset / FADE_FULL_OPACITY_THRESHOLD_DP;
        }
    }

    /**
     * Set the scroll offset.
     * @param scrollOffset The scroll offset.
     */
    void setScrollOffset(float scrollOffset) {
        mScrollOffset = scrollOffset;
    }

    /**
     * @return The strip's current scroll offset.
     */
    float getScrollOffset() {
        return mScrollOffset;
    }

    /**
     * @param margin The distance between the last tab and the edge of the screen.
     * @param isMsbVisible The boolean to determine whether model selector button is visible.
     */
    public void setEndMargin(float margin, boolean isMsbVisible) {
        // When MSB is not visible we add strip end padding here. When MSB is visible strip end
        // padding will be included in MSB margin, so just add padding between NTB and MSB here.
        if (LocalizationUtils.isLayoutRtl()) {
            mLeftMargin =
                    margin
                            + mNewTabButtonWidth
                            + (isMsbVisible
                                    ? NEW_TAB_BUTTON_WITH_MODEL_SELECTOR_BUTTON_PADDING
                                    : mTabStripEndPadding);
        } else {
            mRightMargin =
                    margin
                            + mNewTabButtonWidth
                            + (isMsbVisible
                                    ? NEW_TAB_BUTTON_WITH_MODEL_SELECTOR_BUTTON_PADDING
                                    : mTabStripEndPadding);
        }
        computeAndUpdateTabWidth(false, false);
    }

    /**
     * Sets the left fade width based on which fade is showing.
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
     * @param width  The new available width.
     * @param height The new height this stack should be.
     * @param orientationChanged Whether the screen orientation was changed.
     * @param time The current time of the app in ms.
     */
    public void onSizeChanged(float width, float height, boolean orientationChanged, long time) {
        if (mWidth == width && mHeight == height) return;

        StripLayoutTab selectedTab = getSelectedStripTab();
        boolean wasSelectedTabVisible = selectedTab != null && selectedTab.isVisible();
        boolean widthChanged = mWidth != width;

        mWidth = width;
        mHeight = height;

        for (int i = 0; i < mStripTabs.length; i++) {
            mStripTabs[i].setHeight(mHeight);
        }

        if (widthChanged) {
            computeAndUpdateTabWidth(false, false);
        }
        if (mStripTabs.length > 0) mUpdateHost.requestUpdate();

        // Dismiss tab menu, similar to how the app menu is dismissed on orientation change
        mTabMenu.dismiss();

        if ((orientationChanged && wasSelectedTabVisible) || !mTabStateInitialized) {
            bringSelectedTabToVisibleArea(time, mTabStateInitialized);
        }
    }

    /**
     * Updates all internal resources and dimensions.
     * @param context The current Android {@link Context}.
     */
    public void onContextChanged(Context context) {
        mScroller = new StackScroller(context);
        mContext = context;
    }

    /**
     * Notify the a title has changed.
     *
     * @param tabId     The id of the tab that has changed.
     * @param title     The new title.
     */
    public void tabTitleChanged(int tabId, String title) {
        Tab tab = getTabById(tabId);
        if (tab != null) setAccessibilityDescription(findTabById(tabId), title, tab.isHidden());
    }

    /**
     * Sets the {@link TabModel} that this {@link StripLayoutHelper} will visually represent.
     * @param model The {@link TabModel} to visually represent.
     * @param tabCreator The {@link TabCreator}, used to create new tabs.
     * @param tabStateInitialized Whether the tab model's tab state is fully initialized after
     *                            startup or not.
     */
    public void setTabModel(TabModel model, TabCreator tabCreator, boolean tabStateInitialized) {
        if (mModel == model) return;
        mModel = model;
        mTabCreator = tabCreator;
        mTabStateInitialized = tabStateInitialized;

        // If the tabs are still restoring and the refactoring experiment is enabled, we'll create a
        // placeholder strip. This means we don't need to call computeAndUpdateTabOrders() to
        // generate "real" strip tabs.
        if (!mTabStateInitialized && ChromeFeatureList.sTabStripStartupRefactoring.isEnabled()) {
            // If the placeholder strip is ready, replace the matching placeholders for the tabs
            // that have already been restored.
            mSelectedOnStartup = mModel.isActiveModel();
            if (mPlaceholderStripReady) replacePlaceholdersForRestoredTabs();
        } else {
            RecordHistogram.recordMediumTimesHistogram(
                    PLACEHOLDER_VISIBLE_DURATION_HISTOGRAM_NAME, 0L);

            computeAndUpdateTabOrders(false, false);
        }
    }

    /** Called to notify that the tab state has been initialized. */
    protected void onTabStateInitialized() {
        mTabStateInitialized = true;

        if (ChromeFeatureList.sTabStripStartupRefactoring.isEnabled() && mPlaceholderStripReady) {
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
            RecordHistogram.recordMediumTimesHistogram(
                    PLACEHOLDER_VISIBLE_DURATION_HISTOGRAM_NAME,
                    SystemClock.uptimeMillis() - mPlaceholderCreationTime);
        }

        // Recreate the StripLayoutTabs from the TabModel, now that all of the real Tabs have been
        // restored. This will reuse valid tabs, discard invalid tabs, and correct tab orders.
        computeAndUpdateTabOrders(false, false);
    }

    /**
     * Sets the {@link TabGroupModelFilter} that will access the internal tab group state.
     * @param tabGroupModelFilter The {@link TabGroupModelFilter}.
     */
    public void setTabGroupModelFilter(TabGroupModelFilter tabGroupModelFilter) {
        mTabGroupModelFilter = tabGroupModelFilter;
    }

    /**
     * Helper-specific updates. Cascades the values updated by the animations and flings.
     * @param time The current time of the app in ms.
     * @return     Whether or not animations are done.
     */
    public boolean updateLayout(long time) {
        mLastUpdateTime = time;

        // 1. Handle any Scroller movements (flings).
        updateScrollOffset(time);

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

        return doneAnimating;
    }

    /**
     * Called when a new tab model is selected.
     * @param selected If the new tab model selected is the model that this strip helper associated
     * with.
     */
    public void tabModelSelected(boolean selected) {
        if (selected) {
            bringSelectedTabToVisibleArea(0, false);
        } else {
            mTabMenu.dismiss();
        }
    }

    /**
     * Called when a tab get selected.
     * @param time   The current time of the app in ms.
     * @param id     The id of the selected tab.
     * @param prevId The id of the previously selected tab.
     * @param skipAutoScroll Whether autoscroll to bring selected tab to view can be skipped.
     */
    public void tabSelected(long time, int id, int prevId, boolean skipAutoScroll) {
        StripLayoutTab stripTab = findTabById(id);
        if (stripTab == null) {
            tabCreated(time, id, prevId, true, false, false);
        } else {
            updateVisualTabOrdering();
            updateCloseButtons();

            if (!skipAutoScroll && !mInReorderMode) {
                // If the tab was selected through a method other than the user tapping on the
                // strip, it may not be currently visible. Scroll if necessary.
                bringSelectedTabToVisibleArea(time, true);
            }

            mUpdateHost.requestUpdate();

            setAccessibilityDescription(stripTab, getTabById(id));
            setAccessibilityDescription(findTabById(prevId), getTabById(prevId));
        }
    }

    /**
     * Called when a tab has been moved in the tabModel.
     * @param time     The current time of the app in ms.
     * @param id       The id of the Tab.
     * @param oldIndex The old index of the tab in the {@link TabModel}.
     * @param newIndex The new index of the tab in the {@link TabModel}.
     */
    public void tabMoved(long time, int id, int oldIndex, int newIndex) {
        reorderTab(id, oldIndex, newIndex, false);

        updateVisualTabOrdering();
        mUpdateHost.requestUpdate();
    }

    /**
     * Called when a tab is being closed. When called, the closing tab will not
     * be part of the model.
     * @param time The current time of the app in ms.
     * @param id   The id of the tab being closed.
     */
    public void tabClosed(long time, int id) {
        if (findTabById(id) == null) return;

        // 1. Find out if we're closing the last tab.  This determines if we resize immediately.
        // We know mStripTabs.length >= 1 because findTabById did not return null.
        boolean closingLastTab = mStripTabs[mStripTabs.length - 1].getId() == id;

        // 2. Rebuild the strip.
        computeAndUpdateTabOrders(!closingLastTab, false);

        mUpdateHost.requestUpdate();
    }

    /** Called when all tabs are closed at once. */
    public void willCloseAllTabs() {
        computeAndUpdateTabOrders(true, false);
        mUpdateHost.requestUpdate();
    }

    /**
     * Called when a tab close has been undone and the tab has been restored. This also re-selects
     * the last tab the user was on before the tab was closed.
     * @param time The current time of the app in ms.
     * @param id   The id of the Tab.
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
        if (!mTabStateInitialized && ChromeFeatureList.sTabStripStartupRefactoring.isEnabled()) {
            replaceNextPlaceholder(id, selected, onStartup);

            return;
        }

        // Otherwise, 2. Build any tabs that are missing.
        finishAnimationsAndPushTabUpdates();
        List<Animator> animationList = computeAndUpdateTabOrders(false, !onStartup);
        if (animationList == null) animationList = new ArrayList<>();

        // 3. Start an animation for the newly created tab.
        StripLayoutTab tab = findTabById(id);
        if (tab != null && !onStartup) {
            animationList.add(
                    CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            tab,
                            StripLayoutTab.Y_OFFSET,
                            tab.getHeight(),
                            0f,
                            ANIM_TAB_CREATED_MS));

            mTabCreating = true;
            startAnimationList(
                    animationList,
                    new AnimatorListenerAdapter() {
                        @Override
                        public void onAnimationEnd(Animator animation) {
                            mTabCreating = false;
                        }
                    });
        }

        // 4. If the new tab will be selected, scroll it to view. If the new tab will not be
        // selected, scroll the currently selected tab to view. Skip auto-scrolling if the tab is
        // being created due to a tab closure being undone.
        if (tab != null && !closureCancelled) {
            boolean animate = !onStartup && !mAnimationsDisabledForTesting;
            if (selected) {
                float delta = calculateDeltaToMakeTabVisible(tab);
                setScrollForScrollingTabStacker(delta, animate, time);
            } else {
                bringSelectedTabToVisibleArea(time, animate);
            }
        }

        mUpdateHost.requestUpdate();
    }

    /**
     * Set the relevant tab model metadata prior to the tab state initialization.
     * @param activeTabIndexOnStartup What the active tab index should be after tabs finish
     *                                restoring.
     * @param tabCountOnStartup What the tab count should be after tabs finish restoring.
     * @param createdTabOnStartup If an additional tab was created on startup (e.g. through intent).
     */
    protected void setTabModelStartupInfo(
            int tabCountOnStartup, int activeTabIndexOnStartup, boolean createdTabOnStartup) {
        if (!ChromeFeatureList.sTabStripStartupRefactoring.isEnabled()) return;

        mTabCountOnStartup = tabCountOnStartup;
        mActiveTabIndexOnStartup = activeTabIndexOnStartup;
        mCreatedTabOnStartup = createdTabOnStartup;

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
        if (mPlaceholderStripReady || mTabStateInitialized) return;

        // 1. Fill with placeholder tabs.
        mStripTabs = new StripLayoutTab[mTabCountOnStartup];
        for (int i = 0; i < mStripTabs.length; i++) {
            mStripTabs[i] = createPlaceholderStripTab();
        }

        // 2. Initialize the draw parameters.
        computeAndUpdateTabWidth(false, false);
        updateVisualTabOrdering();

        // 3. Scroll the strip to bring the selected tab to view and ensure that the active tab
        // container is visible.
        if (mActiveTabIndexOnStartup != Tab.INVALID_TAB_ID) {
            bringSelectedTabToVisibleArea(LayoutManagerImpl.time(), false);

            mStripTabs[mActiveTabIndexOnStartup].setContainerOpacity(
                    TAB_OPACITY_VISIBLE_FOREGROUND);
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
            // Should match the index in the model.
            replaceIndex = mCurrentPlaceholderIndex++;
            assert replaceIndex == mModel.indexOf(getTabById(id));
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
                    tab.getWidth() >= TAB_WIDTH_MEDIUM
                            || (tabSelected && shouldShowCloseButton(tab, i));
            mStripTabs[i].setCanShowCloseButton(canShowCloseButton, !mIsFirstLayoutPass);
        }
    }

    private void setTabContainerVisible(StripLayoutTab tab, boolean selected, boolean hovered) {
        // Don't interrupt a hovered tab container visibility animation, this will be handled in the
        // #onHover* methods.
        if (hovered) return;
        // Don't interrupt tab group background tab visibility.
        if (tab.getContainerOpacity() == TAB_OPACITY_VISIBLE_BACKGROUND) return;

        // The container will be visible if the tab is selected or is a placeholder tab.
        float containerOpacity =
                selected || tab.getIsPlaceholder()
                        ? TAB_OPACITY_VISIBLE_FOREGROUND
                        : TAB_OPACITY_HIDDEN;
        tab.setContainerOpacity(containerOpacity);
    }

    /**
     * Called to show/hide dividers and the foreground/hovered tab container. Dividers are only
     * necessary between tabs that both do not have a visible tab container (foreground or
     * background).
     */
    private void updateTabContainersAndDividers() {
        if (mStripTabs.length < 1) return;

        int selectedIndex = getSelectedStripTabIndex();
        int hoveredIndex =
                mLastHoveredTab != null
                        ? findIndexForTab(mLastHoveredTab.getId())
                        : TabModel.INVALID_TAB_INDEX;

        // Divider is never shown for the first tab.
        mStripTabs[0].setStartDividerVisible(false);
        setTabContainerVisible(mStripTabs[0], selectedIndex == 0, hoveredIndex == 0);
        // End divider for first tab is only shown in reorder mode when tab has trailing margin and
        // container is not visible.
        boolean endDividerVisible =
                mInReorderMode
                        && mStripTabs[0].getContainerOpacity() == TAB_OPACITY_HIDDEN
                        && mStripTabs[0].getTrailingMargin() > 0;
        mStripTabs[0].setEndDividerVisible(endDividerVisible);

        for (int i = 1; i < mStripTabs.length; i++) {
            final StripLayoutTab prevTab = mStripTabs[i - 1];
            final StripLayoutTab currTab = mStripTabs[i];
            boolean currTabSelected = selectedIndex == i;
            boolean currTabHovered = hoveredIndex == i;

            // Set container opacity.
            setTabContainerVisible(currTab, currTabSelected, currTabHovered);

            /**
             * Start divider should be visible when: 1. currTab is dragged off of the strip OR 2.
             * currTab container is hidden and (a) prevTab has trailing margin (ie: currTab is start
             * of group or an individual tab) OR (b) prevTab container is also hidden.
             */
            boolean currDraggedOffStrip = currTabSelected && mDraggedTabOffStrip;
            boolean currContainerHidden = currTab.getContainerOpacity() == TAB_OPACITY_HIDDEN;
            boolean prevContainerHidden = prevTab.getContainerOpacity() == TAB_OPACITY_HIDDEN;
            boolean prevTabHasMargin = prevTab.getTrailingMargin() > 0;
            boolean startDividerVisible =
                    currDraggedOffStrip
                            || (currContainerHidden && (prevContainerHidden || prevTabHasMargin));
            currTab.setStartDividerVisible(startDividerVisible);

            /**
             * End divider should be applied when: 1. currTab container is hidden and (a) currTab's
             * trailing margin > 0 (i.e. is last tab in group) OR (b) currTab is last tab in strip
             * (as the last tab does not have trailing margin)
             */
            boolean currTabHasMargin = currTab.getTrailingMargin() > 0;
            boolean currIsLastTab = i == (mStripTabs.length - 1);
            endDividerVisible = currContainerHidden && (currTabHasMargin || currIsLastTab);
            currTab.setEndDividerVisible(endDividerVisible);
        }
    }

    /**
     * Checks whether a tab at the edge of the strip is partially hidden, in which case the
     * close button will be hidden to avoid accidental clicks.
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
                        tab.getDrawX() + mTabOverlapWidth
                                < mNewTabButton.getX() + mNewTabButton.getWidth();
            } else {
                tabStartHidden =
                        tab.getDrawX() + mTabOverlapWidth < getCloseBtnVisibilityThreshold(false);
            }
            tabEndHidden = tab.getDrawX() > mWidth - getCloseBtnVisibilityThreshold(true);
        } else {
            tabStartHidden = tab.getDrawX() + tab.getWidth() < getCloseBtnVisibilityThreshold(true);
            if (isLastTab) {
                tabEndHidden =
                        tab.getDrawX() + tab.getWidth() - mTabOverlapWidth > mNewTabButton.getX();
            } else {
                tabEndHidden =
                        (tab.getDrawX() + tab.getWidth() - mTabOverlapWidth
                                > mWidth - getCloseBtnVisibilityThreshold(false));
            }
        }
        return !tabStartHidden && !tabEndHidden;
    }

    /**
     * Called when a tab has started loading.
     * @param id The id of the Tab.
     */
    public void tabPageLoadStarted(int id) {
        StripLayoutTab tab = findTabById(id);
        if (tab != null) tab.pageLoadingStarted();
    }

    /**
     * Called when a tab has finished loading.
     * @param id The id of the Tab.
     */
    public void tabPageLoadFinished(int id) {
        StripLayoutTab tab = findTabById(id);
        if (tab != null) tab.pageLoadingFinished();
    }

    /**
     * Called when a tab has started loading resources.
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
     * @param x The y coordinate of the end of the drag event.
     * @param y The y coordinate of the end of the drag event.
     * @param deltaX The number of pixels dragged in the x direction.
     */
    public void drag(long time, float x, float y, float deltaX) {
        resetResizeTimeout(false);

        mLastUpdateTime = time;
        deltaX = MathUtils.flipSignIf(deltaX, LocalizationUtils.isLayoutRtl());

        // 1. Reset the button state.
        mNewTabButton.drag(x, y);
        if (mLastPressedCloseButton != null) {
            if (!mLastPressedCloseButton.drag(x, y)) mLastPressedCloseButton = null;
        }

        if (mInReorderMode) {
            // 2.a. Handle reordering tabs.
            // This isn't the accumulated delta since the beginning of the drag.  It accumulates
            // the delta X until a threshold is crossed and then the event gets processed.
            float accumulatedDeltaX = x - mLastReorderX;

            if (Math.abs(accumulatedDeltaX) >= 1.f) {
                if (!LocalizationUtils.isLayoutRtl()) {
                    if (deltaX >= 1.f) {
                        mReorderState |= REORDER_SCROLL_RIGHT;
                    } else if (deltaX <= -1.f) {
                        mReorderState |= REORDER_SCROLL_LEFT;
                    }
                } else {
                    if (deltaX >= 1.f) {
                        mReorderState |= REORDER_SCROLL_LEFT;
                    } else if (deltaX <= -1.f) {
                        mReorderState |= REORDER_SCROLL_RIGHT;
                    }
                }

                mLastReorderX = x;
                updateReorderPosition(accumulatedDeltaX);
            }
        } else if (!mScroller.isFinished()) {
            // 2.b. Still scrolling, update the scroll destination here.
            mScroller.setFinalX((int) (mScroller.getFinalX() + deltaX));
        } else {
            // 2.c. Not scrolling.
            if (!mIsStripScrollInProgress) {
                mIsStripScrollInProgress = true;
                RecordUserAction.record("MobileToolbarSlideTabs");
                onStripScrollStart();
            }
            updateScrollOffsetPosition(mScrollOffset + deltaX);
        }

        // If we're scrolling at all we aren't interacting with any particular tab.
        // We already kicked off a fast expansion earlier if we needed one.  Reorder mode will
        // repopulate this if necessary.
        if (!mInReorderMode) mInteractingTab = null;
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

        velocityX = MathUtils.flipSignIf(velocityX, LocalizationUtils.isLayoutRtl());

        // 1. If we're currently in reorder mode, don't allow the user to fling.
        if (mInReorderMode) return;

        // 2. If we're fast scrolling, figure out the destination of the scroll so we can apply it
        // to the end of this fling.
        int scrollDeltaRemaining = 0;
        if (!mScroller.isFinished()) {
            scrollDeltaRemaining = mScroller.getFinalX() - Math.round(mScrollOffset);

            mInteractingTab = null;
            mScroller.forceFinished(true);
        }

        // 3. Kick off the fling.
        mScroller.fling(
                Math.round(mScrollOffset),
                0,
                (int) velocityX,
                0,
                (int) mMinScrollOffset,
                0,
                0,
                0,
                0,
                0,
                time);
        mScroller.setFinalX(mScroller.getFinalX() + scrollDeltaRemaining);
        mUpdateHost.requestUpdate();
    }

    /**
     * Called on onDown event.
     * @param time      The time stamp in millisecond of the event.
     * @param x         The x position of the event.
     * @param y         The y position of the event.
     * @param fromMouse Whether the event originates from a mouse.
     * @param buttons   State of all buttons that are pressed.
     */
    public void onDown(long time, float x, float y, boolean fromMouse, int buttons) {
        // Prepare for drag and drop beyond the StripLayout view, if needed.
        // The first onDown is passed by the Chrome pipeline directly by GestureHandler. The
        // subsequent ones may be simulated by the DragDrop handler if the pointer goes beyond the
        // strip layout view.
        mActiveClickedTab = null;
        mDraggedTabOffStrip = false;
        mLastOffsetX = 0.f;
        onDownInternal(time, x, y, fromMouse, buttons);
    }

    /**
     * Called when the Android Drag and Drop framework is initiated and takes over receiving the
     * pointer motion events. The tab being moved is already selected. A simulated down event is
     * sent to the @{link StripLayoutHelper#onInternalDown} when the user reenters the tabs layout
     * area so that it can continue with the local reordering of the tabs of the previously selected
     * tab. When a user leaves the tab strip area a simulated up event is sent using @{link
     * StripLayoutHelper#onUpOrCancel}.
     *
     * The @{link DragEvent} are also forwarded using @{link StripLayoutHelper#drag} to handle the
     * local reordering of the tabs when the user hovers over the StripLayout area of the toolbar
     * container.
     *
     * @param time      The time stamp in millisecond of the event.
     * @param x         The x position of the event.
     * @param y         The y position of the event.
     * @param fromMouse Whether the event originates from a mouse.
     * @param buttons   State of all buttons that are pressed.
     */
    protected void onDownInternal(long time, float x, float y, boolean fromMouse, int buttons) {
        resetResizeTimeout(false);

        if (mNewTabButton.onDown(x, y, fromMouse)) {
            mRenderHost.requestRender();
            return;
        }

        final StripLayoutTab clickedTab = getTabAtPosition(x);
        final int index =
                clickedTab != null
                        ? TabModelUtils.getTabIndexById(mModel, clickedTab.getId())
                        : TabModel.INVALID_TAB_INDEX;
        // http://crbug.com/472186 : Needs to handle a case that index is invalid.
        // The case could happen when the current tab is touched while we're inflating the rest of
        // the tabs from disk.
        mInteractingTab =
                index != TabModel.INVALID_TAB_INDEX && index < mStripTabs.length
                        ? mStripTabs[index]
                        : null;
        boolean clickedClose = clickedTab != null && clickedTab.checkCloseHitTest(x, y);
        if (clickedClose) {
            clickedTab.setClosePressed(true, fromMouse);
            mRenderHost.requestRender();
        }

        if (!mScroller.isFinished()) {
            mScroller.forceFinished(true);
            mInteractingTab = null;
        }

        if (fromMouse
                && !clickedClose
                && clickedTab != null
                && (buttons & MotionEvent.BUTTON_TERTIARY) == 0) {
            startReorderMode(time, x, x);
        }
    }

    /**
     * Called on long press touch event.
     * @param time The current time of the app in ms.
     * @param x    The x coordinate of the position of the press event.
     * @param y    The y coordinate of the position of the press event.
     */
    public void onLongPress(long time, float x, float y) {
        final StripLayoutTab clickedTab = getTabAtPosition(x);
        if (clickedTab != null && clickedTab.checkCloseHitTest(x, y)) {
            clickedTab.setClosePressed(false, false);
            mRenderHost.requestRender();
            showTabMenu(clickedTab);
        } else {
            resetResizeTimeout(false);
            startReorderMode(time, x, x);

            // Allow the user to drag the selected tab out of the tab toolbar.
            if (clickedTab != null) {
                allowMovingTabOutOfStripLayout(clickedTab, new PointF(x, y));
            } else {
                // Broadcast to start moving the window instance as the user has long pressed on the
                // open space of the tab strip.
                sendMoveWindowBroadcast(mToolbarContainerView, x, y);
            }
        }
    }

    /**
     * Called on hover enter event.
     *
     * @param x The x coordinate of the position of the hover enter event.
     */
    public void onHoverEnter(float x, float y) {
        if (!isPeripheralsSupportForTabStripEnabled()) return;
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
        if (!isPeripheralsSupportForTabStripEnabled()) return;

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
        if (!isPeripheralsSupportForTabStripEnabled()) return;

        clearLastHoveredTab();

        // Clear tab strip button (NTB and MSB) hover state.
        clearCompositorButtonHoverStateIfNotClicked();

        mUpdateHost.requestUpdate();
    }

    /** Called in post delay task in q#onDown to clear tab hover state. */
    protected void clearTabHoverState() {
        clearLastHoveredTab();
        mUpdateHost.requestUpdate();
    }

    /** Check whether model selector button or new tab button is being hovered. */
    private void updateCompositorButtonHoverState(float x, float y) {
        // Model selector button is being hovered.
        mModelSelectorButton.setHovered(mModelSelectorButton.checkClickedOrHovered(x, y));
        // There's a delay in updating NTB's position/touch target when MSB initially appears on the
        // strip, taking over NTB's position and moving NTB closer to the tabs. Consequently, hover
        // highlights are observed on both NTB and MSB. To address this, this check is added to
        // ensure only one button can be hovered at a time.
        if (!mModelSelectorButton.isHovered()) {
            mNewTabButton.setHovered(
                    ((CompositorButton) mNewTabButton).checkClickedOrHovered(x, y));
        } else {
            mNewTabButton.setHovered(false);
        }
    }

    /** Clear button hover state */
    private void clearCompositorButtonHoverStateIfNotClicked() {
        assert isPeripheralsSupportForTabStripEnabled();

        mNewTabButton.setHovered(false);
        mModelSelectorButton.setHovered(false);
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

    private void clearLastHoveredTab() {
        if (mLastHoveredTab == null) return;
        assert mTabHoverCardView != null : "Hover card view should not be null.";

        // Clear close button hover state.
        mLastHoveredTab.setCloseHovered(false);

        // Remove the highlight from the last hovered tab.
        updateHoveredTabAttachedState(mLastHoveredTab, false);
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
        if (isTabCompletelyHidden(hoveredTab)) return;

        mLastHoveredTab = hoveredTab;
        if (!mAnimationsDisabledForTesting) {
            CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            hoveredTab,
                            StripLayoutTab.OPACITY,
                            hoveredTab.getContainerOpacity(),
                            TAB_OPACITY_VISIBLE_FOREGROUND,
                            ANIM_HOVERED_TAB_CONTAINER_FADE_MS)
                    .start();
        } else {
            hoveredTab.setContainerOpacity(TAB_OPACITY_VISIBLE_FOREGROUND);
        }
        updateHoveredTabAttachedState(mLastHoveredTab, true);

        // Show the tab hover card.
        int hoveredTabIndex = findIndexForTab(mLastHoveredTab.getId());
        mTabHoverCardView.show(
                mModel.getTabAt(hoveredTabIndex),
                isSelectedTab(mLastHoveredTab.getId()),
                mLastHoveredTab.getDrawX(),
                mLastHoveredTab.getWidth(),
                mHeight);
    }

    private void updateHoveredTabAttachedState(StripLayoutTab tab, boolean hovered) {
        if (tab == null) return;

        // Do not update the attached state of a selected tab that is hovered on.
        if (isSelectedTab(tab.getId())) return;

        // If a tab is hovered on, detach its container.
        tab.setFolioAttached(!hovered);
        tab.setBottomMargin(
                hovered ? FOLIO_DETACHED_BOTTOM_MARGIN_DP : FOLIO_ATTACHED_BOTTOM_MARGIN_DP);
    }

    private boolean isPeripheralsSupportForTabStripEnabled() {
        return ChromeFeatureList.isEnabled(
                ChromeFeatureList.ADVANCED_PERIPHERALS_SUPPORT_TAB_STRIP);
    }

    private void handleNewTabClick() {
        if (mModel == null) return;

        if (!mModel.isIncognito()) mModel.commitAllTabClosures();
        mTabCreator.launchNtp();
    }

    @Override
    public void handleCloseButtonClick(final StripLayoutTab tab, long time) {
        // Placeholder tabs are expected to have invalid tab ids.
        if (tab == null || tab.isDying() || tab.getId() == Tab.INVALID_TAB_ID) return;

        mMultiStepTabCloseAnimRunning = false;
        finishAnimationsAndPushTabUpdates();

        // Find out if we're closing the last tab to determine if we resize immediately.
        boolean lastTab =
                mStripTabs.length == 0 || mStripTabs[mStripTabs.length - 1].getId() == tab.getId();

        // When a tab is closed #resizeStripOnTabClose will run animations for the new tab offset
        // and tab x offsets. When there is only 1 tab remaining, we do not need to run those
        // animations, so #resizeTabStrip() is used instead.
        boolean runImprovedTabAnimations = mStripTabs.length > 1;

        Tab nextTab = mModel.getNextTabIfClosed(tab.getId(), /* uponExit= */ false);

        // 1. Set the dying state of the tab.
        tab.setIsDying(true);

        // 2. Start the tab closing animator with a listener to resize/move tabs after the closure.
        AnimatorListener listener =
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        if (runImprovedTabAnimations) {
                            // This removes any closed tabs from the tabModel.
                            finishAnimationsAndPushTabUpdates();
                            resizeStripOnTabClose(tab.getId(), nextTab);
                        } else {
                            mMultiStepTabCloseAnimRunning = false;
                            // Resize the tabs appropriately.
                            resizeTabStrip(!lastTab, false);
                        }
                    }
                };
        runTabRemovalAnimation(tab, listener);

        // 3. Fake a selection on the next tab now.
        if (!runImprovedTabAnimations && nextTab != null) {
            tabSelected(time, nextTab.getId(), tab.getId(), false);
        }
    }

    private void runTabRemovalAnimation(StripLayoutTab tab, AnimatorListener listener) {
        // 1. Setup the close animation.
        List<Animator> tabClosingAnimators = new ArrayList<>();
        tabClosingAnimators.add(
                CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        tab,
                        StripLayoutTab.Y_OFFSET,
                        tab.getOffsetY(),
                        tab.getHeight(),
                        ANIM_TAB_CLOSED_MS));

        // 2. Start the animation.
        mMultiStepTabCloseAnimRunning = true;
        startAnimationList(tabClosingAnimators, listener);
    }

    private void resizeStripOnTabClose(int tabId, Tab nextTab) {
        List<Animator> tabStripAnimators = new ArrayList<>();

        // 1. Add tabs expanding animators to expand remaining tabs to fill scrollable area.
        List<Animator> tabExpandAnimators = computeAndUpdateTabWidth(true, true);
        if (tabExpandAnimators != null) tabStripAnimators.addAll(tabExpandAnimators);

        // 2. Calculate new mScrollOffset and idealX for tab offset animation.
        updateScrollOffsetLimits();
        computeTabInitialPositions();

        // 3. Add tab drawX animators to reposition the tabs correctly.
        for (StripLayoutTab tab : mStripTabs) {
            CompositorAnimator drawXAnimator =
                    CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            tab,
                            StripLayoutTab.DRAW_X,
                            tab.getDrawX(),
                            tab.getIdealX(),
                            ANIM_TAB_DRAW_X_MS);
            tabStripAnimators.add(drawXAnimator);
        }

        // 4. Add new tab button offset animation.
        CompositorAnimator newTabButtonOffsetAnimator = updateNewTabButtonState(true);
        if (newTabButtonOffsetAnimator != null) {
            tabStripAnimators.add(newTabButtonOffsetAnimator);
        }

        // 5. Add animation completion listener and start animations.
        startAnimationList(
                tabStripAnimators,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mMultiStepTabCloseAnimRunning = false;
                    }
                });

        // 6. Schedule next tab selection. Skip auto scroll so users don't lose track of their
        // location in the tab strip after closing a tab.
        if (nextTab != null) {
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    () -> tabSelected(SystemClock.uptimeMillis(), nextTab.getId(), tabId, true),
                    ANIM_TAB_SELECTION_DELAY);
        }
    }

    @Override
    public void handleTabClick(StripLayoutTab tab) {
        if (tab == null || tab.isDying()) return;

        int newIndex = TabModelUtils.getTabIndexById(mModel, tab.getId());

        // Early return, since placeholder tabs are known to not have tab ids.
        if (newIndex == Tab.INVALID_TAB_ID) return;

        TabModelUtils.setIndex(mModel, newIndex, false);
    }

    /**
     * Called on click. This is called before the onUpOrCancel event.
     * @param time      The current time of the app in ms.
     * @param x         The x coordinate of the position of the click.
     * @param y         The y coordinate of the position of the click.
     * @param fromMouse Whether the event originates from a mouse.
     * @param buttons   State of all buttons that were pressed when onDown was invoked.
     */
    public void click(long time, float x, float y, boolean fromMouse, int buttons) {
        resetResizeTimeout(false);

        if (mNewTabButton.click(x, y)) {
            RecordUserAction.record("MobileToolbarNewTab");
            mNewTabButton.handleClick(time);
            return;
        }

        final StripLayoutTab clickedTab = getTabAtPosition(x);
        if (clickedTab == null || clickedTab.isDying()) return;
        if (clickedTab.checkCloseHitTest(x, y)
                || (fromMouse && (buttons & MotionEvent.BUTTON_TERTIARY) != 0)) {
            RecordUserAction.record("MobileToolbarCloseTab");
            clickedTab.getCloseButton().handleClick(time);
        } else {
            RecordUserAction.record("MobileTabSwitched.TabletTabStrip");
            recordTabSwitchTimeHistogram();
            clickedTab.handleClick(time);
        }
    }

    private void recordTabSwitchTimeHistogram() {
        if (mTabScrollStartTime == null || mMostRecentTabScroll == null) return;

        long endTime = SystemClock.elapsedRealtime();
        long duration = endTime - mTabScrollStartTime;
        long timeFromLastInteraction = endTime - mMostRecentTabScroll;

        // Discard sample if last scroll was over the max allowed interval.
        if (timeFromLastInteraction <= TAB_SWITCH_METRICS_MAX_ALLOWED_SCROLL_INTERVAL) {
            RecordHistogram.recordMediumTimesHistogram(
                    "Android.TabStrip.TimeToSwitchTab", duration);
        }

        mTabScrollStartTime = null;
        mMostRecentTabScroll = null;
    }

    /**
     * Called on up or cancel touch events. This is called after the click and fling event if any.
     * @param time The current time of the app in ms.
     */
    public void onUpOrCancel(long time) {
        // 1. Reset the last close button pressed state.
        if (mLastPressedCloseButton != null) mLastPressedCloseButton.onUpOrCancel();
        mLastPressedCloseButton = null;

        // 2. Stop any reordering that is happening.
        stopReorderMode();

        // 3. Reset state
        mInteractingTab = null;
        mReorderState = REORDER_SCROLL_NONE;
        if (mNewTabButton.onUpOrCancel() && mModel != null) {
            if (!mModel.isIncognito()) mModel.commitAllTabClosures();
            mTabCreator.launchNtp();
        }
        mIsStripScrollInProgress = false;
    }

    /**
     * @return Whether or not the tabs are moving.
     */
    public boolean isAnimatingForTesting() {
        return (mRunningAnimator != null && mRunningAnimator.isRunning())
                || !mScroller.isFinished();
    }

    private void finishAnimations() {
        // Force any outstanding animations to finish. Need to recurse as some animations (like the
        // multi-step tab close animation) kick off another animation once the first ends.
        while (mRunningAnimator != null && mRunningAnimator.isRunning()) {
            mRunningAnimator.end();
        }
        mRunningAnimator = null;
    }

    private void startAnimationList(List<Animator> animationList, AnimatorListener listener) {
        AnimatorSet set = new AnimatorSet();
        set.playTogether(animationList);
        if (listener != null) set.addListener(listener);

        finishAnimations();
        setAndStartRunningAnimator(set);
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

    /**
     * Finishes any outstanding animations and propagates any related changes to the
     * {@link TabModel}.
     */
    public void finishAnimationsAndPushTabUpdates() {
        if (mRunningAnimator == null) return;

        // 1. Finish animations.
        finishAnimations();

        // 2. Figure out which tabs need to be closed.
        ArrayList<StripLayoutTab> tabsToRemove = new ArrayList<StripLayoutTab>();
        for (int i = 0; i < mStripTabs.length; i++) {
            StripLayoutTab tab = mStripTabs[i];
            if (tab.isDying()) tabsToRemove.add(tab);
        }

        if (tabsToRemove.isEmpty()) return;

        // 3. Pass the close notifications to the model if the tab isn't already closing.
        //    Do this as a post task as if more tabs are added inside commit all tab closures that
        //    is a concurrent modification exception.
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    for (StripLayoutTab tab : tabsToRemove) {
                        TabModelUtils.closeTabById(mModel, tab.getId(), true);
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

    private void updateScrollOffsetPosition(float pos) {
        float oldScrollOffset = mScrollOffset;
        mScrollOffset = MathUtils.clamp(pos, mMinScrollOffset - mReorderExtraMinScrollOffset, 0);

        if (mInReorderMode && mScroller.isFinished()) {
            float delta =
                    MathUtils.flipSignIf(
                            oldScrollOffset - mScrollOffset, LocalizationUtils.isLayoutRtl());
            updateReorderPosition(delta);
        }
    }

    private void updateScrollOffset(long time) {
        if (mScroller.computeScrollOffset(time)) {
            updateScrollOffsetPosition(mScroller.getCurrX());
            mUpdateHost.requestUpdate();
        }
    }

    @VisibleForTesting
    void updateScrollOffsetLimits() {
        // 1. Compute the width of the available space for all tabs.
        float stripWidth = mWidth - mLeftMargin - mRightMargin;

        // 2. Compute the effective width of every tab.
        float tabsWidth = getNumLiveTabs() * (mCachedTabWidth - mTabOverlapWidth);

        if (mInReorderMode || mTabGroupMarginAnimRunning) {
            tabsWidth += mStripStartMarginForReorder;
            for (int i = 0; i < mStripTabs.length; i++) {
                final StripLayoutTab tab = mStripTabs[i];
                tabsWidth += tab.getTrailingMargin();
            }
        }

        // 3. Correct fencepost error in tabswidth;
        tabsWidth = tabsWidth + mTabOverlapWidth;

        // 4. Calculate the minimum scroll offset.  Round > -EPSILON to 0.
        mMinScrollOffset = Math.min(0.f, stripWidth - tabsWidth);
        if (mMinScrollOffset > -EPSILON) mMinScrollOffset = 0.f;

        // 5. Clamp mScrollOffset to make sure it's in the valid range.
        updateScrollOffsetPosition(mScrollOffset);
    }

    private List<Animator> computeAndUpdateTabOrders(boolean delayResize, boolean deferAnimations) {
        final int count = mModel.getCount();
        StripLayoutTab[] tabs = new StripLayoutTab[count];

        for (int i = 0; i < count; i++) {
            final Tab tab = mModel.getTabAt(i);
            final int id = tab.getId();
            final StripLayoutTab oldTab = findTabById(id);
            tabs[i] = oldTab != null ? oldTab : createStripTab(id);
            setAccessibilityDescription(tabs[i], tab);
        }

        int oldStripLength = mStripTabs.length;
        mStripTabs = tabs;

        List<Animator> animationList = null;
        // If multi-step animation is running, the resize will be handled elsewhere.
        if (mStripTabs.length != oldStripLength && !mMultiStepTabCloseAnimRunning) {
            animationList = resizeTabStrip(delayResize, deferAnimations);
        }

        updateVisualTabOrdering();
        return animationList;
    }

    private List<Animator> resizeTabStrip(boolean delay, boolean deferAnimations) {
        List<Animator> animationList = null;

        if (delay) {
            resetResizeTimeout(true);
        } else {
            animationList = computeAndUpdateTabWidth(true, deferAnimations);
        }

        return animationList;
    }

    private void updateVisualTabOrdering() {
        if (mStripTabs.length != mStripTabsVisuallyOrdered.length) {
            mStripTabsVisuallyOrdered = new StripLayoutTab[mStripTabs.length];
        }

        mStripStacker.createVisualOrdering(
                getSelectedStripTabIndex(), mStripTabs, mStripTabsVisuallyOrdered);
    }

    private StripLayoutTab createPlaceholderStripTab() {
        StripLayoutTab tab =
                new StripLayoutTab(
                        mContext,
                        Tab.INVALID_TAB_ID,
                        this,
                        mTabLoadTrackerHost,
                        mUpdateHost,
                        mIncognito);

        tab.setIsPlaceholder(true);
        tab.setContainerOpacity(TAB_OPACITY_VISIBLE_FOREGROUND);

        // TODO(https://crbug.com/1502238): Added placeholder a11y descriptions to prevent crash due
        //  to invalid a11y node. Replace with official strings when available.
        String description = "Placeholder Tab";
        String title = "Placeholder";
        tab.setAccessibilityDescription(description, title);

        pushPropertiesToTab(tab);

        return tab;
    }

    @VisibleForTesting
    StripLayoutTab createStripTab(int id) {
        // TODO: Cache these
        StripLayoutTab tab =
                new StripLayoutTab(
                        mContext, id, this, mTabLoadTrackerHost, mUpdateHost, mIncognito);

        if (isSelectedTab(id)) {
            tab.setContainerOpacity(TAB_OPACITY_VISIBLE_FOREGROUND);
        }

        pushPropertiesToTab(tab);

        return tab;
    }

    private void pushPropertiesToPlaceholder(StripLayoutTab placeholderTab, Tab tab) {
        placeholderTab.setId(tab.getId());
        placeholderTab.setIsPlaceholder(false);
        placeholderTab.setContainerOpacity(TAB_OPACITY_HIDDEN);

        setAccessibilityDescription(placeholderTab, tab);
    }

    private void pushPropertiesToTab(StripLayoutTab tab) {
        // The close button is visible by default. If it should be hidden on tab creation, do not
        // animate the fade-out. See (https://crbug.com/1342654).
        boolean shouldShowCloseButton = mCachedTabWidth >= TAB_WIDTH_MEDIUM;
        tab.setCanShowCloseButton(shouldShowCloseButton, false);
        tab.setHeight(mHeight);
    }

    /**
     * @param id The Tab id.
     * @return The StripLayoutTab that corresponds to that tabid.
     */
    @VisibleForTesting
    public @Nullable StripLayoutTab findTabById(int id) {
        if (mStripTabs == null) return null;
        for (int i = 0; i < mStripTabs.length; i++) {
            if (mStripTabs[i].getId() == id) return mStripTabs[i];
        }
        return null;
    }

    int findIndexForTab(int id) {
        if (mStripTabs == null || id == Tab.INVALID_TAB_ID) return TabModel.INVALID_TAB_INDEX;
        for (int i = 0; i < mStripTabs.length; i++) {
            final StripLayoutTab stripTab = mStripTabs[i];
            if (stripTab.getId() == id) return i;
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    int getNumLiveTabs() {
        int numLiveTabs = 0;

        for (int i = 0; i < mStripTabs.length; i++) {
            if (!mStripTabs[i].isDying()) numLiveTabs++;
        }

        return numLiveTabs;
    }

    private List<Animator> computeAndUpdateTabWidth(boolean animate, boolean deferAnimations) {
        // Remove any queued resize messages.
        mStripTabEventHandler.removeMessages(MESSAGE_RESIZE);

        int numTabs = Math.max(getNumLiveTabs(), 1);

        // 1. Compute the width of the available space for all tabs.
        float stripWidth = mWidth - mLeftMargin - mRightMargin;

        // 2. Compute additional width we gain from overlapping the tabs.
        float overlapWidth = mTabOverlapWidth * (numTabs - 1);

        // 3. Calculate the optimal tab width.
        float optimalTabWidth = (stripWidth + overlapWidth) / numTabs;

        // 4. Calculate the realistic tab width.
        mCachedTabWidth = MathUtils.clamp(optimalTabWidth, mMinTabWidth, mMaxTabWidth);

        // 5. Prepare animations and propagate width to all tabs.
        finishAnimationsAndPushTabUpdates();
        ArrayList<Animator> resizeAnimationList = null;
        if (animate && !mAnimationsDisabledForTesting) resizeAnimationList = new ArrayList<>();

        for (int i = 0; i < mStripTabs.length; i++) {
            StripLayoutTab tab = mStripTabs[i];
            if (tab.isDying()) continue;
            if (resizeAnimationList != null) {
                CompositorAnimator animator =
                        CompositorAnimator.ofFloatProperty(
                                mUpdateHost.getAnimationHandler(),
                                tab,
                                StripLayoutTab.WIDTH,
                                tab.getWidth(),
                                mCachedTabWidth,
                                ANIM_TAB_RESIZE_MS);
                resizeAnimationList.add(animator);
            } else {
                mStripTabs[i].setWidth(mCachedTabWidth);
            }
        }

        if (resizeAnimationList != null) {
            if (deferAnimations) return resizeAnimationList;
            startAnimationList(resizeAnimationList, null);
        }
        return null;
    }

    private void updateStrip() {
        if (mModel == null) return;

        // TODO(dtrainor): Remove this once tabCreated() is refactored to be called even from
        // restore.
        if (mTabStateInitialized
                && (mStripTabs == null || mModel.getCount() != mStripTabs.length)) {
            computeAndUpdateTabOrders(false, false);
        }

        // 1. Update the scroll offset limits
        updateScrollOffsetLimits();

        // 2. Calculate the ideal tab positions
        computeTabInitialPositions();

        // 3. Calculate the tab stacking and ensure that tabs are sized correctly.
        mStripStacker.setTabOffsets(
                mStripTabs, mMultiStepTabCloseAnimRunning, mTabCreating, mCachedTabWidth);

        // 4. Calculate which tabs are visible.
        mStripStacker.performOcclusionPass(mStripTabs, mWidth);

        // 5. Create render list.
        createRenderList();

        // 6. Figure out where to put the new tab button. If a tab is being closed, the new tab
        // button position will be updated with the tab resize and drawX animations.
        if (!mMultiStepTabCloseAnimRunning) {
            updateNewTabButtonState(false);
        }

        // 7. Invalidate the accessibility provider in case the visible virtual views have changed.
        mRenderHost.invalidateAccessibilityProvider();

        // 8. Hide close buttons if tab width gets lower than 156dp.
        updateCloseButtons();

        // 9. Show dividers between inactive tabs.
        updateTabContainersAndDividers();
    }

    private void computeTabInitialPositions() {
        // Shift all of the tabs over by the the left margin because we're
        // no longer base lined at 0
        float tabPosition;
        if (!LocalizationUtils.isLayoutRtl()) {
            tabPosition = mScrollOffset + mLeftMargin + mStripStartMarginForReorder;
        } else {
            tabPosition =
                    mWidth
                            - mCachedTabWidth
                            - mScrollOffset
                            - mRightMargin
                            - mStripStartMarginForReorder;
        }

        for (int i = 0; i < mStripTabs.length; i++) {
            StripLayoutTab tab = mStripTabs[i];
            tab.setIdealX(tabPosition);
            // idealX represents where a tab should be placed in the tab strip. mCachedTabWidth may
            // be different than tab.getWidth() when a tab is closing because for the improved tab
            // strip animations the tab width expansion animations will not have run yet.
            float tabWidth = mMultiStepTabCloseAnimRunning ? mCachedTabWidth : tab.getWidth();
            float delta = (tabWidth - mTabOverlapWidth) * tab.getWidthWeight();
            if (mInReorderMode || mTabGroupMarginAnimRunning) {
                delta += tab.getTrailingMargin();
            }
            delta = MathUtils.flipSignIf(delta, LocalizationUtils.isLayoutRtl());
            tabPosition += delta;
        }
    }

    private void createRenderList() {
        // 1. Figure out how many tabs will need to be rendered.
        int renderCount = 0;
        for (int i = 0; i < mStripTabsVisuallyOrdered.length; ++i) {
            if (mStripTabsVisuallyOrdered[i].isVisible()) renderCount++;
        }

        // 2. Reallocate the render list if necessary.
        if (mStripTabsToRender.length != renderCount) {
            mStripTabsToRender = new StripLayoutTab[renderCount];
        }

        // 3. Populate it with the visible tabs.
        int renderIndex = 0;
        for (int i = 0; i < mStripTabsVisuallyOrdered.length; ++i) {
            if (mStripTabsVisuallyOrdered[i].isVisible()) {
                mStripTabsToRender[renderIndex++] = mStripTabsVisuallyOrdered[i];
            }
        }
    }

    private CompositorAnimator updateNewTabButtonState(boolean animate) {
        // 1. The NTB is faded out upon entering reorder mode and hidden when the model is empty.
        boolean isEmpty = mStripTabs.length == 0;
        mNewTabButton.setVisible(!isEmpty);
        if (isEmpty) return null;

        // 2. Get offset from strip stacker.
        float offset =
                mStripStacker.computeNewTabButtonOffset(
                        mStripTabs,
                        mTabOverlapWidth,
                        mLeftMargin,
                        mRightMargin,
                        mWidth,
                        mNewTabButtonWidth,
                        mCachedTabWidth,
                        animate);

            boolean isRtl = LocalizationUtils.isLayoutRtl();

        if (!isTabStripFull()) {
            // Move NTB close to tabs by 4 dp when tab strip is not full.
            offset +=
                    isRtl
                            ? NEW_TAB_BUTTON_X_OFFSET_TOWARDS_TABS
                            : -NEW_TAB_BUTTON_X_OFFSET_TOWARDS_TABS;
        }

            // 3. Hide the new tab button if it's not visible on the screen.
            if ((isRtl && offset + mNewTabButtonWidth < 0) || (!isRtl && offset > mWidth)) {
                mNewTabButton.setVisible(false);
                return null;
            }
            mNewTabButton.setVisible(true);

        // 4. Position the new tab button.
        if (animate) {
            return CompositorAnimator.ofFloatProperty(
                    mUpdateHost.getAnimationHandler(),
                    mNewTabButton,
                    CompositorButton.DRAW_X,
                    mNewTabButton.getX(),
                    offset,
                    NEW_TAB_BUTTON_OFFSET_MOVE_MS);
        } else {
            mNewTabButton.setX(offset);
        }
        return null;
    }

    /**
     * @param tab The tab to make fully visible.
     * @return Scroll delta to make the tab fully visible.
     */
    private float calculateDeltaToMakeTabVisible(StripLayoutTab tab) {
        if (tab == null) return 0.f;

        return calculateDeltaToMakeIndexVisible(findIndexForTab(tab.getId()));
    }

    /**
     * @param index The index of the tab to make fully visible.
     * @return Scroll delta to make the tab at the given index fully visible.
     */
    private float calculateDeltaToMakeIndexVisible(int index) {
        if (index == TabModel.INVALID_TAB_INDEX) return 0.f;

        // 1. Calculate offsets to fully show the tab at the start and end of the strip.
        final boolean isRtl = LocalizationUtils.isLayoutRtl();
        final float tabWidth = mCachedTabWidth - mTabOverlapWidth;
        final float startOffset = (isRtl ? mRightFadeWidth : mLeftFadeWidth);
        final float endOffset = (isRtl ? mLeftFadeWidth : mRightFadeWidth);

        final float optimalStart = startOffset + (-index * tabWidth);
        final float optimalEnd = mWidth - endOffset - ((index + 1) * tabWidth) - mTabOverlapWidth;

        // 2. Return the scroll delta to make the given tab fully visible with the least scrolling.
        // This will result in the tab being at either the start or end of the strip.
        final float deltaToOptimalStart = optimalStart - mScrollOffset;
        final float deltaToOptimalEnd = optimalEnd - mScrollOffset;

        return Math.abs(deltaToOptimalStart) < Math.abs(deltaToOptimalEnd)
                ? deltaToOptimalStart
                : deltaToOptimalEnd;
    }

    void setTabAtPositionForTesting(StripLayoutTab tab) {
        mTabAtPositionForTesting = tab;
    }

    StripLayoutTab getTabAtPosition(float x) {
        if (mTabAtPositionForTesting != null) {
            return mTabAtPositionForTesting;
        }

        for (int i = mStripTabsVisuallyOrdered.length - 1; i >= 0; i--) {
            final StripLayoutTab tab = mStripTabsVisuallyOrdered[i];
            if (tab.isVisible()
                    && tab.getTouchTargetLeft() <= x
                    && x <= tab.getTouchTargetRight()) {
                return tab;
            }
        }

        return null;
    }

    /**
     * @param tab The StripLayoutTab to look for.
     * @return The index of the tab in the visual ordering.
     */
    public int visualIndexOfTabForTesting(StripLayoutTab tab) {
        for (int i = 0; i < mStripTabsVisuallyOrdered.length; i++) {
            if (mStripTabsVisuallyOrdered[i] == tab) {
                return i;
            }
        }
        return -1;
    }

    /**
     * @param tab The StripLayoutTab you're looking at.
     * @return Whether or not this tab is the foreground tab.
     */
    public boolean isForegroundTabForTesting(StripLayoutTab tab) {
        return tab == mStripTabsVisuallyOrdered[mStripTabsVisuallyOrdered.length - 1];
    }

    private void updateTabAttachState(
            StripLayoutTab tab, boolean attached, ArrayList<Animator> animationList) {
        float startValue =
                attached ? FOLIO_DETACHED_BOTTOM_MARGIN_DP : FOLIO_ATTACHED_BOTTOM_MARGIN_DP;
        float intermediateValue = FOLIO_ANIM_INTERMEDIATE_MARGIN_DP;
        float endValue =
                attached ? FOLIO_ATTACHED_BOTTOM_MARGIN_DP : FOLIO_DETACHED_BOTTOM_MARGIN_DP;

        if (animationList == null) {
            tab.setBottomMargin(endValue);
            tab.setFolioAttached(attached);
            return;
        }

        ArrayList<Animator> attachAnimationList = new ArrayList<>();
        CompositorAnimator dropAnimation =
                CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        tab,
                        StripLayoutTab.BOTTOM_MARGIN,
                        startValue,
                        intermediateValue,
                        ANIM_FOLIO_DETACH_MS,
                        Interpolators.EMPHASIZED_ACCELERATE);
        CompositorAnimator riseAnimation =
                CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        tab,
                        StripLayoutTab.BOTTOM_MARGIN,
                        intermediateValue,
                        endValue,
                        ANIM_FOLIO_DETACH_MS,
                        Interpolators.EMPHASIZED_DECELERATE);
        dropAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        tab.setFolioAttached(attached);
                    }
                });
        attachAnimationList.add(dropAnimation);
        attachAnimationList.add(riseAnimation);

        AnimatorSet set = new AnimatorSet();
        set.playSequentially(attachAnimationList);
        animationList.add(set);
    }

    public boolean getInReorderModeForTesting() {
        return mInReorderMode;
    }

    public void startReorderModeAtIndexForTesting(int index) {
        StripLayoutTab tab = mStripTabs[index];
        updateStrip();
        startReorderMode(INVALID_TIME, 0f, tab.getDrawX() + (tab.getWidth() / 2));
    }

    public void stopReorderModeForTesting() {
        stopReorderMode();
    }

    @VisibleForTesting
    void startReorderMode(long time, float currentX, float startX) {
        if (mInReorderMode) return;
        RecordUserAction.record("MobileToolbarStartReorderTab");
        // 1. Reset the last pressed close button state.
        if (mLastPressedCloseButton != null && mLastPressedCloseButton.isPressed()) {
            mLastPressedCloseButton.setPressed(false);
        }
        mLastPressedCloseButton = null;

        // 2. Check to see if we have a valid (non-null, non-dying, non-placeholder) tab to start
        // dragging.
        mInteractingTab = mActiveClickedTab == null ? getTabAtPosition(startX) : mActiveClickedTab;
        if (mInteractingTab == null
                || mInteractingTab.isDying()
                || mInteractingTab.getId() == Tab.INVALID_TAB_ID) {
            return;
        }

        // 3. Set initial state parameters.
        finishAnimationsAndPushTabUpdates();
        ArrayList<Animator> animationList =
                mAnimationsDisabledForTesting ? null : new ArrayList<>();
        mLastReorderScrollTime = INVALID_TIME;
        mHoverStartTime = INVALID_TIME;
        mHoverStartOffset = 0;
        mReorderState = REORDER_SCROLL_NONE;
        mLastReorderX = startX;
        mTabMarginWidth = mCachedTabWidth / 2;
        mInReorderMode = true;
        mHoveringOverGroup = false;

        // 4. Select this tab so that it is always in the foreground.
        TabModelUtils.setIndex(
                mModel, TabModelUtils.getTabIndexById(mModel, mInteractingTab.getId()), false);

        // 5. fade-out model selector buttons.
        setCompositorButtonsVisible(false);

        // 6. the selected tab will already be visible, so update tab group and background
        // container.
        Tab tab = getTabById(mInteractingTab.getId());
        computeAndUpdateTabGroupMargins(true, animationList);
        setTabGroupBackgroundContainersVisible(mTabGroupModelFilter.getRootId(tab), true);
        performHapticFeedback(tab);

        // 7. Lift the container off the toolbar.
        mInteractingTab.setIsReordering(true);
        updateTabAttachState(mInteractingTab, false, animationList);

        // 8. Kick-off animations and request an update.
        if (animationList != null) {
            startAnimationList(animationList, getTabGroupMarginAnimatorListener(false));
        }
        mUpdateHost.requestUpdate();
    }

    private void stopReorderMode() {
        if (!mInReorderMode) return;
        ArrayList<Animator> animationList = null;
        if (!mAnimationsDisabledForTesting) animationList = new ArrayList<>();

        // 1. Reset the state variables.
        mReorderState = REORDER_SCROLL_NONE;
        mInReorderMode = false;

        // 2. Clear any drag offset.
        finishAnimationsAndPushTabUpdates();
        if (animationList != null) {
            animationList.add(
                    CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            mInteractingTab,
                            StripLayoutTab.X_OFFSET,
                            mInteractingTab.getOffsetX(),
                            0f,
                            ANIM_TAB_MOVE_MS));
        } else {
            mInteractingTab.setOffsetX(0f);
        }

        // 3. Reset the background tabs and fade-in the new tab & model selector buttons.
        setBackgroundTabContainersVisible(false);
        setCompositorButtonsVisible(true);

        // 4. Clear any tab group margins.
        resetTabGroupMargins(animationList);

        // 5. Reattach the folio container to the toolbar.
        mInteractingTab.setIsReordering(false);
        updateTabAttachState(mInteractingTab, true, animationList);

        // 6. Request an update.
        startAnimationList(animationList, getTabGroupMarginAnimatorListener(true));
        mUpdateHost.requestUpdate();
    }

    /**
     * Sets the trailing margin for the current tab. Animates if necessary.
     *
     * @param tab The tab to update.
     * @param trailingMargin The given tab's new trailing margin.
     * @param animationList The list to add the animation to, or {@code null} if not animating.
     * @return Whether or not the trailing margin for the given tab actually changed.
     */
    private boolean setTrailingMarginForTab(
            StripLayoutTab tab, float trailingMargin, List<Animator> animationList) {
        if (tab.getTrailingMargin() != trailingMargin) {
            if (animationList != null) {
                animationList.add(
                        CompositorAnimator.ofFloatProperty(
                                mUpdateHost.getAnimationHandler(),
                                tab,
                                StripLayoutTab.TRAILING_MARGIN,
                                tab.getTrailingMargin(),
                                trailingMargin,
                                ANIM_TAB_SLIDE_OUT_MS));
            } else {
                tab.setTrailingMargin(trailingMargin);
            }
            return true;
        }
        return false;
    }

    /**
     * Sets the new tab strip's start margin and autoscrolls the required amount to make it appear
     * as though the interacting tab does not move.
     *
     * @param startMarginDelta The change in start margin for the tab strip.
     * @param numMarginsToSlide The number of margins to slide to make it appear as through the
     *         interacting tab does not move.
     * @param animationList The list to add the animation to, or {@code null} if not animating.
     */
    private void autoScrollForTabGroupMargins(
            float startMarginDelta, int numMarginsToSlide, List<Animator> animationList) {
        float delta = (numMarginsToSlide * mTabMarginWidth);
        float startValue = mScrollOffset - startMarginDelta;
        float endValue = startValue - delta;

        // If the current tab width is at its max, this means there are not enough tabs to fill the
        // visible area on the tab strip. In this case, there is not enough room to auto-scroll for
        // tab group margins. Allocate additional space to account for this. See
        // http://crbug.com/1374918 for additional details.
        if (mCachedTabWidth == mMaxTabWidth) {
            mReorderExtraMinScrollOffset = mStripStartMarginForReorder + Math.abs(delta);
        }

        if (animationList != null) {
            animationList.add(
                    CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            this,
                            SCROLL_OFFSET,
                            startValue,
                            endValue,
                            ANIM_TAB_SLIDE_OUT_MS));
        } else {
            mScrollOffset = endValue;
        }
    }

    private AnimatorListener getTabGroupMarginAnimatorListener(boolean resetExtraMinScrollOffset) {
        return new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mTabGroupMarginAnimRunning = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mTabGroupMarginAnimRunning = false;
                if (resetExtraMinScrollOffset) mReorderExtraMinScrollOffset = 0.f;
            }
        };
    }

    private void computeAndUpdateTabGroupMargins(
            boolean autoScroll, ArrayList<Animator> animationList) {
        // 1. Update the trailing margins for each tab.
        boolean pastInteractingTab = false;
        int numMarginsToSlide = 0;
        for (int i = 0; i < mStripTabs.length - 1; i++) {
            final StripLayoutTab stripTab = mStripTabs[i];
            if (stripTab == mInteractingTab) pastInteractingTab = true;

            // 1.a. Calculate the current tab's trailing margin.
            float trailingMargin = 0f;
            Tab currTab = getTabById(stripTab.getId());
            Tab nextTab = getTabById(mStripTabs[i + 1].getId());
            boolean eitherTabInAGroup =
                    mTabGroupModelFilter.hasOtherRelatedTabs(currTab)
                            || mTabGroupModelFilter.hasOtherRelatedTabs(nextTab);
            boolean areRelatedTabs =
                    mTabGroupModelFilter.getRootId(currTab)
                            == mTabGroupModelFilter.getRootId(nextTab);
            if (eitherTabInAGroup && !areRelatedTabs) trailingMargin = mTabMarginWidth;

            // 1.b. Attempt to update the current tab's trailing margin.
            float oldMargin = stripTab.getTrailingMargin();
            boolean didChangeTrailingMargin =
                    setTrailingMarginForTab(stripTab, trailingMargin, animationList);
            if (didChangeTrailingMargin && !pastInteractingTab) {
                numMarginsToSlide += oldMargin < trailingMargin ? 1 : -1;
            }
        }

        // 2. Set the starting and trailing margin for the tab strip.
        boolean firstTabIsInGroup =
                mTabGroupModelFilter.hasOtherRelatedTabs(getTabById(mStripTabs[0].getId()));
        boolean lastTabIsInGroup =
                mTabGroupModelFilter.hasOtherRelatedTabs(
                        getTabById(mStripTabs[mStripTabs.length - 1].getId()));
        float startMargin = firstTabIsInGroup ? mTabMarginWidth : 0f;
        float startMarginDelta = startMargin - mStripStartMarginForReorder;
        mStripStartMarginForReorder = startMargin;
        mStripTabs[mStripTabs.length - 1].setTrailingMargin(
                lastTabIsInGroup ? mTabMarginWidth : 0f);

        // 3. Adjust the scroll offset accordingly to prevent the interacting tab from shifting away
        // from where the user long-pressed.
        if (autoScroll) {
            autoScrollForTabGroupMargins(startMarginDelta, numMarginsToSlide, animationList);
        }

        // 4. Begin slide-out and scroll animation. Update tab positions.
        if (animationList == null) computeTabInitialPositions();
    }

    private void resetTabGroupMargins(ArrayList<Animator> animationList) {
        assert !mInReorderMode;

        // 1. Update the trailing margins for each tab.
        boolean pastInteractingTab = false;
        int numMarginsToSlide = 0;
        for (int i = 0; i < mStripTabs.length; i++) {
            final StripLayoutTab stripTab = mStripTabs[i];
            if (stripTab == mInteractingTab) pastInteractingTab = true;

            boolean didChangeTrailingMargin = setTrailingMarginForTab(stripTab, 0f, animationList);
            if (didChangeTrailingMargin && !pastInteractingTab) numMarginsToSlide--;
        }

        // 2. Adjust the scroll offset accordingly to prevent the interacting tab from shifting away
        // from where the user long-pressed.
        autoScrollForTabGroupMargins(
                -mStripStartMarginForReorder, numMarginsToSlide, animationList);
        mStripStartMarginForReorder = 0f;
    }

    private void setCompositorButtonsVisible(boolean visible) {
        float endOpacity = visible ? 1.f : 0.f;

        CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        mNewTabButton,
                        CompositorButton.OPACITY,
                        mNewTabButton.getOpacity(),
                        endOpacity,
                        ANIM_BUTTONS_FADE_MS)
                .start();
        CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        mModelSelectorButton,
                        CompositorButton.OPACITY,
                        mModelSelectorButton.getOpacity(),
                        endOpacity,
                        ANIM_BUTTONS_FADE_MS)
                .start();
    }

    private void setBackgroundTabContainerVisible(StripLayoutTab tab, boolean visible) {
        if (tab != mInteractingTab) {
            float opacity = visible ? TAB_OPACITY_VISIBLE_BACKGROUND : TAB_OPACITY_HIDDEN;
            tab.setContainerOpacity(opacity);

            updateTabAttachState(tab, !visible, null);
        }
    }

    private void setBackgroundTabContainersVisible(boolean visible) {
        for (int i = 0; i < mStripTabs.length; i++) {
            final StripLayoutTab tab = mStripTabs[i];
            setBackgroundTabContainerVisible(tab, visible);
        }
    }

    private void setTabGroupBackgroundContainersVisible(int groupId, boolean visible) {
        for (int i = 0; i < mStripTabs.length; i++) {
            final StripLayoutTab tab = mStripTabs[i];

            if (mTabGroupModelFilter.getRootId(getTabById(tab.getId())) == groupId) {
                setBackgroundTabContainerVisible(tab, visible);
            }
        }
    }

    /**
     * This method checks whether or not interacting tab has met the conditions to be moved out of
     * its tab group. It moves tab out of group if so and returns the new index for the interacting
     * tab.
     *
     * @param offset The distance the interacting tab has been dragged from its ideal x-position.
     * @param curIndex The index of the interacting tab.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     * @return The new index for the interacting tab if it has been removed from its tab group and
     *         the INVALID_TAB_INDEX otherwise.
     */
    private int maybeMoveOutOfGroup(float offset, int curIndex, boolean towardEnd) {
        // If past threshold, un-dim hovered group and trigger reorder.
        if (Math.abs(offset) > mTabMarginWidth * REORDER_OVERLAP_SWITCH_PERCENTAGE) {
            final int tabId = mInteractingTab.getId();

            setTabGroupBackgroundContainersVisible(
                    mTabGroupModelFilter.getRootId(getTabById(tabId)), false);
            mTabGroupModelFilter.moveTabOutOfGroupInDirection(tabId, towardEnd);
            RecordUserAction.record("MobileToolbarReorderTab.TabRemovedFromGroup");
            return curIndex;
        }

        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * This method checks whether or not interacting tab has met the conditions to be merged into a
     * neighbouring tab group. It merges tab to group if so and returns the new index for the
     * interacting tab.
     *
     * @param offset The distance the interacting tab has been dragged from its ideal x-position.
     * @param curIndex The index of the interacting tab.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     * @return The new index for the interacting tab if it has been moved into a neighboring tab
     *         group and the INVALID_TAB_INDEX otherwise.
     */
    private int maybeMergeToGroup(float offset, int curIndex, boolean towardEnd) {
        // 1. Only attempt to merge if hovering a group for a valid amount of time.
        if (!mHoveringOverGroup) return TabModel.INVALID_TAB_INDEX;

        // 2. Set initial hover variables if we have not yet started or if we have moved too far
        // from the initial hover. Since we have just started a new hover, do not trigger a
        // reorder.
        if (mHoverStartTime == INVALID_TIME
                || Math.abs(mHoverStartOffset - offset) > DROP_INTO_GROUP_MAX_OFFSET) {
            mHoverStartTime = mLastUpdateTime;
            mHoverStartOffset = offset;

            return TabModel.INVALID_TAB_INDEX;
        }

        // 3. If we have not yet hovered for the required amount of time, keep waiting and do not
        // trigger a reorder.
        if (mLastUpdateTime - mHoverStartTime < DROP_INTO_GROUP_MS) {
            mUpdateHost.requestUpdate();

            return TabModel.INVALID_TAB_INDEX;
        }

        // 4. We have hovered for the required time, so trigger a reorder.
        int direction = towardEnd ? 1 : -1;
        StripLayoutTab destTab = mStripTabs[curIndex + direction];
        float effectiveWidth = mCachedTabWidth - mTabOverlapWidth;
        float flipThreshold = effectiveWidth * REORDER_OVERLAP_SWITCH_PERCENTAGE;
        float minFlipOffset = mTabMarginWidth + flipThreshold;
        int numTabsToSkip =
                1 + (int) Math.floor((Math.abs(offset) - minFlipOffset) / effectiveWidth);
        mTabGroupModelFilter.mergeTabsToGroup(mInteractingTab.getId(), destTab.getId(), true);
        RecordUserAction.record("MobileToolbarReorderTab.TabAddedToGroup");

        return towardEnd ? curIndex + 1 + numTabsToSkip : curIndex - numTabsToSkip;
    }

    private int updateHoveringOverGroup(float offset, int curIndex, boolean towardEnd) {
        boolean hoveringOverGroup = Math.abs(offset) > mTabMarginWidth - mTabOverlapWidth;

        // 1. Check if hover state has changed.
        if (mHoveringOverGroup != hoveringOverGroup) {
            // 1.a. Reset hover variables.
            mHoveringOverGroup = hoveringOverGroup;
            mHoverStartTime = INVALID_TIME;
            mHoverStartOffset = 0;

            // 1.b. Set tab group dim as necessary.
            int groupId =
                    mTabGroupModelFilter.getRootId(
                            getTabById(mStripTabs[curIndex + (towardEnd ? 1 : -1)].getId()));
            setTabGroupBackgroundContainersVisible(groupId, mHoveringOverGroup);
        }

        // 2. If we are hovering, attempt to merge to the hovered group.
        if (mHoveringOverGroup) {
            return maybeMergeToGroup(offset, curIndex, towardEnd);
        }

        // 3. Default to not triggering a reorder.
        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * This method determines the new index for the interacting tab, based on whether or not it has
     * met the conditions to be moved past a neighboring tab group.
     *
     * @param offset The distance the interacting tab has been dragged from its ideal x-position.
     * @param curIndex The index of the interacting tab.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     * @return The new index for the interacting tab if it should be moved past the neighboring tab
     *         group and the INVALID_TAB_INDEX otherwise.
     */
    private int maybeMovePastGroup(float offset, int curIndex, boolean towardEnd) {
        int direction = towardEnd ? 1 : -1;
        int groupId =
                mTabGroupModelFilter.getRootId(
                        getTabById(mStripTabs[curIndex + direction].getId()));
        int numTabsToSkip = mTabGroupModelFilter.getRelatedTabCountForRootId(groupId);
        float effectiveTabWidth = mCachedTabWidth - mTabOverlapWidth;
        float threshold = (numTabsToSkip * effectiveTabWidth) + mTabMarginWidth + mTabOverlapWidth;

        // If past threshold, un-dim hovered group and trigger reorder.
        if (Math.abs(offset) > threshold) {
            setTabGroupBackgroundContainersVisible(groupId, false);

            int destIndex = towardEnd ? curIndex + 1 + numTabsToSkip : curIndex - numTabsToSkip;
            return destIndex;
        }

        return TabModel.INVALID_TAB_INDEX;
    }

    private void updateReorderPosition(float deltaX) {
        if (!mInReorderMode || mInteractingTab == null) return;

        float offset = mInteractingTab.getOffsetX() + deltaX;
        int curIndex = findIndexForTab(mInteractingTab.getId());

        if (curIndex == TabModel.INVALID_TAB_INDEX) return;

        // 1. Compute the reorder threshold values.
        final float flipWidth = mCachedTabWidth - mTabOverlapWidth;
        final float flipThreshold = REORDER_OVERLAP_SWITCH_PERCENTAGE * flipWidth;

        // 2. Check if we should swap tabs and track the new destination index.
        int destIndex = TabModel.INVALID_TAB_INDEX;
        boolean towardEnd = (offset >= 0) ^ LocalizationUtils.isLayoutRtl();
        boolean isInGroup =
                mTabGroupModelFilter.hasOtherRelatedTabs(getTabById(mInteractingTab.getId()));
        boolean hasTrailingMargin = mInteractingTab.getTrailingMargin() == mTabMarginWidth;
        boolean hasStartingMargin =
                curIndex == 0
                        ? mStripStartMarginForReorder > 0
                        : mStripTabs[curIndex - 1].getTrailingMargin() == mTabMarginWidth;
        boolean approachingMargin = towardEnd ? hasTrailingMargin : hasStartingMargin;

        if (approachingMargin) {
            if (isInGroup) {
                // 2.a. Tab is in a group and approaching a margin. Maybe drag out of group.
                destIndex = maybeMoveOutOfGroup(offset, curIndex, towardEnd);
            } else {
                // 2.b. Tab is not in a group and approaching a margin. Maybe target tab group.
                destIndex = updateHoveringOverGroup(offset, curIndex, towardEnd);

                // 2.c. Tab is not in a group and approaching a margin. Maybe drag past group.
                if (destIndex == TabModel.INVALID_TAB_INDEX) {
                    destIndex = maybeMovePastGroup(offset, curIndex, towardEnd);
                }
            }
        } else {
            // 2.d Tab is not interacting with tab groups. Reorder as normal.
            boolean pastLeftThreshold = offset < -flipThreshold;
            boolean pastRightThreshold = offset > flipThreshold;
            boolean isNotRightMost = curIndex < mStripTabs.length - 1;
            boolean isNotLeftMost = curIndex > 0;

            if (LocalizationUtils.isLayoutRtl()) {
                boolean oldLeft = pastLeftThreshold;
                pastLeftThreshold = pastRightThreshold;
                pastRightThreshold = oldLeft;
            }

            if (pastRightThreshold && isNotRightMost) {
                destIndex = curIndex + 2;
            } else if (pastLeftThreshold && isNotLeftMost) {
                destIndex = curIndex - 1;
            }
        }

        // 3. If we should swap tabs, make the swap.
        if (destIndex != TabModel.INVALID_TAB_INDEX) {
            // 3. a. Reset internal state.
            mHoveringOverGroup = false;

            // 3.b. Swap the tabs.
            reorderTab(mInteractingTab.getId(), curIndex, destIndex, true);
            mModel.moveTab(mInteractingTab.getId(), destIndex);

            // 3.c. Re-compute tab group margins if necessary.
            float oldIdealX = mInteractingTab.getIdealX();
            float oldOffset = mScrollOffset;
            computeAndUpdateTabGroupMargins(false, null);

            // 3.d. Since we just moved the tab we're dragging, adjust its offset so it stays in
            // the same apparent position.
            if (approachingMargin) {
                offset -= (mInteractingTab.getIdealX() - oldIdealX);
                // When the strip is scrolling, deltaX is already accounted for by idealX. This is
                // because idealX uses the scroll offset which has already been adjusted by deltaX.
                if (mLastReorderScrollTime != 0) offset -= deltaX;

                // Tab group margins can affect minScrollOffset. When a dragged tab is near the
                // strip's edge, the scrollOffset being clamped can affect the apparent position.
                offset -=
                        MathUtils.flipSignIf(
                                (mScrollOffset - oldOffset), LocalizationUtils.isLayoutRtl());
            } else {
                boolean shouldFlip =
                        LocalizationUtils.isLayoutRtl()
                                ? destIndex < curIndex
                                : destIndex > curIndex;
                offset += MathUtils.flipSignIf(flipWidth, shouldFlip);
            }

            // 3.e. Update our curIndex as we have just moved the tab.
            curIndex = destIndex > curIndex ? destIndex - 1 : destIndex;

            // 3.f. Update visual tab ordering.
            updateVisualTabOrdering();
        }

        // 4. Limit offset based on tab position.  First tab can't drag left, last tab can't drag
        // right.
        if (curIndex == 0) {
            offset =
                    LocalizationUtils.isLayoutRtl()
                            ? Math.min(mStripStartMarginForReorder, offset)
                            : Math.max(-mStripStartMarginForReorder, offset);
        }
        if (curIndex == mStripTabs.length - 1) {
            offset =
                    LocalizationUtils.isLayoutRtl()
                            ? Math.max(-mStripTabs[curIndex].getTrailingMargin(), offset)
                            : Math.min(mStripTabs[curIndex].getTrailingMargin(), offset);
        }

        // 5. Set the new offset.
        mInteractingTab.setOffsetX(offset);
    }

    private void reorderTab(int id, int oldIndex, int newIndex, boolean animate) {
        StripLayoutTab tab = findTabById(id);
        if (tab == null || oldIndex == newIndex) return;

        // 1. If the tab is already at the right spot, don't do anything.
        int index = findIndexForTab(id);
        if (index == newIndex) return;

        // 2. Check if it's the tab we are dragging, but we have an old source index.  Ignore in
        // this case because we probably just already moved it.
        if (mInReorderMode && index != oldIndex && tab == mInteractingTab) return;

        // 3. Animate if necessary.
        if (animate && !mAnimationsDisabledForTesting) {
            final boolean towardEnd = oldIndex <= newIndex;
            final float flipWidth = mCachedTabWidth - mTabOverlapWidth;
            final int direction = towardEnd ? 1 : -1;
            final float animationLength =
                    MathUtils.flipSignIf(direction * flipWidth, LocalizationUtils.isLayoutRtl());

            finishAnimationsAndPushTabUpdates();
            ArrayList<Animator> slideAnimationList = new ArrayList<>();
            for (int i = oldIndex + direction; towardEnd == i < newIndex; i += direction) {
                StripLayoutTab slideTab = mStripTabs[i];
                CompositorAnimator animator =
                        CompositorAnimator.ofFloatProperty(
                                mUpdateHost.getAnimationHandler(),
                                slideTab,
                                StripLayoutTab.X_OFFSET,
                                animationLength,
                                0f,
                                ANIM_TAB_MOVE_MS);
                slideAnimationList.add(animator);
                // When the reorder is triggered by an autoscroll, the first frame will not show the
                // sliding tabs with the correct offset. To fix this, we manually set the correct
                // starting offset. See https://crbug.com/1342811.
                slideTab.setOffsetX(animationLength);
            }
            startAnimationList(slideAnimationList, null);
        }

        // 4. Swap the tabs.
        moveElement(mStripTabs, index, newIndex);
    }

    private void handleReorderAutoScrolling(long time) {
        if (!mInReorderMode) return;

        // 1. Track the delta time since the last auto scroll.
        final float deltaSec =
                mLastReorderScrollTime == INVALID_TIME
                        ? 0.f
                        : (time - mLastReorderScrollTime) / 1000.f;
        mLastReorderScrollTime = time;

        final float x = mInteractingTab.getDrawX();

        // 2. Calculate the gutters for accelerating the scroll speed.
        // Speed: MAX    MIN                  MIN    MAX
        // |-------|======|--------------------|======|-------|
        final float dragRange = REORDER_EDGE_SCROLL_START_MAX_DP - REORDER_EDGE_SCROLL_START_MIN_DP;
        final float leftMinX = REORDER_EDGE_SCROLL_START_MIN_DP + mLeftMargin;
        final float leftMaxX = REORDER_EDGE_SCROLL_START_MAX_DP + mLeftMargin;
        final float rightMinX =
                mWidth - mLeftMargin - mRightMargin - REORDER_EDGE_SCROLL_START_MIN_DP;
        final float rightMaxX =
                mWidth - mLeftMargin - mRightMargin - REORDER_EDGE_SCROLL_START_MAX_DP;

        // 3. See if the current draw position is in one of the gutters and figure out how far in.
        // Note that we only allow scrolling in each direction if the user has already manually
        // moved that way.
        float dragSpeedRatio = 0.f;
        if ((mReorderState & REORDER_SCROLL_LEFT) != 0 && x < leftMinX) {
            dragSpeedRatio = -(leftMinX - Math.max(x, leftMaxX)) / dragRange;
        } else if ((mReorderState & REORDER_SCROLL_RIGHT) != 0 && x + mCachedTabWidth > rightMinX) {
            dragSpeedRatio = (Math.min(x + mCachedTabWidth, rightMaxX) - rightMinX) / dragRange;
        }

        dragSpeedRatio = MathUtils.flipSignIf(dragSpeedRatio, LocalizationUtils.isLayoutRtl());

        if (dragSpeedRatio != 0.f) {
            // 4.a. We're in a gutter.  Update the scroll offset.
            float dragSpeed = REORDER_EDGE_SCROLL_MAX_SPEED_DP * dragSpeedRatio;
            updateScrollOffsetPosition(mScrollOffset + dragSpeed * deltaSec);

            mUpdateHost.requestUpdate();
        } else {
            // 4.b. We're not in a gutter.  Reset the scroll delta time tracker.
            mLastReorderScrollTime = INVALID_TIME;
        }
    }

    @VisibleForTesting
    Tab getTabById(int tabId) {
        return TabModelUtils.getTabById(mModel, tabId);
    }

    private int getSelectedTabId() {
        if (mModel == null) return Tab.INVALID_TAB_ID;

        int index = mModel.index();
        if (index == TabModel.INVALID_TAB_INDEX) return Tab.INVALID_TAB_ID;

        Tab tab = mModel.getTabAt(index);
        if (tab == null) return Tab.INVALID_TAB_ID;

        return tab.getId();
    }

    private int getSelectedStripTabIndex() {
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

    @SuppressLint("HandlerLeak")
    private class StripTabEventHandler extends Handler {
        @Override
        public void handleMessage(Message m) {
            switch (m.what) {
                case MESSAGE_RESIZE:
                    computeAndUpdateTabWidth(true, false);
                    mUpdateHost.requestUpdate();
                    break;
                case MESSAGE_UPDATE_SPINNER:
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

    private static <T> void moveElement(T[] array, int oldIndex, int newIndex) {
        if (oldIndex <= newIndex) {
            moveElementUp(array, oldIndex, newIndex);
        } else {
            moveElementDown(array, oldIndex, newIndex);
        }
    }

    private static <T> void moveElementUp(T[] array, int oldIndex, int newIndex) {
        assert oldIndex <= newIndex;
        if (oldIndex == newIndex || oldIndex + 1 == newIndex) return;

        T elem = array[oldIndex];
        for (int i = oldIndex; i < newIndex - 1; i++) {
            array[i] = array[i + 1];
        }
        array[newIndex - 1] = elem;
    }

    private static <T> void moveElementDown(T[] array, int oldIndex, int newIndex) {
        assert oldIndex >= newIndex;
        if (oldIndex == newIndex) return;

        T elem = array[oldIndex];
        for (int i = oldIndex - 1; i >= newIndex; i--) {
            array[i + 1] = array[i];
        }
        array[newIndex] = elem;
    }

    /**
     * Sets the current scroll offset of the TabStrip.
     * @param offset The offset to set the TabStrip's scroll state to.
     */
    public void setScrollOffsetForTesting(float offset) {
        mScrollOffset = offset;
        updateStrip();
    }

    /**
     * Displays the tab menu below the anchor tab.
     *
     * @param anchorTab The tab the menu will be anchored to
     */
    @VisibleForTesting
    void showTabMenu(StripLayoutTab anchorTab) {
        // 1. Bring the anchor tab to the foreground.
        int tabIndex = TabModelUtils.getTabIndexById(mModel, anchorTab.getId());
        TabModelUtils.setIndex(mModel, tabIndex, false);

        // 2. Anchor the popupMenu to the view associated with the tab
        View tabView = TabModelUtils.getCurrentTab(mModel).getView();
        mTabMenu.setAnchorView(tabView);
        // 3. Set the vertical offset to align the tab menu with bottom of the tab strip
        int tabHeight = mManagerHost.getHeight();
        int verticalOffset =
                -(tabHeight - (int) mContext.getResources().getDimension(R.dimen.tab_strip_height));
        mTabMenu.setVerticalOffset(verticalOffset);

        // 4. Set the horizontal offset to align the tab menu with the right side of the tab
        int horizontalOffset =
                Math.round(
                                (anchorTab.getDrawX() + anchorTab.getWidth())
                                        * mContext.getResources().getDisplayMetrics().density)
                        - mTabMenu.getWidth();
        // Cap the horizontal offset so that the tab menu doesn't get drawn off screen.
        horizontalOffset = Math.max(horizontalOffset, 0);
        mTabMenu.setHorizontalOffset(horizontalOffset);

        mTabMenu.show();
    }

    private void setScrollForScrollingTabStacker(float delta, boolean shouldAnimate, long time) {
        if (delta == 0.f) return;

        if (shouldAnimate && !mAnimationsDisabledForTesting) {
            mScroller.startScroll(
                    Math.round(mScrollOffset), 0, (int) delta, 0, time, getScrollDuration());
        } else {
            mScrollOffset = mScrollOffset + delta;
        }
    }

    /**
     * Scales the scroll duration based on the scroll offset.
     * @return the duration in ms.
     */
    private int getScrollDuration() {
        float scrollDistance = Math.abs(mScrollOffset);
        if (scrollDistance <= SCROLL_DISTANCE_SHORT) {
            return SCROLL_DURATION_MS;
        } else if (scrollDistance <= SCROLL_DISTANCE_MEDIUM) {
            return SCROLL_DURATION_MS_MEDIUM;
        } else {
            return SCROLL_DURATION_MS_LONG;
        }
    }

    /** Scrolls to the selected tab if it's not fully visible. */
    private void bringSelectedTabToVisibleArea(long time, boolean animate) {
        if (mWidth == 0) return;

        int index = getSelectedStripTabIndex();
        StripLayoutTab selectedLayoutTab =
                index >= 0 && index < mStripTabs.length ? mStripTabs[index] : null;
        if (selectedLayoutTab == null || isSelectedTabCompletelyVisible(selectedLayoutTab)) {
            return;
        }
        float delta = calculateDeltaToMakeIndexVisible(index);

        setScrollForScrollingTabStacker(delta, animate, time);
    }

    private boolean isSelectedTabCompletelyVisible(StripLayoutTab selectedTab) {
        return selectedTab.isVisible()
                && selectedTab.getDrawX() > mLeftFadeWidth
                && selectedTab.getDrawX() + selectedTab.getWidth() < mWidth - mRightFadeWidth;
    }

    /**
     * Determines whether a drawn tab is hidden completely out of view.
     * @param tab The {@link StripLayoutTab} whose visibility is determined.
     * @return {@code true} if the tab is completely hidden, {@code false} otherwise.
     */
    @VisibleForTesting
    boolean isTabCompletelyHidden(StripLayoutTab tab) {
        return !tab.isVisible()
                || tab.getDrawX() + tab.getWidth() <= mLeftFadeWidth
                || tab.getDrawX() >= mWidth - mRightFadeWidth;
    }

    /**
     * To prevent accidental tab closures, when the close button of a tab is very close to the edge
     * of the tab strip, we hide the close button. The threshold for hiding is different based on
     * the length of the fade at the end of the strip.
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
     * @return true if the tab menu is showing
     */
    public boolean isTabMenuShowingForTesting() {
        return mTabMenu.isShowing();
    }

    /**
     * @param menuItemId The id of the menu item to click
     */
    public void clickTabMenuItemForTesting(int menuItemId) {
        mTabMenu.performItemClick(menuItemId);
    }

    int getScrollDurationForTesting() {
        return getScrollDuration();
    }

    /**
     * @return The with of the tab strip.
     */
    float getWidthForTesting() {
        return mWidth;
    }

    /**
     * @return The strip's minimum scroll offset.
     */
    float getMinimumScrollOffsetForTesting() {
        return mMinScrollOffset;
    }

    /**
     * @return The strip's additional minimum scroll offset for reorder mode.
     */
    float getReorderExtraMinScrollOffsetForTesting() {
        return mReorderExtraMinScrollOffset;
    }

    /**
     * @return The scroller.
     */
    StackScroller getScrollerForTesting() {
        return mScroller;
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
     * @return The amount tabs overlap.
     */
    float getTabOverlapWidthForTesting() {
        return mTabOverlapWidth;
    }

    /**
     * @return The currently interacting tab.
     */
    StripLayoutTab getInteractingTabForTesting() {
        return mInteractingTab;
    }

    /** Disables animations for testing purposes. */
    public void disableAnimationsForTesting() {
        mAnimationsDisabledForTesting = true;
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
        return mLastReorderX;
    }

    private void setAccessibilityDescription(StripLayoutTab stripTab, Tab tab) {
        if (tab != null) setAccessibilityDescription(stripTab, tab.getTitle(), tab.isHidden());
    }

    /**
     * Set the accessibility description of a {@link StripLayoutTab}.
     *
     * @param stripTab  The StripLayoutTab to set the accessibility description.
     * @param title     The title of the tab.
     * @param isHidden  Current visibility state of the Tab.
     */
    private void setAccessibilityDescription(
            StripLayoutTab stripTab, String title, boolean isHidden) {
        if (stripTab == null) return;

        // Separator used to separate the different parts of the content description.
        // Not for sentence construction and hence not localized.
        final String contentDescriptionSeparator = ", ";
        final StringBuilder builder = new StringBuilder();
        if (!TextUtils.isEmpty(title)) {
            builder.append(title);
            builder.append(contentDescriptionSeparator);
        }

        @StringRes int resId;
        if (mIncognito) {
            resId =
                    isHidden
                            ? R.string.accessibility_tabstrip_incognito_identifier
                            : R.string.accessibility_tabstrip_incognito_identifier_selected;
        } else {
            resId =
                    isHidden
                            ? R.string.accessibility_tabstrip_identifier
                            : R.string.accessibility_tabstrip_identifier_selected;
        }
        builder.append(mContext.getResources().getString(resId));

        stripTab.setAccessibilityDescription(builder.toString(), title);
    }

    private void performHapticFeedback(Tab tab) {
        View tabView = tab.getView();
        if (tabView == null) return;
        tabView.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
    }

    protected void clearActiveClickedTab() {
        mActiveClickedTab = null;
        mDraggedTabOffStrip = false;
        mLastOffsetX = 0.f;
    }

    StripLayoutTab getActiveClickedTabForTesting() {
        return mActiveClickedTab;
    }

    @VisibleForTesting
    void allowMovingTabOutOfStripLayout(
            @NonNull StripLayoutTab clickedTab, @NonNull PointF dragStartPointF) {
        if (!TabUiFeatureUtilities.isTabDragEnabled()) return;
        // In addition to reordering, one can drag and drop the tab beyond the strip layout view.
        Tab tabBeingDragged = getTabById(clickedTab.getId());
        if (tabBeingDragged != null) {
            boolean dragStarted =
                    mTabDragSource.startTabDragAction(
                            mToolbarContainerView, tabBeingDragged, dragStartPointF);
            if (dragStarted) {
                mActiveClickedTab = clickedTab;
                mDraggedTabOffStrip = false;
                mLastOffsetX = 0.f;
            }
        }
    }

    void setDraggedTabOffStripForTesting(boolean draggedTabOffStrip) {
        mDraggedTabOffStrip = draggedTabOffStrip;
    }

    boolean isDraggedTabOffStripForTesting() {
        return mDraggedTabOffStrip;
    }

    void setLastOffsetXForTesting(float lastOffsetX) {
        mLastOffsetX = lastOffsetX;
    }

    float getLastOffsetXForTesting() {
        return mLastOffsetX;
    }

    void dragActiveClickedTabOntoStrip(long time, float x) {
        StripLayoutTab draggedTab = getSelectedStripTab();
        assert draggedTab != null;

        finishAnimationsAndPushTabUpdates();
        mDraggedTabOffStrip = false;
        draggedTab.setOffsetX(mLastOffsetX);
        draggedTab.setOffsetY(0);
        mLastOffsetX = 0.f;

        startReorderMode(time, x, x);
    }

    void dragActiveClickedTabOutOfStrip(long time) {
        StripLayoutTab draggedTab = getSelectedStripTab();
        assert draggedTab != null;

        mLastOffsetX = draggedTab.getOffsetX();
        onUpOrCancel(time);
        finishAnimationsAndPushTabUpdates();

        runTabRemovalAnimation(
                draggedTab,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mDraggedTabOffStrip = true;
                        draggedTab.setDrawX(draggedTab.getIdealX());
                        resizeStripOnTabClose(draggedTab.getId(), null);
                    }
                });
    }

    void sendMoveWindowBroadcast(View view, float startXInView, float startYInView) {
        if (!TabUiFeatureUtilities.isTabDragEnabled()) return;
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
}
