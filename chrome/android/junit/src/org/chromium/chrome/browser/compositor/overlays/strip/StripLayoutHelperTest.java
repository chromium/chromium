// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.nullable;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.MAX_TAB_WIDTH_DP;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.MIN_TAB_WIDTH_DP;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.TAB_GROUP_BOTTOM_INDICATOR_WIDTH_OFFSET;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.TAB_OVERLAP_WIDTH_DP;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER2;

import android.animation.Animator;
import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.ContextThemeWrapper;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.ColorUtils;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.ButtonType;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnClickHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnKeyboardFocusHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripIphController.IphType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.TabDragSource;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.dragdrop.ChromeDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabDropDataAndroid;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinatorFactory;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.SharedGroupTestHelper;
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DragDropGlobalState.TrackerToken;
import org.chromium.ui.shadows.ShadowAppCompatResources;
import org.chromium.ui.util.MotionEventUtils;
import org.chromium.ui.util.XrUtils;
import org.chromium.ui.widget.RectProvider;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.stream.IntStream;

/** Tests for {@link StripLayoutHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        qualifiers = "sw600dp",
        shadows = {ShadowAppCompatResources.class})
@LooperMode(Mode.LEGACY)
@DisableFeatures({ChromeFeatureList.DATA_SHARING, ChromeFeatureList.TAB_STRIP_GROUP_REORDER})
public class StripLayoutHelperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mInteractingTabView;
    @Mock private LayoutManagerHost mManagerHost;
    @Mock private LayoutUpdateHost mUpdateHost;
    @Mock private LayoutRenderHost mRenderHost;
    @Mock private CompositorButton mModelSelectorBtn;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private View mToolbarContainerView;
    @Mock private StripTabHoverCardView mTabHoverCardView;
    @Mock private Profile mProfile;
    @Mock private StripLayoutViewOnClickHandler mClickHandler;
    @Mock private StripLayoutViewOnKeyboardFocusHandler mKeyboardFocusHandler;
    @Mock private TabDragSource mTabDragSource;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private LayerTitleCache mLayerTitleCache;
    @Mock private ActionConfirmationManager mActionConfirmationManager;
    @Mock private TabGroupContextMenuCoordinator mTabGroupContextMenuCoordinator;
    @Mock private DataSharingTabManager mDataSharingTabManager;
    @Mock private TabContextMenuCoordinator mTabContextMenuCoordinator;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private ShareDelegate mShareDelegate;
    @Mock private TabGroupListBottomSheetCoordinatorFactory mBottomSheetCoordinatorFactory;
    @Mock private Tab mTab;
    @Mock private TabCreator mTabCreator;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private ServiceStatus mServiceStatus;
    @Mock private DataSharingUIDelegate mDataSharingUiDelegate;
    @Mock private Bitmap mAvatarBitmap;
    @Mock private TintedCompositorButton mCloseButton;
    @Mock TabStripIphController mController;
    @Captor private ArgumentCaptor<DataSharingService.Observer> mSharingObserverCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mSharedImageTilesCaptor;
    @Captor private ArgumentCaptor<TabModelActionListener> mTabModelActionListenerCaptor;
    @Captor private ArgumentCaptor<Callback<TabClosureParams>> mTabRemoverCallbackCaptor;

    private Activity mActivity;
    private Context mContext;
    private SharedGroupTestHelper mSharedGroupTestHelper;

    // TODO(crbug.com/369736293): Verify usages and remove duplicate implementations of
    // `TestTabModel` for tab model.
    private final TestTabModel mModel = spy(new TestTabModel());
    private StripLayoutHelper mStripLayoutHelper;
    private boolean mIncognito;
    private static final int NEW_ANIM_TAB_RESIZE_MS = 200;
    private static final String[] TEST_TAB_TITLES = {"Tab 1", "Tab 2", "Tab 3", "", null};
    private static final String EXPECTED_NO_MARGIN = "The tab should not have a trailing margin.";
    private static final String EXPECTED_TAB = "The view should be a tab.";
    private static final String EXPECTED_TITLE = "The view should be a title.";
    private static final String EXPECTED_NON_TITLE = "The view should not be a title.";
    private static final String IDENTIFIER = "Tab";
    private static final String IDENTIFIER_SELECTED = "Selected Tab";
    private static final String INCOGNITO_IDENTIFIER = "Incognito Tab";
    private static final String INCOGNITO_IDENTIFIER_SELECTED = "Selected Incognito Tab";
    private static final float SCREEN_WIDTH = 800.f;
    private static final float SCREEN_WIDTH_LANDSCAPE = 1200.f;
    // TODO(wenyufu): This needs to be renamed to TAB_STRIP_HEIGHT.
    private static final float SCREEN_HEIGHT = 40.f;
    private static final float TAB_WIDTH_1 = 140.f;
    private static final float TAB_WIDTH_SMALL = 108.f;
    private static final float TAB_WIDTH_MEDIUM = 156.f;
    private static final long TIMESTAMP = 5000;
    private static final float NEW_TAB_BTN_X_RTL = 100.f;
    private static final float NEW_TAB_BTN_X = 700.f;
    private static final float NEW_TAB_BTN_Y = 1400.f;
    private static final float NEW_TAB_BTN_WIDTH = 100.f;
    private static final float NEW_TAB_BTN_HEIGHT = 100.f;
    private static final float PADDING_LEFT = 10.f;
    private static final float PADDING_RIGHT = 20.f;
    private static final float PADDING_TOP = 20.f;
    private static final float REORDER_OVERLAP_SWITCH_PERCENTAGE = 0.53f;
    private static final PointF DRAG_START_POINT = new PointF(70f, 20f);
    private static final float EPSILON = 0.001f;
    private static final String COLLABORATION_ID1 = "A";
    private static final String SYNC_ID1 = "B";
    private static final GURL URL = new GURL("http://example.com");

    /** Reset the environment before each test. */
    @Before
    public void beforeTest() {
        when(mTabGroupModelFilter.isTabInTabGroup(any())).thenReturn(false);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mModel);
        when(mTabGroupModelFilter.getTabUngrouper()).thenReturn(mTabUngrouper);

        mModel.setTabRemover(new TestTabRemover());
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        CompositorAnimationHandler.setTestingMode(true);
        CompositorAnimationHandler mHandler =
                new CompositorAnimationHandler(CallbackUtils.emptyRunnable());
        when(mUpdateHost.getAnimationHandler()).thenReturn(mHandler);
        when(mModel.getProfile()).thenReturn(mProfile);
        DataSharingServiceFactory.setForTesting(mDataSharingService);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        MessagingBackendServiceFactory.setForTesting(mMessagingBackendService);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(false);
        when(mDataSharingService.getUiDelegate()).thenReturn(mDataSharingUiDelegate);
        mSharedGroupTestHelper = new SharedGroupTestHelper(mCollaborationService);
    }

    @After
    public void tearDown() {
        CompositorAnimationHandler.setTestingMode(false);
        if (mStripLayoutHelper != null) {
            mStripLayoutHelper.setTabAtPositionForTesting(null);
            mStripLayoutHelper.setRunningAnimatorForTesting(null);
            mStripLayoutHelper.destroyTabContextMenuForTesting();
        }
        mTabDragSource = null;
    }

    /**
     * Test method for {@link StripLayoutHelper#getVirtualViews(List<VirtualView>)}.
     *
     * <p>Checks that it returns the correct order of tabs, including correct content.
     */
    @Test
    @Feature({"Accessibility"})
    public void testSimpleTabOrder() {
        initializeTest(false, false, 0);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        assertTabStripAndOrder(getExpectedAccessibilityDescriptions(0));
    }

    /**
     * Test method for {@link StripLayoutHelper#getVirtualViews(List<VirtualView>)}.
     *
     * <p>Checks that it returns the correct order of tabs, even when a tab except the first one is
     * selected.
     */
    @Test
    @Feature({"Accessibility"})
    public void testTabOrderWithIndex() {
        initializeTest(false, false, 1);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Tabs should be in left to right order regardless of index
        assertTabStripAndOrder(getExpectedAccessibilityDescriptions(1));
    }

    /**
     * Test method for {@link StripLayoutHelper#getVirtualViews(List<VirtualView>)}.
     *
     * <p>Checks that it returns the correct order of tabs, even in RTL mode.
     */
    @Test
    @Feature({"Accessibility"})
    public void testTabOrderRtl() {
        initializeTest(true, false, 0);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Tabs should be in linear order even in RTL.
        // Android will take care of reversing it.
        assertTabStripAndOrder(getExpectedAccessibilityDescriptions(0));
    }

    /**
     * Test method for {@link StripLayoutHelper#getVirtualViews(List<VirtualView>)}.
     *
     * <p>Checks that it returns the correct order of tabs, even in incognito mode.
     */
    @Test
    @Feature({"Accessibility"})
    public void testIncognitoAccessibilityDescriptions() {
        initializeTest(false, true, 0);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        assertTabStripAndOrder(getExpectedAccessibilityDescriptions(0));
    }

    @Test
    @Feature({"Accessibility"})
    public void testAccessibilityDescriptions_GroupIndicator_OneTab() {
        // Setup and group first tab.
        initializeTest(false, false, 0);
        groupTabs(0, 1);

        // Verify.
        String expectedDescription = "1 tab tab group - Tab 1";
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue("First should be a group title.", views[0] instanceof StripLayoutGroupTitle);
        assertEquals(
                "A11y description for group title was wrong.",
                expectedDescription,
                views[0].getAccessibilityDescription());
    }

    @Test
    @Feature({"Accessibility"})
    public void testAccessibilityDescriptions_GroupIndicator_MultipleTabs() {
        // Setup and group first three tabs.
        initializeTest(false, false, 0);
        groupTabs(0, 3);

        // Verify.
        String expectedDescription = "3 tabs tab group - Tab 1 and 2 other tabs";
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue("First should be a group title.", views[0] instanceof StripLayoutGroupTitle);
        assertEquals(
                "A11y description for group title was wrong.",
                expectedDescription,
                views[0].getAccessibilityDescription());
    }

    @Test
    @Feature({"Accessibility"})
    public void testAccessibilityDescriptions_GroupIndicator_MultipleTabs_NamedGroup() {
        // Setup and group first three tabs. Name the group.
        when(mTabGroupModelFilter.getTabGroupTitle(0)).thenReturn("Group name");
        initializeTest(false, false, 0);
        groupTabs(0, 3);

        // Verify.
        String expectedDescription = "Group name tab group - Tab 1 and 2 other tabs";
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue("First should be a group title.", views[0] instanceof StripLayoutGroupTitle);
        assertEquals(
                "A11y description for group title was wrong.",
                expectedDescription,
                views[0].getAccessibilityDescription());
    }

    @Test
    @Feature({"Accessibility"})
    public void testAccessibilityDescriptions_GroupIndicator_SharedGroup_OneTab() {
        // Setup and group first tab.
        initializeTest(false, false, 0);

        // Create collaboration group.
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ true,
                        /* duringStripBuild= */ false,
                        /* start= */ 0,
                        /* end= */ 1);

        // Verify.
        String expectedDescription = "Shared 1 tab tab group - Tab 1";
        assertEquals(
                "A11y description for group title was wrong.",
                expectedDescription,
                groupTitle.getAccessibilityDescription());
    }

    @Test
    @Feature({"Accessibility"})
    public void testAccessibilityDescriptions_GroupIndicator_SharedGroup_MultipleTabs() {
        // Setup and group first three tabs.
        initializeTest(false, false, 0);

        // Create collaboration group.
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ true,
                        /* duringStripBuild= */ false,
                        /* start= */ 0,
                        /* end= */ 3);

        // Verify.
        String expectedDescription = "Shared 3 tabs tab group - Tab 1 and 2 other tabs";
        assertEquals(
                "A11y description for group title was wrong.",
                expectedDescription,
                groupTitle.getAccessibilityDescription());
    }

    @Test
    @Feature({"Accessibility"})
    public void testAccessibilityDescriptions_GroupIndicator_SharedGroup_MultipleTabs_NamedGroup() {
        // Setup and group first three tabs. Name the group.
        when(mTabGroupModelFilter.getTabGroupTitle(0)).thenReturn("Group name");
        initializeTest(false, false, 0);

        // Create collaboration group.
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ true,
                        /* duringStripBuild= */ false,
                        /* start= */ 0,
                        /* end= */ 3);

        // Verify.
        String expectedDescription = "Shared Group name tab group - Tab 1 and 2 other tabs";
        assertEquals(
                "A11y description for group title was wrong.",
                expectedDescription,
                groupTitle.getAccessibilityDescription());
    }

    @Test
    @Feature({"Accessibility"})
    public void testAccessibilityDescriptions_GroupIndicator_SharedGroup_Notification() {
        // Setup and group first three tabs. Name the group.
        when(mTabGroupModelFilter.getTabGroupTitle(0)).thenReturn("Group name");
        initializeTest(false, false, 0);

        // Create collaboration group and show notification bubble on group title.
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ true,
                        /* duringStripBuild= */ false,
                        /* start= */ 0,
                        /* end= */ 3);
        mStripLayoutHelper.collapseTabGroupForTesting(groupTitle, /* isCollapsed= */ true);
        Set<Integer> tabIds = new HashSet<>(Collections.singleton(groupTitle.getRootId()));
        mStripLayoutHelper.updateTabStripNotificationBubble(tabIds, /* hasUpdate= */ true);

        // Verify.
        String expectedDescription =
                "Shared Group name tab group with new activity - Tab 1 and 2 other tabs";
        assertEquals(
                "A11y description for group title was wrong.",
                expectedDescription,
                groupTitle.getAccessibilityDescription());
    }

    @Test
    @Feature({"Accessibility"})
    public void testAccessibilityDescriptions_TabWithUpdate_SharedGroup_Notification() {
        // Setup and group first three tabs. Name the group.
        when(mTabGroupModelFilter.getTabGroupTitle(0)).thenReturn("Group name");
        initializeTest(false, false, 0);

        // Create collaboration group and show notification bubble on group title.
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ true,
                        /* duringStripBuild= */ false,
                        /* start= */ 0,
                        /* end= */ 3);
        Set<Integer> tabIds = new HashSet<>(Collections.singleton(groupTitle.getRootId()));
        mStripLayoutHelper.updateTabStripNotificationBubble(tabIds, /* hasUpdate= */ true);

        // Verify.
        String expectedDescription = "Tab 1, New or Updated Tab";
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertEquals(
                "A11y description for updated tab was wrong.",
                expectedDescription,
                views[1].getAccessibilityDescription());
    }

    @Test
    public void testResizeStripOnTabClose_DoNotAnimateIfNotMoving() {
        final int numTabs = 10;
        initializeTest(false, false, 0, numTabs);
        // Trigger a size change so the strip layout tab heights and widths get set.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        // Set the initial scroll offset to trigger an update to draw X positions.
        mStripLayoutHelper.setScrollOffsetForTesting(0);

        final StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        // Somewhat arbitrary, just pick a tab with an index in (0, numTabs).
        final int closeTabIndex = 8;

        final StripLayoutHelper stripLayoutHelperSpy = spy(mStripLayoutHelper);
        stripLayoutHelperSpy.handleCloseButtonClick(
                tabs[closeTabIndex], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);

        final Animator runningAnimator = stripLayoutHelperSpy.getRunningAnimatorForTesting();
        // Initial animation is the tab removal animation, and after that ends the
        // resizeStripOnTabClose animations begin.
        runningAnimator.end();

        final ArgumentCaptor<List<Animator>> animationListCaptor =
                ArgumentCaptor.forClass(List.class);
        final InOrder stripLayoutOrder = inOrder(stripLayoutHelperSpy);
        stripLayoutOrder.verify(stripLayoutHelperSpy).startAnimations(any(), any());
        stripLayoutOrder
                .verify(stripLayoutHelperSpy)
                .startAnimations(animationListCaptor.capture(), any());
        final List<Animator> animationList = animationListCaptor.getValue();
        // Only the tabs that come after the closed tab should have to move and get animations
        // created, plus the new tab button offset animation.
        final int expectedAnimationCount = numTabs - closeTabIndex;
        assertEquals(expectedAnimationCount, animationList.size());
    }

    @Test
    public void
            testResizeStripOnTabClose_DoNotAnimateIfNotVisible_OutsideVisibleBounds_ToTheRight() {
        final int numTabs = 50;
        initializeTest(false, false, 0, numTabs);
        // Trigger a size change so the strip layout tab heights and widths get set.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        // Simplify the visible bounds by removing the right fade.
        mStripLayoutHelper.setRightFadeWidth(0);
        // Set the initial scroll offset to trigger an update to draw X positions.
        mStripLayoutHelper.setScrollOffsetForTesting(0);

        final StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        // Pick a tab that will be well outside of the visible bounds to the right.
        final int closeTabIndex = 40;
        assertTrue(
                "Tab getting closed should be outside of the visible bounds",
                tabs[closeTabIndex].getDrawX() > mStripLayoutHelper.getVisibleRightBound());

        final StripLayoutHelper stripLayoutHelperSpy = spy(mStripLayoutHelper);
        stripLayoutHelperSpy.handleCloseButtonClick(
                tabs[closeTabIndex], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);

        final Animator runningAnimator = stripLayoutHelperSpy.getRunningAnimatorForTesting();
        // Initial animation is the tab removal animation, and after that ends the
        // resizeStripOnTabClose animations begin.
        runningAnimator.end();

        final ArgumentCaptor<List<Animator>> animationListCaptor =
                ArgumentCaptor.forClass(List.class);
        final InOrder stripLayoutOrder = inOrder(stripLayoutHelperSpy);
        stripLayoutOrder.verify(stripLayoutHelperSpy).startAnimations(any(), any());
        stripLayoutOrder
                .verify(stripLayoutHelperSpy)
                .startAnimations(animationListCaptor.capture(), any());
        final List<Animator> animationList = animationListCaptor.getValue();
        assertEquals(
                "The only animation should be for the new tab button offset",
                1,
                animationList.size());
    }

    @Test
    public void
            testResizeStripOnTabClose_DoNotAnimateIfNotVisible_OutsideVisibleBounds_ToTheLeft() {
        final int numTabs = 50;
        initializeTest(false, false, 0, numTabs);
        // Trigger a size change so the strip layout tab heights and widths get set.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        // Simplify the visible bounds by removing the left fade.
        mStripLayoutHelper.setLeftFadeWidth(0);
        // Set the initial scroll offset to trigger an update to draw X positions.
        mStripLayoutHelper.setScrollOffsetForTesting(-1000);

        final StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();

        // Pick a tab that will be outside the visible bounds to the left.
        final int closeTabIndex = 5;

        assertTrue(
                "Tab getting closed should be outside of the visible bounds",
                tabs[closeTabIndex].getDrawX() + tabs[closeTabIndex].getWidth()
                        < mStripLayoutHelper.getVisibleLeftBound());

        final StripLayoutHelper stripLayoutHelperSpy = spy(mStripLayoutHelper);
        stripLayoutHelperSpy.handleCloseButtonClick(
                tabs[closeTabIndex], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);

        final Animator runningAnimator = stripLayoutHelperSpy.getRunningAnimatorForTesting();
        // Initial animation is the tab removal animation, and after that ends the
        // resizeStripOnTabClose animations begin.
        runningAnimator.end();

        final ArgumentCaptor<List<Animator>> animationListCaptor =
                ArgumentCaptor.forClass(List.class);
        final InOrder stripLayoutOrder = inOrder(stripLayoutHelperSpy);
        stripLayoutOrder.verify(stripLayoutHelperSpy).startAnimations(any(), any());
        stripLayoutOrder
                .verify(stripLayoutHelperSpy)
                .startAnimations(animationListCaptor.capture(), any());
        final List<Animator> animationList = animationListCaptor.getValue();
        assertEquals(
                "There should be 11 animations for the visible tabs, "
                        + "plus the new tab button offset animation",
                12,
                animationList.size());
    }

    @Test
    public void testResizeStripOnTabClose_AnimateTab_MovingIntoVisibleBounds() {
        final int numTabs = 50;
        initializeTest(false, false, 0, numTabs);
        // Trigger a size change so the strip layout tab heights and widths get set.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        // Set the initial scroll offset to trigger an update to draw X positions.
        mStripLayoutHelper.setScrollOffsetForTesting(0);

        final StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        // Simplify the visible bounds by removing the right fade.
        mStripLayoutHelper.setRightFadeWidth(0);

        final int firstNotVisibleIndex =
                IntStream.range(0, tabs.length)
                        .filter(i -> tabs[i].getDrawX() > mStripLayoutHelper.getVisibleRightBound())
                        .findFirst()
                        .getAsInt();

        final int closeTabIndex = firstNotVisibleIndex - 1;
        assertTrue(
                "Tab getting closed should be inside of the visible bounds",
                tabs[closeTabIndex].getDrawX() <= mStripLayoutHelper.getVisibleRightBound());

        final StripLayoutHelper stripLayoutHelperSpy = spy(mStripLayoutHelper);
        stripLayoutHelperSpy.handleCloseButtonClick(
                tabs[closeTabIndex], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);

        final Animator runningAnimator = stripLayoutHelperSpy.getRunningAnimatorForTesting();
        // Initial animation is the tab removal animation, and after that ends the
        // resizeStripOnTabClose animations begin.
        runningAnimator.end();

        final ArgumentCaptor<List<Animator>> animationListCaptor =
                ArgumentCaptor.forClass(List.class);
        final InOrder stripLayoutOrder = inOrder(stripLayoutHelperSpy);
        stripLayoutOrder.verify(stripLayoutHelperSpy).startAnimations(any(), any());
        stripLayoutOrder
                .verify(stripLayoutHelperSpy)
                .startAnimations(animationListCaptor.capture(), any());
        final List<Animator> animationList = animationListCaptor.getValue();
        assertEquals(
                "There should be one animation for the tab moving into the visible bounds, "
                        + "plus the new tab button offset animation",
                2,
                animationList.size());
    }

    @Test
    public void testComputeAndUpdateTabWidth_DontAnimateIfSizeNotChanging() {
        // Create a high number of tabs to ensure they're already at the minimum size
        final int numTabs = 50;
        initializeTest(false, false, 0, numTabs);
        // Trigger a size change so the strip layout tab heights and widths get set.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        // Set the initial scroll offset to trigger an update to draw X positions.
        mStripLayoutHelper.setScrollOffsetForTesting(0);

        assertEquals(
                "Tabs should be at minimum width for this test to be valid",
                MIN_TAB_WIDTH_DP,
                mStripLayoutHelper.getCachedTabWidthForTesting(),
                EPSILON);

        final StripLayoutHelper stripLayoutHelperSpy = spy(mStripLayoutHelper);
        mModel.addTab("New tab");
        stripLayoutHelperSpy.tabCreated(
                TIMESTAMP, mModel.getTabAt(mModel.getCount() - 1).getId(), 0, true, false, false);

        final ArgumentCaptor<List<Animator>> animationListCaptor =
                ArgumentCaptor.forClass(List.class);
        verify(stripLayoutHelperSpy).startAnimations(animationListCaptor.capture(), any());
        final List<Animator> animationList = animationListCaptor.getValue();
        assertEquals(
                "There should be one animation for the newly created tab width, "
                        + "plus one more animation for the new tab y offset",
                2,
                animationList.size());
    }

    @Test
    public void testAllTabsClosed() {
        initializeTest(false, false, 0);
        assertTrue(
                mStripLayoutHelper.getStripLayoutTabsForTesting().length == TEST_TAB_TITLES.length);

        // Close all tabs
        mModel.getTabRemover()
                .closeTabs(TabClosureParams.closeAllTabs().build(), /* allowDialog= */ false);

        // Notify strip of tab closure
        mStripLayoutHelper.willCloseAllTabs();

        // Verify strip has no tabs.
        assertTrue(mStripLayoutHelper.getStripLayoutTabsForTesting().length == 0);
    }

    @Test
    public void testTabSelected_SelectedNonLastTab_ShowCloseBtn() {
        initializeTest(false, true, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Non-last tab not overlapping strip fade:
        // drawX(530) + tabWidth(140 - 28) < width(800) - offsetXRight(20) - longRightFadeWidth(136)
        when(tabs[3].getDrawX()).thenReturn(530.f);
        mStripLayoutHelper.tabSelected(TIMESTAMP, 3, Tab.INVALID_TAB_ID);

        // Close btn is visible on the selected tab.
        verify(tabs[3]).setCanShowCloseButton(true, false);
        // Close btn is hidden for the rest of tabs.
        verify(tabs[0]).setCanShowCloseButton(false, false);
        verify(tabs[1]).setCanShowCloseButton(false, false);
        verify(tabs[2]).setCanShowCloseButton(false, false);
        verify(tabs[4]).setCanShowCloseButton(false, false);
    }

    @Test
    public void testTabSelected_SelectedNonLastTab_HideCloseBtn() {
        initializeTest(false, true, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Non-last tab overlapping strip fade:
        // drawX(600) + tabWidth(140 - 28) > width(800) - offsetXRight(20) - longRightFadeWidth(136)
        when(tabs[3].getDrawX()).thenReturn(600.f);
        mStripLayoutHelper.tabSelected(TIMESTAMP, 3, Tab.INVALID_TAB_ID);

        // Close btn is hidden on the selected tab.
        verify(tabs[3]).setCanShowCloseButton(false, false);
        // Close btn is hidden for the rest of tabs as well.
        verify(tabs[0]).setCanShowCloseButton(false, false);
        verify(tabs[1]).setCanShowCloseButton(false, false);
        verify(tabs[2]).setCanShowCloseButton(false, false);
        verify(tabs[4]).setCanShowCloseButton(false, false);
    }

    @Test
    public void testTabSelected_SelectedLastTab_ShowCloseBtn() {
        initializeTest(false, true, 4);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.getNewTabButton().setDrawX(NEW_TAB_BTN_X);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Last tab not overlapping NTB:
        // drawX(550) > NTB_X(700) + tabOverlapWidth(28) - tabWidth(140)
        when(tabs[4].getDrawX()).thenReturn(550.f);
        mStripLayoutHelper.tabSelected(TIMESTAMP, 4, Tab.INVALID_TAB_ID);

        // Close btn is visible on the selected last tab.
        verify(tabs[4]).setCanShowCloseButton(true, false);
        // Close button is hidden for the rest of tabs.
        verify(tabs[0]).setCanShowCloseButton(false, false);
        verify(tabs[1]).setCanShowCloseButton(false, false);
        verify(tabs[2]).setCanShowCloseButton(false, false);
        verify(tabs[3]).setCanShowCloseButton(false, false);
    }

    @Test
    public void testTabSelected_SelectedLastTab_HideCloseBtn() {
        initializeTest(false, true, 4);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.getNewTabButton().setDrawX(NEW_TAB_BTN_X);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Last tab overlapping NTB:
        // drawX(600) > NTB_X(700) + tabOverlapWidth(28) - tabWidth(140)
        when(tabs[4].getDrawX()).thenReturn(600.f);
        mStripLayoutHelper.tabSelected(TIMESTAMP, 4, Tab.INVALID_TAB_ID);

        // Close btn is hidden on the selected last tab.
        verify(tabs[4]).setCanShowCloseButton(false, false);
        // Close button is hidden for the rest of tabs.
        verify(tabs[0]).setCanShowCloseButton(false, false);
        verify(tabs[1]).setCanShowCloseButton(false, false);
        verify(tabs[2]).setCanShowCloseButton(false, false);
        verify(tabs[3]).setCanShowCloseButton(false, false);
    }

    @Test
    public void testTabSelected_SelectedNonLastTab_NoModelSelBtn_HideCloseBtn() {
        initializeTest(false, false, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Non-last tab overlapping strip fade:
        // drawX(630) + tabWidth(140 - 28) > width(800) - offsetXRight(20) -
        // mediumRightFadeWidth(72)
        when(tabs[3].getDrawX()).thenReturn(630.f);
        mStripLayoutHelper.tabSelected(TIMESTAMP, 3, Tab.INVALID_TAB_ID);

        // Close button is hidden for selected tab.
        verify(tabs[3]).setCanShowCloseButton(false, false);
        // Close button is hidden for the rest of tabs as well.
        verify(tabs[0]).setCanShowCloseButton(false, false);
        verify(tabs[1]).setCanShowCloseButton(false, false);
        verify(tabs[2]).setCanShowCloseButton(false, false);
        verify(tabs[4]).setCanShowCloseButton(false, false);
    }

    @Test
    public void testTabSelected_SelectedNonLastTab_NoModelSelBtn_ShowCloseBtn() {
        initializeTest(false, false, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Non-last tab not overlapping strip fade:
        // drawX(580) + tabWidth(140 - 28) > width(800) - offsetXRight(20) -
        // mediumRightFadeWidth(72)
        when(tabs[3].getDrawX()).thenReturn(580.f);
        mStripLayoutHelper.tabSelected(TIMESTAMP, 3, Tab.INVALID_TAB_ID);

        // Close button is visible for selected tab
        verify(tabs[3]).setCanShowCloseButton(true, false);
        // Close button is hidden for the rest of tabs.
        verify(tabs[0]).setCanShowCloseButton(false, false);
        verify(tabs[1]).setCanShowCloseButton(false, false);
        verify(tabs[2]).setCanShowCloseButton(false, false);
        verify(tabs[4]).setCanShowCloseButton(false, false);
    }

    @Test
    public void testTabSelected_SelectedLastTab_Rtl_HideCloseBtn() {
        initializeTest(true, false, 4);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.getNewTabButton().setDrawX(NEW_TAB_BTN_X_RTL);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Last tab overlapping NTB:
        // drawX(100) + tabOverlapWidth(28) < NTB_X(100) + NTB_WIDTH(100)
        when(tabs[4].getDrawX()).thenReturn(100.f);
        mStripLayoutHelper.tabSelected(TIMESTAMP, 4, Tab.INVALID_TAB_ID);

        // Close button is hidden for the selected last tab.
        verify(tabs[4]).setCanShowCloseButton(false, false);
        // Close button is hidden for the rest of tabs as well.
        verify(tabs[0]).setCanShowCloseButton(false, false);
        verify(tabs[1]).setCanShowCloseButton(false, false);
        verify(tabs[2]).setCanShowCloseButton(false, false);
        verify(tabs[3]).setCanShowCloseButton(false, false);
    }

    @Test
    public void testTabSelected_SelectedLastTab_Rtl_ShowCloseBtn() {
        initializeTest(true, false, 4);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.getNewTabButton().setDrawX(NEW_TAB_BTN_X_RTL);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Last tab not overlapping NTB:
        // drawX(200) + tabOverlapWidth(28) > NTB_X(100) + NTB_WIDTH(100)
        when(tabs[4].getDrawX()).thenReturn(200.f);
        mStripLayoutHelper.tabSelected(TIMESTAMP, 4, Tab.INVALID_TAB_ID);

        // Close button is visible for selected last tab.
        verify(tabs[4]).setCanShowCloseButton(true, false);
        // Close button is hidden for the rest of tabs.
        verify(tabs[0]).setCanShowCloseButton(false, false);
        verify(tabs[1]).setCanShowCloseButton(false, false);
        verify(tabs[2]).setCanShowCloseButton(false, false);
        verify(tabs[3]).setCanShowCloseButton(false, false);
    }

    @Test
    public void testTabSelected_SelectedNonLastTab_Rtl_HideCloseBtn() {
        initializeTest(true, false, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Non-last tab overlapping strip fade:
        // drawX(50) + tabOverlapWidth(28) < offsetXRight(20) + mediumRightFadeWidth(72)
        when(tabs[3].getDrawX()).thenReturn(50.f);
        mStripLayoutHelper.tabSelected(TIMESTAMP, 3, Tab.INVALID_TAB_ID);

        // Close btn is hidden for selected tab.
        verify(tabs[3]).setCanShowCloseButton(false, false);
        // Close btn is hidden for all the rest of tabs as well.
        verify(tabs[0]).setCanShowCloseButton(false, false);
        verify(tabs[1]).setCanShowCloseButton(false, false);
        verify(tabs[2]).setCanShowCloseButton(false, false);
        verify(tabs[4]).setCanShowCloseButton(false, false);
    }

    @Test
    public void testTabSelected_SelectedNonLastTab_Rtl_ShowCloseBtn() {
        initializeTest(true, false, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.getNewTabButton().setDrawX(NEW_TAB_BTN_X);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Non-last tab not overlapping strip fade:
        // drawX(70) + tabOverlapWidth(28) > offsetXRight(20) + mediumRightFadeWidth(72)
        when(tabs[3].getDrawX()).thenReturn(70.f);
        mStripLayoutHelper.tabSelected(TIMESTAMP, 3, Tab.INVALID_TAB_ID);

        // Close button is visible for the selected tab.
        verify(tabs[3]).setCanShowCloseButton(true, false);
        // Close button is hidden for the rest of tabs.
        verify(tabs[0]).setCanShowCloseButton(false, false);
        verify(tabs[1]).setCanShowCloseButton(false, false);
        verify(tabs[2]).setCanShowCloseButton(false, false);
        verify(tabs[4]).setCanShowCloseButton(false, false);
    }

    @Test
    public void testUpdateDividers_WithTabSelected() {
        // Setup with 5 tabs. Select tab 2.
        initializeTest(false, false, 2);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        // group 2nd and 3rd tab.
        groupTabs(1, 3);

        // Trigger update to set divider values.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify tabs 2 and 3's start dividers are hidden due to selection.
        assertFalse(
                "First start divider should always be hidden.", tabs[0].isStartDividerVisible());
        assertFalse(
                "Start divider is next to group indicator and should be hidden.",
                tabs[1].isStartDividerVisible());
        assertFalse(
                "Start divider is for selected tab and should be hidden.",
                tabs[2].isStartDividerVisible());
        assertFalse(
                "Start divider is adjacent to selected tab and should be hidden.",
                tabs[3].isStartDividerVisible());
        assertTrue("Start divider should be visible.", tabs[4].isStartDividerVisible());

        // Verify only last tab's end divider is visible.
        assertTrue(
                "End divider is next to group indicator should be visible.",
                tabs[0].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[1].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[2].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[3].isEndDividerVisible());
        assertTrue("End divider should be visible.", tabs[4].isEndDividerVisible());
    }

    @Test
    public void testUpdateDividers_InReorderMode() {
        // Setup with 5 tabs. Select 2nd tab.
        initializeTest(false, false, 1, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Start reorder mode at 2nd tab
        mStripLayoutHelper.startReorderModeAtIndexForTesting(1);
        // Trigger update to set divider values.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        // Verify only 4th and 5th tab's start divider is visible.
        assertFalse(
                "First start divider should always be hidden.", tabs[0].isStartDividerVisible());
        assertFalse("Start divider should be hidden.", tabs[1].isStartDividerVisible());
        assertFalse("Start divider should be hidden.", tabs[2].isStartDividerVisible());
        assertTrue("Start divider should be visible.", tabs[3].isStartDividerVisible());
        assertTrue("Start divider should be visible.", tabs[4].isStartDividerVisible());

        // Verify end divider visible only for 5th tab.
        assertFalse("End divider should be hidden.", tabs[0].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[1].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[2].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[3].isEndDividerVisible());
        assertTrue("End divider should be visible.", tabs[4].isEndDividerVisible());
    }

    @Test
    public void testUpdateDividers_InReorderModeWithTabGroups() {
        // Setup with 5 tabs. Select 2nd tab.
        initializeTest(false, false, 1, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        // group 2nd and 3rd tab.
        groupTabs(1, 3);

        // Start reorder mode at 2nd tab
        mStripLayoutHelper.startReorderModeAtIndexForTesting(1);
        // Trigger update to set divider values.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        // Verify only 4th and 5th tab's start divider is visible.
        assertFalse(
                "First start divider should always be hidden.", tabs[0].isStartDividerVisible());
        assertFalse("Start divider should be hidden.", tabs[1].isStartDividerVisible());
        assertFalse("Start divider should be hidden.", tabs[2].isStartDividerVisible());
        assertTrue("Start divider should be visible.", tabs[3].isStartDividerVisible());
        assertTrue("Start divider should be visible.", tabs[4].isStartDividerVisible());

        // Verify end divider visible for 1st and 5th tab.
        assertTrue("End divider should be visible.", tabs[0].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[1].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[2].isEndDividerVisible());
        assertFalse("End divider should be hidden.", tabs[3].isEndDividerVisible());
        assertTrue("End divider should be visible.", tabs[4].isEndDividerVisible());
    }

    @Test
    public void testUpdateForegroundTabContainers() {
        // Setup with 5 tabs. Select tab 2.
        initializeTest(false, false, 2);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();

        // Trigger update to set foreground container visibility.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify tabs 2 and 3's dividers are hidden due to selection.
        float hiddenOpacity = StripLayoutHelper.TAB_OPACITY_HIDDEN;
        float visibleOpacity = StripLayoutHelper.TAB_OPACITY_VISIBLE;
        assertEquals(
                "Tab is not selected and container should not be visible.",
                hiddenOpacity,
                tabs[0].getContainerOpacity(),
                EPSILON);
        assertEquals(
                "Tab is not selected and container should not be visible.",
                hiddenOpacity,
                tabs[1].getContainerOpacity(),
                EPSILON);
        assertEquals(
                "Tab is selected and container should be visible.",
                visibleOpacity,
                tabs[2].getContainerOpacity(),
                EPSILON);
        assertEquals(
                "Tab is not selected and container should not be visible.",
                hiddenOpacity,
                tabs[3].getContainerOpacity(),
                EPSILON);
        assertEquals(
                "Tab is not selected and container should not be visible.",
                hiddenOpacity,
                tabs[4].getContainerOpacity(),
                EPSILON);
    }

    @Test
    public void testNewTabButtonYPosition_Folio() {
        int tabCount = 4;
        initializeTest(false, false, 3, tabCount);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Set New tab button position.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify new tab button y-position.
        assertEquals(
                "New tab button y-position is not as expected",
                3.f,
                mStripLayoutHelper.getNewTabButton().getDrawY(),
                EPSILON);
    }

    @Test
    public void testNewTabButtonXPosition() {
        // Setup
        int tabCount = 1;
        initializeTest(false, false, 0, tabCount);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify new tab button position.
        // tabWidth(237) + tabOverLapWidth(28) - ntbOffsetTowardsTabs(4) + offsetXLeft(10) = 271
        assertEquals(
                "New tab button x-position is not as expected",
                271.f,
                mStripLayoutHelper.getNewTabButton().getDrawX(),
                EPSILON);
        // rightBound(311) = expectedNtbDrawX(271) + ntbWidth(32) + touchSlop(8)
        assertEquals(
                "TouchableRect does not match. Right size should match ntb.getDrawX() + width.",
                new RectF(PADDING_LEFT, 0, 311.f, SCREEN_HEIGHT),
                mStripLayoutHelper.getTouchableRect());
    }

    @Test
    public void testNewTabButtonXPosition_TabStripFull() {
        // Setup
        int tabCount = 5;
        initializeTest(false, false, 0, tabCount);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify new tab button position.
        // stripWidth(800) - offsetXRight(20) - stripEndPadding(8) - NtbWidth(32) = 740
        assertEquals(
                "New tab button x-position is not as expected",
                740.f,
                mStripLayoutHelper.getNewTabButton().getDrawX(),
                EPSILON);
        assertEquals(
                "TouchableRect does not match. Strip is full, touch size should match the strip.",
                new RectF(PADDING_LEFT, 0, SCREEN_WIDTH - PADDING_RIGHT, SCREEN_HEIGHT),
                mStripLayoutHelper.getTouchableRect());
    }

    @Test
    public void testNewTabButtonXPosition_Rtl() {
        int tabCount = 1;
        initializeTest(true, false, 0, tabCount);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify new tab button position.
        // stripWidth(800) - offsetXRight(20) - tabWidth(237) - tabOverLapWidth(28) - NtbWidth(32) +
        // ntbOffsetTowardsTabs(4) = 487
        assertEquals(
                "New tab button x-position is not as expected",
                487,
                mStripLayoutHelper.getNewTabButton().getDrawX(),
                EPSILON);
        // leftBound(479) = drawX(487) - touchSlop(8)
        assertEquals(
                "TouchableRect does not match. Left side should equal to ntb.getDrawX()",
                new RectF(479.f, 0, SCREEN_WIDTH - PADDING_RIGHT, SCREEN_HEIGHT),
                mStripLayoutHelper.getTouchableRect());
    }

    @Test
    public void testNewTabButtonXPosition_TabStripFull_Rtl() {
        // Setup
        int tabCount = 5;
        initializeTest(true, false, 0, tabCount);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify new tab button position.
        // offsetXLeft(10) + buttonEndPadding(8) = 28.
        assertEquals(
                "New tab button x-position is not as expected",
                18.f,
                mStripLayoutHelper.getNewTabButton().getDrawX(),
                EPSILON);
        assertEquals(
                "TouchableRect does not match. Strip is full, touch size should match the strip.",
                new RectF(PADDING_LEFT, 0, SCREEN_WIDTH - PADDING_RIGHT, SCREEN_HEIGHT),
                mStripLayoutHelper.getTouchableRect());
    }

    @Test
    public void testNewTabButtonStyle_ButtonStyleDisabled() {
        int tabCount = 1;
        initializeTest(false, false, 0, tabCount);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        assertEquals(
                "Unexpected incognito button color.",
                AppCompatResources.getColorStateList(mContext, R.color.default_icon_color_tint_list)
                        .getDefaultColor(),
                ((org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton)
                                mStripLayoutHelper.getNewTabButton())
                        .getTint());
    }

    @Test
    @Feature("Advanced Peripherals Support")
    public void testNewTabButtonHoverHighlightProperties() {
        // Setup
        initializeTest(false, false, 0, 1);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify new tab button hover highlight resource id.
        assertEquals(
                "New tab button hover highlight is not as expected",
                R.drawable.bg_circle_tab_strip_button,
                mStripLayoutHelper.getNewTabButton().getBackgroundResourceId());

        // Verify new tab button hover highlight default tint.
        TintedCompositorButton ntb = spy(mStripLayoutHelper.getNewTabButton());
        when(ntb.isHovered()).thenReturn(true);

        int defaultNtbHoverBackgroundTint =
                ColorUtils.setAlphaComponent(
                        SemanticColorUtils.getDefaultTextColor(mContext), (int) (0.08 * 255));
        assertEquals(
                "New tab button hover highlight default tint is not as expected",
                defaultNtbHoverBackgroundTint,
                ntb.getBackgroundTint());

        // Verify new tab button hover highlight pressed tint.
        when(ntb.isHovered()).thenReturn(false);
        when(ntb.isPressed()).thenReturn(true);
        when(ntb.isPressedFromMouse()).thenReturn(true);
        int pressedNtbHoverBackgroundTint =
                ColorUtils.setAlphaComponent(
                        SemanticColorUtils.getDefaultTextColor(mContext), (int) (0.12 * 255));
        assertEquals(
                "New tab button hover highlight pressed tint is not as expected",
                pressedNtbHoverBackgroundTint,
                ntb.getBackgroundTint());
        when(ntb.isPressedFromMouse()).thenReturn(false);

        // Verify new tab button incognito hover highlight default tint.
        when(ntb.isHovered()).thenReturn(true);
        when(ntb.isIncognito()).thenReturn(true);
        int defaultNtbHoverBackgroundIncognitoTint =
                ColorUtils.setAlphaComponent(
                        mContext.getColor(R.color.tab_strip_button_hover_bg_color),
                        (int) (0.08 * 255));
        assertEquals(
                "New tab button hover highlight default tint is not as expected",
                defaultNtbHoverBackgroundIncognitoTint,
                ntb.getBackgroundTint());

        // Verify new tab button incognito hover highlight pressed tint.
        when(ntb.isHovered()).thenReturn(false);
        when(ntb.isPressed()).thenReturn(true);
        when(ntb.isPressedFromMouse()).thenReturn(true);
        int hoverBackgroundPressedIncognitoColor =
                ColorUtils.setAlphaComponent(
                        mContext.getColor(R.color.tab_strip_button_hover_bg_color),
                        (int) (0.12 * 255));
        assertEquals(
                "New tab button hover highlight pressed tint is not as expected",
                hoverBackgroundPressedIncognitoColor,
                ntb.getBackgroundTint());
    }

    @Test
    @Feature("Advanced Peripherals Support")
    public void testNewTabButtonHoverEnter() {
        // Setup
        initializeTest(false, false, 0, 1);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify new tab button is hovered.
        int x = (int) mStripLayoutHelper.getNewTabButton().getDrawX();
        mStripLayoutHelper.onHoverEnter(
                x + 1, 0); // mouse position within NTB range(32dp width + 12dp click slop).
        assertTrue(
                "New tab button should be hovered",
                mStripLayoutHelper.getNewTabButton().isHovered());

        // Verify new tab button is NOT hovered
        mStripLayoutHelper.onHoverEnter(
                x + 45, 0); // mouse position out of NTB range(32dp width + 12dp click slop).
        assertFalse(
                "New tab button should NOT be hovered",
                mStripLayoutHelper.getNewTabButton().isHovered());
    }

    @Test
    @Feature("Advanced Peripherals Support")
    public void testNewTabButtonHoverOnDown() {
        // Setup
        initializeTest(false, false, 0, 1);

        // Verify new tab button is in pressed state, not hover state, when clicked from mouse.
        mStripLayoutHelper.onDown(mStripLayoutHelper.getNewTabButton().getDrawX() + 1, 0, 1);
        assertFalse(
                "New tab button should not be hovered",
                mStripLayoutHelper.getNewTabButton().isHovered());
        assertTrue(
                "New tab button should be pressed from mouse",
                mStripLayoutHelper.getNewTabButton().isPressedFromMouse());
    }

    @Test
    @Feature("Advanced Peripherals Support")
    public void testCloseButtonHoverHighlightProperties() {
        // Setup
        initializeTest(false, false, 2);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        TintedCompositorButton closeButton = spy(tabs[0].getCloseButton());

        // Verify close button hover highlight resource id.
        assertEquals(
                "Close button hover highlight is not as expected",
                R.drawable.tab_close_button_bg,
                tabs[0].getCloseButton().getBackgroundResourceId());

        // Verify the non-hover background tint for the close button. It should always be
        // transparent, as no background should be applied when it is not being hovered over.
        assertEquals(
                "Close button non-hover background tint is not as expected",
                Color.TRANSPARENT,
                closeButton.getBackgroundTint());

        // Verify close button hover highlight default tint.
        when(closeButton.isHovered()).thenReturn(true);

        int defaultCloseButtonHoverBackgroundTint =
                ColorUtils.setAlphaComponent(
                        SemanticColorUtils.getDefaultTextColor(mContext), (int) (0.08 * 255));
        assertEquals(
                "Close button hover highlight default tint is not as expected",
                defaultCloseButtonHoverBackgroundTint,
                closeButton.getBackgroundTint());

        // Verify close button hover highlight pressed tint.
        when(closeButton.isHovered()).thenReturn(false);
        when(closeButton.isPressed()).thenReturn(true);
        when(closeButton.isPressedFromMouse()).thenReturn(true);
        int pressedCloseButtonHoverBackgroundTint =
                ColorUtils.setAlphaComponent(
                        SemanticColorUtils.getDefaultTextColor(mContext), (int) (0.12 * 255));
        assertEquals(
                "Close button hover highlight pressed tint is not as expected",
                pressedCloseButtonHoverBackgroundTint,
                closeButton.getBackgroundTint());

        when(closeButton.isPressed()).thenReturn(false);
        when(closeButton.isPressedFromMouse()).thenReturn(false);

        // Verify close button incognito hover highlight default tint.
        when(closeButton.isIncognito()).thenReturn(true);
        when(closeButton.isHovered()).thenReturn(true);
        int defaultNtbHoverBackgroundIncognitoTint =
                ColorUtils.setAlphaComponent(
                        mContext.getColor(R.color.tab_strip_button_hover_bg_color),
                        (int) (0.08 * 255));
        assertEquals(
                "Close button hover highlight default tint is not as expected",
                defaultNtbHoverBackgroundIncognitoTint,
                closeButton.getBackgroundTint());

        // Verify close button incognito hover highlight pressed tint.
        when(closeButton.isHovered()).thenReturn(false);
        when(closeButton.isPressed()).thenReturn(true);
        when(closeButton.isPressedFromMouse()).thenReturn(true);
        int hoverBackgroundPressedIncognitoColor =
                ColorUtils.setAlphaComponent(
                        mContext.getColor(R.color.tab_strip_button_hover_bg_color),
                        (int) (0.12 * 255));
        assertEquals(
                "Close button hover highlight pressed tint is not as expected",
                hoverBackgroundPressedIncognitoColor,
                closeButton.getBackgroundTint());
    }

    @Test
    @Feature("Advanced Peripherals Support")
    public void testCloseButtonHoverEnter() {
        // Setup
        initializeTest(false, false, 2);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutTab tab = spy(tabs[0]);
        TintedCompositorButton closeButton =
                new TintedCompositorButton(
                        mContext,
                        ButtonType.TAB_CLOSE,
                        tab,
                        24.f,
                        24.f,
                        mClickHandler,
                        mKeyboardFocusHandler,
                        R.drawable.btn_tab_close_normal,
                        0f);
        closeButton.setOpacity(1.f);
        int x = (int) closeButton.getDrawX();
        int y = (int) closeButton.getDrawY();
        mStripLayoutHelper.setTabAtPositionForTesting(tab);
        tab.setCloseButtonForTesting(closeButton);
        tab.setShowingCloseButtonForTesting(true);

        // Verify close button is hovered on.
        mStripLayoutHelper.onHoverEnter(
                x + 1,
                y + 1); // mouse position within close button range(24dp width + 12dp click slop)
        assertTrue("Close button should be hovered", tab.isCloseHovered());

        // Verify close button is NOT hovered on.
        mStripLayoutHelper.onHoverEnter(
                x + 37,
                y); // mouse position out of close button range(24dp width + 12dp click slop).
        assertFalse("Close button should NOT be hovered on", tab.isCloseHovered());
    }

    @Test
    @Feature("Advanced Peripherals Support")
    public void testCloseButtonHoverOnDown() {
        // Setup
        initializeTest(false, false, 2);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        TintedCompositorButton closeButton =
                new TintedCompositorButton(
                        mContext,
                        ButtonType.TAB_CLOSE,
                        tabs[0],
                        24.f,
                        24.f,
                        mClickHandler,
                        mKeyboardFocusHandler,
                        R.drawable.btn_tab_close_normal,
                        0f);
        closeButton.setOpacity(1.f);
        int x = (int) closeButton.getDrawX();
        int y = (int) closeButton.getDrawY();
        tabs[0].setCloseButtonForTesting(closeButton);

        // Verify close button is in pressed state, not hover state, when clicked from mouse.
        mStripLayoutHelper.onDown(x + 1, y + 1, 1);
        assertFalse("Close button should not be hovered", closeButton.isHovered());
        mStripLayoutHelper.onDown((int) x + 1, y + 1, 1);
        assertFalse("Close should NOT be hovered", closeButton.isPressedFromMouse());

        // Verify close button is not in hover state or press state when long-pressed.
        mStripLayoutHelper.onLongPress(x + 1, y + 1);
        assertFalse("Close button should NOT be hovered", closeButton.isHovered());
        assertFalse("Close button should NOT be pressed", closeButton.isPressed());
    }

    @Test
    public void testScrollOffset_OnResume_StartOnLeft_SelectedRightmostTab() {
        // Arrange: Initialize tabs with tenth tab selected and MSB visible (long fade).
        initializeTest(false, true, 9, 12);
        mStripLayoutHelper.setIsFirstLayoutPassForTesting(false);
        float scrollOffsetBefore = mStripLayoutHelper.getScrollOffset();
        StripLayoutTab selectedTab = mStripLayoutHelper.getStripLayoutTabsForTesting()[9];

        // Set screen width to 800dp and scroll selected tab to view.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        mStripLayoutHelper.scrollTabToView(TIMESTAMP, false);
        // Complete animations
        mStripLayoutHelper.finishScrollForTesting();

        // optimalEndDelta =
        // stripWidth(800) - rightPadding(60) - rightFade(136) - selectedTab.idealX -
        // mCachedTabWidth(108)
        // expectedOffset = scrollOffsetBefore + optimalEndDelta
        float expectedOffset =
                scrollOffsetBefore
                        + SCREEN_WIDTH
                        - 60
                        - StripLayoutHelperManager.FADE_LONG_WIDTH_DP
                        - selectedTab.getIdealX()
                        - 108;
        assertEquals(expectedOffset, mStripLayoutHelper.getScrollOffset(), EPSILON);
    }

    @Test
    public void testScrollOffset_OnResume_StartOnLeft_NoModelSelBtn_SelectedRightmostTab() {
        // Arrange: Initialize tabs with tenth tab selected and MSB not visible (medium fade).
        initializeTest(false, false, 9, 12);
        mStripLayoutHelper.setIsFirstLayoutPassForTesting(false);
        float scrollOffsetBefore = mStripLayoutHelper.getScrollOffset();
        StripLayoutTab selectedTab = mStripLayoutHelper.getStripLayoutTabsForTesting()[9];

        // Set screen width to 800dp and scroll selected tab to view.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        mStripLayoutHelper.scrollTabToView(TIMESTAMP, false);
        // Complete scroll to update offset.
        mStripLayoutHelper.finishScrollForTesting();

        // optimalEndDelta =
        // stripWidth(800) - rightPadding(60) - rightFade(72) - selectedTab.idealX -
        // mCachedTabWidth(108)
        // expectedOffset = scrollOffsetBefore + optimalEndDelta
        float expectedOffset =
                scrollOffsetBefore
                        + SCREEN_WIDTH
                        - 60
                        - StripLayoutHelperManager.FADE_MEDIUM_WIDTH_DP
                        - selectedTab.getIdealX()
                        - 108;
        assertEquals(expectedOffset, mStripLayoutHelper.getScrollOffset(), EPSILON);
    }

    @Test
    public void testScrollOffset_OnResume_StartOnRight_SelectedLeftmostTab() {
        // Arrange: Initialize tabs with first tab selected.
        initializeTest(false, true, 0, 10);

        // Set screen width to 800dp and scroll selected tab to view.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.scrollTabToView(TIMESTAMP, false);

        // optimalStart = leftFade(60) + leftPadding(10) - (index(0) * tabWidth(108-28))
        int expectedFinalX = 70;
        assertEquals(expectedFinalX, mStripLayoutHelper.getScrollerForTesting().getFinalX());
    }

    @Test
    public void testScrollOffset_OnOrientationChange_SelectedTabVisible() {
        // Arrange: Initialize tabs with last tab selected.
        initializeTest(false, false, 9, 10);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_SMALL, 150.f, 10);
        when(tabs[9].isVisible()).thenReturn(true);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);
        StripLayoutTab selectedTab = mStripLayoutHelper.getStripLayoutTabsForTesting()[9];

        // Set screen width to 1200 to start.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH_LANDSCAPE,
                SCREEN_HEIGHT,
                false,
                TIMESTAMP,
                PADDING_LEFT,
                PADDING_RIGHT,
                0f);

        // Assert: finalX value before orientation change.
        int initialFinalX = 0;
        assertEquals(initialFinalX, mStripLayoutHelper.getScrollerForTesting().getFinalX());

        // Act: change orientation.
        // drawX: tabWidth(108-28) * 9
        when(tabs[9].getDrawX()).thenReturn(720.f);
        when(tabs[9].getIdealX()).thenReturn(720.f);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, true, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Assert: finalX value after orientation change.
        // stripWidth(800) - rightPadding(60) - rightFade(72) - selectedTab.idealX -
        // mCachedTabWidth(108)
        float expectedFinalX =
                SCREEN_WIDTH
                        - 60
                        - StripLayoutHelperManager.FADE_MEDIUM_WIDTH_DP
                        - selectedTab.getIdealX()
                        - 108;
        assertEquals(
                expectedFinalX, mStripLayoutHelper.getScrollerForTesting().getFinalX(), EPSILON);
    }

    @Test
    public void testScrollOffset_OnOrientationChange_SelectedTabNotVisible() {
        // Arrange: Initialize tabs with last tab selected.
        initializeTest(false, false, 9, 10);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM, 150.f, 10);
        when(tabs[9].isVisible()).thenReturn(false);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Set screen width to 1200 to start
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH_LANDSCAPE,
                SCREEN_HEIGHT,
                false,
                TIMESTAMP,
                PADDING_LEFT,
                PADDING_RIGHT,
                0f);

        // Assert: finalX value before orientation change.
        int initialFinalX = 0;
        assertEquals(initialFinalX, mStripLayoutHelper.getScrollerForTesting().getFinalX());

        // Act: change orientation.
        when(tabs[9].getDrawX()).thenReturn(-1.f);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, true, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Assert: finalX value remains the same on orientation change.
        assertEquals(initialFinalX, mStripLayoutHelper.getScrollerForTesting().getFinalX());
    }

    @Test
    public void testTabSelected_AfterTabClose_SkipsAutoScroll() {
        initializeTest(false, true, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);
        // Set initial scroller position to 1000.
        mStripLayoutHelper.getScrollerForTesting().setFinalX(1000);

        // Act: close a non selected tab.
        mStripLayoutHelper.handleCloseButtonClick(
                tabs[1], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);

        // Assert: scroller position is not modified.
        assertEquals(1000, mStripLayoutHelper.getScrollerForTesting().getFinalX());
    }

    @Test
    public void testTabSelected_AfterSelectedTabClose_SkipsAutoScroll() {
        initializeTest(false, true, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);
        // Set initial scroller position to 1000.
        mStripLayoutHelper.getScrollerForTesting().setFinalX(1000);

        // Act: close the selected tab.
        mStripLayoutHelper.handleCloseButtonClick(
                tabs[3], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);

        // Assert: scroller position is not modified.
        assertEquals(1000, mStripLayoutHelper.getScrollerForTesting().getFinalX());
    }

    @Test
    public void testScroll_onKeyboardFocus() {
        // Arrange: Initialize tabs with last tab selected.
        initializeTest(false, true, 11, 12);

        // Set screen width to 800dp and scroll selected tab to view.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        StripLayoutTab tabToFocus = mStripLayoutHelper.getStripLayoutTabsForTesting()[0];

        // Set keyboard focus state of the last tab, as if we focus looped around.
        tabToFocus.setKeyboardFocused(true);

        // Complete animations.
        mStripLayoutHelper.finishScrollForTesting();

        assertEquals(0.0, mStripLayoutHelper.getScrollOffset(), EPSILON);
    }

    @Test
    public void testInReorderMode_StripStartMargin() {
        // Initialize.
        initializeTest(false, false, 5);
        groupTabs(0, 2);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Update layout.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Start reorder mode on the third tab.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(2);

        // Verify that we enter reorder mode.
        assertTrue("Should in reorder mode.", mStripLayoutHelper.getInReorderModeForTesting());

        // Assert: StripStartMargin is about 1/4 tab width to create space for dragging first tab
        // out of group on strip.
        float expectedMargin =
                ((mStripLayoutHelper.getCachedTabWidthForTesting() - TAB_OVERLAP_WIDTH_DP) / 2)
                        * REORDER_OVERLAP_SWITCH_PERCENTAGE;
        assertEquals(
                "StripStartMargin is incorrect",
                expectedMargin,
                mStripLayoutHelper.getStripStartMarginForReorderForTesting(),
                0.1f);

        // Assert: There should be a scroll offset equal to counter the stripStartMargin, so that
        // the interacting tab would remain visually stationary.
        assertEquals(
                "scrollOffset is incorrect",
                -expectedMargin,
                mStripLayoutHelper.getScrollOffset(),
                0.1f);
    }

    @Test
    public void testInReorderMode_StripEndMargin() {
        // Initialize.
        initializeTest(false, false, 4);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        groupTabs(3, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Update layout.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Start reorder mode on the fourth tab.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(3);

        // Verify that we enter reorder mode.
        assertTrue("Should in reorder mode.", mStripLayoutHelper.getInReorderModeForTesting());

        // Assert: Last tab's trailingMargin should be about 1/4 tab width to create space for
        // dragging last tab out of group on strip.
        float expectedMargin =
                ((mStripLayoutHelper.getCachedTabWidthForTesting() - TAB_OVERLAP_WIDTH_DP) / 2)
                        * REORDER_OVERLAP_SWITCH_PERCENTAGE;
        assertEquals(
                "Strip end margin is incorrect", expectedMargin, tabs[4].getTrailingMargin(), 0.1f);
    }

    @Test
    public void testTabCreated_Animation() {
        // Initialize with default amount of tabs. Clear any animations.
        initializeTest(false, false, 3);
        mStripLayoutHelper.finishAnimationsAndPushTabUpdates();
        assertNull(
                "Animation should not be running.",
                mStripLayoutHelper.getRunningAnimatorForTesting());

        // Act: Create new tab in model and trigger update in tab strip.
        mModel.addTab("new tab");
        mStripLayoutHelper.tabCreated(TIMESTAMP, 5, 3, true, false, false);

        // Assert: Animation is running.
        assertNotNull(
                "Animation should running.", mStripLayoutHelper.getRunningAnimatorForTesting());
    }

    @Test
    public void testTabCreated_RestoredTab_SkipsAutoscroll() {
        initializeTest(false, true, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        // Set initial scroller position to 1200.
        mStripLayoutHelper.getScrollerForTesting().setFinalX((int) SCREEN_WIDTH_LANDSCAPE);

        // Act: Tab was restored after undoing a tab closure.
        boolean closureCancelled = true;
        mModel.addTab("new tab");
        mStripLayoutHelper.tabCreated(TIMESTAMP, 5, 3, false, closureCancelled, false);

        // Assert: scroller position is not modified.
        assertEquals(1200, mStripLayoutHelper.getScrollerForTesting().getFinalX());
    }

    @Test
    public void testTabCreated_NonRestoredTab_Autoscrolls() {
        testTabCreated_NonRestoredTab_Autoscrolls(/* isRtl= */ false);
    }

    @Test
    public void testTabCreated_NonRestoredTab_Autoscrolls_Rtl() {
        testTabCreated_NonRestoredTab_Autoscrolls(/* isRtl= */ true);
    }

    private void testTabCreated_NonRestoredTab_Autoscrolls(boolean isRtl) {
        initializeTest(isRtl, true, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_MEDIUM);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        // Set initial scroller position to 1200.
        mStripLayoutHelper.getScrollerForTesting().setFinalX((int) SCREEN_WIDTH_LANDSCAPE);

        // Act: Tab was not restored after undoing a tab closure.
        boolean closureCancelled = false;
        mModel.addTab("new tab");
        mStripLayoutHelper.tabCreated(TIMESTAMP, 5, 3, false, closureCancelled, false);

        // Assert: scroller position is modified.
        assertNotEquals(1200, mStripLayoutHelper.getScrollerForTesting().getFinalX());
    }

    @Test
    public void testTabCreated_BringSelectedTabToVisibleArea_StartupRestoredUnselectedTab() {
        testTabCreated_BringSelectedTabToVisibleArea_StartupRestoredUnselectedTab(
                /* isRtl= */ false);
    }

    @Test
    public void testTabCreated_BringSelectedTabToVisibleArea_StartupRestoredUnselectedTab_Rtl() {
        testTabCreated_BringSelectedTabToVisibleArea_StartupRestoredUnselectedTab(
                /* isRtl= */ true);
    }

    private void testTabCreated_BringSelectedTabToVisibleArea_StartupRestoredUnselectedTab(
            boolean isRtl) {
        // Setup:
        int selectedTabIndex = 1;
        initializeTest(isRtl, false, selectedTabIndex, 11);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Set initial scroller position to -500 under ScrollDelegate's dynamic coordinate system
        // (not the static window coordinate system), which means:
        // * For LTR layouts: scroll the tab strip to the left by 500dp.
        // * For RTL layouts: scroll the tab strip to the right by 500dp.
        // In both cases, the selected tab (selectedTabIndex) should not be visible, which means:
        // * For LTR layouts: the selected tab's ideal X is to the left of the window's left edge.
        // * For RTL layouts: the selected tab's ideal X is to the right of the window's right edge.
        mStripLayoutHelper.setScrollOffsetForTesting(-500);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // mStripLayoutHelper.getScrollOffset() returns a vector under ScrollDelegate's dynamic
        // coordinate system, so we need to convert it to the static window coordinate system for
        // consistency in the test.
        float scrollOffsetBefore =
                MathUtils.flipSignIf(mStripLayoutHelper.getScrollOffset(), isRtl);
        StripLayoutTab selectedTab =
                mStripLayoutHelper.getStripLayoutTabsForTesting()[selectedTabIndex];

        // Act: Tab was restored during startup.
        mModel.addTab("new tab");
        mStripLayoutHelper.tabCreated(
                TIMESTAMP, 12, 12, /* selected= */ false, false, /* onStartup= */ true);

        // Assert: We don't scroll to the newly created tab because the selected tab is not visible,
        // so we should scroll to the selected tab.
        // First, calculate the expected scroll offset under the static window coordinate system.
        float expectedScrollOffset;
        if (isRtl) {
            // Width reserved on the right side of the window.
            float reservedWidthRight = PADDING_RIGHT + StripLayoutHelperManager.FADE_SHORT_WIDTH_DP;

            // The setup moved the selected tab beyond the window's right edge. To make the tab
            // visible, we should scroll the tab strip to the left, i.e., "scrollDelta" below should
            // be negative.
            float expectedTabX = SCREEN_WIDTH - reservedWidthRight - selectedTab.getWidth();
            float scrollDelta = expectedTabX - selectedTab.getIdealX();
            expectedScrollOffset = scrollOffsetBefore + scrollDelta;
        } else {
            // Width reserved on the left side of the window.
            // This is also the expected X position for the tab to be visible.
            float reservedWidthLeft = PADDING_LEFT + StripLayoutHelperManager.FADE_SHORT_WIDTH_DP;

            // The setup moved the selected tab beyond the window's left edge. To make the tab
            // visible, we should scroll the tab to the right, i.e., "scrollDelta" below should be
            // positive.
            float scrollDelta = reservedWidthLeft - selectedTab.getIdealX();
            expectedScrollOffset = scrollOffsetBefore + scrollDelta;
        }

        // mStripLayoutHelper.getScrollOffset() returns a vector under ScrollDelegate's dynamic
        // coordinate system, so we need to convert it to the static window coordinate system for
        // consistency in the test.
        float actualScrollOffset =
                MathUtils.flipSignIf(mStripLayoutHelper.getScrollOffset(), isRtl);
        assertEquals(
                "We should scroll to the selected tab",
                expectedScrollOffset,
                actualScrollOffset,
                EPSILON);
    }

    @Test
    public void testOnDown_OnNewTabButton() {
        // Initialize.
        initializeTest(false, false, 0, 5);

        // Set new tab button location and dimensions.
        mStripLayoutHelper.getNewTabButton().setDrawX(NEW_TAB_BTN_X);
        mStripLayoutHelper.getNewTabButton().setDrawY(NEW_TAB_BTN_Y);
        mStripLayoutHelper.getNewTabButton().setWidth(NEW_TAB_BTN_WIDTH);
        mStripLayoutHelper.getNewTabButton().setHeight(NEW_TAB_BTN_HEIGHT);

        // Press down on new tab button.
        // CenterX = getX() + (getWidth() / 2) = 700 + (100 / 2) = 750
        // CenterY = getY() + (getHeight() / 2) = 1400 + (100 / 2) = 1450
        mStripLayoutHelper.onDown(750f, 1450f, 0);

        // Verify.
        assertTrue(
                "New tab button should be pressed.",
                mStripLayoutHelper.getNewTabButton().isPressed());
        assertFalse(
                "Should not start reorder mode when pressing down on new tab button.",
                mStripLayoutHelper.getInReorderModeForTesting());
    }

    @Test
    public void testOnDown_OnTab() {
        // Initialize.
        initializeTest(false, false, 0, 5);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Press down on second tab.
        when(tabs[1].checkCloseHitTest(anyFloat(), anyFloat())).thenReturn(false);
        mStripLayoutHelper.setTabAtPositionForTesting(tabs[1]);
        mStripLayoutHelper.onDown(150f, 0f, 0);

        // Verify.
        assertFalse(
                "New tab button should not be pressed.",
                mStripLayoutHelper.getNewTabButton().isPressed());
        assertNull(
                "No tab was clicked by mouse.",
                mStripLayoutHelper.getDelayedReorderViewForTesting());
        assertFalse(
                "Should not start reorder mode when pressing down on tab without mouse.",
                mStripLayoutHelper.getInReorderModeForTesting());
        verify(tabs[1], never()).setClosePressed(anyBoolean(), anyInt());
    }

    @Test
    public void testOnDownAndDrag_OnTab_WithMouse() {
        // Initialize.
        initializeTest(false, false, 0, 5);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Press down on second tab with mouse followed by drag.
        when(tabs[1].checkCloseHitTest(anyFloat(), anyFloat())).thenReturn(false);
        mStripLayoutHelper.setTabAtPositionForTesting(tabs[1]);
        mStripLayoutHelper.onDown(DRAG_START_POINT.x, 0, MotionEvent.BUTTON_PRIMARY);
        mStripLayoutHelper.drag(TIMESTAMP, DRAG_START_POINT.x, DRAG_START_POINT.y, 30f);

        // Verify.
        assertEquals(
                "Second tab should be interacting tab.",
                tabs[1],
                mStripLayoutHelper.getInteractingTabForTesting());
        assertTrue(
                "Should start reorder mode when dragging on pressed on tab with mouse.",
                mStripLayoutHelper.getInReorderModeForTesting());
        verify(mTabDragSource)
                .startTabDragAction(
                        mToolbarContainerView,
                        mModel.getTabAt(1),
                        DRAG_START_POINT,
                        tabs[1].getDrawX(),
                        tabs[1].getWidth());
    }

    @Test
    public void testOnDown_OnTabCloseButton() {
        // Initialize.
        initializeTest(false, false, 0, 5);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Press down on second tab's close button.
        when(tabs[1].checkCloseHitTest(anyFloat(), anyFloat())).thenReturn(true);
        mStripLayoutHelper.setTabAtPositionForTesting(tabs[1]);
        mStripLayoutHelper.onDown(150f, 0f, 0);

        // Verify.
        assertFalse(
                "New tab button should not be pressed.",
                mStripLayoutHelper.getNewTabButton().isPressed());
        assertNull(
                "No tab was clicked by mouse.",
                mStripLayoutHelper.getDelayedReorderViewForTesting());
        assertFalse(
                "Should not start reorder mode from close button.",
                mStripLayoutHelper.getInReorderModeForTesting());
        verify(tabs[1]).setClosePressed(eq(true), eq(0));
    }

    @Test
    public void testOnDown_OnTabCloseButton_WithMouse() {
        // Initialize.
        initializeTest(false, false, 0, 5);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Press down on second tab's close button with mouse.
        when(tabs[1].checkCloseHitTest(anyFloat(), anyFloat())).thenReturn(true);
        mStripLayoutHelper.setTabAtPositionForTesting(tabs[1]);
        mStripLayoutHelper.onDown(150f, 0f, MotionEvent.BUTTON_PRIMARY);

        // Verify.
        assertFalse(
                "New tab button should not be pressed.",
                mStripLayoutHelper.getNewTabButton().isPressed());
        assertNull(
                "No tab was clicked by mouse.",
                mStripLayoutHelper.getDelayedReorderViewForTesting());
        assertFalse(
                "Should not start reorder mode from close button.",
                mStripLayoutHelper.getInReorderModeForTesting());
        verify(tabs[1]).setClosePressed(eq(true), eq(MotionEvent.BUTTON_PRIMARY));
    }

    @Test
    public void testOnDown_WhileScrolling() {
        // Initialize and assert scroller is finished.
        initializeTest(false, false, 0, 5);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);
        assertTrue(
                "Scroller should be finished right after initializing.",
                mStripLayoutHelper.getScrollerForTesting().isFinished());

        // Start scroll and assert scroller is not finished.
        mStripLayoutHelper.getScrollerForTesting().startScroll(0, 0, 0, 0, TIMESTAMP, 1000);
        assertFalse(
                "Scroller should not be finished after starting scroll.",
                mStripLayoutHelper.getScrollerForTesting().isFinished());

        // Press down on second tab and assert scroller is finished.
        mStripLayoutHelper.setTabAtPositionForTesting(tabs[1]);
        mStripLayoutHelper.onDown(150f, 0f, 0);
        assertFalse(
                "New tab button should not be pressed.",
                mStripLayoutHelper.getNewTabButton().isPressed());
        assertFalse(
                "Should not start reorder mode when pressing down down on strip to stop scroll.",
                mStripLayoutHelper.getInReorderModeForTesting());
        assertTrue(
                "Scroller should be force finished after pressing down on strip.",
                mStripLayoutHelper.getScrollerForTesting().isFinished());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.TAB_STRIP_CONTEXT_MENU})
    public void testOnLongPress_OnTab() {
        var tabs = initializeTest_ForTab();
        onLongPress_OnTab(tabs);
        // Verify that we don't show the tab menu.
        assertFalse(
                "Should not show tab menu after long press on tab.",
                mStripLayoutHelper.isCloseButtonMenuShowingForTesting());
        // Verify we directly enter reorder mode.
        assertTrue(
                "Should be in reorder mode after long press on tab.",
                mStripLayoutHelper.getInReorderModeForTesting());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_STRIP_CONTEXT_MENU})
    @Feature("Tab Context Menu")
    public void testOnLongPress_OnTab_FeaturesEnabled() {
        var tabs = initializeTest_ForTab();
        setupForIndividualTabContextMenu();
        // Long press on tab
        onLongPress_OnTab(tabs);
        // Verify we performed haptic feedback for a long-press.
        verify(mToolbarContainerView, times(1))
                .performHapticFeedback(eq(HapticFeedbackConstants.LONG_PRESS));
        ArgumentCaptor<RectProvider> rectProviderArgumentCaptor =
                ArgumentCaptor.forClass(RectProvider.class);
        // Verify tab context menu is showing.
        verify(mTabContextMenuCoordinator).showMenu(rectProviderArgumentCaptor.capture(), anyInt());
        // Verify anchorView coordinates.
        StripLayoutView view = mStripLayoutHelper.getViewAtPositionX(10f, true);
        assertThat(view, instanceOf(StripLayoutTab.class));
        Rect expectedRect = new Rect();
        view.getAnchorRect(expectedRect);
        Rect actualRect = rectProviderArgumentCaptor.getValue().getRect();
        assertEquals("Anchor view for menu is positioned incorrectly", expectedRect, actualRect);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_STRIP_CONTEXT_MENU})
    @Feature("Tab Context Menu")
    public void testOnLongPress_OnTab_WithTopPadding_AndScreenDensity() {
        DisplayMetrics displayMetrics = mContext.getResources().getDisplayMetrics();
        float densityBeforeTest = displayMetrics.density;
        float densityForTest = 2.0f;
        displayMetrics.density = densityForTest;

        var tabs = initializeTest_ForTab();
        setupForIndividualTabContextMenu();
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH,
                SCREEN_HEIGHT,
                false,
                TIMESTAMP,
                PADDING_LEFT,
                PADDING_RIGHT,
                PADDING_TOP);
        // Long press on tab.
        onLongPress_OnTab(tabs);
        // Set this up to capture rectProvider.
        ArgumentCaptor<RectProvider> rectProviderArgumentCaptor =
                ArgumentCaptor.forClass(RectProvider.class);
        // Verify tab context menu is showing.
        verify(mTabContextMenuCoordinator).showMenu(rectProviderArgumentCaptor.capture(), anyInt());
        // Verify anchorView coordinates.
        StripLayoutView view = mStripLayoutHelper.getViewAtPositionX(10f, true);
        assertThat(view, instanceOf(StripLayoutTab.class));
        Rect expectedRect = new Rect();
        view.getAnchorRect(expectedRect);
        expectedRect.offset(0, Math.round(densityForTest * PADDING_TOP));
        Rect actualRect = rectProviderArgumentCaptor.getValue().getRect();
        assertEquals(
                "Anchor view for menu should take into account top padding and screen density",
                expectedRect,
                actualRect);

        displayMetrics.density = densityBeforeTest; // Clean up.
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_STRIP_CONTEXT_MENU})
    @Feature("Tab Context Menu")
    public void testTabContextMenu_PreventsHovercard() {
        // Setup.
        initializeTabHoverTest();
        mStripLayoutHelper.setTabContextMenuCoordinatorForTesting(mTabContextMenuCoordinator);
        when(mTabContextMenuCoordinator.isMenuShowing()).thenReturn(true);

        // Now try to hover on the tab.
        mStripLayoutHelper.updateLastHoveredTab(
                mStripLayoutHelper.getStripLayoutTabsForTesting()[0]);

        verify(mTabHoverCardView, never())
                .show(
                        nullable(Tab.class),
                        anyBoolean(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_STRIP_CONTEXT_MENU})
    @Feature("Tab Group Context Menu")
    public void testTabGroupContextMenu_PreventsHovercard() {
        // Setup.
        initializeTabHoverTest();
        mStripLayoutHelper.setTabGroupContextMenuCoordinatorForTesting(
                mTabGroupContextMenuCoordinator);
        when(mTabGroupContextMenuCoordinator.isMenuShowing()).thenReturn(true);

        // Now try to hover on the tab.
        mStripLayoutHelper.updateLastHoveredTab(
                mStripLayoutHelper.getStripLayoutTabsForTesting()[0]);

        verify(mTabHoverCardView, never())
                .show(
                        nullable(Tab.class),
                        anyBoolean(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_STRIP_CONTEXT_MENU})
    @Feature("Advanced Peripherals Support")
    public void testCloseTabsContextMenu_PreventsHovercard() {
        // Set up: see testOnLongPress_OnCloseButton setup.
        initializeTest(false, false, 0);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Mock tab's view.
        View tabView = new View(mActivity);
        tabView.setLayoutParams(new MarginLayoutParams(150, 50));
        when(mModel.getTabAt(1).getView()).thenReturn(tabView);

        // Long press on second tab's close button.
        StripLayoutTab tab = tabs[1];
        when(tab.checkCloseHitTest(anyFloat(), anyFloat())).thenReturn(true);
        when(tab.getCloseButton()).thenReturn(mCloseButton);
        when(mCloseButton.getParentView()).thenReturn(tab);
        when(mCloseButton.getType()).thenReturn(ButtonType.TAB_CLOSE);
        mStripLayoutHelper.setTabAtPositionForTesting(tab);
        mStripLayoutHelper.onLongPress(150f, 0f);

        assertTrue(
                "Expected 'close all tabs' context menu to be showing",
                mStripLayoutHelper.isCloseButtonMenuShowingForTesting());

        // Now try to hover on the tab.
        mStripLayoutHelper.updateLastHoveredTab(tab);

        verify(mTabHoverCardView, never())
                .show(
                        nullable(Tab.class),
                        anyBoolean(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_STRIP_CONTEXT_MENU})
    @Feature("Tab Context Menu")
    public void testBottomSheet_constructedWithoutDestroyHide() {
        var tabs = initializeTest_ForTab();
        setupForContextMenu();
        when(mModel.getTabById(anyInt())).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(URL);

        // Initialize the menu.
        mStripLayoutHelper.showTabContextMenuForTesting(tabs[0]);

        verify(mBottomSheetCoordinatorFactory, times(1))
                .create(
                        eq(mActivity),
                        eq(mProfile),
                        any(),
                        any(),
                        eq(mTabGroupModelFilter),
                        eq(mBottomSheetController),
                        eq(true),
                        eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_CONTEXT_MENU)
    @Config(sdk = Build.VERSION_CODES.R)
    public void testOnLongPress_WithDragDrop_OnTab_ContextMenuEnabled() {
        var tabs = initializeTest_ForTab();
        setTabDragSourceMock();
        setupForIndividualTabContextMenu();
        mStripLayoutHelper.onTabStateInitialized(); // drag is disabled if tab state is not init'ed
        onLongPress_OnTab(tabs);

        // Make the drag delta larger than INITIATE_REORDER_DRAG_THRESHOLD
        mStripLayoutHelper.drag(TIMESTAMP, /* x= */ 110f, /* y= */ 10f, /* deltaX= */ 40f);
        assertTrue(mStripLayoutHelper.getReorderDelegateForTesting().getInReorderMode());
    }

    /** Sets up tabModel and menu coordinator. */
    private void setupForContextMenu() {
        MockTabModel tabModel = new MockTabModel(mProfile, null);
        when(mProfile.isOffTheRecord()).thenReturn(true);
        mStripLayoutHelper.setTabModel(tabModel, mTabCreator, false);
        tabModel.setActive(true);
    }

    private void setupForGroupContextMenu() {
        setupForContextMenu();
        mStripLayoutHelper.setTabGroupContextMenuCoordinatorForTesting(
                mTabGroupContextMenuCoordinator);
    }

    private void setupForIndividualTabContextMenu() {
        setupForContextMenu();
        when(mModel.getTabById(anyInt())).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(URL);
        mStripLayoutHelper.setTabContextMenuCoordinatorForTesting(mTabContextMenuCoordinator);
    }

    @Test
    @Feature("Tab Group Context Menu")
    public void testOnLongPress_OnGroupTitle() {
        // Initialize.
        initializeTest(false, false, 0);
        groupTabs(0, 1);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);
        // NTB is after group indicator and tabs.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.getNewTabButton().setDrawX(TAB_WIDTH_1 + tabs.length * TAB_WIDTH_1);
        setupForGroupContextMenu();

        // Long press on group title.
        mStripLayoutHelper.onLongPress(10f, 0f);
        // Verify we performed haptic feedback for a long-press.
        verify(mToolbarContainerView, times(1))
                .performHapticFeedback(eq(HapticFeedbackConstants.LONG_PRESS));
        ArgumentCaptor<RectProvider> rectProviderArgumentCaptor =
                ArgumentCaptor.forClass(RectProvider.class);
        // Verify tab group context menu is showing.
        verify(mTabGroupContextMenuCoordinator)
                .showMenu(rectProviderArgumentCaptor.capture(), any());
        // Verify anchorView coordinates.
        StripLayoutView view = mStripLayoutHelper.getViewAtPositionX(10f, true);
        assertTrue(view instanceof StripLayoutGroupTitle);
        StripLayoutGroupTitle titleView = (StripLayoutGroupTitle) view;
        Rect expectedRect =
                new Rect(
                        Math.round(titleView.getPaddedX()),
                        Math.round(titleView.getDrawY()),
                        Math.round(titleView.getPaddedX() + titleView.getPaddedWidth()),
                        Math.round(titleView.getDrawY() + titleView.getHeight()));
        Rect actualRect = rectProviderArgumentCaptor.getValue().getRect();
        assertEquals("Anchor view for menu is positioned incorrectly", expectedRect, actualRect);
    }

    @Test
    @Feature("Tab Group Context Menu")
    public void testDragToScroll_WithoutContextMenu() {
        // Initialize.
        initializeTest(false, false, 0);
        groupTabs(0, 1);
        setupForGroupContextMenu();

        // Verify drag without context menu starts a scroll.
        mStripLayoutHelper.drag(TIMESTAMP, /* x= */ 10f, /* y= */ 10f, /* deltaX= */ 10f);
        assertTrue(
                "Scroll should be in progress.",
                mStripLayoutHelper.getIsStripScrollInProgressForTesting());
    }

    @Test
    @Feature("Tab Group Context Menu")
    public void testDragToScroll_WithContextMenu() {
        // Initialize.
        initializeTest(false, false, 0);
        groupTabs(0, 1);
        setupForGroupContextMenu();

        // Long press on group title and verify drag with context menu does not start a scroll.
        when(mTabGroupContextMenuCoordinator.isMenuShowing()).thenReturn(true);
        mStripLayoutHelper.drag(TIMESTAMP, /* x= */ 10f, /* y= */ 10f, /* deltaX= */ 10f);
        assertFalse(
                "Scroll should not be in progress.",
                mStripLayoutHelper.getIsStripScrollInProgressForTesting());
    }

    @Test
    @Feature("Tab Group Context Menu")
    @EnableFeatures({
        ChromeFeatureList.TAB_STRIP_GROUP_REORDER,
        ChromeFeatureList.TAB_STRIP_GROUP_DRAG_DROP_ANDROID
    })
    public void testDrag_DismissContextMenu() {
        // Initialize.
        initializeTest(false, false, 0);
        groupTabs(0, 1);
        setupForGroupContextMenu();
        // NTB is after group indicator and tabs.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        mStripLayoutHelper.getNewTabButton().setDrawX(views.length * views[0].getWidth());

        // Long press on group title and verify drag with context menu does not start a scroll.
        // Long press on group title.
        mStripLayoutHelper.onLongPress(10f, 0f);
        verify(mTabGroupContextMenuCoordinator).showMenu(any(), any());
        when(mTabGroupContextMenuCoordinator.isMenuShowing()).thenReturn(true);
        mStripLayoutHelper.drag(TIMESTAMP, /* x= */ 60f, /* y= */ 10f, /* deltaX= */ 50f);
        verify(mTabGroupContextMenuCoordinator).dismiss();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_CONTEXT_MENU)
    @Config(sdk = Build.VERSION_CODES.R)
    public void testOnLongPress_WithDragDrop_OnTab() {
        var tabs = initializeTest_ForTab();
        setTabDragSourceMock();
        onLongPress_OnTab(tabs);
        // Verify drag invoked
        verify(mTabDragSource).startTabDragAction(any(), any(), any(), anyFloat(), anyFloat());
    }

    private StripLayoutTab[] initializeTest_ForTab() {
        initializeTest(false, false, 0);

        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);
        return tabs;
    }

    private void onLongPress_OnTab(StripLayoutTab[] tabs) {
        // Long press on second tab.
        when(tabs[1].checkCloseHitTest(anyFloat(), anyFloat())).thenReturn(false);
        mStripLayoutHelper.setTabAtPositionForTesting(tabs[1]);
        mStripLayoutHelper.onLongPress(150f, 0f);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_STRIP_CONTEXT_MENU})
    public void testOnLongPress_OnCloseButton() {
        // Initialize.
        initializeTest(false, false, 0);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Mock tab's view.
        View tabView = new View(mActivity);
        tabView.setLayoutParams(new MarginLayoutParams(150, 50));
        when(mModel.getTabAt(1).getView()).thenReturn(tabView);

        // Long press on second tab's close button.
        TintedCompositorButton closeButton = mock(TintedCompositorButton.class);
        when(tabs[1].checkCloseHitTest(anyFloat(), anyFloat())).thenReturn(true);
        when(tabs[1].getCloseButton()).thenReturn(closeButton);
        when(closeButton.getParentView()).thenReturn(tabs[1]);
        when(closeButton.getType()).thenReturn(ButtonType.TAB_CLOSE);
        mStripLayoutHelper.setTabAtPositionForTesting(tabs[1]);
        mStripLayoutHelper.onLongPress(150f, 0f);

        // Verify that we show the popup menu anchored on the close button.
        assertFalse(
                "Should not be in reorder mode after long press on tab close button.",
                mStripLayoutHelper.getInReorderModeForTesting());
        assertTrue(
                "Should show menu anchored on close button after long press.",
                mStripLayoutHelper.isCloseButtonMenuShowingForTesting());
    }

    @Test
    public void testOnLongPress_OffTab() {
        setupDragDropState();
        onLongPress_OffTab();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testOnLongPress_WithDragDrop_OffTab() {
        // Extra setup for DragDrop
        setTabDragSourceMock();
        Activity activity = spy(mActivity);
        when(mToolbarContainerView.getContext()).thenReturn(activity);

        onLongPress_OffTab();
        // verify tab drag not invoked.
        verifyNoInteractions(mTabDragSource);
    }

    private void onLongPress_OffTab() {
        // Initialize.
        initializeTest(false, false, 0);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Long press past the last tab.
        mStripLayoutHelper.setTabAtPositionForTesting(null);
        mStripLayoutHelper.onLongPress(150f, 0f);

        // Verify that we show the popup menu anchored on the close button.
        assertFalse(
                "Should not be in reorder mode after long press on empty space on tab strip.",
                mStripLayoutHelper.getInReorderModeForTesting());
        assertFalse(
                "Should not show after long press on empty space on tab strip.",
                mStripLayoutHelper.isCloseButtonMenuShowingForTesting());
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testTabGroupMargins_BetweenTabs() {
        // Initialize with 3 tabs.
        initializeTest(false, false, 0, 3);
        setupDragDropState();
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Start reorder.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);

        // Verify no tabs have a trailing margin, since there are no tab groups.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        assertEquals(EXPECTED_NO_MARGIN, 0f, tabs[0].getTrailingMargin(), EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, 0f, tabs[1].getTrailingMargin(), EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, 0f, tabs[2].getTrailingMargin(), EPSILON);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testTabGroupMargins_ResetMarginsOnStopReorder() {
        // Mock 1 tab to the left of a tab group with 3 tabs.
        initializeTest(false, false, 0, 4);
        setupDragDropState();
        groupTabs(1, 4);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Start then stop reorder.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);
        mStripLayoutHelper.stopReorderMode();

        // Verify no tabs have a trailing margin when reordering is stopped.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        assertEquals(EXPECTED_NO_MARGIN, 0f, tabs[0].getTrailingMargin(), EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, 0f, tabs[1].getTrailingMargin(), EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, 0f, tabs[2].getTrailingMargin(), EPSILON);
        assertEquals(EXPECTED_NO_MARGIN, 0f, tabs[3].getTrailingMargin(), EPSILON);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testTabGroupMargins_NoScrollOnReorder() {
        // Mock 2 tabs to the left and 1 tab to the right of a tab group with two tabs.
        initializeTest(false, false, 0, 5);
        setupDragDropState();
        groupTabs(2, 4);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.setScrollOffsetForTesting(0);

        // Start reorder on leftmost tab. No margins to left of tab, so shouldn't scroll.
        // Verify the scroll offset is still 0.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);
        assertEquals(
                "There are no margins left of the selected tab, so we shouldn't scroll.",
                0f,
                mStripLayoutHelper.getScrollOffset(),
                EPSILON);

        // Stop reorder. Verify the scroll offset is still 0.
        mStripLayoutHelper.stopReorderMode();
        assertEquals(
                "Scroll offset should return to 0 after stopping reorder mode.",
                0f,
                mStripLayoutHelper.getScrollOffset(),
                EPSILON);
    }

    @Test
    public void testTabOutline_SelectedTabInGroup_Show() {
        // Initiailze 5 tabs and make 2 tab groups each containing 2 tabs.
        initializeTest(false, false, 0, 5);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);
        groupTabs(0, 2);
        groupTabs(2, 4);

        // Test tab outline should show for selected tab in group.
        assertTrue(
                "Tab outline should show for selected tab in group",
                mStripLayoutHelper.shouldShowTabOutline(tabs[0]));

        // Test tab outline should not show for the rest of tabs.
        assertFalse(
                "Tab outline should not show.", mStripLayoutHelper.shouldShowTabOutline(tabs[1]));
        assertFalse(
                "Tab outline should not show.", mStripLayoutHelper.shouldShowTabOutline(tabs[2]));
        assertFalse(
                "Tab outline should not show.", mStripLayoutHelper.shouldShowTabOutline(tabs[3]));
        assertFalse(
                "Tab outline should not show.", mStripLayoutHelper.shouldShowTabOutline(tabs[4]));
    }

    @Test
    public void testTabOutline_ForegroundedTabInGroup_TabDroppedOntoDestinationStrip_Show() {
        XrUtils.setXrDeviceForTesting(true);
        // Setup with 3 tabs and select the first tab.
        setupDragDropState();
        initializeTest(false, false, 0, 3);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH_LANDSCAPE,
                SCREEN_HEIGHT,
                false,
                TIMESTAMP,
                PADDING_LEFT,
                PADDING_RIGHT,
                0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        groupTabs(0, 3);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();

        // Start reorder for tab drop between the 2nd and 3rd tab.
        mStripLayoutHelper.handleDragEnter(/* currX= */ 300.f, /* lastX= */ 0f, false, false);

        // Test tab outline should show for the foregrounded tab in destination window during tab
        // drop.
        assertTrue("Tab outline should show.", mStripLayoutHelper.shouldShowTabOutline(tabs[0]));

        // Test tab outline should not show for the rest of tabs.
        assertFalse(
                "Tab outline should not show.", mStripLayoutHelper.shouldShowTabOutline(tabs[1]));
        assertFalse(
                "Tab outline should not show.", mStripLayoutHelper.shouldShowTabOutline(tabs[2]));
    }

    @Test
    public void testTabOutline_ReorderMode_NotShow() {
        // Mock 5 tabs and make 2 tab groups each containing 2 tabs.
        initializeTest(false, false, 0, 5);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1, 150f, 5);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);
        groupTabs(0, 2);
        groupTabs(2, 4);

        // Enter reorder mode.
        mStripLayoutHelper.setInReorderModeForTesting(true);

        // Test tab outline should not show for selected tab in group when enter reorder mode.
        assertFalse(
                "Tab outline should not show.", mStripLayoutHelper.shouldShowTabOutline(tabs[0]));

        // Test tab outline should not show for the rest of tabs.
        assertFalse(
                "Tab outline should not show.", mStripLayoutHelper.shouldShowTabOutline(tabs[1]));
        assertFalse(
                "Tab outline should not show.", mStripLayoutHelper.shouldShowTabOutline(tabs[2]));
        assertFalse(
                "Tab outline should not show.", mStripLayoutHelper.shouldShowTabOutline(tabs[3]));
        assertFalse(
                "Tab outline should not show.", mStripLayoutHelper.shouldShowTabOutline(tabs[4]));
    }

    @Test
    public void testReorder_SetSelectedTabGroupContainersVisible() {
        // Mock 5 tabs. Group the first two tabs.
        initializeTest(false, false, 2, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        groupTabs(0, 2);

        // Start reorder mode on third tab. Drag to hover over the tab group.
        // -100 < -marginWidth = -95
        mStripLayoutHelper.startReorderModeAtIndexForTesting(2);
        float dragDistance = -100f;
        float startX = mStripLayoutHelper.getLastReorderXForTesting();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance);

        // Verify hovered group tab containers are not visible for Tab Group Indicator.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        float expectedHidden = StripLayoutHelper.TAB_OPACITY_HIDDEN;
        float expectedVisibleForeground = StripLayoutHelper.TAB_OPACITY_VISIBLE;
        assertEquals(
                "Container in hovered group should not be visible.",
                expectedHidden,
                tabs[0].getContainerOpacity(),
                EPSILON);
        assertEquals(
                "Container in hovered group should not be visible.",
                expectedHidden,
                tabs[1].getContainerOpacity(),
                EPSILON);
        assertEquals(
                "Selected container should be visible.",
                expectedVisibleForeground,
                tabs[2].getContainerOpacity(),
                EPSILON);
        assertEquals(
                "Background containers should not be visible.",
                expectedHidden,
                tabs[3].getContainerOpacity(),
                EPSILON);
        assertEquals(
                "Background containers should not be visible.",
                expectedHidden,
                tabs[4].getContainerOpacity(),
                EPSILON);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testReorder_HapticFeedback() {
        // Mock 5 tabs.
        initializeTest(false, false, 0);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Start reorder mode on first tab.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);

        // Verify we performed haptic feedback for a long-press.
        verify(mToolbarContainerView, times(1))
                .performHapticFeedback(eq(HapticFeedbackConstants.LONG_PRESS));
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testReorder_NoGroups() {
        // Mock 5 tabs.
        initializeTest(false, false, 0, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutTab thirdTab = tabs[2];

        // Start reorder on third tab. Drag right to trigger swap with fourth tab.
        // 100 > tabWidth * flipThreshold = (190-24) * 0.53 = 88
        mStripLayoutHelper.startReorderModeAtIndexForTesting(2);
        float dragDistance = 100f;
        float startX = mStripLayoutHelper.getLastReorderXForTesting();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance);

        // Verify the TabModel was updated.
        verify(mModel).moveTab(thirdTab.getTabId(), 4);
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testReorder_DragOutOfGroup() {
        // Mock a tab group with 3 tabs with 1 tab to the left and 1 tab to the right.
        initializeTest(false, false, 0, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutTab fourthTab = tabs[3];
        groupTabs(1, 4);

        // Start reorder on fourth tab. Drag right out of the tab group.
        // 60 > marginWidth * flipThreshold = 95 * 0.53 = 51
        mStripLayoutHelper.startReorderModeAtIndexForTesting(3);
        float dragDistance = 60f;
        float startX = mStripLayoutHelper.getLastReorderXForTesting();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance);

        // Verify fourth tab was dragged out of group, but not reordered.
        assertEquals("Fourth tab should not have moved.", fourthTab, tabs[3]);
        verify(mTabUngrouper)
                .ungroupTabs(
                        eq(List.of(mModel.getTabById(fourthTab.getTabId()))),
                        /* trailing= */ eq(true),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture());
        assertNotNull(mTabModelActionListenerCaptor.getValue());
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testReorder_DragOutOfGroup_StartOfStrip() {
        // Mock a tab group with 3 tabs with 2 tabs to the right.
        initializeTest(false, false, 0, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutTab firstTab = tabs[0];
        groupTabs(0, 3);

        // Start reorder on first tab. Drag left out of the tab group.
        // -100 < -(marginWidth(95) * flipThreshold(0.53)) - groupTitleWidth(46) = -96.35
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);
        float dragDistance = -100.f;
        float startX = mStripLayoutHelper.getLastReorderXForTesting();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance);

        // Verify first tab was dragged out of group, but not reordered.
        assertEquals("First tab should not have moved.", firstTab, tabs[0]);
        verify(mTabUngrouper)
                .ungroupTabs(
                        eq(List.of(mModel.getTabById(firstTab.getTabId()))),
                        /* trailing= */ eq(false),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture());
        assertNotNull(mTabModelActionListenerCaptor.getValue());
    }

    @Test
    @Feature("Tab Groups on Tab Strip")
    public void testReorder_DragOutOfGroup_EndOfStrip() {
        // Mock a tab group with 3 tabs with 2 tabs to the left.
        initializeTest(false, false, 0, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutTab fifthTab = tabs[4];
        groupTabs(2, 5);

        // Start reorder on fifth tab. Drag right out of the tab group.
        // 60 > marginWidth * flipThreshold = 95 * 0.53 = 51
        mStripLayoutHelper.startReorderModeAtIndexForTesting(4);
        float dragDistance = 60f;
        float startX = mStripLayoutHelper.getLastReorderXForTesting();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance);

        // Verify fifth tab was dragged out of group, but not reordered.
        assertEquals("Fifth tab should not have moved.", fifthTab, tabs[4]);
        verify(mTabUngrouper)
                .ungroupTabs(
                        eq(List.of(mModel.getTabById(fifthTab.getTabId()))),
                        /* trailing= */ eq(true),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture());
        assertNotNull(mTabModelActionListenerCaptor.getValue());
    }

    @Test
    public void testReorder_MergeToGroup() {
        // Mock 5 tabs. Group the first two tabs.
        initializeTest(false, false, 0, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutTab thirdTab = tabs[2];
        int oldSecondTabId = tabs[1].getTabId();
        groupTabs(0, 2);

        // Start reorder mode on third tab. Drag between tabs in group.
        // -300 < -(tabWidth + marginWidth) = -(190 + 95) = -285
        mStripLayoutHelper.startReorderModeAtIndexForTesting(2);
        float dragDistance = -200f;
        float startX = mStripLayoutHelper.getLastReorderXForTesting();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance);

        // Verify interacting tab was merged into group.
        verify(mTabGroupModelFilter)
                .mergeTabsToGroup(eq(thirdTab.getTabId()), eq(oldSecondTabId), eq(true));
    }

    @Test
    public void testReorder_MovePastCollapsedGroup() {
        // Mock 5 tabs. Group the second and third tabs.
        initializeTest(false, false, 3, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        groupTabs(1, 3);

        // Collapse the group.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        mStripLayoutHelper.collapseTabGroupForTesting((StripLayoutGroupTitle) views[1], true);

        // Start reorder mode on fourth tab. Drag past the collapsed group.
        // -50 < -groupTitleWidth(46)
        mStripLayoutHelper.startReorderModeAtIndexForTesting(3);
        StripLayoutView draggedTab = views[4];
        assertEquals(
                "Should be dragging the fourth tab.",
                draggedTab,
                mStripLayoutHelper.getInteractingTabForTesting());

        float dragDistance = -50f;
        float startX = mStripLayoutHelper.getLastReorderXForTesting();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance);

        // Verify interacting tab was moved past the collapsed group and is now the second tab.
        verify(mModel).moveTab(mStripLayoutHelper.getInteractingTabForTesting().getTabId(), 1);
    }

    @Test
    public void testBottomIndicatorWidth_MergeToGroup() {
        // Mock 5 tabs. Group the first two tabs.
        initializeTest(false, false, 0, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutTab thirdTab = tabs[2];
        groupTabs(0, 2);
        int oldSecondTabId = tabs[1].getTabId();

        // Assert: first view should be group title.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);
        StripLayoutGroupTitle groupTitle = ((StripLayoutGroupTitle) views[0]);

        // Calculate tab and bottom indicator width.
        float tabWidth = views[1].getWidth();
        float expectedStartWidth = calculateExpectedBottomIndicatorWidth(tabWidth, 2, groupTitle);

        // Assert: bottom indicator start width.
        assertEquals(
                "Bottom indicator start width is incorrect",
                expectedStartWidth,
                groupTitle.getBottomIndicatorWidth(),
                EPSILON);

        // Start reorder mode on third tab. Drag between tabs in group.
        // -300 < -(tabWidth + marginWidth) = -(190 + 95) = -285
        mStripLayoutHelper.startReorderModeAtIndexForTesting(2);
        float dragDistance = -200f;
        float startX = mStripLayoutHelper.getLastReorderXForTesting();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance);

        // Verify interacting tab was merged into group.
        verify(mTabGroupModelFilter)
                .mergeTabsToGroup(eq(thirdTab.getTabId()), eq(oldSecondTabId), eq(true));
    }

    @Test
    public void testBottomIndicatorWidth_DragOutOfGroup() {
        // Mock 5 tabs. Group the first two tabs.
        initializeTest(false, false, 0, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutTab thirdTab = tabs[2];
        groupTabs(0, 3);

        // Assert: first view should be group title.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);
        StripLayoutGroupTitle groupTitle = ((StripLayoutGroupTitle) views[0]);

        // Calculate tab and bottom indicator width.
        float tabWidth = views[1].getWidth();
        float expectedStartWidth = calculateExpectedBottomIndicatorWidth(tabWidth, 3, groupTitle);

        // Assert: bottom indicator start width.
        assertEquals(
                "Bottom indicator start width is incorrect",
                expectedStartWidth,
                groupTitle.getBottomIndicatorWidth(),
                EPSILON);

        // Start reorder on fifth tab. Drag right out of the tab group.
        // 60 > marginWidth * flipThreshold = 95 * 0.53 = 51
        mStripLayoutHelper.startReorderModeAtIndexForTesting(2);
        float dragDistance = 60f;
        float startX = mStripLayoutHelper.getLastReorderXForTesting();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance);

        // Verify third tab was dragged out of group.
        verify(mTabUngrouper)
                .ungroupTabs(
                        eq(List.of(mModel.getTabById(thirdTab.getTabId()))),
                        /* trailing= */ eq(true),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture());
        assertNotNull(mTabModelActionListenerCaptor.getValue());
    }

    @Test
    public void testBottomIndicatorWidthAfterTabResize_UngroupedTabClosed() {
        // Arrange
        int tabCount = 6;
        initializeTest(false, false, 3, tabCount);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        groupTabs(0, 2);

        // Assert: first view should be group title.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);
        StripLayoutGroupTitle groupTitle = ((StripLayoutGroupTitle) views[0]);

        // Update layout and set up animation.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        setupForAnimations();
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Check initial bottom indicator width.
        float expectedStartWidth =
                calculateExpectedBottomIndicatorWidth(
                        mStripLayoutHelper.getCachedTabWidthForTesting(), 2, groupTitle);
        assertEquals(
                "Unexpected bottom indicator width before resize.",
                expectedStartWidth,
                groupTitle.getBottomIndicatorWidth(),
                0.1f);

        // Act: Call on close tab button handler.
        mStripLayoutHelper.handleCloseButtonClick(
                tabs[2], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);

        // Assert: Animations started.
        assertTrue(
                "MultiStepAnimations should have started.",
                mStripLayoutHelper.isMultiStepCloseAnimationsRunningForTesting());

        // Assert: Animations are still running.
        assertTrue(
                "MultiStepAnimations should still be running.",
                mStripLayoutHelper.isMultiStepCloseAnimationsRunningForTesting());

        // Act: Set animation time forward by 250ms for next set of animations.
        mStripLayoutHelper.getRunningAnimatorForTesting().end();

        // Act: End the animations to apply final values.
        Animator runningAnimator = mStripLayoutHelper.getRunningAnimatorForTesting();
        runningAnimator.end();

        // availableSize = width(800) - NTB(32) - endPadding(8) - offsetXLeft(10) - offsetXRight(20)
        // - groupTitleWidth(46) - titleOverlapWidth(4) = 680.
        // ExpectedWidth = (availableSize(680) + 4 * overlap(28)) / 5 = 160
        float expectedWidthAfterResize = 160.f;
        StripLayoutTab[] updatedTabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        for (int i = 0; i < updatedTabs.length; i++) {
            StripLayoutTab stripTab = updatedTabs[i];
            assertEquals(
                    "Unexpected tab width after resize.",
                    expectedWidthAfterResize,
                    stripTab.getWidth(),
                    0.1f);
        }
        assertFalse(
                "MultiStepAnimations should have ended.",
                mStripLayoutHelper.isMultiStepCloseAnimationsRunningForTesting());

        // Check bottom indicator end width.
        float expectedEndWidth =
                calculateExpectedBottomIndicatorWidth(expectedWidthAfterResize, 2, groupTitle);
        assertEquals(
                "Unexpected bottom indicator width after resize.",
                expectedEndWidth,
                groupTitle.getBottomIndicatorWidth(),
                0.1f);
    }

    @Test
    public void testBottomIndicatorWidthAfterTabResize_GroupedTabClosed() {
        // Arrange
        int tabCount = 6;
        initializeTest(false, false, 0, tabCount);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        groupTabs(0, 2);

        // Assert: first view should be a GroupTitle.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);
        StripLayoutGroupTitle groupTitle = ((StripLayoutGroupTitle) views[0]);

        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Check initial bottom indicator width.
        float expectedStartWidth =
                calculateExpectedBottomIndicatorWidth(
                        mStripLayoutHelper.getCachedTabWidthForTesting(), 2, groupTitle);
        assertEquals(
                "Unexpected bottom indicator width before resize.",
                expectedStartWidth,
                groupTitle.getBottomIndicatorWidth(),
                0.1f);

        setupForAnimations();
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Act: Close tab and remove from group.
        mStripLayoutHelper.handleCloseButtonClick(
                tabs[0], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);
        when(mTabGroupModelFilter.getTabCountForGroup(groupTitle.getTabGroupId())).thenReturn(1);
        mStripLayoutHelper.finishAnimationsAndPushTabUpdates();

        // availableSize = width(800) - NTB(32) - endPadding(8) - offsetXLeft(10) - offsetXRight(20)
        // - groupTitleWidth(46) - titleOverlapWidth(4) = 680
        // ExpectedWidth = (availableSize(680) + 4 * overlap(28)) / 5  = 160
        float openTabWidth = 160.f;
        StripLayoutTab[] updatedTabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        for (int i = 0; i < updatedTabs.length; i++) {
            StripLayoutTab stripTab = updatedTabs[i];
            float expectedWidth = stripTab.isClosed() ? TAB_OVERLAP_WIDTH_DP : openTabWidth;
            assertEquals(
                    "Unexpected tab width after resize.", expectedWidth, stripTab.getWidth(), 0.1f);
        }

        // Check bottom indicator end width.
        float expectedEndWidth = calculateExpectedBottomIndicatorWidth(openTabWidth, 1, groupTitle);
        assertEquals(
                "Unexpected bottom indicator width after resize.",
                expectedEndWidth,
                groupTitle.getBottomIndicatorWidth(),
                0.1f);
    }

    @Test
    public void testBottomIndicatorWidth_CollapseAndExpand() {
        // Mock 5 tabs, group first 3 tabs as group1 and group the rest as group2.
        initializeTest(false, false, 0, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        groupTabs(0, 3);
        groupTabs(3, 5);

        // Assert: the first and fourth view should be group title.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);
        assertTrue(EXPECTED_TITLE, views[4] instanceof StripLayoutGroupTitle);
        StripLayoutGroupTitle groupTitle1 = ((StripLayoutGroupTitle) views[0]);
        StripLayoutGroupTitle groupTitle2 = ((StripLayoutGroupTitle) views[4]);

        // Calculate tab and bottom indicator initial width.
        float initialTabWidth = tabs[0].getWidth();
        float expectedStartWidth1 =
                calculateExpectedBottomIndicatorWidth(initialTabWidth, 3, groupTitle1);
        float expectedStartWidth2 =
                calculateExpectedBottomIndicatorWidth(initialTabWidth, 2, groupTitle1);

        // Assert: bottom indicator start width as usual.
        assertEquals(
                "Group 1 bottom indicator start width is incorrect",
                expectedStartWidth1,
                groupTitle1.getBottomIndicatorWidth(),
                EPSILON);
        assertEquals(
                "Group 2 bottom indicator start width is incorrect",
                expectedStartWidth2,
                groupTitle2.getBottomIndicatorWidth(),
                EPSILON);

        // Click to collapse the first tab group.
        mStripLayoutHelper.collapseTabGroupForTesting((StripLayoutGroupTitle) views[0], true);

        // Assert: check bottom indicator end width for the 1st tab group should be 0.
        assertEquals(
                "Bottom indicator end width is incorrect",
                0.f,
                groupTitle1.getBottomIndicatorWidth(),
                EPSILON);

        // Assert: check bottom indicator end width for the 2nd tab group should been adjusted to
        // match the new tab width after collapse, since there are only 2 active tabs on strip, tab
        // width should become the max width.
        assertEquals(
                "Bottom indicator end width is incorrect",
                calculateExpectedBottomIndicatorWidth(265.f, 2, groupTitle2),
                groupTitle2.getBottomIndicatorWidth(),
                EPSILON);

        // Click to expand the first tab group.
        mStripLayoutHelper.collapseTabGroupForTesting((StripLayoutGroupTitle) views[0], false);

        // Assert: check bottom indicator end width for the 1st tab group has been expanded to the
        // initial length.
        assertEquals(
                "Bottom indicator end width is incorrect",
                expectedStartWidth1,
                groupTitle1.getBottomIndicatorWidth(),
                EPSILON);

        // Assert: check bottom indicator end width for the 2st tab group has been adjusted to the
        // initial length.
        assertEquals(
                "Bottom indicator end width is incorrect",
                expectedStartWidth2,
                groupTitle2.getBottomIndicatorWidth(),
                EPSILON);
    }

    @Test
    public void testBottomIndicatorWidth_TabHoveredOntoTabGroup() {
        XrUtils.setXrDeviceForTesting(true);
        // Arrange
        setupDragDropState();
        int tabCount = 6;
        initializeTest(false, false, 0, tabCount);
        groupTabs(0, 2);

        // Assert: first view should be a GroupTitle.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);
        StripLayoutGroupTitle groupTitle = ((StripLayoutGroupTitle) views[0]);

        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Check initial bottom indicator width.
        float expectedStartWidth =
                calculateExpectedBottomIndicatorWidth(
                        mStripLayoutHelper.getCachedTabWidthForTesting(), 2, groupTitle);
        assertEquals(
                "Unexpected bottom indicator width before tab hover.",
                expectedStartWidth,
                groupTitle.getBottomIndicatorWidth(),
                0.1f);

        setupForAnimations();
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Start reorder for tab drop between the 1st and 2nd tab.
        mStripLayoutHelper.handleDragEnter(/* currX= */ 150.f, /* lastX= */ 0f, false, false);

        float expectedEndWidth =
                expectedStartWidth
                        + (mStripLayoutHelper.getCachedTabWidthForTesting() - TAB_OVERLAP_WIDTH_DP)
                                / 2;
        assertEquals(
                "Unexpected bottom indicator width after tab hover.",
                expectedEndWidth,
                groupTitle.getBottomIndicatorWidth(),
                0.5f);
    }

    private float calculateExpectedBottomIndicatorWidth(
            float tabWidth, float tabCount, StripLayoutGroupTitle groupTitle) {
        // (tabWidth - tabOverlap(28.f)) * tabCount + groupTitleWidth -
        //      bottomIndicatorWidthOffset(27.f).
        return (tabWidth - TAB_OVERLAP_WIDTH_DP) * tabCount
                + groupTitle.getWidth()
                - TAB_GROUP_BOTTOM_INDICATOR_WIDTH_OFFSET;
    }

    @Test
    public void testGroupTitleSlidingAnimation_MergeToGroup() {
        // Mock 5 tabs. Group the first two tabs.
        initializeTest(false, false, 0, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        groupTabs(1, 3);
        int firstTabId = tabs[0].getTabId();
        int secondTabId = tabs[1].getTabId();

        // Assert: first view should be group title.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[1] instanceof StripLayoutGroupTitle);

        // Start reorder mode on first tab. Drag between tabs in group.
        // 70 = (80(halfTabWidth) - 28(tabOverlapWidth)) * 0.53(ReorderOverlapSwitchPercentage).
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);
        float dragDistance = 70f;
        float startX = mStripLayoutHelper.getLastReorderXForTesting();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance);

        // Verify interacting tab was merged into group.
        verify(mTabGroupModelFilter).mergeTabsToGroup(eq(firstTabId), eq(secondTabId), eq(true));
    }

    @Test
    public void testGroupTitleSlidingAnimation_dragOutOfGroup() {
        // Mock 5 tabs. Group the first two tabs.
        initializeTest(false, false, 0, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        groupTabs(1, 3);
        int secondTabId = tabs[1].getTabId();

        // Assert: first view should be group title.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[1] instanceof StripLayoutGroupTitle);
        StripLayoutGroupTitle groupTitle = ((StripLayoutGroupTitle) views[1]);

        // Start reorder mode on first tab. Drag between tabs in group.
        // 35 > ((tabWidth(160) - tabOverlapWidth(28)) / 2) * ReorderOverlapSwitchPercentage(0.53)
        mStripLayoutHelper.startReorderModeAtIndexForTesting(1);
        float dragDistance = -35f - groupTitle.getWidth();
        float startX = mStripLayoutHelper.getLastReorderXForTesting();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance);

        // Verify interacting tab was moved out of group.
        verify(mTabUngrouper)
                .ungroupTabs(
                        eq(List.of(mModel.getTabById(secondTabId))),
                        /* trailing= */ eq(false),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture());
        assertNotNull(mTabModelActionListenerCaptor.getValue());

        // Assert: verify bottom indicator width correctly updated.
        float expectedEndWidth =
                calculateExpectedBottomIndicatorWidth(tabs[0].getWidth(), 2, groupTitle);
        assertEquals(
                "Bottom indicator end width is incorrect",
                expectedEndWidth,
                groupTitle.getBottomIndicatorWidth(),
                EPSILON);
    }

    // Note that the testTabGroupDeleteDialog_* tests only cover the behaviors relevant to the
    // tab strip. Tests for much of the internals and dialog flows themselves are in
    // StripTabModelActionListenerUnitTest, TabRemoverImplUnitTest, and TabUngrouperImplUnitTest.
    @Test
    public void testTabGroupDeleteDialog_Reorder_Collaboration() {
        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1);
        setupDragDropState();
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();

        // Start dragging tab out of group.
        startDraggingTab(tabs, false, 0);

        // Ungroup should start.
        verify(mTabUngrouper)
                .ungroupTabs(
                        eq(List.of(mModel.getTabById(tabs[0].getTabId()))),
                        /* trailing= */ eq(true),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture());

        mTabModelActionListenerCaptor
                .getValue()
                .willPerformActionOrShowDialog(
                        DialogType.COLLABORATION, /* willSkipDialog= */ false);

        // Verify group title is not temporarily disappeared from the tab strip since the operation
        // is immediate. A real TabUngrouper would at this point perform the ungroup.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);

        // Outcome here doesn't matter as it is delegated to the collaboration/sync system to either
        // close or keep the group.
        mTabModelActionListenerCaptor
                .getValue()
                .onConfirmationDialogResult(
                        DialogType.COLLABORATION, ActionConfirmationResult.CONFIRMATION_POSITIVE);
    }

    @Test
    public void testTabGroupDeleteDialog_Reorder_Sync_ImmediateContinue() {
        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1);
        setupDragDropState();
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();

        // Start dragging tab out of group.
        startDraggingTab(tabs, false, 0);

        // Ungroup should start.
        verify(mTabUngrouper)
                .ungroupTabs(
                        eq(List.of(mModel.getTabById(tabs[0].getTabId()))),
                        /* trailing= */ eq(true),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture());

        mTabModelActionListenerCaptor
                .getValue()
                .willPerformActionOrShowDialog(DialogType.SYNC, /* willSkipDialog= */ true);

        // Verify group title is not temporarily disappeared from the tab strip since the operation
        // is immediate. A real TabUngrouper would at this point perform the ungroup.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);

        mTabModelActionListenerCaptor
                .getValue()
                .onConfirmationDialogResult(
                        DialogType.SYNC, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        // Nothing to assert on since updating the strip is delegated to TabUngrouper which is
        // mocked here.
    }

    @Test
    public void testTabGroupDeleteDialog_Reorder_Sync_Positive() {
        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1);
        setupDragDropState();
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();

        // Start dragging tab out of group.
        startDraggingTab(tabs, false, 0);

        // Ungroup should start.
        verify(mTabUngrouper)
                .ungroupTabs(
                        eq(List.of(mModel.getTabById(tabs[0].getTabId()))),
                        /* trailing= */ eq(true),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture());

        mTabModelActionListenerCaptor
                .getValue()
                .willPerformActionOrShowDialog(DialogType.SYNC, /* willSkipDialog= */ false);

        // Verify group title is temporarily disappeared from the tab strip
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertFalse(EXPECTED_NON_TITLE, views[0] instanceof StripLayoutGroupTitle);

        mTabModelActionListenerCaptor
                .getValue()
                .onConfirmationDialogResult(
                        DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_POSITIVE);
        // Nothing to assert on since updating the strip is delegated to TabUngrouper which is
        // mocked here.
    }

    @Test
    public void testTabGroupDeleteDialog_Reorder_Sync_Negative() {
        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1);
        setupDragDropState();
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();

        // Start dragging tab out of group.
        startDraggingTab(tabs, false, 0);

        // Ungroup should start.
        verify(mTabUngrouper)
                .ungroupTabs(
                        eq(List.of(mModel.getTabById(tabs[0].getTabId()))),
                        /* trailing= */ eq(true),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture());

        mTabModelActionListenerCaptor
                .getValue()
                .willPerformActionOrShowDialog(DialogType.SYNC, /* willSkipDialog= */ false);

        // Verify group title is temporarily disappeared from the tab strip
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertFalse(EXPECTED_NON_TITLE, views[0] instanceof StripLayoutGroupTitle);

        mTabModelActionListenerCaptor
                .getValue()
                .onConfirmationDialogResult(
                        DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_NEGATIVE);

        // View is restored.
        views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);
    }

    @Test
    public void testTabGroupDeleteDialog_DragOffStrip_NotLastTab() {
        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 2);
        setTabDragSourceMock();
        setupDragDropState();
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.startDragAndDropTabForTesting(tabs[0], DRAG_START_POINT);

        // Start dragging tab out of group.
        startDraggingTab(tabs, true, 0);

        // No ungroup should start.
        verify(mTabUngrouper, never()).ungroupTabs(any(), anyBoolean(), anyBoolean(), any());

        // Assume the drop is unsuccessful; the tab and the tab group will be restored to its
        // original position.
        mStripLayoutHelper.stopReorderMode();
    }

    @Test
    public void testTabGroupDeleteDialog_DragOffStrip_DialogSkipped() {
        when(mActionConfirmationManager.willSkipUngroupTabAttempt()).thenReturn(true);

        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1);
        setTabDragSourceMock();
        setupDragDropState();
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.startDragAndDropTabForTesting(tabs[0], DRAG_START_POINT);

        // Start dragging tab out of group.
        startDraggingTab(tabs, true, 0);

        // No ungroup should start.
        verify(mTabUngrouper, never()).ungroupTabs(any(), anyBoolean(), anyBoolean(), any());

        // Assume the drop is unsuccessful; the tab and the tab group will be restored to its
        // original position.
        mStripLayoutHelper.stopReorderMode();
    }

    @Test
    public void testTabGroupDeleteDialog_DragOffStrip_Collaboration() {
        // Collaboration groups override the check for skipping.
        when(mActionConfirmationManager.willSkipUngroupTabAttempt()).thenReturn(true);

        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1);
        setTabDragSourceMock();
        setupDragDropState();
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.startDragAndDropTabForTesting(tabs[0], DRAG_START_POINT);
        Tab tab = mModel.getTabAt(0);

        assertNotNull(tab.getTabGroupId());
        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(new LocalTabGroupId(tab.getTabGroupId())))
                .thenReturn(savedGroup);

        // Start dragging tab out of group.
        startDraggingTab(tabs, true, 0);

        // Ungroup should start.
        verify(mTabUngrouper)
                .ungroupTabs(
                        eq(List.of(tab)),
                        /* trailing= */ eq(false),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture());

        mTabModelActionListenerCaptor
                .getValue()
                .willPerformActionOrShowDialog(
                        DialogType.COLLABORATION, /* willSkipDialog= */ false);

        // Verify group title is not temporarily disappeared from the tab strip since the operation
        // is immediate. A real TabUngrouper would at this point perform the ungroup.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);

        // Outcome here doesn't matter as it is delegated to the collaboration/sync system to either
        // close or keep the group.
        mTabModelActionListenerCaptor
                .getValue()
                .onConfirmationDialogResult(
                        DialogType.COLLABORATION, ActionConfirmationResult.CONFIRMATION_POSITIVE);
    }

    @Test
    public void testTabGroupDeleteDialog_DragOffStrip_Sync_Positive() {
        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1);
        setTabDragSourceMock();
        setupDragDropState();
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.startDragAndDropTabForTesting(tabs[0], DRAG_START_POINT);

        // Start dragging tab out of group.
        startDraggingTab(tabs, true, 0);

        // Ungroup should start.
        verify(mTabUngrouper)
                .ungroupTabs(
                        eq(List.of(mModel.getTabById(tabs[0].getTabId()))),
                        /* trailing= */ eq(false),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture());

        mTabModelActionListenerCaptor
                .getValue()
                .willPerformActionOrShowDialog(DialogType.SYNC, /* willSkipDialog= */ false);

        // Verify group title is temporarily disappeared from the tab strip.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertFalse(EXPECTED_NON_TITLE, views[0] instanceof StripLayoutGroupTitle);

        // Assume the action is successful. A real TabUngrouper would take care of this for us.
        mTabModelActionListenerCaptor
                .getValue()
                .onConfirmationDialogResult(
                        DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_POSITIVE);

        // Group is still hidden as it was fully ungrouped.
        views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertFalse(EXPECTED_NON_TITLE, views[0] instanceof StripLayoutGroupTitle);
    }

    @Test
    public void testTabGroupDeleteDialog_DragOffStrip_Sync_Negative() {
        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1);
        setTabDragSourceMock();
        setupDragDropState();
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.startDragAndDropTabForTesting(tabs[0], DRAG_START_POINT);

        // Start dragging tab out of group.
        startDraggingTab(tabs, true, 0);

        // Ungroup should start.
        verify(mTabUngrouper)
                .ungroupTabs(
                        eq(List.of(mModel.getTabById(tabs[0].getTabId()))),
                        /* trailing= */ eq(false),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture());

        mTabModelActionListenerCaptor
                .getValue()
                .willPerformActionOrShowDialog(DialogType.SYNC, /* willSkipDialog= */ false);

        // Verify group title is temporarily disappeared from the tab strip
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertFalse(EXPECTED_NON_TITLE, views[0] instanceof StripLayoutGroupTitle);

        // Assume the action is unsuccessful.
        mTabModelActionListenerCaptor
                .getValue()
                .onConfirmationDialogResult(
                        DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_NEGATIVE);

        // Group should be restored.
        views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);
    }

    @Test
    public void testTabGroupDeleteDialog_Close_Collaboration() {
        TabRemover tabRemover = mock(TabRemover.class);
        mModel.setTabRemover(tabRemover);

        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();

        // Close the first tab.
        mStripLayoutHelper.handleCloseButtonClick(
                tabs[0], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);
        verify(tabRemover)
                .prepareCloseTabs(
                        argThat(params -> params.tabs.get(0).getId() == tabs[0].getTabId()),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture(),
                        mTabRemoverCallbackCaptor.capture());

        mTabModelActionListenerCaptor
                .getValue()
                .willPerformActionOrShowDialog(
                        DialogType.COLLABORATION, /* willSkipDialog= */ false);

        // Group title doesn't need to hide as close is immediate.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);
        assertFalse("Tab should not be closing yet", tabs[0].isDying());
        mTabRemoverCallbackCaptor
                .getValue()
                .onResult(
                        TabClosureParams.closeTab(mModel.getTabById(tabs[0].getTabId()))
                                .allowUndo(true)
                                .build());
        assertTrue("Tab should be closing", tabs[0].isDying());

        // No further view assertions are required as the state don't have changed.

        // Outcome here doesn't matter as it is delegated to the collaboration/sync system to either
        // close or keep the group.
        mTabModelActionListenerCaptor
                .getValue()
                .onConfirmationDialogResult(
                        DialogType.COLLABORATION, ActionConfirmationResult.CONFIRMATION_POSITIVE);
    }

    @Test
    public void testTabGroupDeleteDialog_Close_Sync_ImmediateContinue() {
        TabRemover tabRemover = mock(TabRemover.class);
        mModel.setTabRemover(tabRemover);

        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();

        // Close the first tab.
        mStripLayoutHelper.handleCloseButtonClick(
                tabs[0], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);
        verify(tabRemover)
                .prepareCloseTabs(
                        argThat(params -> params.tabs.get(0).getId() == tabs[0].getTabId()),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture(),
                        mTabRemoverCallbackCaptor.capture());

        mTabModelActionListenerCaptor
                .getValue()
                .willPerformActionOrShowDialog(DialogType.SYNC, /* willSkipDialog= */ true);

        // Group title doesn't need to hide as close is immediate.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);
        assertFalse("Tab should not be closing yet", tabs[0].isDying());

        // Simulate the continuation of the operation.
        mTabRemoverCallbackCaptor
                .getValue()
                .onResult(
                        TabClosureParams.closeTab(mModel.getTabById(tabs[0].getTabId()))
                                .allowUndo(true)
                                .build());
        mTabModelActionListenerCaptor
                .getValue()
                .onConfirmationDialogResult(
                        DialogType.SYNC, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        assertTrue("Tab should be closing", tabs[0].isDying());
    }

    @Test
    public void testTabGroupDeleteDialog_Close_Sync_Positive() {
        TabRemover tabRemover = mock(TabRemover.class);
        mModel.setTabRemover(tabRemover);

        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();

        // Close the first tab.
        mStripLayoutHelper.handleCloseButtonClick(
                tabs[0], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);
        verify(tabRemover)
                .prepareCloseTabs(
                        argThat(params -> params.tabs.get(0).getId() == tabs[0].getTabId()),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture(),
                        mTabRemoverCallbackCaptor.capture());

        mTabModelActionListenerCaptor
                .getValue()
                .willPerformActionOrShowDialog(DialogType.SYNC, /* willSkipDialog= */ false);

        // Verify group title is temporarily disappeared from the tab strip.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertFalse(EXPECTED_NON_TITLE, views[0] instanceof StripLayoutGroupTitle);
        assertFalse("Tab should not be closing yet", tabs[0].isDying());

        // Simulate the operation interrupted by the dialog being continued.
        mTabRemoverCallbackCaptor
                .getValue()
                .onResult(
                        TabClosureParams.closeTab(mModel.getTabById(tabs[0].getTabId()))
                                .allowUndo(true)
                                .build());
        mTabModelActionListenerCaptor
                .getValue()
                .onConfirmationDialogResult(
                        DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_POSITIVE);
        assertTrue("Tab should be closing", tabs[0].isDying());
    }

    @Test
    public void testTabGroupDeleteDialog_Close_Sync_Negative() {
        TabRemover tabRemover = mock(TabRemover.class);
        mModel.setTabRemover(tabRemover);

        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();

        // Close the first tab.
        mStripLayoutHelper.handleCloseButtonClick(
                tabs[0], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);
        verify(tabRemover)
                .prepareCloseTabs(
                        argThat(params -> params.tabs.get(0).getId() == tabs[0].getTabId()),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture(),
                        mTabRemoverCallbackCaptor.capture());

        mTabModelActionListenerCaptor
                .getValue()
                .willPerformActionOrShowDialog(DialogType.SYNC, /* willSkipDialog= */ false);

        // Verify group title is temporarily disappeared from the tab strip.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertFalse(EXPECTED_NON_TITLE, views[0] instanceof StripLayoutGroupTitle);

        // Simulate the operation interrupted by the dialog being stopped.
        mTabModelActionListenerCaptor
                .getValue()
                .onConfirmationDialogResult(
                        DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_NEGATIVE);
        assertFalse("Tab should not be closing", tabs[0].isDying());

        // Verify group title is restored.
        views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);
    }

    private void setupTabGroup(int groupStartIndex, int groupEndIndex) {
        // Mock 5 tabs. Group tab from start to endIndex.
        initializeTest(false, false, 0, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        groupTabs(groupStartIndex, groupEndIndex);
        mStripLayoutHelper.setTabModel(mModel, mTabCreator, false);

        // Assert: View should be group title.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[groupStartIndex] instanceof StripLayoutGroupTitle);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.DATA_SHARING})
    public void testSharedGroupNotificationBubbleShowAndHide_CollapsedGroup() {
        // Initialize shared tab group and collapse group.
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ true,
                        /* duringStripBuild= */ false,
                        /* start= */ 0,
                        /* end= */ 1);
        mStripLayoutHelper.collapseTabGroupForTesting(groupTitle, /* isCollapsed= */ true);

        // Update the root tab.
        Set<Integer> tabIds = new HashSet<>(Collections.singleton(groupTitle.getRootId()));
        mStripLayoutHelper.updateTabStripNotificationBubble(tabIds, /* hasUpdate= */ true);

        // Verify group title and tab bubble should show.
        assertTrue(
                "Notification bubble on group title should show.",
                groupTitle.getNotificationBubbleShown());
        verify(mLayerTitleCache).updateTabBubble(groupTitle.getRootId(), /* showBubble= */ true);

        // Verify tab bubble should hide when update is removed.
        mStripLayoutHelper.updateTabStripNotificationBubble(tabIds, /* hasUpdate= */ false);
        assertFalse(
                "Notification bubble on group title should hide.",
                groupTitle.getNotificationBubbleShown());
        verify(mLayerTitleCache).updateTabBubble(groupTitle.getRootId(), /* showBubble= */ false);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.DATA_SHARING})
    public void testSharedGroupNotificationBubbleShowAndHide_ExpandedGroup() {
        // Initialize shared tab group.
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ true,
                        /* duringStripBuild= */ false,
                        /* start= */ 0,
                        /* end= */ 1);

        // The root tab is updated from message backend service.
        Set<Integer> tabIds = new HashSet<>(Collections.singleton(groupTitle.getRootId()));
        mStripLayoutHelper.updateTabStripNotificationBubble(tabIds, /* hasUpdate= */ true);

        // Verify only the tab bubble should show.
        assertFalse(
                "Notification bubble on group title should hide.",
                groupTitle.getNotificationBubbleShown());
        verify(mLayerTitleCache).updateTabBubble(groupTitle.getRootId(), /* showBubble= */ true);

        // Verify tab bubble should hide when update is removed.
        mStripLayoutHelper.updateTabStripNotificationBubble(tabIds, /* hasUpdate= */ false);
        assertFalse(
                "Notification bubble on group title should hide.",
                groupTitle.getNotificationBubbleShown());
        verify(mLayerTitleCache).updateTabBubble(groupTitle.getRootId(), /* showBubble= */ false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testSharedGroupStateDuringStripBuild_OnlyOneCollaborator_AvatarNotShow() {
        // Initialize shared tab group with only one collaborator during strip build.
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ false,
                        /* duringStripBuild= */ true,
                        /* start= */ 0,
                        /* end= */ 1);

        // Verify group unshared and avatar resources cleared when only one collaborator.
        verifySharedGroupState(groupTitle, false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testSharedGroupStateDuringStripBuild_MultipleCollaborators_AvatarShow() {
        // Initialize shared tab group with multiple collaborators during strip build.
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ true,
                        /* duringStripBuild= */ true,
                        /* start= */ 0,
                        /* end= */ 1);

        // Verify group shared state is updated and avatar resource is initialized.
        verifySharedGroupState(groupTitle, true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testSharedGroupStateOnGroupAdded_OnlyOneCollaborator_AvatarNotShow() {
        // Group shared but no other collaborator joined yet.
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ false,
                        /* duringStripBuild= */ false,
                        /* start= */ 0,
                        /* end= */ 1);

        // Verify group unshared and avatar resources cleared when only one collaborator.
        verifySharedGroupState(groupTitle, false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testSharedGroupStateOnGroupChanged_OnlyOneCollaborator_AvatarNotShow() {
        // Group shared with multiple collaborators.
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ true,
                        /* duringStripBuild= */ false,
                        /* start= */ 0,
                        /* end= */ 1);

        // Verify group shared state is updated and avatar resource is initialized.
        verifySharedGroupState(groupTitle, true);

        // Group changed that only one collaborator remains.
        mSharingObserverCaptor
                .getValue()
                .onGroupChanged(
                        SharedGroupTestHelper.newGroupData(
                                COLLABORATION_ID1, SharedGroupTestHelper.GROUP_MEMBER1));

        // Verify group unshared and avatar resources cleared when only one collaborator.
        verifySharedGroupState(groupTitle, false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testSharedGroupStateOnGroupChanged_MultipleCollaborators_AvatarShow() {
        // Group shared but no other collaborator joined yet.
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ false,
                        /* duringStripBuild= */ false,
                        /* start= */ 0,
                        /* end= */ 1);

        // Verify group unshared and avatar resources cleared when only one collaborator.
        verifySharedGroupState(groupTitle, false);

        // Populate face pile during SharedImageTilesCoordinator#updateCollaborationId.
        mSharedGroupTestHelper.mockGetGroupData(COLLABORATION_ID1, GROUP_MEMBER1, GROUP_MEMBER2);

        // Group changed that shared with multiple collaborators.
        mSharingObserverCaptor
                .getValue()
                .onGroupChanged(
                        SharedGroupTestHelper.newGroupData(
                                COLLABORATION_ID1,
                                SharedGroupTestHelper.GROUP_MEMBER1,
                                SharedGroupTestHelper.GROUP_MEMBER2));

        loadAvatarBitmap();

        // Verify group shared state is updated and avatar resource is initialized.
        verifySharedGroupState(groupTitle, true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testSharedGroupStateOnGroupRemoved_AvatarNotShow() {
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ true,
                        /* duringStripBuild= */ false,
                        /* start= */ 0,
                        /* end= */ 1);

        // Verify group shared state is updated and avatar resource is initialized.
        verifySharedGroupState(groupTitle, true);

        // Group removed.
        mSharingObserverCaptor.getValue().onGroupRemoved(COLLABORATION_ID1);

        // Verify group unshared and avatar resources cleared when only one collaborator.
        verifySharedGroupState(groupTitle, false);
    }

    @Test
    public void testTabSelectionStateSet_whenTabModelSet() {
        mStripLayoutHelper = spy(createStripLayoutHelper(/* rtl= */ false, /* incognito= */ false));
        mModel.setIndex(0);
        when(mModel.getTabAt(0)).thenReturn(mTab);
        int tabId = 100;
        when(mTab.getId()).thenReturn(tabId);
        mStripLayoutHelper.setTabModel(mModel, mTabCreator, true);
        verify(mStripLayoutHelper, times(1))
                .tabSelected(anyLong(), eq(tabId), eq(Tab.INVALID_TAB_ID));
    }

    @Test
    public void testTabSelectionStateSet_whenTabStateInitialized() {
        mStripLayoutHelper = spy(createStripLayoutHelper(/* rtl= */ false, /* incognito= */ false));
        mModel.setIndex(0);
        when(mModel.getTabAt(0)).thenReturn(mTab);
        int tabId = 100;
        when(mTab.getId()).thenReturn(tabId);
        mStripLayoutHelper.setTabModel(mModel, mTabCreator, true);
        verify(mStripLayoutHelper, times(1))
                .tabSelected(anyLong(), eq(tabId), eq(Tab.INVALID_TAB_ID));
        mStripLayoutHelper.onTabStateInitialized();
        verify(mStripLayoutHelper, times(2))
                .tabSelected(anyLong(), eq(tabId), eq(Tab.INVALID_TAB_ID));
    }

    private SavedTabGroup setupTabGroupSync(Token tabGroupId) {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.localId = new LocalTabGroupId(tabGroupId);
        SavedTabGroupTab savedTab = new SavedTabGroupTab();
        SavedTabGroupTab savedTab2 = new SavedTabGroupTab();
        savedTabGroup.savedTabs = Arrays.asList(new SavedTabGroupTab[] {savedTab, savedTab2});
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_ID1)).thenReturn(savedTabGroup);
        when(mTabGroupSyncService.getGroup(savedTabGroup.localId)).thenReturn(savedTabGroup);
        when(mTabGroupSyncService.isRemoteDevice(any())).thenReturn(true);
        return savedTabGroup;
    }

    private StripLayoutGroupTitle createCollaborationGroup(
            boolean multipleCollaborators, boolean duringStripBuild, int start, int end) {
        // Mock 5 tabs.
        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);
        initializeTest(false, false, 3, 5);
        verify(mDataSharingService).addObserver(mSharingObserverCaptor.capture());
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        if (multipleCollaborators) {
            mSharedGroupTestHelper.mockGetGroupData(
                    COLLABORATION_ID1, GROUP_MEMBER1, GROUP_MEMBER2);
        } else {
            mSharedGroupTestHelper.mockGetGroupData(COLLABORATION_ID1, GROUP_MEMBER1);
        }

        // Group the tabs and setup the tab group sync state.
        SavedTabGroup savedTabGroup = null;
        if (duringStripBuild) {
            // Do this before grouping the tabs for the case of building the strip to ensure we
            // emulate the state when building correctly.
            savedTabGroup = setupTabGroupSync(new Token(0L, mModel.getTabAt(0).getId()));
            savedTabGroup.collaborationId = COLLABORATION_ID1;
            groupTabs(start, end);
        } else {
            groupTabs(start, end);
            savedTabGroup = setupTabGroupSync(mModel.getTabAt(0).getTabGroupId());
        }

        // Verify group title is present.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);
        StripLayoutGroupTitle groupTitle = ((StripLayoutGroupTitle) views[0]);
        @TabGroupColorId int color = TabGroupColorId.GREY;
        groupTitle.updateTint(color);

        if (multipleCollaborators) {
            if (!duringStripBuild) {
                // Collaboration group added through Data sharing observer.
                savedTabGroup.collaborationId = COLLABORATION_ID1;
                mSharingObserverCaptor
                        .getValue()
                        .onGroupAdded(
                                SharedGroupTestHelper.newGroupData(
                                        COLLABORATION_ID1,
                                        SharedGroupTestHelper.GROUP_MEMBER1,
                                        SharedGroupTestHelper.GROUP_MEMBER2));
            }
            loadAvatarBitmap();
        } else {
            if (!duringStripBuild) {
                // Collaboration group added through Data sharing observer.
                savedTabGroup.collaborationId = COLLABORATION_ID1;
                mSharingObserverCaptor
                        .getValue()
                        .onGroupAdded(
                                SharedGroupTestHelper.newGroupData(
                                        COLLABORATION_ID1, SharedGroupTestHelper.GROUP_MEMBER1));
            }
        }
        return groupTitle;
    }

    private void loadAvatarBitmap() {
        ArgumentCaptor<DataSharingAvatarBitmapConfig> configCaptor =
                ArgumentCaptor.forClass(DataSharingAvatarBitmapConfig.class);
        verify(mDataSharingUiDelegate, times(2)).getAvatarBitmap(configCaptor.capture());
        configCaptor
                .getAllValues()
                .get(0)
                .getDataSharingAvatarCallback()
                .onAvatarLoaded(mAvatarBitmap);
        configCaptor
                .getAllValues()
                .get(1)
                .getDataSharingAvatarCallback()
                .onAvatarLoaded(mAvatarBitmap);
    }

    private void verifySharedGroupState(StripLayoutGroupTitle groupTitle, boolean shouldShare) {
        if (shouldShare) {
            assertTrue("Group should be shared.", groupTitle.isGroupShared());
            assertNotNull(
                    "SharedImageTilesCoordinator for shared group should be initialized",
                    groupTitle.getSharedImageTilesCoordinatorForTesting());
            assertNotNull(
                    "Avatar resource for shared group should be initialized",
                    groupTitle.getAvatarResourceForTesting());
            assertNotNull(
                    "Notification bubbler for shared group should be initialized",
                    groupTitle.getTabBubbler());
        } else {
            assertFalse("Group should be unshared.", groupTitle.isGroupShared());
            assertNull(
                    "SharedImageTilesCoordinator for shared group should be cleared",
                    groupTitle.getSharedImageTilesCoordinatorForTesting());
            assertNull(
                    "Avatar resource for shared group should be cleared",
                    groupTitle.getAvatarResourceForTesting());
        }
    }

    private void startDraggingTab(
            StripLayoutTab[] tabs, boolean draggingTabOffStrip, int tabIndexToDrag) {
        // Start drag tab out of group or drag off strip.
        if (draggingTabOffStrip) {
            // Drag onto strip before dragged off.
            mStripLayoutHelper.handleDragEnter(0f, 0f, true, false);
            mStripLayoutHelper.handleDragExit(true, false);
        } else {
            float dragDistance =
                    ((tabs[0].getWidth() - TAB_OVERLAP_WIDTH_DP) / 2)
                            * REORDER_OVERLAP_SWITCH_PERCENTAGE;
            startDragTabOutOfTabGroup(tabIndexToDrag, dragDistance + 1);
        }
    }

    private void startDragTabOutOfTabGroup(int index, float dragDistance) {
        // Start reorder and drag tab out of the tab group through group end.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(index);
        float startX = mStripLayoutHelper.getLastReorderXForTesting();
        mStripLayoutHelper.drag(TIMESTAMP, startX + dragDistance, 0f, dragDistance);
    }

    @Test
    public void testTabClosed() {
        // Initialize with 10 tabs.
        int tabCount = 10;
        initializeTest(false, false, 0, tabCount);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Remove tab from model and verify that the tab strip has not yet updated.
        int closedTabId = 1;
        int expectedNumTabs = tabCount;
        mModel.getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTab(mModel.getTabAt(closedTabId)).build(),
                        /* allowDialog= */ false);
        assertEquals(
                "Tab strip should not yet have changed.",
                expectedNumTabs,
                mStripLayoutHelper.getStripLayoutTabsForTesting().length);

        // Trigger update and verify the tab strip matches the tab model.
        expectedNumTabs = 9;
        mStripLayoutHelper.tabClosed(TIMESTAMP, closedTabId);
        assertEquals(
                "Tab strip should match tab model.",
                expectedNumTabs,
                mStripLayoutHelper.getStripLayoutTabsForTesting().length);
        verify(mUpdateHost, times(8)).requestUpdate();
    }

    @Test
    public void testTabClosing_OnlyOneTabOpen_CloseButtonClickedWithTouchInput_AllowUndo() {
        testUndoDuringTabClosing(
                /* tabCount= */ 1,
                MotionEventUtils.MOTION_EVENT_BUTTON_NONE,
                /* shouldAllowUndo= */ true);
    }

    @Test
    public void
            testTabClosing_OnlyOneTabOpen_CloseButtonClickedWithPeripheralButton_DisallowUndo() {
        testUndoDuringTabClosing(
                /* tabCount= */ 1, MotionEvent.BUTTON_PRIMARY, /* shouldAllowUndo= */ false);
    }

    @Test
    public void testTabClosing_MultipleTabsOpen_CloseButtonClickedWithTouchInput_AllowUndo() {
        testUndoDuringTabClosing(
                /* tabCount= */ 15,
                MotionEventUtils.MOTION_EVENT_BUTTON_NONE,
                /* shouldAllowUndo= */ true);
    }

    @Test
    public void
            testTabClosing_MultipleTabsOpen_CloseButtonClickedWithPeripheralButton_DisallowUndo() {
        testUndoDuringTabClosing(
                /* tabCount= */ 15, MotionEvent.BUTTON_PRIMARY, /* shouldAllowUndo= */ false);
    }

    /**
     * Tests the "allowUndo" behavior during {@link
     * StripLayoutHelper#handleCloseButtonClick(StripLayoutTab, int)}.
     *
     * @param tabCount number of tabs to set up.
     * @param motionEventButtonState parameter for {@link
     *     StripLayoutHelper#handleCloseButtonClick(StripLayoutTab, int)}
     * @param shouldAllowUndo whether to allow undo tab closure; this is a field in {@link
     *     TabClosureParams} for {@link TabRemover}.
     */
    private void testUndoDuringTabClosing(
            int tabCount, int motionEventButtonState, boolean shouldAllowUndo) {
        // Setup
        int selectedTabIndex = tabCount - 1;
        initializeTest(/* rtl= */ false, /* incognito= */ false, selectedTabIndex, tabCount);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH,
                SCREEN_HEIGHT,
                /* orientationChanged= */ false,
                TIMESTAMP,
                PADDING_LEFT,
                PADDING_RIGHT,
                /* topPadding= */ 0f);
        setupForAnimations();
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Act
        mStripLayoutHelper.handleCloseButtonClick(tabs[selectedTabIndex], motionEventButtonState);
        mStripLayoutHelper.getRunningAnimatorForTesting().end(); // end the closing animation

        // Assert
        TestTabRemover testTabRemover = (TestTabRemover) mModel.getTabRemover();
        assertNotNull(testTabRemover.mLastParamsForPrepareCloseTabs);
        assertEquals(shouldAllowUndo, testTabRemover.mLastParamsForPrepareCloseTabs.allowUndo);
        assertNotNull(testTabRemover.mLastParamsForForceCloseTabs);
        assertEquals(shouldAllowUndo, testTabRemover.mLastParamsForForceCloseTabs.allowUndo);
    }

    @Test
    public void testTabClosing_NoTabResize() {
        // Arrange
        int tabCount = 15;
        initializeTest(false, false, 14, tabCount);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        setupForAnimations();

        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Act: Call on close tab button handler.
        mStripLayoutHelper.handleCloseButtonClick(
                tabs[14], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);

        // Assert: Animations started.
        assertTrue(
                "MultiStepAnimations should have started.",
                mStripLayoutHelper.isMultiStepCloseAnimationsRunningForTesting());

        // Act: End the tab closing animations to apply final values.
        Animator runningAnimator = mStripLayoutHelper.getRunningAnimatorForTesting();
        runningAnimator.end();

        // Assert: Tab is closed and animations are still running.
        int expectedTabCount = 14;
        assertEquals(
                "Unexpected tabs count",
                expectedTabCount,
                mStripLayoutHelper.getStripLayoutTabsForTesting().length);
        assertTrue(
                "MultiStepAnimations should still be running.",
                mStripLayoutHelper.isMultiStepCloseAnimationsRunningForTesting());

        // Act: End next set of animations to apply final values.
        mStripLayoutHelper.getRunningAnimatorForTesting().end();

        // Assert: Animations completed. The tab width is not resized and drawX does not change.
        // stripRightBound = width(800) - offsetXRight(20) = 780;
        // visibleTabRightBound = rightBound(780)- NTBWidth(32) - endPadding(8) = 740
        // lastTabDrawX = visibleTabRightBound(740) - tabWidth(108) = 632
        float expectedDrawX = 632.f;
        StripLayoutTab[] updatedTabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        for (int i = updatedTabs.length - 1; i >= 0; i--) {
            StripLayoutTab stripTab = updatedTabs[i];
            assertEquals("Unexpected tab width after resize.", 108.f, stripTab.getWidth(), 0);
            assertEquals("Unexpected tab position.", expectedDrawX, stripTab.getDrawX(), 0);
            expectedDrawX -= TAB_WIDTH_SMALL - TAB_OVERLAP_WIDTH_DP;
        }
        assertFalse(
                "MultiStepAnimations should have stopped running.",
                mStripLayoutHelper.isMultiStepCloseAnimationsRunningForTesting());
    }

    @Test
    public void testTabClosing_NonLastTab_TabResize() {
        // Arrange
        int tabCount = 4;
        initializeTest(false, false, 3, tabCount);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        setupForAnimations();

        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Act: Call on close tab button handler.
        mStripLayoutHelper.handleCloseButtonClick(
                tabs[2], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);

        // Assert: Animations started.
        assertTrue(
                "MultiStepAnimations should have started.",
                mStripLayoutHelper.isMultiStepCloseAnimationsRunningForTesting());

        // Act: End the animations to apply final values.
        Animator runningAnimator = mStripLayoutHelper.getRunningAnimatorForTesting();
        runningAnimator.end();

        // Assert: Tab is closed and animations are still running.
        int expectedTabCount = 3;
        assertEquals(expectedTabCount, mStripLayoutHelper.getStripLayoutTabsForTesting().length);
        assertTrue(
                "MultiStepAnimations should still be running.",
                mStripLayoutHelper.isMultiStepCloseAnimationsRunningForTesting());

        // Act: Set animation time forward by 250ms for next set of animations.
        mStripLayoutHelper.getRunningAnimatorForTesting().end();

        // Assert: Animations completed. The tab width is resized, tab.drawX is changed and
        // newTabButton.drawX is also changed.
        float expectedDrawX = 10.f; // offsetXLeft(10)
        // availableSize = width(800) - NTB(32) - endPadding(8) - offsetXLeft(10) - offsetXRight(20)
        // = 730
        // ExpectedWidth = (availableSize(730) + 2 * overlap(28)) / 3  = 262
        float expectedWidthAfterResize = 262.f;
        StripLayoutTab[] updatedTabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        for (int i = 0; i < updatedTabs.length; i++) {
            StripLayoutTab stripTab = updatedTabs[i];
            assertEquals(
                    "Unexpected tab width after resize.",
                    expectedWidthAfterResize,
                    stripTab.getWidth(),
                    0.1f);
            assertEquals("Unexpected tab position.", expectedDrawX, stripTab.getDrawX(), 0.1f);
            expectedDrawX += (expectedWidthAfterResize - TAB_OVERLAP_WIDTH_DP);
        }
        assertFalse(
                "MultiStepAnimations should have ended.",
                mStripLayoutHelper.isMultiStepCloseAnimationsRunningForTesting());
    }

    @Test
    public void testTabClosingClearsTabHoverState() {
        initializeTabHoverTest();
        var tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();

        // Hover on tabs[2], and close it.
        mStripLayoutHelper.updateLastHoveredTab(tabs[2]);
        verify(mTabHoverCardView)
                .show(any(), anyBoolean(), anyFloat(), anyFloat(), anyFloat(), anyFloat());
        mStripLayoutHelper.handleCloseButtonClick(
                tabs[2], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);

        // End the tab closure animation.
        var runningAnimator = mStripLayoutHelper.getRunningAnimatorForTesting();
        runningAnimator.end();

        verify(mTabHoverCardView).hide();
    }

    @Test
    public void testChangingModelClearsTabHoverState() {
        // Initialize hover card, then hover on a tab.
        initializeTabHoverTest();
        mStripLayoutHelper.updateLastHoveredTab(
                mStripLayoutHelper.getStripLayoutTabsForTesting()[0]);

        // Now switch to a different tab model.
        mStripLayoutHelper.tabModelSelected(false);

        // Assert that the hover card view is closed and the last hovered tab is null.
        verify(mTabHoverCardView, times(1)).hide();
        assertNull(mStripLayoutHelper.getLastHoveredTab());
    }

    @Test
    public void testClickingClearsTabHoverState() {
        // Initialize hover card, then hover on a tab.
        initializeTabHoverTest();
        StripLayoutTab hoveredTab = mStripLayoutHelper.getStripLayoutTabsForTesting()[0];
        mStripLayoutHelper.updateLastHoveredTab(hoveredTab);

        // Now click on the tab that's originating the hovercard.
        mStripLayoutHelper.click(1000L, hoveredTab.getDrawX() + 1, hoveredTab.getDrawY() + 1, 0);

        // Assert that the hover card view is closed and the last hovered tab is null.
        verify(mTabHoverCardView, times(1)).hide();
        assertNull(mStripLayoutHelper.getLastHoveredTab());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_CONTEXT_MENU)
    public void testRightClickingClearsTabHoverState() {
        // Initialize hover card, then hover on a tab.
        initializeTabHoverTest();
        StripLayoutTab hoveredTab = mStripLayoutHelper.getStripLayoutTabsForTesting()[0];
        mStripLayoutHelper.updateLastHoveredTab(hoveredTab);

        // Set up things necessary for right-click.
        setupForIndividualTabContextMenu();

        // Now right-click on the tab that's originating the hovercard.
        mStripLayoutHelper.click(
                1000L,
                hoveredTab.getDrawX() + 1,
                hoveredTab.getDrawY() + 1,
                MotionEvent.BUTTON_SECONDARY);

        // Assert that the hover card view is closed and the last hovered tab is null.
        verify(mTabHoverCardView, times(1)).hide();
        assertNull(mStripLayoutHelper.getLastHoveredTab());
    }

    @Test
    public void testFlingLeft() {
        // Arrange
        initializeTest(false, false, 11, 12);
        // Disable the padding as changing the visible width change the existing expected fling
        // distance.
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, 0, 0, 0);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        mStripLayoutHelper.setScrollOffsetForTesting(-150);

        // Act: Perform a fling and update layout.
        float velocityX = -7000f;
        // The velocityX value is used to calculate the scroller.finalX value.
        mStripLayoutHelper.fling(TIMESTAMP, 0, 0, velocityX, 0);
        // This will use the scroller.finalX value to update the scrollOffset. The timestamp
        // value here will determine the fling duration and affects the final offset value.
        mStripLayoutHelper.updateLayout(TIMESTAMP + 10);

        // Assert: Final scrollOffset.
        // The calculation of this value is done using the velocity. The velocity along a friction
        // constant is used to calculate deceleration and distance. That together with the animation
        // duration determines the final scroll offset position.
        float expectedOffset = -220.f;
        assertEquals(
                "Unexpected scroll offset.",
                expectedOffset,
                mStripLayoutHelper.getScrollOffset(),
                0.0);
    }

    @Test
    public void testFlingRight() {
        // Arrange
        initializeTest(false, false, 10, 11);
        // Disable the padding as changing the visible width change the existing expected fling
        // distance.
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, 0, 0, 0);
        // When updateLayout is called for the first time, bringSelectedTabToVisibleArea() method is
        // invoked. That also affects the scrollOffset value. So we call updateLayout before
        // performing a fling so that bringSelectedTabToVisible area isn't called after the fling.
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        mStripLayoutHelper.setScrollOffsetForTesting(-150);

        // Act: Perform a fling and update layout.
        float velocity = 5000f;
        // The velocityX value is used to calculate the scroller.finalX value.
        mStripLayoutHelper.fling(TIMESTAMP, 0, 0, velocity, 0);
        // This will use the scroller.finalX value to update the scrollOffset. The timestamp
        // value here will determine the fling duration and affects the final offset value.
        mStripLayoutHelper.updateLayout(TIMESTAMP + 20);

        // Assert: Final scrollOffset.
        // The calculation of this value is done using the velocity. The velocity along a friction
        // constant is used to calculate deceleration and distance. That together with the animation
        // duration determines the final scroll offset position.
        float expectedOffset = -48.f;
        assertEquals(
                "Unexpected scroll offset.",
                expectedOffset,
                mStripLayoutHelper.getScrollOffset(),
                0.0);
    }

    @Test
    @Feature("Tab Group Context Menu")
    public void testFling_WithContextMenu() {
        // Arrange
        initializeTest(false, false, 10, 11);
        groupTabs(0, 1);
        setupForGroupContextMenu();
        when(mTabGroupContextMenuCoordinator.isMenuShowing()).thenReturn(true);
        // Disable the padding as changing the visible width change the existing expected fling
        // distance.
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, 0, 0, 0);
        // When updateLayout is called for the first time, bringSelectedTabToVisibleArea() method is
        // invoked. That also affects the scrollOffset value. So we call updateLayout before
        // performing a fling so that bringSelectedTabToVisible area isn't called after the fling.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Set initial scroll offset.
        float initialScrollOffset = -150;
        mStripLayoutHelper.setScrollOffsetForTesting(initialScrollOffset);

        // Act: Perform a fling and update layout.
        float velocity = 5000f;
        // The velocityX value is used to calculate the scroller.finalX value.
        mStripLayoutHelper.fling(TIMESTAMP, 0, 0, velocity, 0);
        // This will use the scroller.finalX value to update the scrollOffset. The timestamp
        // value here will determine the fling duration and affects the final offset value.
        mStripLayoutHelper.updateLayout(TIMESTAMP + 20);

        // Assert: Final scrollOffset. Should not have moved as context menu is showing.
        assertEquals(
                "Unexpected scroll offset.",
                initialScrollOffset,
                mStripLayoutHelper.getScrollOffset(),
                0.0);
    }

    @Test
    public void testDrag_UpdatesScrollOffset_ScrollingStrip() {
        // Arrange
        initializeTest(false, false, 13, 14);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        // When updateLayout is called for the first time, bringSelectedTabToVisibleArea() method is
        // invoked. That also affects the scrollOffset value. So we call updateLayout before
        // performing a fling so that bringSelectedTabToVisible area isn't called after the fling.
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        mStripLayoutHelper.setScrollOffsetForTesting(-150);

        // Act: Drag and update layout.
        float dragDeltaX = -200.f;
        mStripLayoutHelper.drag(TIMESTAMP, 374.74f, 24.276f, dragDeltaX);

        float expectedOffset = -350; // mScrollOffset + dragDeltaX = -200 - 150 = -350
        // Assert scroll offset position.
        assertEquals(
                "Unexpected scroll offset.",
                expectedOffset,
                mStripLayoutHelper.getScrollOffset(),
                0.0);
        // Reorder mode is disabled for scrolling strip.
        assertFalse(mStripLayoutHelper.getInReorderModeForTesting());
    }

    @Test
    public void testPlaceholderStripLayout_NoTabModel() {
        // Create StripLayoutHelper and mark that after tabs finish restoring, there will be five
        // tabs, where the third tab will be the active tab.
        mStripLayoutHelper = createStripLayoutHelper(false, false);
        mStripLayoutHelper.setTabModelStartupInfo(5, 2, false);

        // Verify there are 5 placeholders.
        StripLayoutTab[] stripTabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        assertTrue("Tab at position 0 should be a placeholder.", stripTabs[0].getIsPlaceholder());
        assertTrue("Tab at position 1 should be a placeholder.", stripTabs[1].getIsPlaceholder());
        assertTrue("Tab at position 2 should be a placeholder.", stripTabs[2].getIsPlaceholder());
        assertTrue("Tab at position 3 should be a placeholder.", stripTabs[3].getIsPlaceholder());
        assertTrue("Tab at position 4 should be a placeholder.", stripTabs[4].getIsPlaceholder());
    }

    @Test
    public void testPlaceholderStripLayout_PrepareOnSetTabModel() {
        // Create StripLayoutHelper and mark that after tabs finish restoring, there will be five
        // tabs, where the third tab will be the active tab.
        mStripLayoutHelper = createStripLayoutHelper(false, false);
        mStripLayoutHelper.setTabModelStartupInfo(5, 2, false);

        // Mock a tab model and set it in the StripLayoutHelper.
        int expectedActiveTabId = 0;
        MockTabModel tabModel = new MockTabModel(mProfile, null);
        tabModel.addTab(expectedActiveTabId);
        tabModel.setIndex(0, TabSelectionType.FROM_NEW);
        tabModel.setActive(true);
        mStripLayoutHelper.setTabModel(tabModel, mTabCreator, false);

        // Verify that the real and placeholder strip tabs were generated in the correct indices.
        StripLayoutTab[] stripTabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        assertTrue("Tab at position 0 should be a placeholder.", stripTabs[0].getIsPlaceholder());
        assertTrue("Tab at position 1 should be a placeholder.", stripTabs[1].getIsPlaceholder());
        assertFalse(
                "Tab at position 2 should not be a placeholder.", stripTabs[2].getIsPlaceholder());
        assertEquals(
                "Tab at position 2 should be the same from the mock.",
                expectedActiveTabId,
                stripTabs[2].getTabId());
        assertTrue("Tab at position 3 should be a placeholder.", stripTabs[3].getIsPlaceholder());
        assertTrue("Tab at position 4 should be a placeholder.", stripTabs[4].getIsPlaceholder());
    }

    @Test
    public void testPlaceholderStripLayout_PrepareOnSetTabModelInfo() {
        // Create StripLayoutHelper and mock a tab model and set it in the StripLayoutHelper.
        int expectedActiveTabId = 0;
        MockTabModel tabModel = new MockTabModel(mProfile, null);
        tabModel.addTab(expectedActiveTabId);
        tabModel.setIndex(0, TabSelectionType.FROM_NEW);
        tabModel.setActive(true);
        mStripLayoutHelper = createStripLayoutHelper(false, false);
        mStripLayoutHelper.setTabModel(tabModel, mTabCreator, false);

        // Verify that there are no placeholders yet.
        StripLayoutTab[] stripTabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        assertEquals("There should be no placeholders yet.", 0, stripTabs.length);

        // Mark that after tabs finish restoring, there will be five tabs, where the third tab will
        // be the active tab.
        mStripLayoutHelper.setTabModelStartupInfo(5, 2, false);

        // Verify that the real and placeholder strip tabs were generated in the correct indices.
        stripTabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        assertTrue("Tab at position 0 should be a placeholder.", stripTabs[0].getIsPlaceholder());
        assertTrue("Tab at position 1 should be a placeholder.", stripTabs[1].getIsPlaceholder());
        assertFalse(
                "Tab at position 2 should not be a placeholder.", stripTabs[2].getIsPlaceholder());
        assertEquals(
                "Tab at position 2 should be the same from the mock.",
                expectedActiveTabId,
                stripTabs[2].getTabId());
        assertTrue("Tab at position 3 should be a placeholder.", stripTabs[3].getIsPlaceholder());
        assertTrue("Tab at position 4 should be a placeholder.", stripTabs[4].getIsPlaceholder());
    }

    @Test
    public void testPlaceholderStripLayout_TabCreated() {
        // Create StripLayoutHelper and mark that after tabs finish restoring, there will be five
        // tabs, where the third tab will be the active tab.
        mStripLayoutHelper = createStripLayoutHelper(false, false);
        mStripLayoutHelper.setTabModelStartupInfo(5, 2, false);

        // Mock a tab model and set it in the StripLayoutHelper.
        int expectedActiveTabId = 0;
        MockTabModel tabModel = new MockTabModel(mProfile, null);
        tabModel.addTab(expectedActiveTabId);
        tabModel.setIndex(0, TabSelectionType.FROM_NEW);
        tabModel.setActive(true);
        mStripLayoutHelper.setTabModel(tabModel, mTabCreator, false);

        // Mark that a tab was restored.
        int expectedRestoredTabId = 1;
        tabModel.addTab(
                new MockTab(expectedRestoredTabId, mProfile),
                0,
                TabLaunchType.FROM_RESTORE,
                TabCreationState.FROZEN_ON_RESTORE);
        mStripLayoutHelper.tabCreated(
                TIMESTAMP, expectedRestoredTabId, Tab.INVALID_TAB_ID, false, false, true);

        // Verify that the third (active) and first tab are real.
        StripLayoutTab[] stripTabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        assertFalse(
                "Tab at position 0 should not be a placeholder.", stripTabs[0].getIsPlaceholder());
        assertEquals(
                "Tab at position 0 should be the same from the mock.",
                expectedRestoredTabId,
                stripTabs[0].getTabId());
        assertTrue("Tab at position 1 should be a placeholder.", stripTabs[1].getIsPlaceholder());
        assertFalse(
                "Tab at position 2 should not be a placeholder.", stripTabs[2].getIsPlaceholder());
        assertEquals(
                "Tab at position 2 should be the same from the mock.",
                expectedActiveTabId,
                stripTabs[2].getTabId());
        assertTrue("Tab at position 3 should be a placeholder.", stripTabs[3].getIsPlaceholder());
        assertTrue("Tab at position 4 should be a placeholder.", stripTabs[4].getIsPlaceholder());
    }

    @Test
    public void testPlaceholderStripLayout_OnTabStateInitialized() {
        // Create StripLayoutHelper and mark that after tabs finish restoring, there will be five
        // tabs, where the third tab will be the active tab.
        mStripLayoutHelper = createStripLayoutHelper(false, false);
        mStripLayoutHelper.setTabModelStartupInfo(5, 2, false);

        // Mock a tab model and set it in the StripLayoutHelper.
        MockTabModel tabModel = new MockTabModel(mProfile, null);
        tabModel.setActive(true);
        mStripLayoutHelper.setTabModel(tabModel, mTabCreator, false);

        // Verify there are placeholders.
        StripLayoutTab[] stripTabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        assertTrue("Tab at position 0 should be a placeholder.", stripTabs[0].getIsPlaceholder());
        assertTrue("Tab at position 1 should be a placeholder.", stripTabs[1].getIsPlaceholder());
        assertTrue("Tab at position 2 should be a placeholder.", stripTabs[2].getIsPlaceholder());
        assertTrue("Tab at position 3 should be a placeholder.", stripTabs[3].getIsPlaceholder());
        assertTrue("Tab at position 4 should be a placeholder.", stripTabs[4].getIsPlaceholder());

        // Add the remaining tabs and mark that the tab state is finished initializing.
        tabModel.addTab(0);
        tabModel.addTab(1);
        tabModel.addTab(2);
        tabModel.addTab(3);
        tabModel.addTab(4);
        tabModel.setIndex(2, TabSelectionType.FROM_NEW);
        mStripLayoutHelper.onTabStateInitialized();

        // Verify the placeholders have been replaced.
        stripTabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        assertFalse(
                "Tab at position 0 should not be a placeholder.", stripTabs[0].getIsPlaceholder());
        assertFalse(
                "Tab at position 1 should not be a placeholder.", stripTabs[1].getIsPlaceholder());
        assertFalse(
                "Tab at position 2 should not be a placeholder.", stripTabs[2].getIsPlaceholder());
        assertFalse(
                "Tab at position 3 should not be a placeholder.", stripTabs[3].getIsPlaceholder());
        assertFalse(
                "Tab at position 4 should not be a placeholder.", stripTabs[4].getIsPlaceholder());
    }

    @Test
    public void testPlaceholderStripLayout_ReorderBeforeTabStateInitialized() {
        // Setup drag drop state.
        setupDragDropState();

        // Create StripLayoutHelper and mark that after tabs finish restoring, there will be five
        // tabs, where the third tab will be the active tab.
        mStripLayoutHelper = createStripLayoutHelper(false, false);
        mStripLayoutHelper.setTabModelStartupInfo(5, 2, false);

        // Attempt to start a reorder and verify that we don't start it.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);
        assertFalse(
                "Should not start reorder mode before tab restore finishes.",
                mStripLayoutHelper.getInReorderModeForTesting());
    }

    @Test
    public void testPlaceholderStripLayout_DragBeforeTabStateInitialized() {
        // Create StripLayoutHelper and mark that after tabs finish restoring, there will be five
        // tabs, where the third tab will be the active tab.
        mStripLayoutHelper = createStripLayoutHelper(false, false);
        mStripLayoutHelper.setTabModelStartupInfo(5, 2, false);

        // Attempt to start a drag and drop and verify that we don't start it.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.startDragAndDropTabForTesting(tabs[2], DRAG_START_POINT);
        verify(mTabDragSource, never())
                .startTabDragAction(any(), any(), any(), anyFloat(), anyFloat());
    }

    @Test
    public void testPlaceholderStripLayout_ScrollOnStartup() {
        // Create StripLayoutHelper and mark that after tabs finish restoring, there will be 20
        // tabs, where the last tab will be the active tab.
        mStripLayoutHelper = createStripLayoutHelper(false, false);
        mStripLayoutHelper.setTabModelStartupInfo(20, 19, false);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Mock a tab model and set it in the StripLayoutHelper.
        MockTabModel tabModel = new MockTabModel(mProfile, null);
        tabModel.setActive(true);
        mStripLayoutHelper.setTabModel(tabModel, mTabCreator, false);
        assertEquals("Offset should be 0.", 0, mStripLayoutHelper.getScrollOffset(), EPSILON);

        // Set size.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        assertNotEquals(
                "Offset should have changed.", 0, mStripLayoutHelper.getScrollOffset(), EPSILON);
    }

    @Test
    public void testPlaceholderStripLayout_CreatedTabOnStartup() {
        // Create StripLayoutHelper and mark that after tabs finish restoring, there will be five
        // tabs, where the third tab will be the active tab.
        mStripLayoutHelper = createStripLayoutHelper(false, false);
        mStripLayoutHelper.setTabModelStartupInfo(5, 2, true);

        // Mock a tab model and set it in the StripLayoutHelper.
        int expectedCreatedTabId = 4;
        MockTabModel tabModel = new MockTabModel(mProfile, null);
        tabModel.addTab(expectedCreatedTabId);
        tabModel.setIndex(0, TabSelectionType.FROM_NEW);
        tabModel.setActive(true);
        mStripLayoutHelper.setTabModel(tabModel, mTabCreator, false);

        // Verify that the fifth (tab created from "intent") is real.
        StripLayoutTab[] stripTabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        assertTrue("Tab at position 0 should be a placeholder.", stripTabs[0].getIsPlaceholder());
        assertTrue("Tab at position 1 should be a placeholder.", stripTabs[1].getIsPlaceholder());
        assertTrue("Tab at position 2 should be a placeholder.", stripTabs[2].getIsPlaceholder());
        assertTrue("Tab at position 3 should be a placeholder.", stripTabs[3].getIsPlaceholder());
        assertFalse(
                "Tab at position 4 should not be a placeholder.", stripTabs[4].getIsPlaceholder());
        assertEquals(
                "Tab at position 4 should be the same from the mock.",
                expectedCreatedTabId,
                stripTabs[4].getTabId());
    }

    private void setupForAnimations() {
        CompositorAnimationHandler mHandler =
                new CompositorAnimationHandler(CallbackUtils.emptyRunnable());
        when(mUpdateHost.getAnimationHandler()).thenReturn(mHandler);

        // Update layout when updateHost.requestUpdate is called.
        doAnswer(
                        invocation -> {
                            mStripLayoutHelper.updateLayout(TIMESTAMP);
                            return null;
                        })
                .when(mUpdateHost)
                .requestUpdate();
    }

    private void initializeTest(boolean rtl, boolean incognito, int tabIndex, int numTabs) {
        mStripLayoutHelper = createStripLayoutHelper(rtl, incognito);
        mIncognito = incognito;

        if (rtl) {
            mStripLayoutHelper.setLeftFadeWidth(
                    incognito
                            ? StripLayoutHelperManager.FADE_LONG_WIDTH_DP
                            : StripLayoutHelperManager.FADE_MEDIUM_WIDTH_DP);
            mStripLayoutHelper.setRightFadeWidth(StripLayoutHelperManager.FADE_SHORT_WIDTH_DP);
        } else {
            mStripLayoutHelper.setLeftFadeWidth(StripLayoutHelperManager.FADE_SHORT_WIDTH_DP);
            mStripLayoutHelper.setRightFadeWidth(
                    incognito
                            ? StripLayoutHelperManager.FADE_LONG_WIDTH_DP
                            : StripLayoutHelperManager.FADE_MEDIUM_WIDTH_DP);
        }

        if (numTabs <= 5) {
            for (int i = 0; i < numTabs; i++) {
                mModel.addTab(TEST_TAB_TITLES[i]);
                when(mModel.getTabAt(i).isHidden()).thenReturn(tabIndex != i);
                when(mModel.getTabAt(i).getView()).thenReturn(mInteractingTabView);
                when(mModel.getTabAt(i).getRootId()).thenReturn(i);
            }
        } else {
            for (int i = 0; i < numTabs; i++) {
                mModel.addTab("Tab " + i);
                when(mModel.getTabAt(i).isHidden()).thenReturn(tabIndex != i);
                when(mModel.getTabAt(i).getView()).thenReturn(mInteractingTabView);
                when(mModel.getTabAt(i).getRootId()).thenReturn(i);
            }
        }
        mModel.setIndex(tabIndex);
        mStripLayoutHelper.setTabModel(mModel, mTabCreator, true);
        mStripLayoutHelper.setTabStripIphControllerForTesting(mController);
        when(mController.wouldTriggerIph(anyInt())).thenReturn(true);
        mStripLayoutHelper.setLayerTitleCache(mLayerTitleCache);
        mStripLayoutHelper.setTabGroupModelFilter(mTabGroupModelFilter);
        mStripLayoutHelper.tabSelected(0, tabIndex, 0);
        // Flush UI updated
    }

    private void initializeTest(int tabIndex) {
        initializeTest(false, false, tabIndex);
    }

    private void initializeTest(boolean rtl, boolean incognito, int tabIndex) {
        initializeTest(rtl, incognito, tabIndex, 5);
    }

    private void assertTabStripAndOrder(String[] expectedAccessibilityDescriptions) {
        // Each tab has a "close button", and there is one additional "new tab" button
        final int expectedNumberOfViews = 2 * expectedAccessibilityDescriptions.length + 1;

        final List<VirtualView> views = new ArrayList<>();
        mStripLayoutHelper.getVirtualViews(views);
        assertEquals(expectedNumberOfViews, views.size());

        // Tab titles
        for (int i = 0; i < expectedNumberOfViews - 1; i++) {
            final String expectedDescription =
                    i % 2 == 0
                            ? expectedAccessibilityDescriptions[i / 2]
                            : String.format("Close %1$s tab", TEST_TAB_TITLES[i / 2]);
            assertEquals(expectedDescription, views.get(i).getAccessibilityDescription());
        }

        assertEquals(
                mActivity
                        .getResources()
                        .getString(
                                mIncognito
                                        ? R.string.accessibility_toolbar_btn_new_incognito_tab
                                        : R.string.accessibility_toolbar_btn_new_tab),
                views.get(views.size() - 1).getAccessibilityDescription());
    }

    private StripLayoutHelper createStripLayoutHelper(boolean rtl, boolean incognito) {
        LocalizationUtils.setRtlForTesting(rtl);
        return new StripLayoutHelper(
                mActivity,
                mManagerHost,
                mUpdateHost,
                mRenderHost,
                incognito,
                mModelSelectorBtn,
                mTabDragSource,
                mToolbarContainerView,
                mWindowAndroid,
                mActionConfirmationManager,
                mDataSharingTabManager,
                () -> true,
                mBottomSheetController,
                mMultiInstanceManager,
                () -> mShareDelegate,
                mBottomSheetCoordinatorFactory);
    }

    private String[] getExpectedAccessibilityDescriptions(int tabIndex) {
        final String[] expectedAccessibilityDescriptions = new String[TEST_TAB_TITLES.length];
        for (int i = 0; i < TEST_TAB_TITLES.length; i++) {
            final boolean isHidden = (i != tabIndex);
            String suffix;
            if (mIncognito) {
                suffix = isHidden ? INCOGNITO_IDENTIFIER : INCOGNITO_IDENTIFIER_SELECTED;
            } else {
                suffix = isHidden ? IDENTIFIER : IDENTIFIER_SELECTED;
            }
            String expectedDescription = "";
            expectedDescription += TEST_TAB_TITLES[i] + ", ";
            expectedAccessibilityDescriptions[i] = expectedDescription + suffix;
        }
        return expectedAccessibilityDescriptions;
    }

    private StripLayoutTab[] getMockedStripLayoutTabs(float tabWidth, float drawX, int numTabs) {
        StripLayoutTab[] tabs = new StripLayoutTab[mModel.getCount()];

        final float delta = tabWidth - TAB_OVERLAP_WIDTH_DP;
        for (int i = 0; i < numTabs; i++) {
            final StripLayoutTab tab = mockStripTab(i, tabWidth, drawX + i * delta);
            tabs[i] = tab;
        }

        return tabs;
    }

    private StripLayoutTab[] getMockedStripLayoutTabs(float tabWidth) {
        return getMockedStripLayoutTabs(tabWidth, 0.f, 5);
    }

    private StripLayoutTab mockStripTab(int id, float tabWidth, float drawX) {
        StripLayoutTab tab = mock(StripLayoutTab.class);
        when(tab.getWidth()).thenReturn(tabWidth);
        when(tab.getTabId()).thenReturn(id);
        when(tab.getDrawX()).thenReturn(drawX);
        return tab;
    }

    /**
     * Mock that the sequence of tabs from startIndex to endIndex are part of that same tab group.
     *
     * @param startIndex The index where we start including tabs in the group (inclusive).
     * @param endIndex The index where we stop including tabs in the group (exclusive).
     */
    private void groupTabs(int startIndex, int endIndex) {
        int groupRootId = mModel.getTabAt(startIndex).getId();
        Token tabGroupId = new Token(0L, groupRootId);
        int numTabs = endIndex - startIndex;
        List<Tab> relatedTabs = new ArrayList<>();
        for (int i = startIndex; i < endIndex; i++) {
            Tab tab = mModel.getTabAt(i);
            when(mTabGroupModelFilter.isTabInTabGroup(eq(tab))).thenReturn(true);
            when(tab.getRootId()).thenReturn(groupRootId);
            when(tab.getTabGroupId()).thenReturn(tabGroupId);
            relatedTabs.add(tab);
        }
        when(mTabGroupModelFilter.getTabCountForGroup(eq(tabGroupId))).thenReturn(numTabs);
        when(mTabGroupModelFilter.getTabsInGroup(eq(tabGroupId))).thenReturn(relatedTabs);

        mStripLayoutHelper.updateGroupTextAndSharedState(groupRootId);
        mStripLayoutHelper.rebuildStripViews();
        if (mStripLayoutHelper.getRunningAnimatorForTesting() != null) {
            mStripLayoutHelper.getRunningAnimatorForTesting().end();
        }
    }

    private void setTabDragSourceMock() {
        when(mTabDragSource.startTabDragAction(any(), any(), any(), anyFloat(), anyFloat()))
                .thenReturn(true);
        MultiWindowTestUtils.enableMultiInstance();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testDrag_AllowMovingTabOutOfStripLayout_SetActiveTab() {
        // Setup with 10 tabs and select tab 5.
        setTabDragSourceMock();
        initializeTest(false, false, 5, 10);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1, 150f, 10);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);
        mStripLayoutHelper.tabSelected(1, 5, 0);
        // Trigger update to set foreground container visibility.
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        StripLayoutTab theClickedTab = tabs[5];

        // Clean active tab environment and ensure.
        mStripLayoutHelper.stopReorderMode();
        assertFalse(
                "Reorder should not be in progress.",
                mStripLayoutHelper.getInReorderModeForTesting());

        // Act and verify.
        mStripLayoutHelper.startDragAndDropTabForTesting(theClickedTab, DRAG_START_POINT);

        verify(mTabDragSource, times(1))
                .startTabDragAction(any(), any(), any(), anyFloat(), anyFloat());
        assertTrue(
                "Drag action should initiate reorder.",
                mStripLayoutHelper.getInReorderModeForTesting());
        assertTrue(
                "Dragged Tab should match selected tab during drag action.",
                mStripLayoutHelper.getReorderDelegateForTesting().getInteractingTabForTesting()
                        == theClickedTab);
        mStripLayoutHelper.stopReorderMode();
        assertFalse(
                "Reorder should not be in progress.",
                mStripLayoutHelper.getInReorderModeForTesting());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testDrag_clearState() {
        initializeTest(3);
        setTabDragSourceMock();
        mStripLayoutHelper.startDragAndDropTabForTesting(
                mStripLayoutHelper.getStripLayoutTabsForTesting()[0], DRAG_START_POINT);

        // Act and verify.
        mStripLayoutHelper.stopReorderMode();
        assertFalse(
                "Should not be in reorder mode", mStripLayoutHelper.getInReorderModeForTesting());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testDrag_sendMoveWindowBroadcast_success() {
        XrUtils.setXrDeviceForTesting(true);
        // Setup with tabs and select first tab.
        setTabDragSourceMock();
        when(mToolbarContainerView.getContext()).thenReturn(mActivity);
        initializeTest(false, false, 0, 5);

        // Act and verify the broadcast is sent.
        onLongPress_OffTab();
        verify(mWindowAndroid, times(1)).sendBroadcast(any());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testDrag_DragOntoSourceStrip() {
        // Setup and mark the active clicked tab.
        initializeTest(false, false, 0, 5);
        ReorderDelegate mockDelegate = mock(ReorderDelegate.class);
        mStripLayoutHelper.setReorderDelegateForTesting(mockDelegate);
        when(mockDelegate.getInReorderMode()).thenReturn(true);

        // Drag tab back onto strip.
        mStripLayoutHelper.handleDragEnter(0f, 0f, true, false);

        // Verify we continue reorder.
        verify(mockDelegate)
                .updateReorderPosition(
                        any(),
                        any(),
                        any(),
                        anyFloat(),
                        anyFloat(),
                        eq(ReorderType.DRAG_ONTO_STRIP));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testDrag_DragOutOfSourceStrip() {
        // Setup and start drag.
        initializeTest(false, false, 1, 5);
        setTabDragSourceMock();
        setupDragDropState();
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutTab draggedTab = tabs[1];
        mStripLayoutHelper.startDragAndDropTabForTesting(draggedTab, DRAG_START_POINT);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Drag tab out of strip.
        mStripLayoutHelper.setTabAtPositionForTesting(draggedTab);
        mStripLayoutHelper.handleDragEnter(0f, 0f, true, false);
        mStripLayoutHelper.handleDragExit(true, false);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify 3rd, 4th and 5th tab's start divider is visible.
        assertFalse("Start divider should be hidden.", tabs[0].isStartDividerVisible());
        assertFalse("DraggedTab divider should be hidden.", draggedTab.isStartDividerVisible());
        assertTrue(
                "Tab after draggedTab start divider should be visible.",
                tabs[2].isStartDividerVisible());
        assertTrue("Start divider should be visible.", tabs[3].isStartDividerVisible());
        assertTrue("Start divider should be visible.", tabs[4].isStartDividerVisible());
    }

    @Test
    public void testGetTabIndexForTabDrop_FirstHalfOfTab() {
        // Setup with 3 tabs.
        initializeTest(false, false, 1, 3);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH_LANDSCAPE,
                SCREEN_HEIGHT,
                false,
                TIMESTAMP,
                PADDING_LEFT,
                PADDING_RIGHT,
                0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // First half of second tab:
        // tabWidth(265) - overlapWidth(28) + inset(16) to +halfTabWidth(132.5) = 253 to 385.5
        int expectedIndex = 1;
        float dropX = 300.f;
        assertEquals(
                "Should prepare to drop at index 1.",
                expectedIndex,
                mStripLayoutHelper.getTabIndexForTabDrop(dropX));
    }

    @Test
    public void testGetTabIndexForTabDrop_SecondHalfOfTab() {
        // Setup with 3 tabs.
        initializeTest(false, false, 1, 3);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH_LANDSCAPE,
                SCREEN_HEIGHT,
                false,
                TIMESTAMP,
                PADDING_LEFT,
                PADDING_RIGHT,
                0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // First half of second tab:
        // tabWidth(265) - overlapWidth(28) + inset(16) to +halfTabWidth(132.5) = 253 to 385.5
        int expectedIndex = 2;
        float dropX = 400.f;
        assertEquals(
                "Should prepare to drop at index 2.",
                expectedIndex,
                mStripLayoutHelper.getTabIndexForTabDrop(dropX));
    }

    @Test
    public void testGetTabIndexForTabDrop_FirstHalfOfCollapsedGroupTitle() {
        // Setup with 3 tabs, make two groups and collapse both groups.
        initializeTest(false, false, 0, 3);
        groupTabs(0, 1);
        groupTabs(1, 2);
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();

        StripLayoutGroupTitle groupTitle1 = (StripLayoutGroupTitle) views[0];
        StripLayoutGroupTitle groupTitle2 = (StripLayoutGroupTitle) views[2];
        StripLayoutTab collapsedTab1 = (StripLayoutTab) views[1];
        StripLayoutTab collapsedTab2 = (StripLayoutTab) views[3];
        groupTitle1.setCollapsed(true);
        groupTitle2.setCollapsed(true);
        collapsedTab1.setCollapsed(true);
        collapsedTab2.setCollapsed(true);
        collapsedTab1.setWidth(TAB_OVERLAP_WIDTH_DP);
        collapsedTab2.setWidth(TAB_OVERLAP_WIDTH_DP);

        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH_LANDSCAPE,
                SCREEN_HEIGHT,
                false,
                TIMESTAMP,
                PADDING_LEFT,
                PADDING_RIGHT,
                0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // First half of the group title in 2nd position:
        // firstGroupTitleRightEdge(68) - groupTitleOverlapWidth(4) + 0 to halfGroupTitleWidth(23) =
        // 64 to 87.
        int expectedIndex = 1;
        float dropX = 80.f;
        assertEquals(
                "Should prepare to drop at index 1.",
                expectedIndex,
                mStripLayoutHelper.getTabIndexForTabDrop(dropX));
    }

    @Test
    public void testGetTabIndexForTabDrop_SecondHalfOfCollapsedGroupTitle() {
        // Setup with 3 tabs, make two groups and collapse both groups.
        initializeTest(false, false, 0, 3);
        groupTabs(0, 1);
        groupTabs(1, 2);
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutGroupTitle groupTitle1 = (StripLayoutGroupTitle) views[0];
        StripLayoutGroupTitle groupTitle2 = (StripLayoutGroupTitle) views[2];
        StripLayoutTab collapsedTab1 = (StripLayoutTab) views[1];
        StripLayoutTab collapsedTab2 = (StripLayoutTab) views[3];
        groupTitle1.setCollapsed(true);
        groupTitle2.setCollapsed(true);
        collapsedTab1.setCollapsed(true);
        collapsedTab2.setCollapsed(true);
        collapsedTab1.setWidth(TAB_OVERLAP_WIDTH_DP);
        collapsedTab2.setWidth(TAB_OVERLAP_WIDTH_DP);

        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH_LANDSCAPE,
                SCREEN_HEIGHT,
                false,
                TIMESTAMP,
                PADDING_LEFT,
                PADDING_RIGHT,
                0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // First half of the group title in 2nd position:
        // firstGroupTitleRightEdge(68) - groupTitleOverlapWidth(4) + 0 to halfGroupTitleWidth(23) =
        // 64 to 87.
        int expectedIndex = 2;
        float dropX = 100.f;
        assertEquals(
                "Should prepare to drop at index 2.",
                expectedIndex,
                mStripLayoutHelper.getTabIndexForTabDrop(dropX));
    }

    @Test
    public void testGetTabIndexForTabDrop_OnStartGap() {
        // Setup with 3 tabs.
        setupDragDropState();
        initializeTest(false, false, 1, 3);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH_LANDSCAPE,
                SCREEN_HEIGHT,
                false,
                TIMESTAMP,
                PADDING_LEFT,
                PADDING_RIGHT,
                0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Prepare for tab drop.
        mStripLayoutHelper.handleDragEnter(0.f, 0.f, false, false);
        // Start gap will be tabWidth(265) / 2 = 132.5
        mStripLayoutHelper.setScrollOffsetForTesting(-132);

        // Last tab ends at:
        // 3 * (tabWidth(265) - overlapWidth(28)) = 711
        int expectedIndex = 0;
        float dropX = 50;
        assertEquals(
                "Should prepare to drop at index 0.",
                expectedIndex,
                mStripLayoutHelper.getTabIndexForTabDrop(dropX));
    }

    @Test
    public void testGetTabIndexForTabDrop_OnEndGap() {
        // Setup with 3 tabs.
        initializeTest(false, false, 1, 3);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH_LANDSCAPE,
                SCREEN_HEIGHT,
                false,
                TIMESTAMP,
                PADDING_LEFT,
                PADDING_RIGHT,
                0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Last tab ends at:
        // 3 * (tabWidth(265) - overlapWidth(28)) = 711
        int expectedIndex = 3;
        float dropX = 750;
        assertEquals(
                "Should prepare to drop at index 3.",
                expectedIndex,
                mStripLayoutHelper.getTabIndexForTabDrop(dropX));
    }

    @Test
    public void testHandleDragEnter() {
        // Setup with 5 tabs.
        setupDragDropState();
        initializeTest(false, false, 1, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH_LANDSCAPE,
                SCREEN_HEIGHT,
                false,
                TIMESTAMP,
                PADDING_LEFT,
                PADDING_RIGHT,
                0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        // Group 2nd and 3rd tab.
        groupTabs(1, 3);

        // Prepare for tab drop.
        mStripLayoutHelper.handleDragEnter(0.f, 0.f, false, false);

        // Verify.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        assertTrue("Should be in reorder mode.", mStripLayoutHelper.getInReorderModeForTesting());
        assertEquals(
                "Should not be tab margin after tab 0.", 0, tabs[0].getTrailingMargin(), EPSILON);
        assertEquals(
                "Should not be tab margin after tab 1.", 0, tabs[1].getTrailingMargin(), EPSILON);
        assertEquals(
                "Should not be tab margin after tab 2.", 0, tabs[2].getTrailingMargin(), EPSILON);
        assertEquals(
                "Should not be tab margin after tab 3.", 0, tabs[3].getTrailingMargin(), EPSILON);
        assertNotEquals(
                "Should be tab margin after tab 4.", 0, tabs[4].getTrailingMargin(), EPSILON);

        assertEquals(
                "TouchableRect does not match. Touch size should match the strip during drag.",
                new RectF(PADDING_LEFT, 0, SCREEN_WIDTH_LANDSCAPE - PADDING_RIGHT, SCREEN_HEIGHT),
                mStripLayoutHelper.getTouchableRect());
    }

    @Test
    public void testUpdateReorderPositionForTabDrop() {
        // Setup with 4 tabs.
        setupDragDropState();
        initializeTest(false, false, 1, 3);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH_LANDSCAPE,
                SCREEN_HEIGHT,
                false,
                TIMESTAMP,
                PADDING_LEFT,
                PADDING_RIGHT,
                0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Prepare for tab drop.
        mStripLayoutHelper.handleDragEnter(0.f, 0.f, false, false);
        mStripLayoutHelper.finishAnimations();
        // Verify initial trailing margins before hover.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        assertEquals(
                "Should not be tab margin after tab 0.", 0, tabs[0].getTrailingMargin(), EPSILON);
        assertEquals(
                "Should not be tab margin after tab 1.", 0, tabs[1].getTrailingMargin(), EPSILON);
        // Last tab has trailing margin.
        assertNotEquals(
                "Should be tab margin after tab 2.", 0, tabs[2].getTrailingMargin(), EPSILON);

        // Drag - Hover between 2nd and 3rd tab:
        // 2 * (tabWidth(265) - overlapWidth(28)) = 474
        mStripLayoutHelper.drag(TIMESTAMP, 474.f, 0, 0);

        // Verify.
        assertNotEquals(
                "Should be tab margin after tab 1.", 0, tabs[1].getTrailingMargin(), EPSILON);

        // Now drag - hover between 1st and 2nd tab:
        // tabWidth(265) - overlapWidth(28) = 237
        mStripLayoutHelper.drag(TIMESTAMP, 237.f, 0, 0);

        // Verify.
        assertNotEquals(
                "Should be tab margin after tab 0.", 0, tabs[0].getTrailingMargin(), EPSILON);
    }

    @Test
    public void testUpdateReorderPositionForTabDrop_StartAndEndGap() {
        // Setup with 3 tabs.
        setupDragDropState();
        initializeTest(false, false, 1, 3);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH_LANDSCAPE,
                SCREEN_HEIGHT,
                false,
                TIMESTAMP,
                PADDING_LEFT,
                PADDING_RIGHT,
                0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Prepare for tab drop.
        mStripLayoutHelper.handleDragEnter(0.f, 0.f, false, false);
        // Start gap will be tabWidth(265) / 2 = 132.5
        mStripLayoutHelper.setScrollOffsetForTesting(-132);
        mStripLayoutHelper.finishAnimations();

        // Verify initial trailing margins before hover.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        assertEquals(
                "Should not be tab margin after tab 0.", 0, tabs[0].getTrailingMargin(), EPSILON);
        assertEquals(
                "Should not be tab margin after tab 1.", 0, tabs[1].getTrailingMargin(), EPSILON);
        // Last tab has trailing margin.
        assertNotEquals(
                "Should be tab margin after tab 2.", 0, tabs[2].getTrailingMargin(), EPSILON);

        // Drag - Hover in start gap:
        mStripLayoutHelper.drag(TIMESTAMP, 50, 0, 0);

        // Verify first tab is not impacted.
        assertEquals(
                "Should not be tab margin after tab 0.", 0, tabs[0].getTrailingMargin(), EPSILON);
        // When hovering in edge gaps, last tab margin is reset (since hoveredTab == lastTab)
        assertEquals(
                "Should not be tab margin after tab 2.", 0, tabs[2].getTrailingMargin(), EPSILON);

        // Drag - Hover in end gap:
        mStripLayoutHelper.drag(TIMESTAMP, 1100, 0, 0);

        // When hovering in edge gaps, last tab margin is reset (since hoveredTab == lastTab)
        assertEquals(
                "Should not be tab margin after tab 2.", 0, tabs[2].getTrailingMargin(), EPSILON);
    }

    @Test
    public void testDestinationStripForTabDrop_DifferentIncognitoState() {
        // Setup with 3 tabs.
        boolean isIncognito = false;
        initializeTest(false, isIncognito, 1, 3);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Prepare and verify no interaction.
        mStripLayoutHelper.handleDragEnter(0.f, 0.f, false, !isIncognito);
        assertFalse(
                "Shouldn't start reorder when dragged tab Incognito state is different.",
                mStripLayoutHelper.getInReorderModeForTesting());

        // Drag and verify no interaction.
        float expectedOffset = mStripLayoutHelper.getScrollOffset();
        mStripLayoutHelper.handleDragWithin(TIMESTAMP, PADDING_LEFT, 0.f, 50.f, !isIncognito);
        assertEquals(
                "Shouldn't have scrolled when dragged tab Incognito is different.",
                expectedOffset,
                mStripLayoutHelper.getScrollOffset(),
                EPSILON);

        // Set reorder mode for testing, then clear for tab drop and verify no interaction.
        mStripLayoutHelper.startReorderModeAtIndexForTesting(0);
        mStripLayoutHelper.handleDragExit(false, !isIncognito);
        assertTrue(
                "Shouldn't stop reorder when dragged tab Incognito state is different.",
                mStripLayoutHelper.getInReorderModeForTesting());
    }

    @Test
    public void testRebuildNonTabViews() {
        // Initialize with 10 tabs. Group tabs 2 through 3. Group tabs 5 through 8.
        initializeTest(false, false, 0, 10);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        groupTabs(1, 3);
        groupTabs(4, 8);

        // Verify.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertEquals("Should be 12 views (10 tabs and 2 titles).", 12, views.length);
        assertTrue(EXPECTED_TAB, views[0] instanceof StripLayoutTab);
        assertTrue(EXPECTED_TITLE, views[1] instanceof StripLayoutGroupTitle);
        assertTrue(EXPECTED_TAB, views[2] instanceof StripLayoutTab);
        assertTrue(EXPECTED_TAB, views[3] instanceof StripLayoutTab);
        assertTrue(EXPECTED_TAB, views[4] instanceof StripLayoutTab);
        assertTrue(EXPECTED_TITLE, views[5] instanceof StripLayoutGroupTitle);
        assertTrue(EXPECTED_TAB, views[6] instanceof StripLayoutTab);
        assertTrue(EXPECTED_TAB, views[7] instanceof StripLayoutTab);
        assertTrue(EXPECTED_TAB, views[8] instanceof StripLayoutTab);
        assertTrue(EXPECTED_TAB, views[9] instanceof StripLayoutTab);
        assertTrue(EXPECTED_TAB, views[10] instanceof StripLayoutTab);
        assertTrue(EXPECTED_TAB, views[11] instanceof StripLayoutTab);

        // verify bottom indicator width.
        float tabWidth = views[0].getWidth() - TAB_OVERLAP_WIDTH_DP;
        float expectedWidth1 =
                views[1].getWidth() + tabWidth * 2 - TAB_GROUP_BOTTOM_INDICATOR_WIDTH_OFFSET;
        float expectedWidth2 =
                views[5].getWidth() + tabWidth * 4 - TAB_GROUP_BOTTOM_INDICATOR_WIDTH_OFFSET;
        assertEquals(
                expectedWidth1, ((StripLayoutGroupTitle) views[1]).getBottomIndicatorWidth(), 0.f);
        assertEquals(
                expectedWidth2, ((StripLayoutGroupTitle) views[5]).getBottomIndicatorWidth(), 0.f);
    }

    @Test
    public void testHandleGroupTitleClick_Collapse() {
        // Initialize with 4 tabs. Group first three tabs.
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher("Android.TabStrip.TabGroupCollapsed", true);
        initializeTest(false, false, 3, 4);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        groupTabs(0, 3);

        // Fake a click on the group indicator.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        mStripLayoutHelper.onClick(TIMESTAMP, views[0], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);

        // Verify the proper event was sent to the TabGroupModelFilter.
        verify(mTabGroupModelFilter)
                .setTabGroupCollapsed(
                        /* rootId= */ 0, /* isCollapsed= */ true, /* animate= */ true);
        // Verify we record the correct metric.
        histogramWatcher.assertExpected("Should record true, since we're collapsing.");
    }

    @Test
    public void testHandleGroupTitleClick_Expand() {
        // Initialize with 4 tabs. Group first three tabs.
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabStrip.TabGroupCollapsed", false);
        initializeTest(false, false, 3, 4);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        groupTabs(0, 3);

        // Mark the group as collapsed. Fake a click on the group indicator.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        mStripLayoutHelper.collapseTabGroupForTesting((StripLayoutGroupTitle) views[0], true);
        when(mTabGroupModelFilter.getTabGroupCollapsed(0)).thenReturn(true);
        mStripLayoutHelper.onClick(TIMESTAMP, views[0], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);

        // Verify the proper event was sent to the TabGroupModelFilter.
        verify(mTabGroupModelFilter)
                .setTabGroupCollapsed(
                        /* rootId= */ 0, /* isCollapsed= */ false, /* animate= */ true);
        // Verify we record the correct metric.
        histogramWatcher.assertExpected("Should record false, since we're expanding.");
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_CONTEXT_MENU)
    public void testSecondaryClick() {
        initializeTest(false, false, 0, 4);
        // Update layout to set view draw properties
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        // Group all tabs
        groupTabs(0, 3);
        mStripLayoutHelper.setTabGroupContextMenuCoordinatorForTesting(
                mTabGroupContextMenuCoordinator);
        mStripLayoutHelper.setTabContextMenuCoordinatorForTesting(mTabContextMenuCoordinator);
        StripLayoutView[] stripViews = mStripLayoutHelper.getStripLayoutViewsForTesting();

        // Secondary click on group indicator - show menu.
        float viewMidX =
                stripViews[0].getTouchTargetBounds().left
                        + (stripViews[0].getTouchTargetBounds().right
                                        - stripViews[0].getTouchTargetBounds().left)
                                / 2;
        mStripLayoutHelper.click(TIMESTAMP, viewMidX, 0, MotionEvent.BUTTON_SECONDARY);
        verify(mTabGroupContextMenuCoordinator).showMenu(any(), any());

        // Secondary click on tab - show menu.
        viewMidX =
                stripViews[1].getTouchTargetBounds().left
                        + (stripViews[1].getTouchTargetBounds().right
                                        - stripViews[1].getTouchTargetBounds().left)
                                / 2;
        mStripLayoutHelper.click(TIMESTAMP, viewMidX, 0, MotionEvent.BUTTON_SECONDARY);
        verify(mTabContextMenuCoordinator).showMenu(any(), anyInt());

        // Secondary click on tab close - show menu.
        // Mock tab's view.
        View tabView = new View(mActivity);
        tabView.setLayoutParams(new MarginLayoutParams(150, 50));
        when(mModel.getTabAt(0).getView()).thenReturn(tabView);
        CompositorButton tabCloseButton = ((StripLayoutTab) stripViews[1]).getCloseButton();
        viewMidX =
                tabCloseButton.getTouchTargetBounds().left
                        + (tabCloseButton.getTouchTargetBounds().right
                                        - tabCloseButton.getTouchTargetBounds().left)
                                / 2;
        mStripLayoutHelper.click(TIMESTAMP, viewMidX, 0, MotionEvent.BUTTON_SECONDARY);
        assertTrue(
                "Should show tab menu after secondary click on tab close.",
                mStripLayoutHelper.isCloseButtonMenuShowingForTesting());
    }

    @Test
    public void testUpdateTabGroupCollapsed_Collapse() {
        // Initialize with 4 tabs. Group first three tabs.
        initializeTest(false, false, 3, 4);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        groupTabs(0, 3);

        // Verify initial dimensions.
        // availableSize = width(800) - NTB(32) - endPadding(8) - offsetXLeft(10) - offsetXRight(20)
        // - groupTitleWidth(46) - titleOverlapWidth(4) = 680.
        // tabWidth = (availableSize(680) + 3 * overlap(28)) / 4 = 193.f
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        float initialTabWidth = 193.f;
        assertEquals("Tab width is incorrect.", initialTabWidth, views[1].getWidth(), EPSILON);
        assertEquals("Tab width is incorrect.", initialTabWidth, views[2].getWidth(), EPSILON);
        assertEquals("Tab width is incorrect.", initialTabWidth, views[3].getWidth(), EPSILON);
        assertEquals("Tab width is incorrect.", initialTabWidth, views[4].getWidth(), EPSILON);

        // Collapse the group.
        mStripLayoutHelper.collapseTabGroupForTesting((StripLayoutGroupTitle) views[0], true);

        // Verify final dimensions.
        float collapsedWidth = TAB_OVERLAP_WIDTH_DP;
        float endTabWidth = MAX_TAB_WIDTH_DP;
        assertEquals("Tab width is incorrect.", collapsedWidth, views[1].getWidth(), EPSILON);
        assertEquals("Tab width is incorrect.", collapsedWidth, views[2].getWidth(), EPSILON);
        assertEquals("Tab width is incorrect.", collapsedWidth, views[3].getWidth(), EPSILON);
        assertEquals("Tab width is incorrect.", endTabWidth, views[4].getWidth(), EPSILON);
    }

    @Test
    public void testUpdateTabGroupCollapsed_Expand() {
        // Initialize with 4 tabs. Group first three tabs.
        initializeTest(false, false, 3, 4);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        groupTabs(0, 3);

        // Collapse the group.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        mStripLayoutHelper.collapseTabGroupForTesting((StripLayoutGroupTitle) views[0], true);

        // Verify initial dimensions.
        float collapsedWidth = TAB_OVERLAP_WIDTH_DP;
        float initialTabWidth = MAX_TAB_WIDTH_DP;
        assertEquals("Tab width is incorrect.", collapsedWidth, views[1].getWidth(), EPSILON);
        assertEquals("Tab width is incorrect.", collapsedWidth, views[2].getWidth(), EPSILON);
        assertEquals("Tab width is incorrect.", collapsedWidth, views[3].getWidth(), EPSILON);
        assertEquals("Tab width is incorrect.", initialTabWidth, views[4].getWidth(), EPSILON);

        // Fake a click on the tab group to expand.
        mStripLayoutHelper.collapseTabGroupForTesting((StripLayoutGroupTitle) views[0], false);

        // Verify final dimensions.
        // availableSize = width(800) - NTB(32) - endPadding(8) - offsetXLeft(10) - offsetXRight(20)
        // - groupTitleWidth(46) - titleOverlapWidth(4) = 680.
        // tabWidth = (availableSize(680) + 3 * overlap(28)) / 4 = 193.f
        float endTabWidth = 193.f;
        assertEquals("Tab width is incorrect.", endTabWidth, views[1].getWidth(), EPSILON);
        assertEquals("Tab width is incorrect.", endTabWidth, views[2].getWidth(), EPSILON);
        assertEquals("Tab width is incorrect.", endTabWidth, views[3].getWidth(), EPSILON);
        assertEquals("Tab width is incorrect.", endTabWidth, views[4].getWidth(), EPSILON);
    }

    @Test
    public void testSelectedTabCollapse_MiddleGroup_PrevTabSelected() {
        // Initialize with 5 tabs. Group last two tabs.
        initializeTest(false, false, 3, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        groupTabs(3, 4);

        // Assert: the 4th tab is selected.
        assertEquals(
                "The tab selected is incorrect.", 3, mStripLayoutHelper.getSelectedStripTabIndex());

        // Assert: the fourth view should be group title.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[3] instanceof StripLayoutGroupTitle);

        // Click to collapse the first tab group.
        mStripLayoutHelper.collapseTabGroupForTesting((StripLayoutGroupTitle) views[3], true);

        // Assert: the previous tab is selected as there is no expanded tab towards the end.
        assertEquals(
                "The tab selected is incorrect.", 2, mStripLayoutHelper.getSelectedStripTabIndex());
    }

    @Test
    public void testSelectedTabCollapse_StartGroup_NextTabSelected() {
        // Initialize with 5 tabs. Group first three tabs.
        initializeTest(false, false, 1, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        groupTabs(0, 3);

        // Assert: the 2nd tab is selected.
        assertEquals(
                "The tab selected is incorrect.", 1, mStripLayoutHelper.getSelectedStripTabIndex());

        // Assert: the first view should be group title.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);

        // Click to collapse the first tab group.
        mStripLayoutHelper.collapseTabGroupForTesting((StripLayoutGroupTitle) views[0], true);

        // Assert: the fourth tab is selected.
        assertEquals(
                "The tab selected is incorrect.", 3, mStripLayoutHelper.getSelectedStripTabIndex());
    }

    @Test
    public void testCollapseSelectedTab_EndGroup_PrevTabSelected() {
        // Initialize with 5 tabs. Group last two tabs.
        initializeTest(false, false, 3, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        groupTabs(3, 5);

        // Assert: the 4th tab is selected.
        assertEquals(
                "The tab selected is incorrect.", 3, mStripLayoutHelper.getSelectedStripTabIndex());

        // Assert: the fourth view should be group title.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[3] instanceof StripLayoutGroupTitle);

        // Click to collapse the first tab group.
        mStripLayoutHelper.collapseTabGroupForTesting((StripLayoutGroupTitle) views[3], true);

        // Assert: the previous tab is selected as there is no expanded tab towards the end.
        assertEquals(
                "The tab selected is incorrect.", 2, mStripLayoutHelper.getSelectedStripTabIndex());
    }

    @Test
    public void testCollapseSelectedTab_OpenNtp() {
        // Initialize with 5 tabs. Group all five tabs.
        initializeTest(false, false, 3, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        groupTabs(0, 5);

        // Assert: the 4th tab is selected.
        assertEquals(
                "The tab selected is incorrect.", 3, mStripLayoutHelper.getSelectedStripTabIndex());

        // Assert: the first view should be group title.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);

        // Click to collapse the first tab group.
        mStripLayoutHelper.collapseTabGroupForTesting((StripLayoutGroupTitle) views[0], true);

        // Verify: Ntp opened since there is no expanded tab on strip.
        verify(mTabCreator).launchNtp();
    }

    @Test
    public void testTabSelected_ExpandsGroup() {
        // Group first two tabs and collapse.
        int startIndex = 3;
        int groupId = 0;
        initializeTest(startIndex);
        groupTabs(groupId, 2);
        when(mTabGroupModelFilter.getTabGroupCollapsed(groupId)).thenReturn(true);

        // Select the first tab.
        mStripLayoutHelper.tabSelected(TIMESTAMP, groupId, startIndex);

        // Verify we auto-expand.
        verify(mTabGroupModelFilter).deleteTabGroupCollapsed(groupId);
    }

    private void testTabCreated_InCollapsedGroup(boolean selected) {
        // Group first two tabs and collapse.
        int groupId = 0;
        initializeTest(/* tabIndex= */ 3);
        groupTabs(groupId, 2);
        when(mTabGroupModelFilter.getTabGroupCollapsed(groupId)).thenReturn(true);

        // Create a tab in the collapsed group.
        int tabId = 5;
        mModel.addTab("new tab");
        mModel.getTabById(tabId).setRootId(groupId);
        mStripLayoutHelper.tabCreated(
                TIMESTAMP,
                tabId,
                tabId,
                selected,
                /* closureCancelled */ false,
                /* onStartup= */ false);

        // Verify we only auto-expand if selected.
        verify(mTabGroupModelFilter, times(selected ? 1 : 0)).deleteTabGroupCollapsed(groupId);
    }

    @Test
    public void testTabCreated_InCollapsedGroup_Selected() {
        testTabCreated_InCollapsedGroup(/* selected= */ true);
    }

    @Test
    public void testTabCreated_InCollapsedGroup_NotSelected() {
        testTabCreated_InCollapsedGroup(/* selected= */ false);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID, ChromeFeatureList.DATA_SHARING})
    public void testTabGroupSyncIph_GroupTitleBubbleIph_ShowSequentially() {
        // Setup tab strip and group the first tab group.
        setupTabGroup(1, 2);

        // group the second tab group.
        groupTabs(3, 5);

        // Get the group titles of both groups.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutGroupTitle groupTitle1 = (StripLayoutGroupTitle) views[1];
        StripLayoutGroupTitle groupTitle2 = (StripLayoutGroupTitle) views[4];

        // Show notification on collapsed group title of the first group.
        groupTitle1.setCollapsed(true);
        int tabId = mModel.getTabAt(1).getId();
        Set<Integer> tabIds = new HashSet<>(Arrays.asList(tabId));
        mStripLayoutHelper.updateTabStripNotificationBubble(tabIds, /* hasUpdate= */ true);

        // Sync both group and share the first group.
        SavedTabGroup savedTabGroup = setupTabGroupSync(mModel.getTabAt(1).getTabGroupId());
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        setupTabGroupSync(mModel.getTabAt(4).getTabGroupId());
        mStripLayoutHelper.rebuildStripViews();

        // Trigger show iph the first time.
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        // Verify tab group sync iph is first displayed.
        verify(mController)
                .showIphOnTabStrip(
                        eq(groupTitle2), eq(null), any(), eq(IphType.TAB_GROUP_SYNC), anyFloat());

        // Trigger show iph the second time.
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        // Verify iph on tab bubble is displayed.
        verify(mController)
                .showIphOnTabStrip(
                        eq(groupTitle1),
                        eq(null),
                        any(),
                        eq(IphType.GROUP_TITLE_NOTIFICATION_BUBBLE),
                        anyFloat());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID, ChromeFeatureList.DATA_SHARING})
    public void testTabGroupSyncIph_TabBubbleIph_ShowSequentially() {
        // Setup tab strip and group the first tab group.
        setupTabGroup(0, 2);

        // group the second tab group.
        groupTabs(3, 5);

        // Get the group titles of both groups.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutGroupTitle groupTitle1 = (StripLayoutGroupTitle) views[0];
        StripLayoutGroupTitle groupTitle2 = (StripLayoutGroupTitle) views[4];

        // Show notification bubble on the second tab of the first group.
        groupTitle1.setCollapsed(false);
        int tabId = mModel.getTabAt(1).getId();
        StripLayoutTab tab = (StripLayoutTab) views[2];
        Set<Integer> tabIds = new HashSet<>(Arrays.asList(tabId));
        mStripLayoutHelper.updateTabStripNotificationBubble(tabIds, /* hasUpdate= */ true);

        // Sync both group and share the first group.
        SavedTabGroup savedTabGroup = setupTabGroupSync(mModel.getTabAt(0).getTabGroupId());
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        setupTabGroupSync(mModel.getTabAt(4).getTabGroupId());
        mStripLayoutHelper.rebuildStripViews();

        // Trigger show iph the first time.
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        // Verify tab group sync iph is first displayed.
        verify(mController)
                .showIphOnTabStrip(
                        eq(groupTitle2), eq(null), any(), eq(IphType.TAB_GROUP_SYNC), anyFloat());

        // Trigger show iph the second time.
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        // Verify iph on tab bubble is displayed.
        verify(mController)
                .showIphOnTabStrip(
                        eq(groupTitle1),
                        eq(tab),
                        any(),
                        eq(IphType.TAB_NOTIFICATION_BUBBLE),
                        anyFloat());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID, ChromeFeatureList.DATA_SHARING})
    public void testTabGroupSyncIph_NotShowForCollaboration() {
        // Setup tab strip and group the first tab group.
        setupTabGroup(3, 5);

        // Get the group title.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutGroupTitle groupTitle = (StripLayoutGroupTitle) views[3];

        // Share the tab group and rebuild strip.
        SavedTabGroup savedTabGroup = setupTabGroupSync(mModel.getTabAt(4).getTabGroupId());
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        mStripLayoutHelper.rebuildStripViews();
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify tab group sync iph is not shown due to collaboration.
        verify(mController, never())
                .showIphOnTabStrip(
                        eq(groupTitle), eq(null), any(), eq(IphType.TAB_GROUP_SYNC), anyFloat());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID})
    public void testTabGroupSyncIph_DismissOnOrientationChanged() {
        // Setup tab group and Tab Group Sync iph.
        setupTabGroup(4, 5);
        mStripLayoutHelper.setLastSyncedGroupIdForTesting(
                mModel.getTabAt(mModel.getCount() - 1).getId());
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutGroupTitle groupTitle = ((StripLayoutGroupTitle) views[4]);

        // Trigger show iph.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify iph is displayed at the correct horizontal position.
        verify(mController)
                .showIphOnTabStrip(
                        eq(groupTitle), eq(null), any(), eq(IphType.TAB_GROUP_SYNC), anyFloat());

        // Change orientation.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_HEIGHT, SCREEN_WIDTH, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Verify iph text bubble is dismissed on screen size change.
        verify(mController, times(2)).dismissTextBubble();
    }

    @Test
    public void testUpdateLastHoveredTab() {
        // Assume tab0 is selected, tab1 is hovered on.
        initializeTabHoverTest();
        var hoveredTab = mStripLayoutHelper.getStripLayoutTabsForTesting()[1];
        mStripLayoutHelper.updateLastHoveredTab(hoveredTab);
        assertEquals(
                "Last hovered tab is not set.", hoveredTab, mStripLayoutHelper.getLastHoveredTab());
        verify(mTabHoverCardView)
                .show(
                        mModel.getTabAt(1),
                        false,
                        hoveredTab.getDrawX(),
                        hoveredTab.getWidth(),
                        SCREEN_HEIGHT,
                        0f);
        assertEquals(
                "Tab container opacity is incorrect.",
                StripLayoutHelper.TAB_OPACITY_VISIBLE,
                hoveredTab.getContainerOpacity(),
                0.0);
    }

    @Test
    public void testUpdateLastHoveredTab_NonZeroStripTopPadding() {
        // Assume tab0 is selected, tab1 is hovered on.
        initializeTabHoverTest();

        // Apply top padding to the strip.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH,
                SCREEN_HEIGHT,
                false,
                TIMESTAMP,
                PADDING_LEFT,
                PADDING_RIGHT,
                PADDING_TOP);

        var hoveredTab = mStripLayoutHelper.getStripLayoutTabsForTesting()[1];
        mStripLayoutHelper.updateLastHoveredTab(hoveredTab);
        assertEquals(
                "Last hovered tab is not set.", hoveredTab, mStripLayoutHelper.getLastHoveredTab());
        verify(mTabHoverCardView)
                .show(
                        mModel.getTabAt(1),
                        false,
                        hoveredTab.getDrawX(),
                        hoveredTab.getWidth(),
                        SCREEN_HEIGHT,
                        PADDING_TOP);
        assertEquals(
                "Tab container opacity is incorrect.",
                StripLayoutHelper.TAB_OPACITY_VISIBLE,
                hoveredTab.getContainerOpacity(),
                0.0);
    }

    @Test
    public void testUpdateLastHoveredTab_animationRunning() {
        initializeTabHoverTest();
        var hoveredTab = mStripLayoutHelper.getStripLayoutTabsForTesting()[1];

        // Assume that animations are running.
        var animator = mock(Animator.class);
        when(animator.isRunning()).thenReturn(true);
        mStripLayoutHelper.setRunningAnimatorForTesting(animator);
        mStripLayoutHelper.updateLastHoveredTab(hoveredTab);
        verify(mTabHoverCardView, never())
                .show(any(), anyBoolean(), anyFloat(), anyFloat(), anyFloat(), anyFloat());
    }

    @Test
    public void testIsViewCompletelyHidden() {
        initializeTabHoverTest();
        var hoveredTab = mStripLayoutHelper.getStripLayoutTabsForTesting()[1];

        // Set simulated hovered StripLayoutTab drawX and width to assume a position beyond the left
        // fade.
        hoveredTab.setDrawX(-50.0f);
        hoveredTab.setWidth(
                StripLayoutHelperManager.FADE_SHORT_WIDTH_DP - 1 - hoveredTab.getDrawX());
        assertTrue(
                "Tab should be considered hidden for hover state.",
                mStripLayoutHelper.isViewCompletelyHidden(hoveredTab));

        // Set simulated hovered StripLayoutTab drawX to assume a position beyond the right fade.
        hoveredTab.setDrawX(SCREEN_WIDTH - StripLayoutHelperManager.FADE_MEDIUM_WIDTH_DP + 1);
        assertTrue(
                "Tab should be considered hidden for hover state.",
                mStripLayoutHelper.isViewCompletelyHidden(hoveredTab));
    }

    private void initializeTabHoverTest() {
        initializeTest(false, false, 0, 3);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.setTabHoverCardView(mTabHoverCardView);
        // For ease of dp/px calculation.
        mContext.getResources().getDisplayMetrics().density = 1f;
    }

    @Test
    public void testSetTabGroupModelFilter() {
        // Setup and verify initial state.
        initializeTest(false, false, 0);
        TabGroupModelFilterObserver observer =
                mStripLayoutHelper.getTabGroupModelFilterObserverForTesting();
        verify(mTabGroupModelFilter).addTabGroupObserver(observer);

        // Set a new TabGroupModelFilter.
        TabGroupModelFilter newModelFilter = mock(TabGroupModelFilter.class);
        when(newModelFilter.getTabModel()).thenReturn(mModel);
        mStripLayoutHelper.setTabGroupModelFilter(newModelFilter);

        // Verify the observers have been updated as expected.
        verify(mTabGroupModelFilter).removeTabGroupObserver(observer);
        verify(newModelFilter).addTabGroupObserver(observer);
    }

    @Test
    public void testSetLayerTitleCache() {
        // Setup. Group 2nd and 3rd tab.
        String expectedTitle = TabGroupTitleUtils.getDefaultTitle(mContext, 2);
        initializeTest(false, false, 0);
        groupTabs(1, 3);

        // Set a new LayerTitleCache.
        LayerTitleCache newTitleCache = mock(LayerTitleCache.class);
        mStripLayoutHelper.setLayerTitleCache(newTitleCache);

        // Verify the observers have been updated as expected.
        verify(newTitleCache).getGroupTitleWidth(eq(false), eq(expectedTitle));
    }

    @Test
    public void testDestroy() {
        // Setup.
        initializeTest(false, false, 0);
        TabGroupModelFilterObserver observer =
                mStripLayoutHelper.getTabGroupModelFilterObserverForTesting();

        // Destroy the instance.
        mStripLayoutHelper.destroy();

        // Verify the observer has been removed as expected.
        verify(mTabGroupModelFilter).removeTabGroupObserver(observer);
    }

    @Test
    public void testHoverCardDelay() {
        initializeTabHoverTest();

        assertEquals(
                "Hover card delay for min tab is incorrect.",
                StripLayoutHelper.MIN_HOVER_CARD_DELAY_MS,
                mStripLayoutHelper.getHoverCardDelay(TAB_WIDTH_SMALL));
        assertEquals(
                "Hover card delay for width < min tab is incorrect.",
                StripLayoutHelper.MIN_HOVER_CARD_DELAY_MS,
                mStripLayoutHelper.getHoverCardDelay(TAB_WIDTH_SMALL - 1.f));
        assertEquals(
                "Hover card delay for medium tab is incorrect.",
                684,
                mStripLayoutHelper.getHoverCardDelay(TAB_WIDTH_MEDIUM));
        assertEquals(
                "Hover card delay for max tab is incorrect.",
                StripLayoutHelper.MAX_HOVER_CARD_DELAY_MS,
                mStripLayoutHelper.getHoverCardDelay(MAX_TAB_WIDTH_DP));
    }

    @Test
    public void testTabHoverCardViewIsNullMetric() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                StripLayoutHelper
                                        .NULL_TAB_HOVER_CARD_VIEW_SHOW_DELAYED_HISTOGRAM_NAME)
                        .build();

        initializeTabHoverTest();

        // Create calls on the correct states.
        mStripLayoutHelper.updateLastHoveredTab(
                mStripLayoutHelper.getStripLayoutTabsForTesting()[0]);
        mStripLayoutHelper.clearTabHoverState();

        histogramWatcher.assertExpected(
                "Shouldn't record any unexpectedly null mTabHoverCardView calls.");

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                StripLayoutHelper
                                        .NULL_TAB_HOVER_CARD_VIEW_SHOW_DELAYED_HISTOGRAM_NAME,
                                false)
                        .build();

        // Destroy the instance.
        mStripLayoutHelper.destroy();

        // Create calls on the incorrect states.
        mStripLayoutHelper.updateLastHoveredTab(
                mStripLayoutHelper.getStripLayoutTabsForTesting()[0]);

        // Check histograms.
        histogramWatcher.assertExpected(
                "Should record an unexpectedly null mTabHoverCardView during the immediate"
                        + " ShowTabHoverCardView call.");
    }

    @Test
    public void testTouchTargetBoundsOnTopPaddingUpdate() {
        // Setup some tabs and group some.
        initializeTest(false, false, 1, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        groupTabs(1, 3);

        // Simulate top padding update.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH,
                SCREEN_HEIGHT,
                false,
                TIMESTAMP,
                PADDING_LEFT,
                PADDING_RIGHT,
                PADDING_TOP);

        // Verify touch target bounds of all strip views including tab close buttons and the NTB.
        var stripViews = mStripLayoutHelper.getStripLayoutViewsForTesting();
        for (int i = 0; i < stripViews.length; i++) {
            assertEquals(
                    "Touch target top bound for view "
                            + stripViews[i].getAccessibilityDescription()
                            + " is incorrect.",
                    PADDING_TOP,
                    stripViews[i].getTouchTargetBounds().top,
                    0f);
            assertEquals(
                    "Touch target bottom bound for view "
                            + stripViews[i].getAccessibilityDescription()
                            + " is incorrect.",
                    PADDING_TOP + SCREEN_HEIGHT,
                    stripViews[i].getTouchTargetBounds().bottom,
                    0f);
            if (stripViews[i] instanceof StripLayoutTab tab) {
                assertEquals(
                        "Touch target top bound for tab's close button is incorrect.",
                        PADDING_TOP,
                        tab.getCloseButton().getTouchTargetBounds().top,
                        0f);
                assertEquals(
                        "Touch target bottom bound for tab's close button is incorrect.",
                        PADDING_TOP + SCREEN_HEIGHT,
                        tab.getCloseButton().getTouchTargetBounds().bottom,
                        0f);
            }
        }
        // topBound(15) = ntbOffsetY(3) + topPadding(20) - touchSlop(8)
        // bottomBound(63) = ntbOffsetY(3) + topPadding(20) + ntbHeight(32) + touchSlop(8)
        assertEquals(
                "Touch target top bound for NTB is incorrect.",
                15,
                mStripLayoutHelper.getNewTabButton().getTouchTargetBounds().top,
                0f);
        assertEquals(
                "Touch target bottom bound for NTB is incorrect.",
                63,
                mStripLayoutHelper.getNewTabButton().getTouchTargetBounds().bottom,
                0f);
    }

    @Test
    public void testScroll_VerticalAxis() {
        // verticalAxisScroll below is positive, so scroll to the right towards the last tab,
        // by SCROLL_SPEED_FACTOR.
        testScroll(
                /* horizontalAxisScroll= */ 0.0f,
                /* verticalAxisScroll= */ 2.4f,
                /* isRtl= */ false,
                /* expectedScrollDelta= */ StripLayoutHelper.SCROLL_SPEED_FACTOR);
    }

    @Test
    public void testScroll_VerticalAxis_Rtl() {
        // verticalAxisScroll below is positive, so scroll to the left towards the last tab
        // (RTL layouts), by SCROLL_SPEED_FACTOR.
        testScroll(
                /* horizontalAxisScroll= */ 0.0f,
                /* verticalAxisScroll= */ 2.4f,
                /* isRtl= */ true,
                /* expectedScrollDelta= */ -StripLayoutHelper.SCROLL_SPEED_FACTOR);
    }

    @Test
    public void testScroll_HorizontalAxis() {
        // horizontalAxisScroll below is positive, so scroll to the right by SCROLL_SPEED_FACTOR.
        testScroll(
                /* horizontalAxisScroll= */ 2.4f,
                /* verticalAxisScroll= */ 0.0f,
                /* isRtl= */ false,
                /* expectedScrollDelta= */ StripLayoutHelper.SCROLL_SPEED_FACTOR);
    }

    @Test
    public void testScroll_HorizontalAxis_Rtl() {
        // horizontalAxisScroll below is positive, so scroll to the right by SCROLL_SPEED_FACTOR.
        // Note that this is still true for RTL layouts. We should respect the user's
        // scrolling direction.
        testScroll(
                /* horizontalAxisScroll= */ 2.4f,
                /* verticalAxisScroll= */ 0.0f,
                /* isRtl= */ true,
                /* expectedScrollDelta= */ StripLayoutHelper.SCROLL_SPEED_FACTOR);
    }

    @Test
    public void testScroll_BothVerticalAndHorizontalAxes() {
        // Only honor horizontalAxisScroll, which is positive, so scroll to the right by
        // SCROLL_SPEED_FACTOR.
        testScroll(
                /* horizontalAxisScroll= */ 2.4f,
                /* verticalAxisScroll= */ -2.4f,
                /* isRtl= */ false,
                /* expectedScrollDelta= */ StripLayoutHelper.SCROLL_SPEED_FACTOR);
    }

    @Test
    public void testScroll_BothVerticalAndHorizontalAxes_Rtl() {
        // Only honor horizontalAxisScroll, which is positive, so scroll to the right by
        // SCROLL_SPEED_FACTOR.
        // Note that this is still true for RTL layouts. We should respect the user's
        // scrolling direction.
        testScroll(
                /* horizontalAxisScroll= */ 2.4f,
                /* verticalAxisScroll= */ -2.4f,
                /* isRtl= */ true,
                /* expectedScrollDelta= */ StripLayoutHelper.SCROLL_SPEED_FACTOR);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TABLET_TAB_STRIP_ANIMATION})
    public void testTabCreated_HorizontalAnimation() {
        // Initialize with default amount of tabs. Clear any animations.
        initializeTest(false, false, 3);
        mStripLayoutHelper.finishAnimationsAndPushTabUpdates();
        assertNull(
                "Animation should not be running.",
                mStripLayoutHelper.getRunningAnimatorForTesting());

        // Act: Create new tab in model and trigger update in tab strip.
        mModel.addTab("new tab");
        mStripLayoutHelper.tabCreated(TIMESTAMP, 5, 3, true, false, false);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TABLET_TAB_STRIP_ANIMATION})
    public void testTabClosing_NoTabResize_HorizontalAnimation() {
        // Arrange
        int tabCount = 10;
        initializeTest(false, false, 9, tabCount);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        setupForAnimations();

        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Act: Call on close tab button handler.
        mStripLayoutHelper.handleCloseButtonClick(
                tabs[9], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);

        // Assert: One set of animations started.
        assertFalse(
                "MultiStepAnimations should not have started.",
                mStripLayoutHelper.isMultiStepCloseAnimationsRunningForTesting());

        // Act: End the tab closing animations to apply final values.
        Animator runningAnimator = mStripLayoutHelper.getRunningAnimatorForTesting();
        assertNotNull(runningAnimator);
        runningAnimator.end();

        // Assert: Tab is closed.
        int expectedTabCount = 9;
        assertEquals(
                "Unexpected tabs count",
                expectedTabCount,
                mStripLayoutHelper.getStripLayoutTabsForTesting().length);

        // Assert: There should only be one set of animations.
        assertFalse(mStripLayoutHelper.getRunningAnimatorForTesting().isRunning());
    }

    /**
     * Tests {@link StripLayoutHelper#onScroll(float, float)}
     *
     * @param horizontalAxisScroll parameter to pass to {@link StripLayoutHelper#onScroll(float,
     *     float)}
     * @param verticalAxisScroll parameter to pass to {@link StripLayoutHelper#onScroll(float,
     *     float)}
     * @param isRtl whether to test RTL layouts
     * @param expectedScrollDelta a 1-D vector on the X axis under the window coordinate system,
     *     representing the direction and distance the tab strip should be scrolled
     */
    private void testScroll(
            float horizontalAxisScroll,
            float verticalAxisScroll,
            boolean isRtl,
            float expectedScrollDelta) {
        initializeTest(isRtl, false, 2, 22);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Set initial scroll offset under ScrollDelegate's dynamic coordinate system
        // (not the static window coordinate system), which means:
        // * For LTR layouts: scroll the tab strip to the left by 1000dp.
        // * For RTL layouts: scroll the tab strip to the right by 1000dp.
        mStripLayoutHelper.setScrollOffsetForTesting(-1000);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // mStripLayoutHelper.getScrollOffset() returns a vector under ScrollDelegate's dynamic
        // coordinate system, so we need to convert it to the static window coordinate system for
        // consistency in the test.
        float originalOffset = MathUtils.flipSignIf(mStripLayoutHelper.getScrollOffset(), isRtl);

        mStripLayoutHelper.onScroll(horizontalAxisScroll, verticalAxisScroll);
        mStripLayoutHelper.finishScrollForTesting();
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Assert scroll offset position (under the window coordinate system).
        float expectedOffset = originalOffset + expectedScrollDelta;
        float actualOffset = MathUtils.flipSignIf(mStripLayoutHelper.getScrollOffset(), isRtl);
        assertEquals(
                "StripLayoutHelper scrolled to the wrong offset.",
                expectedOffset,
                actualOffset,
                0.0);
    }

    private void setupDragDropState() {
        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder().withTab(mTab).build();
        TrackerToken dragTrackerToken =
                DragDropGlobalState.store(
                        /* dragSourceInstanceId= */ 1, dropData, /* dragShadowBuilder= */ null);
        TabDragSource.setDragTrackerTokenForTesting(dragTrackerToken);
    }

    private final class TestTabRemover implements TabRemover {
        @Nullable TabClosureParams mLastParamsForPrepareCloseTabs;
        @Nullable TabClosureParams mLastParamsForForceCloseTabs;

        @Override
        public void closeTabs(
                @NonNull TabClosureParams tabClosureParams,
                boolean allowDialog,
                @Nullable TabModelActionListener listener) {
            forceCloseTabs(tabClosureParams);
        }

        @Override
        public void prepareCloseTabs(
                @NonNull TabClosureParams tabClosureParams,
                boolean allowDialog,
                @Nullable TabModelActionListener listener,
                @NonNull Callback<TabClosureParams> onPreparedCallback) {
            onPreparedCallback.onResult(tabClosureParams);
            mLastParamsForPrepareCloseTabs = tabClosureParams;
        }

        @Override
        public void forceCloseTabs(@NonNull TabClosureParams tabClosureParams) {
            mModel.closeTabs(tabClosureParams);
            mLastParamsForForceCloseTabs = tabClosureParams;
        }

        @Override
        public void removeTab(
                @NonNull Tab tab, boolean allowDialog, @Nullable TabModelActionListener listener) {
            assert false : "Not reached.";
        }
    }
}
