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
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
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
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.PINNED_TAB_WIDTH_DP;
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
import android.view.KeyEvent;
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
import org.chromium.base.DeviceInfo;
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
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.TooltipHandler;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnClickHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnKeyboardFocusHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripIphController.IphType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.TabStripDragHandler;
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
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver.DidRemoveTabGroupReason;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.TabDragHandlerBase;
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
@DisableFeatures({
    ChromeFeatureList.DATA_SHARING,
    ChromeFeatureList.TAB_STRIP_MOUSE_CLOSE_RESIZE_DELAY
})
@EnableFeatures(ChromeFeatureList.TAB_STRIP_AUTO_SELECT_ON_CLOSE_CHANGE)
public class StripLayoutHelperTest {
    private static final Token TAB_GROUP_ID_1 = new Token(1L, 1L);
    private static final Token TAB_GROUP_ID_2 = new Token(1L, 2L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mInteractingTabView;
    @Mock private StripLayoutHelperManager mManager;
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
    @Mock private TooltipHandler mTooltipHandler;
    @Mock private StripLayoutViewOnKeyboardFocusHandler mKeyboardFocusHandler;
    @Mock private TabStripDragHandler mTabStripDragHandler;
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
    @Mock private TabStripContextMenuCoordinator mTabStripContextMenuCoordinator;

    @Captor private ArgumentCaptor<DataSharingService.Observer> mSharingObserverCaptor;
    @Captor private ArgumentCaptor<TabModelActionListener> mTabModelActionListenerCaptor;
    @Captor private ArgumentCaptor<Callback<TabClosureParams>> mTabRemoverCallbackCaptor;
    @Captor private ArgumentCaptor<List<Tab>> mTabListCaptor;

    private Activity mActivity;
    private Context mContext;
    private SharedGroupTestHelper mSharedGroupTestHelper;

    // TODO(crbug.com/369736293): Verify usages and remove duplicate implementations of
    // `TestTabModel` for tab model.
    private final TestTabModel mModel = spy(new TestTabModel());
    private StripLayoutHelper mStripLayoutHelper;
    private boolean mIncognito;
    private static final String[] TEST_TAB_TITLES = {"Tab 1", "Tab 2", "Tab 3", "", null};
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
    private static final float LONG_PRESS_X = 150.f;
    private static final float LONG_PRESS_Y = 0.f;
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
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        CompositorAnimationHandler.setTestingMode(true);
        when(mUpdateHost.getAnimationHandler())
                .thenReturn(new CompositorAnimationHandler(CallbackUtils.emptyRunnable()));
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
        mTabStripDragHandler = null;
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
        groupTabs(0, 1, TAB_GROUP_ID_1);

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
        groupTabs(0, 3, TAB_GROUP_ID_1);

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
        when(mTabGroupModelFilter.getTabGroupTitle(TAB_GROUP_ID_1)).thenReturn("Group name");
        initializeTest(false, false, 0);
        groupTabs(0, 3, TAB_GROUP_ID_1);

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
                        /* end= */ 1,
                        TAB_GROUP_ID_1);

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
                        /* end= */ 3,
                        TAB_GROUP_ID_1);

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
        when(mTabGroupModelFilter.getTabGroupTitle(TAB_GROUP_ID_1)).thenReturn("Group name");
        initializeTest(false, false, 0);

        // Create collaboration group.
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ true,
                        /* duringStripBuild= */ false,
                        /* start= */ 0,
                        /* end= */ 3,
                        TAB_GROUP_ID_1);

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
        when(mTabGroupModelFilter.getTabGroupTitle(TAB_GROUP_ID_1)).thenReturn("Group name");
        initializeTest(false, false, 0);

        // Create collaboration group and show notification bubble on group title.
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ true,
                        /* duringStripBuild= */ false,
                        /* start= */ 0,
                        /* end= */ 3,
                        TAB_GROUP_ID_1);
        int tabId = mModel.getTabAt(0).getId();
        mStripLayoutHelper.collapseTabGroupForTesting(groupTitle, /* isCollapsed= */ true);
        Set<Integer> tabIds = new HashSet<>(Collections.singleton(tabId));
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
        when(mTabGroupModelFilter.getTabGroupTitle(TAB_GROUP_ID_1)).thenReturn("Group name");
        initializeTest(false, false, 0);

        // Create collaboration group and show notification bubble on group title.
        createCollaborationGroup(
                /* multipleCollaborators= */ true,
                /* duringStripBuild= */ false,
                /* start= */ 0,
                /* end= */ 3,
                TAB_GROUP_ID_1);
        int tabId = mModel.getTabAt(0).getId();
        Set<Integer> tabIds = new HashSet<>(Collections.singleton(tabId));
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
    // TODO(crbug.com/425740363): Rewrite the test to work with the animation enabled.
    @DisableFeatures({ChromeFeatureList.TABLET_TAB_STRIP_ANIMATION})
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
        // created.
        final int expectedAnimationCount = numTabs - closeTabIndex - 1;
        assertEquals(expectedAnimationCount, animationList.size());
    }

    @Test
    // TODO(crbug.com/425740363): Rewrite the test to work with the animation enabled.
    @DisableFeatures({ChromeFeatureList.TABLET_TAB_STRIP_ANIMATION})
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
                tabs[closeTabIndex].getDrawX()
                        > mStripLayoutHelper.getVisibleRightBound(
                                /* clampToUnpinnedViews= */ true));

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
        assertEquals("There should not be an animation", 0, animationList.size());
    }

    @Test
    // TODO(crbug.com/425740363): Rewrite the test to work with the animation enabled.
    @DisableFeatures({ChromeFeatureList.TABLET_TAB_STRIP_ANIMATION})
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
                        < mStripLayoutHelper.getVisibleLeftBound(/* clampToUnpinnedViews= */ true));

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
                "There should be 11 animations for the visible tabs.", 11, animationList.size());
    }

    @Test
    // TODO(crbug.com/425740363): Rewrite the test to work with the animation enabled.
    @DisableFeatures({ChromeFeatureList.TABLET_TAB_STRIP_ANIMATION})
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
                        .filter(
                                i ->
                                        tabs[i].getDrawX()
                                                > mStripLayoutHelper.getVisibleRightBound(
                                                        /* clampToUnpinnedViews= */ true))
                        .findFirst()
                        .getAsInt();

        final int closeTabIndex = firstNotVisibleIndex - 1;
        assertTrue(
                "Tab getting closed should be inside of the visible bounds",
                tabs[closeTabIndex].getDrawX()
                        <= mStripLayoutHelper.getVisibleRightBound(
                                /* clampToUnpinnedViews= */ true));

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
                "There should be one animation for the tab moving into the visible bounds",
                1,
                animationList.size());
    }

    @Test
    // TODO(crbug.com/425740363): Rewrite the test to work with the animation enabled.
    @DisableFeatures({ChromeFeatureList.TABLET_TAB_STRIP_ANIMATION})
    public void testResizeStripOnTabClose_AnimateNtb() {
        int numTabs = 50;
        initializeTest(false, false, 0, numTabs);
        // Trigger a size change so the strip layout tab heights and widths get set.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        // Set the initial scroll offset to trigger an update to draw X positions.
        mStripLayoutHelper.setScrollOffsetForTesting(0);

        final StripLayoutHelper stripLayoutHelperSpy = spy(mStripLayoutHelper);
        final StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        stripLayoutHelperSpy.handleCloseButtonClick(
                tabs[tabs.length - 1], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);

        // Verify the initial NTB offset.
        assertEquals(
                mStripLayoutHelper.getNewTabButton().getOffsetX(),
                mStripLayoutHelper.getUnpinnedTabWidthForTesting() - TAB_OVERLAP_WIDTH_DP,
                EPSILON);

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
                "There should be one animation for the NTB sliding to its new position.",
                1,
                animationList.size());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.TABLET_TAB_STRIP_ANIMATION})
    public void testResizeStripOnTabClose_AnimateNtb_OneTab() {
        initializeTest(
                /* rtl= */ false, /* incognito= */ false, /* tabIndex= */ 0, /* numTabs= */ 1);

        final StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.handleCloseButtonClick(
                tabs[0], MotionEventUtils.MOTION_EVENT_BUTTON_NONE);

        // Verify the initial NTB offset.
        assertEquals(
                mStripLayoutHelper.getNewTabButton().getOffsetX(),
                mStripLayoutHelper.getUnpinnedTabWidthForTesting() - TAB_OVERLAP_WIDTH_DP,
                EPSILON);

        final Animator runningAnimator = mStripLayoutHelper.getRunningAnimatorForTesting();
        // Initial animation is the tab removal animation, and after that ends the
        // resizeStripOnTabClose animations begin.
        runningAnimator.end();

        // Verify that the end offset is immediately set, since we skip the animations when closing
        // the final tab.
        float expectedOffsetX = 0.f;
        assertEquals(
                "The NTB offset should immediately be reset.",
                expectedOffsetX,
                mStripLayoutHelper.getNewTabButton().getOffsetX(),
                EPSILON);
    }

    @Test
    // TODO(crbug.com/425740363): Rewrite the test to work with the animation enabled.
    @DisableFeatures({ChromeFeatureList.TABLET_TAB_STRIP_ANIMATION})
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
                mStripLayoutHelper.getUnpinnedTabWidthForTesting(),
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
    public void testQueueAnimationsForNonStripClosures() {
        // Disable testing mode so we can queue animations. Initialize and group first two tabs.
        CompositorAnimationHandler.setTestingMode(/* enabled= */ false);
        initializeTest(false, false, 0);
        groupTabs(0, 2, TAB_GROUP_ID_1);

        // Notify tab closures and verify state.
        List<Tab> closingTabs = new ArrayList<>();
        closingTabs.add(mModel.getTabAt(0));
        closingTabs.add(mModel.getTabAt(1));
        mModel.closeTabs(TabClosureParams.closeTabs(closingTabs).build());
        mStripLayoutHelper.multipleTabsClosed(closingTabs);
        int numClosingTabs = mStripLayoutHelper.getClosingTabsForTesting().size();
        assertEquals("Should have two closing tabs.", 2, numClosingTabs);

        // Notify group removal and verify state.
        when(mTabGroupModelFilter.isTabInTabGroup(any())).thenReturn(false);
        mStripLayoutHelper
                .getTabGroupModelFilterObserverForTesting()
                .didRemoveTabGroup(
                        Tab.INVALID_TAB_ID, TAB_GROUP_ID_1, DidRemoveTabGroupReason.CLOSE);
        int numClosingGroupTitles = mStripLayoutHelper.getClosingGroupTitlesForTesting().size();
        assertEquals("Should have one closing group title.", 1, numClosingGroupTitles);

        // Verify no animations have started yet.
        assertNull(
                "Animations should not yet be started.",
                mStripLayoutHelper.getRunningAnimatorForTesting());

        // Update layout. Verify the queued animations have been started, and the views have not yet
        // been removed.
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        assertNotNull(
                "Animations should now be started.",
                mStripLayoutHelper.getRunningAnimatorForTesting());
        assertEquals(
                "Closed views should still be present while the animations are running.",
                6,
                mStripLayoutHelper.getStripLayoutViewsForTesting().length);

        // Finish animations, then verify the closing views have been removed.
        mStripLayoutHelper.finishAnimations();
        assertEquals(
                "Closed views should no longer be present.",
                3,
                mStripLayoutHelper.getStripLayoutViewsForTesting().length);
    }

    @Test
    public void testQueueAnimationsForNonStripClosures_Unselected() {
        // Disable testing mode so we can queue animations. Initialize and group first two tabs.
        CompositorAnimationHandler.setTestingMode(/* enabled= */ false);
        initializeTest(false, false, 0);
        groupTabs(0, 2, TAB_GROUP_ID_1);
        mStripLayoutHelper.tabModelSelected(/* selected= */ false);

        // Notify tab closures and verify state.
        List<Tab> closingTabs = new ArrayList<>();
        closingTabs.add(mModel.getTabAt(0));
        closingTabs.add(mModel.getTabAt(1));
        mModel.closeTabs(TabClosureParams.closeTabs(closingTabs).build());
        mStripLayoutHelper.multipleTabsClosed(closingTabs);
        int numClosingTabs = mStripLayoutHelper.getClosingTabsForTesting().size();
        assertEquals("Should have no closing tabs.", 0, numClosingTabs);

        // Notify group removal and verify state.
        when(mTabGroupModelFilter.isTabInTabGroup(any())).thenReturn(false);
        mStripLayoutHelper
                .getTabGroupModelFilterObserverForTesting()
                .didRemoveTabGroup(
                        Tab.INVALID_TAB_ID, TAB_GROUP_ID_1, DidRemoveTabGroupReason.CLOSE);
        int numClosingGroupTitles = mStripLayoutHelper.getClosingGroupTitlesForTesting().size();
        assertEquals("Should have no closing group titles.", 0, numClosingGroupTitles);

        // Verify no animations have started.
        assertNull(
                "Animations should not yet be started.",
                mStripLayoutHelper.getRunningAnimatorForTesting());

        // Update layout. Verify the queued animations have still not started, since this model is
        // not showing.
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        mStripLayoutHelper.rebuildStripViews();
        assertNull(
                "Animations should still not yet be started.",
                mStripLayoutHelper.getRunningAnimatorForTesting());
        assertEquals(
                "Closed views should be removed already.",
                3,
                mStripLayoutHelper.getStripLayoutViewsForTesting().length);
    }

    @Test
    public void testQueueAnimationsForNonStripClosures_ClearedOnFinishAnimations() {
        // Disable testing mode so we can queue animations.
        CompositorAnimationHandler.setTestingMode(/* enabled= */ false);
        initializeTest(/* tabIndex= */ 0);

        // Notify tab closures and verify state.
        List<Tab> closingTabs = new ArrayList<>();
        closingTabs.add(mModel.getTabAt(0));
        closingTabs.add(mModel.getTabAt(1));
        mModel.closeTabs(TabClosureParams.closeTabs(closingTabs).build());
        mStripLayoutHelper.multipleTabsClosed(closingTabs);

        // Verify no animations have started yet and closed views are still present.
        assertNull(
                "Animations should not yet be started.",
                mStripLayoutHelper.getRunningAnimatorForTesting());
        assertEquals(
                "Closed views should still be present.",
                5,
                mStripLayoutHelper.getStripLayoutViewsForTesting().length);

        // Manually finish animations. Verify the closed views have been removed.
        mStripLayoutHelper.finishAnimations();
        assertEquals(
                "Closed views should no longer be present.",
                3,
                mStripLayoutHelper.getStripLayoutViewsForTesting().length);

        // Update layout. Verify the queued animations did not start, as they were cleared by the
        // previous #finishAnimations call.
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        assertNull(
                "Animations should still not be started.",
                mStripLayoutHelper.getRunningAnimatorForTesting());
    }

    @Test
    @Feature("Pinned Tabs")
    public void testTabSelected_Pinned_HideCloseBtn() {
        initializeTest(false, true, 3);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);

        // Non-last tab not overlapping strip fade:
        // drawX(530) + tabWidth(140 - 28) < width(800) - offsetXRight(20) - longRightFadeWidth(136)
        when(tabs[3].getDrawX()).thenReturn(530.f);
        when(tabs[3].getIsSelected()).thenReturn(true);

        // Pin the third tab.
        when(tabs[3].getIsPinned()).thenReturn(true);
        mStripLayoutHelper.tabSelected(TIMESTAMP, 3, Tab.INVALID_TAB_ID);

        // Close btn is hidden on the selected tab, because its pinned.
        verify(tabs[3]).setCanShowCloseButton(false, false);
        // Close btn is hidden for the rest of tabs.
        verify(tabs[0]).setCanShowCloseButton(false, false);
        verify(tabs[1]).setCanShowCloseButton(false, false);
        verify(tabs[2]).setCanShowCloseButton(false, false);
        verify(tabs[4]).setCanShowCloseButton(false, false);
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
        when(tabs[3].getIsSelected()).thenReturn(true);
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
        when(tabs[3].getIsSelected()).thenReturn(true);
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
        when(tabs[4].getIsSelected()).thenReturn(true);
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
        when(tabs[4].getIsSelected()).thenReturn(true);
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
        when(tabs[3].getIsSelected()).thenReturn(true);
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
        when(tabs[3].getIsSelected()).thenReturn(true);
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
        when(tabs[4].getIsSelected()).thenReturn(true);
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
        when(tabs[4].getIsSelected()).thenReturn(true);
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
        when(tabs[3].getIsSelected()).thenReturn(true);
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
        when(tabs[3].getIsSelected()).thenReturn(true);
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
        groupTabs(1, 3, TAB_GROUP_ID_1);

        // Trigger update to set divider values.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify start dividers.
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

        // Verify end dividers.
        assertTrue(
                "End divider is next to group indicator should be visible.",
                tabs[0].isEndDividerVisible());
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
        float hiddenOpacity = StripLayoutTabDelegate.TAB_OPACITY_HIDDEN;
        float visibleOpacity = StripLayoutTabDelegate.TAB_OPACITY_VISIBLE;
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
        // rightBound(247) = tabWidth(237) + tabOverLapWidth(28) + offsetXLeft(10)
        assertEquals(
                "TouchableRect does not match. Right size should match last tab's right edge.",
                new RectF(PADDING_LEFT, 0, 275.f, SCREEN_HEIGHT),
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
        // visualLeftBound(543) = stripWidth(800) - PADDING_RIGHT(20) - tabWidth(237)
        // touchableLeftBound(515) = visualLeftBound(543) - TAB_OVERLAP_WIDTH_DP(28)
        assertEquals(
                "TouchableRect does not match. Left side should be extended by tab overlap.",
                new RectF(515.f, 0, SCREEN_WIDTH - PADDING_RIGHT, SCREEN_HEIGHT),
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
                        mTooltipHandler,
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
                        mTooltipHandler,
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
        mStripLayoutHelper.drag(DRAG_START_POINT.x, DRAG_START_POINT.y, 30f);

        // Verify.
        assertEquals(
                "Second tab should be interacting tab.",
                tabs[1],
                mStripLayoutHelper.getInteractingTabForTesting());
        assertTrue(
                "Should start reorder mode when dragging on pressed on tab with mouse.",
                mStripLayoutHelper.getInReorderModeForTesting());
        verify(mTabStripDragHandler)
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
    public void testOnLongPress_OnTab_StartReorder() {
        // Setup
        var tabs = initializeTest_ForTab();
        setupForIndividualTabContextMenu();
        ReorderDelegate mockDelegate = mock(ReorderDelegate.class);
        mStripLayoutHelper.setReorderDelegateForTesting(mockDelegate);
        mStripLayoutHelper.onTabStateInitialized();
        float dragDistance = 40f; // Greater than INITIATE_REORDER_DRAG_THRESHOLD

        // Act
        onLongPress_OnTab(tabs);

        mStripLayoutHelper.drag(LONG_PRESS_X + dragDistance, LONG_PRESS_Y, dragDistance);

        // Verify we start reorder mode.
        verify(mockDelegate)
                .startReorderMode(
                        any(),
                        any(),
                        any(),
                        eq(mStripLayoutHelper.getTabAtPosition(LONG_PRESS_X)),
                        eq(new PointF(LONG_PRESS_X, LONG_PRESS_Y)),
                        eq(ReorderType.START_DRAG_DROP));
    }

    @Test
    public void testOnLongPress_OnTab_NoReorder() {
        // Setup
        var tabs = initializeTest_ForTab();
        ReorderDelegate mockDelegate = mock(ReorderDelegate.class);
        mStripLayoutHelper.setReorderDelegateForTesting(mockDelegate);
        mStripLayoutHelper.onTabStateInitialized();
        setupForIndividualTabContextMenu();

        // Act
        onLongPress_OnTab(tabs);

        // Verify we start reorder mode.
        verify(mockDelegate, never()).startReorderMode(any(), any(), any(), any(), any(), anyInt());
    }

    @Test
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
        List<Integer> expectedTabIds = Collections.singletonList(tabs[1].getTabId());
        verify(mTabContextMenuCoordinator)
                .showMenu(
                        rectProviderArgumentCaptor.capture(),
                        argThat(anchorInfo -> anchorInfo.getAllTabIds().equals(expectedTabIds)));
        // Verify anchorView coordinates.
        StripLayoutView view = mStripLayoutHelper.getViewAtPositionX(10f, true);
        assertThat(view, instanceOf(StripLayoutTab.class));
        Rect expectedRect = new Rect();
        view.getAnchorRect(expectedRect);
        Rect actualRect = rectProviderArgumentCaptor.getValue().getRect();
        assertEquals("Anchor view for menu is positioned incorrectly", expectedRect, actualRect);
    }

    @Test
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
        verify(mTabContextMenuCoordinator).showMenu(rectProviderArgumentCaptor.capture(), any());
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
    @Feature("Tab Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testBottomSheet_constructedWithoutDestroyHide() {
        var tabs = initializeTest_ForTab();
        MockTabModel tabModel = new MockTabModel(mProfile, null);
        when(mProfile.isOffTheRecord()).thenReturn(true);
        mStripLayoutHelper.setTabModel(tabModel, mTabCreator, false);
        tabModel.setActive(true);
        tabModel.addTab(tabs[0].getTabId());
        when(mTab.getUrl()).thenReturn(URL);

        // Initialize the menu.
        mStripLayoutHelper.showTabContextMenuForTesting(
                Collections.singletonList(tabs[0].getTabId()), tabs[0]);

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
        groupTabs(0, 1, TAB_GROUP_ID_1);
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
        groupTabs(0, 1, TAB_GROUP_ID_1);
        setupForGroupContextMenu();

        // Verify drag without context menu starts a scroll.
        mStripLayoutHelper.drag(/* x= */ 10f, /* y= */ 10f, /* deltaX= */ 10f);
        assertTrue(
                "Scroll should be in progress.",
                mStripLayoutHelper.getIsStripScrollInProgressForTesting());
    }

    @Test
    @Feature("Tab Group Context Menu")
    public void testDragToScroll_WithContextMenu() {
        // Initialize.
        initializeTest(false, false, 0);
        groupTabs(0, 1, TAB_GROUP_ID_1);
        setupForGroupContextMenu();

        // Long press on group title and verify drag with context menu does not start a scroll.
        when(mTabGroupContextMenuCoordinator.isMenuShowing()).thenReturn(true);
        mStripLayoutHelper.drag(/* x= */ 10f, /* y= */ 10f, /* deltaX= */ 10f);
        assertFalse(
                "Scroll should not be in progress.",
                mStripLayoutHelper.getIsStripScrollInProgressForTesting());
    }

    @Test
    @Feature("Tab Group Context Menu")
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_GROUP_DRAG_DROP_ANDROID)
    public void testDrag_DismissContextMenu() {
        // Initialize.
        initializeTest(false, false, 0);
        groupTabs(0, 1, TAB_GROUP_ID_1);
        setupForGroupContextMenu();
        // NTB is after group indicator and tabs.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        mStripLayoutHelper.getNewTabButton().setDrawX(views.length * views[0].getWidth());

        // Long press on group title and verify drag with context menu does not start a scroll.
        // Long press on group title.
        mStripLayoutHelper.onLongPress(10f, 0f);
        verify(mTabGroupContextMenuCoordinator).showMenu(any(), any());
        when(mTabGroupContextMenuCoordinator.isMenuShowing()).thenReturn(true);
        mStripLayoutHelper.drag(/* x= */ 60f, /* y= */ 10f, /* deltaX= */ 50f);
        verify(mTabGroupContextMenuCoordinator).dismiss();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testOnLongPress_WithDragDrop_OnTab() {
        var tabs = initializeTest_ForTab();
        setupForIndividualTabContextMenu();
        setTabStripDragHandlerMock();
        mStripLayoutHelper.onTabStateInitialized();
        onLongPress_OnTab(tabs);
        float dragDistance = 40f; // Greater than INITIATE_REORDER_DRAG_THRESHOLD
        mStripLayoutHelper.drag(LONG_PRESS_X + dragDistance, LONG_PRESS_Y, dragDistance);
        // Verify drag invoked
        verify(mTabStripDragHandler)
                .startTabDragAction(any(), any(), any(), anyFloat(), anyFloat());
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
        mStripLayoutHelper.onLongPress(LONG_PRESS_X, LONG_PRESS_Y);
    }

    @Test
    public void testDrag_updateReorderPosition() {
        // Mock 5 tabs.
        initializeTest(false, false, 0, 5);

        // Enter reorder mode and drag.
        ReorderDelegate mockDelegate = mock(ReorderDelegate.class);
        mStripLayoutHelper.setReorderDelegateForTesting(mockDelegate);
        when(mockDelegate.getInReorderMode()).thenReturn(true);
        float dragDistance = 100.f;
        float endX = 50.f + dragDistance;
        mStripLayoutHelper.drag(endX, 0f, dragDistance);

        // Verify we update reorder position.
        verify(mockDelegate)
                .updateReorderPosition(
                        any(),
                        any(),
                        any(),
                        eq(endX),
                        eq(dragDistance),
                        eq(ReorderType.DRAG_WITHIN_STRIP));
    }

    @Test
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
        setTabStripDragHandlerMock();
        Activity activity = spy(mActivity);
        when(mToolbarContainerView.getContext()).thenReturn(activity);

        onLongPress_OffTab();
        // verify tab drag not invoked.
        verifyNoInteractions(mTabStripDragHandler);
    }

    private void onLongPress_OffTab() {
        // Initialize.
        initializeTest(false, false, 0);
        // Set internal state for height, width and paddings.
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, 0, 0, 0, 0);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);
        mStripLayoutHelper.setTabStripContextMenuCoordinatorForTesting(
                mTabStripContextMenuCoordinator);

        // Long press past the last tab.
        int x = (int) SCREEN_WIDTH - 10;
        int y = 0;
        mStripLayoutHelper.setTabAtPositionForTesting(null);
        mStripLayoutHelper.onLongPress(x, y);

        // Verify that we do not show the popup menu anchored on the close button.
        assertFalse(
                "Should not be in reorder mode after long press on empty space on tab strip.",
                mStripLayoutHelper.getInReorderModeForTesting());
        assertFalse(
                "Should not show after long press on empty space on tab strip.",
                mStripLayoutHelper.isCloseButtonMenuShowingForTesting());

        // Verify that we show the strip context menu.
        var rectProviderCaptor = ArgumentCaptor.forClass(RectProvider.class);
        verify(mTabStripContextMenuCoordinator)
                .showMenu(rectProviderCaptor.capture(), eq(mIncognito), any());
        Rect rect = rectProviderCaptor.getValue().getRect();
        assertEquals(new Rect(x, y, x, y), rect);
    }

    @Test
    public void testRightClickOnEmptyStripSpaceShowsStripContextMenu() {
        // Initialize.
        initializeTest(false, false, 0);
        // Set internal state for height, width and paddings.
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, 0, 0, 0, 0);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(150f);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);
        mStripLayoutHelper.setTabStripContextMenuCoordinatorForTesting(
                mTabStripContextMenuCoordinator);

        // Right-click on the empty strip space.
        int x = (int) SCREEN_WIDTH - 10;
        int y = 0;
        mStripLayoutHelper.click(0, x, y, MotionEvent.BUTTON_SECONDARY, 0);

        // Verify that we show the strip context menu.
        var rectProviderCaptor = ArgumentCaptor.forClass(RectProvider.class);
        verify(mTabStripContextMenuCoordinator)
                .showMenu(rectProviderCaptor.capture(), eq(mIncognito), any());
        Rect rect = rectProviderCaptor.getValue().getRect();
        assertEquals(new Rect(x, y, x, y), rect);
    }

    @Test
    public void testTabOutline_SelectedTabInGroup_Show() {
        // Initialize 5 tabs and make 2 tab groups each containing 2 tabs.
        initializeTest(false, false, 0, 5);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);
        groupTabs(0, 2, TAB_GROUP_ID_1);
        groupTabs(2, 4, TAB_GROUP_ID_2);

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
    public void testTabOutline_ReorderMode_NotShow() {
        // Mock 5 tabs and make 2 tab groups each containing 2 tabs.
        initializeTest(false, false, 0, 5);
        StripLayoutTab[] tabs = getMockedStripLayoutTabs(TAB_WIDTH_1, 150f, 5);
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);
        groupTabs(0, 2, TAB_GROUP_ID_1);
        groupTabs(2, 4, TAB_GROUP_ID_2);

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
    // TODO(crbug.com/425740363): Rewrite the test to work with the animation enabled.
    @DisableFeatures(ChromeFeatureList.TABLET_TAB_STRIP_ANIMATION)
    public void testBottomIndicatorWidthAfterTabResize_UngroupedTabClosed() {
        // Arrange
        int tabCount = 6;
        initializeTest(false, false, 3, tabCount);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        groupTabs(0, 2, TAB_GROUP_ID_1);

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
                        mStripLayoutHelper.getUnpinnedTabWidthForTesting(), 2, groupTitle);
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

        // Act: End the animations to apply final values.
        mStripLayoutHelper.finishAnimations();

        // Act: Fake the tab closure and end the animation, so the tab is removed from the model.
        Tab closingTab = mModel.getTabAt(2);
        mStripLayoutHelper.tabClosed(closingTab);
        mStripLayoutHelper.finishAnimations();

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
        groupTabs(0, 2, TAB_GROUP_ID_1);

        // Assert: first view should be a GroupTitle.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);
        StripLayoutGroupTitle groupTitle = ((StripLayoutGroupTitle) views[0]);

        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Check initial bottom indicator width.
        float expectedStartWidth =
                calculateExpectedBottomIndicatorWidth(
                        mStripLayoutHelper.getUnpinnedTabWidthForTesting(), 2, groupTitle);
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
        groupTabs(0, 3, TAB_GROUP_ID_1);
        groupTabs(3, 5, TAB_GROUP_ID_2);

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

    private float calculateExpectedBottomIndicatorWidth(
            float tabWidth, float tabCount, StripLayoutGroupTitle groupTitle) {
        // (tabWidth - tabOverlap(28.f)) * tabCount + groupTitleWidth -
        //      bottomIndicatorWidthOffset(27.f).
        return (tabWidth - TAB_OVERLAP_WIDTH_DP) * tabCount
                + groupTitle.getWidth()
                - TAB_GROUP_BOTTOM_INDICATOR_WIDTH_OFFSET;
    }

    // Note that the testTabGroupDeleteDialog_* tests only cover the behaviors relevant to the
    // tab strip. Tests for much of the internals and dialog flows themselves are in
    // StripTabModelActionListenerUnitTest, TabRemoverImplUnitTest, and TabUngrouperImplUnitTest.
    @Test
    public void testTabGroupDeleteDialog_Reorder_Collaboration() {
        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1, TAB_GROUP_ID_1);
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
    }

    @Test
    public void testTabGroupDeleteDialog_Reorder_Sync_ImmediateContinue() {
        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1, TAB_GROUP_ID_1);
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
    }

    @Test
    public void testTabGroupDeleteDialog_Reorder_Sync_Positive() {
        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1, TAB_GROUP_ID_1);
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
    }

    @Test
    public void testTabGroupDeleteDialog_Reorder_Sync_Negative() {
        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1, TAB_GROUP_ID_1);
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
        setupTabGroup(0, 2, TAB_GROUP_ID_1);
        setTabStripDragHandlerMock();
        setupDragDropState();
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.startDragAndDropTabForTesting(tabs[0], DRAG_START_POINT);

        // Start dragging tab out of group.
        startDraggingTab(tabs, true, 0);

        // No ungroup should start.
        verify(mTabUngrouper, never()).ungroupTabs(any(), anyBoolean(), anyBoolean(), any());
    }

    @Test
    public void testTabGroupDeleteDialog_DragOffStrip_DialogSkipped() {
        when(mActionConfirmationManager.willSkipUngroupTabAttempt()).thenReturn(true);

        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1, TAB_GROUP_ID_1);
        setTabStripDragHandlerMock();
        setupDragDropState();
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.startDragAndDropTabForTesting(tabs[0], DRAG_START_POINT);

        // Start dragging tab out of group.
        startDraggingTab(tabs, true, 0);

        // No ungroup should start.
        verify(mTabUngrouper, never()).ungroupTabs(any(), anyBoolean(), anyBoolean(), any());
    }

    @Test
    public void testTabGroupDeleteDialog_DragOffStrip_Collaboration() {
        // Collaboration groups override the check for skipping.
        when(mActionConfirmationManager.willSkipUngroupTabAttempt()).thenReturn(true);

        // Set up resources for testing tab group delete dialog.
        setupTabGroup(0, 1, TAB_GROUP_ID_1);
        setTabStripDragHandlerMock();
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
        setupTabGroup(0, 1, TAB_GROUP_ID_1);
        setTabStripDragHandlerMock();
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
        setupTabGroup(0, 1, TAB_GROUP_ID_1);
        setTabStripDragHandlerMock();
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
        setupTabGroup(0, 1, TAB_GROUP_ID_1);
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
        setupTabGroup(0, 1, TAB_GROUP_ID_1);
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
        setupTabGroup(0, 1, TAB_GROUP_ID_1);
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
        setupTabGroup(0, 1, TAB_GROUP_ID_1);
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

    private void setupTabGroup(int groupStartIndex, int groupEndIndex, Token tabGroupId) {
        // Mock 5 tabs. Group tab from start to endIndex.
        initializeTest(false, false, 0, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        groupTabs(groupStartIndex, groupEndIndex, tabGroupId);
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
                        /* end= */ 1,
                        TAB_GROUP_ID_1);
        mStripLayoutHelper.collapseTabGroupForTesting(groupTitle, /* isCollapsed= */ true);

        int tabId = mModel.getTabAt(0).getId();

        // Update the root tab.
        Set<Integer> tabIds = new HashSet<>(Collections.singleton(tabId));
        mStripLayoutHelper.updateTabStripNotificationBubble(tabIds, /* hasUpdate= */ true);

        // Verify group title and tab bubble should show.
        assertTrue(
                "Notification bubble on group title should show.",
                groupTitle.getNotificationBubbleShown());
        verify(mLayerTitleCache).updateTabBubble(tabId, /* showBubble= */ true);

        // Verify tab bubble should hide when update is removed.
        mStripLayoutHelper.updateTabStripNotificationBubble(tabIds, /* hasUpdate= */ false);
        assertFalse(
                "Notification bubble on group title should hide.",
                groupTitle.getNotificationBubbleShown());
        verify(mLayerTitleCache).updateTabBubble(tabId, /* showBubble= */ false);
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
                        /* end= */ 1,
                        TAB_GROUP_ID_1);

        int tabId = mModel.getTabAt(0).getId();

        // The root tab is updated from message backend service.
        Set<Integer> tabIds = new HashSet<>(Collections.singleton(tabId));
        mStripLayoutHelper.updateTabStripNotificationBubble(tabIds, /* hasUpdate= */ true);

        // Verify only the tab bubble should show.
        assertFalse(
                "Notification bubble on group title should hide.",
                groupTitle.getNotificationBubbleShown());
        verify(mLayerTitleCache).updateTabBubble(tabId, /* showBubble= */ true);

        // Verify tab bubble should hide when update is removed.
        mStripLayoutHelper.updateTabStripNotificationBubble(tabIds, /* hasUpdate= */ false);
        assertFalse(
                "Notification bubble on group title should hide.",
                groupTitle.getNotificationBubbleShown());
        verify(mLayerTitleCache).updateTabBubble(tabId, /* showBubble= */ false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testSharedGroupStateDuringStripBuild_OnlyOneCollaborator_AvatarShow() {
        // Initialize shared tab group with only one collaborator during strip build.
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ false,
                        /* duringStripBuild= */ true,
                        /* start= */ 0,
                        /* end= */ 1,
                        TAB_GROUP_ID_1);

        // Verify group shared and avatar resources are present when only one collaborator.
        verifySharedGroupState(groupTitle, true);
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
                        /* end= */ 1,
                        TAB_GROUP_ID_1);

        // Verify group shared state is updated and avatar resource is initialized.
        verifySharedGroupState(groupTitle, true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testSharedGroupStateOnGroupAdded_OnlyOneCollaborator_AvatarShow() {
        // Group shared but no other collaborator joined yet.
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ false,
                        /* duringStripBuild= */ false,
                        /* start= */ 0,
                        /* end= */ 1,
                        TAB_GROUP_ID_1);

        // Verify group shared and avatar resources are present when only one collaborator.
        verifySharedGroupState(groupTitle, true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testSharedGroupStateOnGroupChanged_OnlyOneCollaborator_AvatarShow() {
        // Group shared with multiple collaborators.
        StripLayoutGroupTitle groupTitle =
                createCollaborationGroup(
                        /* multipleCollaborators= */ true,
                        /* duringStripBuild= */ false,
                        /* start= */ 0,
                        /* end= */ 1,
                        TAB_GROUP_ID_1);

        // Verify group shared state is updated and avatar resource is initialized.
        verifySharedGroupState(groupTitle, true);

        // Group changed that only one collaborator remains.
        mSharingObserverCaptor
                .getValue()
                .onGroupChanged(
                        SharedGroupTestHelper.newGroupData(
                                COLLABORATION_ID1, SharedGroupTestHelper.GROUP_MEMBER1));

        // Verify group shared and avatar resources are present when only one collaborator.
        verifySharedGroupState(groupTitle, true);
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
                        /* end= */ 1,
                        TAB_GROUP_ID_1);

        // Verify group shared and avatar resources are present when only one collaborator.
        verifySharedGroupState(groupTitle, true);

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
        // 2 members + 1 member for single case.
        loadAvatarBitmap(3);

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
                        /* end= */ 1,
                        TAB_GROUP_ID_1);

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
            boolean multipleCollaborators,
            boolean duringStripBuild,
            int start,
            int end,
            Token tabGroupId) {
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
            savedTabGroup = setupTabGroupSync(tabGroupId);
            savedTabGroup.collaborationId = COLLABORATION_ID1;
            groupTabs(start, end, tabGroupId);
        } else {
            groupTabs(start, end, tabGroupId);
            savedTabGroup = setupTabGroupSync(tabGroupId);
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
            loadAvatarBitmap(2);
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
            loadAvatarBitmap(1);
        }
        return groupTitle;
    }

    private void loadAvatarBitmap(int callCount) {
        ArgumentCaptor<DataSharingAvatarBitmapConfig> configCaptor =
                ArgumentCaptor.forClass(DataSharingAvatarBitmapConfig.class);
        verify(mDataSharingUiDelegate, times(callCount)).getAvatarBitmap(configCaptor.capture());
        for (var item : configCaptor.getAllValues()) {
            item.getDataSharingAvatarCallback().onAvatarLoaded(mAvatarBitmap);
        }
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
        mStripLayoutHelper.drag(startX + dragDistance, 0f, dragDistance);
    }

    @Test
    public void testTabClosed() {
        // Initialize with 10 tabs.
        int tabCount = 10;
        setupForAnimations();
        initializeTest(false, false, 0, tabCount);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Remove tab from model and verify that the tab strip has not yet updated.
        int closedTabId = 1;
        int expectedNumTabs = tabCount;
        Tab tab = mModel.getTabAt(closedTabId);
        mModel.getTabRemover()
                .closeTabs(TabClosureParams.closeTab(tab).build(), /* allowDialog= */ false);
        assertEquals(
                "Tab strip should not yet have changed.",
                expectedNumTabs,
                mStripLayoutHelper.getStripLayoutTabsForTesting().length);

        // Trigger update and verify the tab strip matches the tab model.
        expectedNumTabs = 9;
        mStripLayoutHelper.tabClosed(tab);
        mStripLayoutHelper.finishAnimations();
        assertEquals(
                "Tab strip should match tab model.",
                expectedNumTabs,
                mStripLayoutHelper.getStripLayoutTabsForTesting().length);
        verify(mUpdateHost, times(9)).requestUpdate();
        verify(mUpdateHost, times(4)).requestUpdate(any());
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
        mStripLayoutHelper.finishAnimations(); // end the closing animation

        // Assert
        TestTabRemover testTabRemover = (TestTabRemover) mModel.getTabRemover();
        assertNotNull(testTabRemover.mLastParamsForPrepareCloseTabs);
        assertEquals(shouldAllowUndo, testTabRemover.mLastParamsForPrepareCloseTabs.allowUndo);
        assertNotNull(testTabRemover.mLastParamsForForceCloseTabs);
        assertEquals(shouldAllowUndo, testTabRemover.mLastParamsForForceCloseTabs.allowUndo);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.TABLET_TAB_STRIP_ANIMATION})
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

        // Act: Only end the first animation, so the multi-step animations are still running.
        Tab closingTab = mModel.getTabAt(14);
        mStripLayoutHelper.getRunningAnimatorForTesting().end();
        mStripLayoutHelper.tabClosed(closingTab);

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
        mStripLayoutHelper.finishAnimations();

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
    // TODO(crbug.com/425740363): Rewrite the test to work with the animation enabled.
    @DisableFeatures({ChromeFeatureList.TABLET_TAB_STRIP_ANIMATION})
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

        // Act: Only end the first animation, so the multi-step animations are still running.
        Tab closingTab = mModel.getTabAt(2);
        mStripLayoutHelper.getRunningAnimatorForTesting().end();
        mStripLayoutHelper.tabClosed(closingTab);

        // Assert: Tab is closed and animations are still running.
        int expectedTabCount = 3;
        assertEquals(expectedTabCount, mStripLayoutHelper.getStripLayoutTabsForTesting().length);
        assertTrue(
                "MultiStepAnimations should still be running.",
                mStripLayoutHelper.isMultiStepCloseAnimationsRunningForTesting());

        // Act: Finish the remaining animations.
        mStripLayoutHelper.finishAnimations();

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
        mStripLayoutHelper.finishAnimations();

        verify(mTabHoverCardView).hide();
    }

    private void verifyPendingMouseTabClosure(boolean expectedPendingMouseTabClosure) {
        assertEquals(
                "Unexpected pending mouse tab closure state.",
                expectedPendingMouseTabClosure,
                mStripLayoutHelper.getPendingMouseTabClosureForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_MOUSE_CLOSE_RESIZE_DELAY)
    public void testPendingMouseTabClosure_SetOnClose() {
        // Initialize.
        initializeTest(/* tabIndex= */ 0);

        // Fake a closure from mouse click.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.handleCloseButtonClick(tabs[0], MotionEvent.BUTTON_PRIMARY);

        // Verify state is set.
        verifyPendingMouseTabClosure(/* expectedPendingMouseTabClosure= */ true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_MOUSE_CLOSE_RESIZE_DELAY)
    public void testPendingMouseTabClosure_ClearOnTabClosure() {
        // Initialize and mark a pending a mouse tab closure.
        setupForAnimations();
        initializeTest(/* tabIndex= */ 0);
        mStripLayoutHelper.setPendingMouseTabClosureForTesting(true);

        // Fake a tab closure.
        mStripLayoutHelper.tabClosed(mModel.getTabAt(0));
        mStripLayoutHelper.finishAnimations();

        // Verify state is cleared.
        verifyPendingMouseTabClosure(/* expectedPendingMouseTabClosure= */ false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_MOUSE_CLOSE_RESIZE_DELAY)
    public void testPendingMouseTabClosure_SuppressResize() {
        // Initialize and mark a pending a mouse tab closure.
        initializeTest(/* tabIndex= */ 0);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, 0, 0, 0);
        mStripLayoutHelper.setPendingMouseTabClosureForTesting(true);

        // Attempt a resize.
        mStripLayoutHelper.resizeTabStrip(
                /* animate= */ true, /* tabToAnimate= */ null, /* tabAddedAnimation= */ false);

        // Verify resize was suppressed.
        verifyPendingMouseTabClosure(/* expectedPendingMouseTabClosure= */ true);
        assertNull(
                "Resize animation should not be running.",
                mStripLayoutHelper.getRunningAnimatorForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_MOUSE_CLOSE_RESIZE_DELAY)
    public void testPendingMouseTabClosure_ResizeOnHoverExit_InTabStrip() {
        // Initialize and mark a pending a mouse tab closure.
        initializeTest(/* tabIndex= */ 0);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, 0, 0, 0);
        mStripLayoutHelper.setPendingMouseTabClosureForTesting(true);

        // Notify a hover exit event occurred in the tab strip.
        mStripLayoutHelper.onHoverExit(/* inTabStrip= */ true);

        // Verify suppressed resize state was not cleared.
        verifyPendingMouseTabClosure(/* expectedPendingMouseTabClosure= */ true);
        assertNull(
                "Resize animation should not be running.",
                mStripLayoutHelper.getRunningAnimatorForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_MOUSE_CLOSE_RESIZE_DELAY)
    public void testPendingMouseTabClosure_ResizeOnHoverExit_NotInTabStrip() {
        // Initialize and mark a pending a mouse tab closure.
        initializeTest(/* tabIndex= */ 0);
        mStripLayoutHelper.onSizeChanged(SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, 0, 0, 0);
        mStripLayoutHelper.setPendingMouseTabClosureForTesting(true);

        // Notify a hover exit event occurred outside the tab strip.
        mStripLayoutHelper.onHoverExit(/* inTabStrip= */ false);

        // Verify suppressed resize state was cleared.
        verifyPendingMouseTabClosure(/* expectedPendingMouseTabClosure= */ false);
        assertNotNull(
                "Resize animation should be running.",
                mStripLayoutHelper.getRunningAnimatorForTesting());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_AUTO_SELECT_ON_CLOSE_CHANGE)
    public void testSelectedTabClose_AutoSelect() {
        // Initialize and select the tab at index 2.
        initializeTest(2);
        when(mTab.getId()).thenReturn(2);
        when(mModel.getTabAt(2)).thenReturn(mTab);

        // Fake a close button click on the tab at index 2
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.handleCloseTab(tabs[2], /* allowUndo= */ true);

        // Verify the tab to the left was selected.
        verify(mModel).setIndex(eq(1), anyInt());
    }

    @Test
    public void testSelectedTabClose_AutoSelectOnCloseChange() {
        // Initialize and select the tab at index 2.
        initializeTest(2);
        when(mTab.getId()).thenReturn(2);
        when(mModel.getTabAt(2)).thenReturn(mTab);

        // Fake a close button click on the tab at index 2
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        mStripLayoutHelper.handleCloseTab(tabs[2], /* allowUndo= */ true);

        // Verify the tab to the right was selected.
        verify(mModel).setIndex(eq(3), anyInt());
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
        mStripLayoutHelper.click(1000L, hoveredTab.getDrawX() + 1, hoveredTab.getDrawY() + 1, 0, 0);

        // Assert that the hover card view is closed and the last hovered tab is null.
        verify(mTabHoverCardView, times(1)).hide();
        assertNull(mStripLayoutHelper.getLastHoveredTab());
    }

    @Test
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
                MotionEvent.BUTTON_SECONDARY,
                0);

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
        mStripLayoutHelper.fling(TIMESTAMP, velocityX);
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
        mStripLayoutHelper.fling(TIMESTAMP, velocity);
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
        groupTabs(0, 1, TAB_GROUP_ID_1);
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
        mStripLayoutHelper.fling(TIMESTAMP, velocity);
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
        mStripLayoutHelper.drag(374.74f, 24.276f, dragDeltaX);

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
        verify(mTabStripDragHandler, never())
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
        initializeTest(rtl, incognito, tabIndex, numTabs, mTabGroupModelFilter);
    }

    private void initializeTest(
            boolean rtl,
            boolean incognito,
            int tabIndex,
            int numTabs,
            TabGroupModelFilter tabGroupModelFilter) {
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
        mStripLayoutHelper.tabModelSelected(/* selected= */ true);
        mStripLayoutHelper.setTabModel(mModel, mTabCreator, true);
        mStripLayoutHelper.setTabStripIphControllerForTesting(mController);
        when(mController.wouldTriggerIph(anyInt())).thenReturn(true);
        mStripLayoutHelper.setLayerTitleCache(mLayerTitleCache);
        if (tabGroupModelFilter != null) {
            mStripLayoutHelper.setTabGroupModelFilter(tabGroupModelFilter);
        }
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
                mManager,
                mManagerHost,
                mUpdateHost,
                mRenderHost,
                incognito,
                mModelSelectorBtn,
                mTabStripDragHandler,
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
    private void groupTabs(int startIndex, int endIndex, Token tabGroupId) {
        int numTabs = endIndex - startIndex;
        List<Tab> relatedTabs = new ArrayList<>();
        for (int i = startIndex; i < endIndex; i++) {
            Tab tab = mModel.getTabAt(i);
            when(mTabGroupModelFilter.isTabInTabGroup(eq(tab))).thenReturn(true);
            when(mTabGroupModelFilter.getIndexOfTabInGroup(tab)).thenReturn(i - startIndex);
            when(tab.getTabGroupId()).thenReturn(tabGroupId);
            relatedTabs.add(tab);
        }
        when(mTabGroupModelFilter.tabGroupExists(tabGroupId)).thenReturn(true);
        when(mTabGroupModelFilter.getTabCountForGroup(eq(tabGroupId))).thenReturn(numTabs);
        when(mTabGroupModelFilter.getTabsInGroup(eq(tabGroupId))).thenReturn(relatedTabs);

        mStripLayoutHelper.updateGroupTextAndSharedState(tabGroupId);
        mStripLayoutHelper.rebuildStripViews();
        mStripLayoutHelper.finishAnimations();
    }

    private void setTabStripDragHandlerMock() {
        when(mTabStripDragHandler.startTabDragAction(any(), any(), any(), anyFloat(), anyFloat()))
                .thenReturn(true);
        MultiWindowTestUtils.enableMultiInstance();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testDrag_AllowMovingTabOutOfStripLayout_SetActiveTab() {
        // Setup with 10 tabs and select tab 5.
        setTabStripDragHandlerMock();
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

        verify(mTabStripDragHandler, times(1))
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
        setTabStripDragHandlerMock();
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
    @Feature("Pinned Tabs")
    @EnableFeatures({ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP})
    public void testGetTabIndexForTabDrop_DropPinnedTabOverUnpinnedTab() {
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
        int expectedIndex = 0;
        float dropX = 300.f;
        assertEquals(
                "Should prepare to drop at index 0.",
                expectedIndex,
                mStripLayoutHelper.getTabIndexForTabDrop(dropX, /* isPinned= */ true));
    }

    @Test
    @Feature("Pinned Tabs")
    @EnableFeatures({ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP})
    public void testGetTabIndexForTabDrop_DropUnpinnedTabOverPinnedTab() {
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

        // Pin first two tabs
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        tabs[0].setIsPinned(true);
        tabs[1].setIsPinned(true);

        // First half of second tab:
        // tabWidth(265) - overlapWidth(28) + inset(16) to +halfTabWidth(132.5) = 253 to 385.5
        int expectedIndex = 2;
        float dropX = 300.f;
        assertEquals(
                "Should prepare to drop at index 2.",
                expectedIndex,
                mStripLayoutHelper.getTabIndexForTabDrop(dropX, /* isPinned= */ false));
    }

    @Test
    @Feature("Pinned Tabs")
    @EnableFeatures({ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP})
    public void testGetTabIndexForTabDrop_DropPinnedTabOverPinnedTab() {
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

        // Pin first two tabs
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        tabs[0].setIsPinned(true);
        tabs[1].setIsPinned(true);

        // First half of second tab:
        // tabWidth(265) - overlapWidth(28) + inset(16) to +halfTabWidth(132.5) = 253 to 385.5
        int expectedIndex = 1;
        float dropX = 300.f;
        assertEquals(
                "Should prepare to drop at index 1.",
                expectedIndex,
                mStripLayoutHelper.getTabIndexForTabDrop(dropX, /* isPinned= */ true));
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
                mStripLayoutHelper.getTabIndexForTabDrop(dropX, /* isPinned= */ false));
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
                mStripLayoutHelper.getTabIndexForTabDrop(dropX, /* isPinned= */ false));
    }

    @Test
    public void testGetTabIndexForTabDrop_FirstHalfOfCollapsedGroupTitle() {
        // Setup with 3 tabs, make two groups and collapse both groups.
        initializeTest(false, false, 0, 3);
        groupTabs(0, 1, TAB_GROUP_ID_1);
        groupTabs(1, 2, TAB_GROUP_ID_2);
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
                mStripLayoutHelper.getTabIndexForTabDrop(dropX, /* isPinned= */ false));
    }

    @Test
    public void testGetTabIndexForTabDrop_SecondHalfOfCollapsedGroupTitle() {
        // Setup with 3 tabs, make two groups and collapse both groups.
        initializeTest(false, false, 0, 3);
        groupTabs(0, 1, TAB_GROUP_ID_1);
        groupTabs(1, 2, TAB_GROUP_ID_2);
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
                mStripLayoutHelper.getTabIndexForTabDrop(dropX, /* isPinned= */ false));
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
                mStripLayoutHelper.getTabIndexForTabDrop(dropX, /* isPinned= */ false));
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
                mStripLayoutHelper.getTabIndexForTabDrop(dropX, /* isPinned= */ false));
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
        groupTabs(1, 3, TAB_GROUP_ID_1);

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
        mStripLayoutHelper.handleDragWithin(PADDING_LEFT, 0.f, 50.f, !isIncognito);
        assertEquals(
                "Shouldn't have scrolled when dragged tab Incognito is different.",
                expectedOffset,
                mStripLayoutHelper.getScrollOffset(),
                EPSILON);

        // Set reorder mode for testing, then clear for tab drop and verify no interaction.
        mStripLayoutHelper.setInReorderModeForTesting(true);
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
        groupTabs(1, 3, TAB_GROUP_ID_1);
        groupTabs(4, 8, TAB_GROUP_ID_2);

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
    public void testRebuildStripViews_WithGroupIdToHideWithoutMatchingTab_ClearsInvalidState() {
        initializeTest(/* tabIndex= */ 0);

        // Fake that the tab group ID exists without a matching Tab.
        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID_1)).thenReturn(true);

        // Set nonexistent group ID to hide.
        mStripLayoutHelper.getGroupIdToHideSupplierForTesting().set(TAB_GROUP_ID_1);

        // Verify the invalid state is automatically cleared.
        assertNull(
                "The invalid tab group ID to hide should have been cleared.",
                mStripLayoutHelper.getGroupIdToHideSupplierForTesting().get());
    }

    @Test
    public void testRebuildStripViews_WithNonExistentGroupIdToHide_Asserts() {
        initializeTest(/* tabIndex= */ 0);

        // Fake that there's a matching Tab for the group ID, but that the group ID doesn't exist in
        // the model.
        groupTabs(0, 1, TAB_GROUP_ID_1);
        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID_1)).thenReturn(false);

        // Set nonexistent group ID to hide and verify an AssertionError is thrown.
        try {
            mStripLayoutHelper.getGroupIdToHideSupplierForTesting().set(TAB_GROUP_ID_1);
            throw new Error("Expected assert to be triggered with invalid group ID to hide.");
        } catch (AssertionError ignored) {
        }
    }

    @Test
    public void testRebuildStripViews_WithNonContiguousTabGroup_Asserts() {
        initializeTest(/* tabIndex= */ 0);

        // Create a non-contiguous tab group and verify an AssertionError is thrown.
        try {
            groupTabs(0, 1, TAB_GROUP_ID_1);
            groupTabs(3, 4, TAB_GROUP_ID_1);
            throw new Error("Expected assert to be triggered with a non-contiguous tab group.");
        } catch (AssertionError ignored) {
        }
    }

    @Test
    public void testHandleGroupTitleClick_Collapse() {
        // Initialize with 4 tabs. Group first three tabs.
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher("Android.TabStrip.TabGroupCollapsed", true);
        initializeTest(false, false, 3, 4);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        groupTabs(0, 3, TAB_GROUP_ID_1);

        // Fake a click on the group indicator.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        mStripLayoutHelper.onClick(
                TIMESTAMP, views[0], MotionEventUtils.MOTION_EVENT_BUTTON_NONE, 0);

        // Verify the proper event was sent to the TabGroupModelFilter.
        verify(mTabGroupModelFilter)
                .setTabGroupCollapsed(TAB_GROUP_ID_1, /* isCollapsed= */ true, /* animate= */ true);
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
        groupTabs(0, 3, TAB_GROUP_ID_1);

        // Mark the group as collapsed. Fake a click on the group indicator.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        mStripLayoutHelper.collapseTabGroupForTesting((StripLayoutGroupTitle) views[0], true);
        when(mTabGroupModelFilter.getTabGroupCollapsed(TAB_GROUP_ID_1)).thenReturn(true);
        mStripLayoutHelper.onClick(
                TIMESTAMP, views[0], MotionEventUtils.MOTION_EVENT_BUTTON_NONE, 0);

        // Verify the proper event was sent to the TabGroupModelFilter.
        verify(mTabGroupModelFilter)
                .setTabGroupCollapsed(
                        TAB_GROUP_ID_1, /* isCollapsed= */ false, /* animate= */ true);
        // Verify we record the correct metric.
        histogramWatcher.assertExpected("Should record false, since we're expanding.");
    }

    @Test
    public void testSecondaryClick() {
        initializeTest(false, false, 0, 4);
        // Update layout to set view draw properties
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        // Group all tabs
        groupTabs(0, 3, TAB_GROUP_ID_1);
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
        mStripLayoutHelper.click(TIMESTAMP, viewMidX, 0, MotionEvent.BUTTON_SECONDARY, 0);
        verify(mTabGroupContextMenuCoordinator).showMenu(any(), any());

        // Secondary click on tab - show menu.
        viewMidX =
                stripViews[1].getTouchTargetBounds().left
                        + (stripViews[1].getTouchTargetBounds().right
                                        - stripViews[1].getTouchTargetBounds().left)
                                / 2;
        mStripLayoutHelper.click(TIMESTAMP, viewMidX, 0, MotionEvent.BUTTON_SECONDARY, 0);
        verify(mTabContextMenuCoordinator).showMenu(any(), any());

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
        mStripLayoutHelper.click(TIMESTAMP, viewMidX, 0, MotionEvent.BUTTON_SECONDARY, 0);
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
        groupTabs(0, 3, TAB_GROUP_ID_1);

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
        groupTabs(0, 3, TAB_GROUP_ID_1);

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
        groupTabs(3, 5, TAB_GROUP_ID_1);

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
        groupTabs(0, 3, TAB_GROUP_ID_1);

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
        groupTabs(3, 5, TAB_GROUP_ID_1);

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
        groupTabs(0, 5, TAB_GROUP_ID_1);

        // Assert: the 4th tab is selected.
        assertEquals(
                "The tab selected is incorrect.", 3, mStripLayoutHelper.getSelectedStripTabIndex());

        // Assert: the first view should be group title.
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(EXPECTED_TITLE, views[0] instanceof StripLayoutGroupTitle);

        // Click to collapse the first tab group.
        mStripLayoutHelper.collapseTabGroupForTesting((StripLayoutGroupTitle) views[0], true);

        // Verify: Ntp opened since there is no expanded tab on strip.
        verify(mTabCreator).launchNtp(anyInt());
    }

    @Test
    public void testTabSelected_ExpandsGroup() {
        // Group first two tabs and collapse.
        int startIndex = 3;
        initializeTest(startIndex);
        groupTabs(0, 2, TAB_GROUP_ID_1);
        when(mTabGroupModelFilter.getTabGroupCollapsed(TAB_GROUP_ID_1)).thenReturn(true);

        // Select the first tab.
        mStripLayoutHelper.tabSelected(TIMESTAMP, 0, startIndex);

        // Verify we auto-expand.
        verify(mTabGroupModelFilter).deleteTabGroupCollapsed(TAB_GROUP_ID_1);
    }

    private void testTabCreated_InCollapsedGroup(boolean selected) {
        // Group first two tabs and collapse.
        initializeTest(/* tabIndex= */ 3);
        groupTabs(0, 2, TAB_GROUP_ID_1);
        when(mTabGroupModelFilter.getTabGroupCollapsed(TAB_GROUP_ID_1)).thenReturn(true);

        // Create a tab in the collapsed group.
        int tabId = 5;
        mModel.addTab("new tab");
        Tab tab = mModel.getTabById(tabId);
        when(tab.getRootId()).thenReturn(0);
        when(tab.getTabGroupId()).thenReturn(TAB_GROUP_ID_1);
        mStripLayoutHelper.tabCreated(
                TIMESTAMP,
                tabId,
                tabId,
                selected,
                /* closureCancelled */ false,
                /* onStartup= */ false);

        // Verify we only auto-expand if selected.
        verify(mTabGroupModelFilter, times(selected ? 1 : 0))
                .deleteTabGroupCollapsed(TAB_GROUP_ID_1);
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
    @EnableFeatures({ChromeFeatureList.DATA_SHARING})
    public void testTabGroupSyncIph_GroupTitleBubbleIph_ShowSequentially() {
        // Setup tab strip and group the first tab group.
        setupTabGroup(1, 2, TAB_GROUP_ID_1);

        // group the second tab group.
        groupTabs(3, 5, TAB_GROUP_ID_2);

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
                        eq(groupTitle2),
                        eq(null),
                        any(),
                        eq(IphType.TAB_GROUP_SYNC),
                        anyFloat(),
                        eq(false));

        // Trigger show iph the second time.
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        // Verify iph on tab bubble is displayed.
        verify(mController)
                .showIphOnTabStrip(
                        eq(groupTitle1),
                        eq(null),
                        any(),
                        eq(IphType.GROUP_TITLE_NOTIFICATION_BUBBLE),
                        anyFloat(),
                        eq(false));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.DATA_SHARING})
    public void testTabGroupSyncIph_TabBubbleIph_ShowSequentially() {
        // Setup tab strip and group the first tab group.
        setupTabGroup(0, 2, TAB_GROUP_ID_1);

        // group the second tab group.
        groupTabs(3, 5, TAB_GROUP_ID_2);

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
                        eq(groupTitle2),
                        eq(null),
                        any(),
                        eq(IphType.TAB_GROUP_SYNC),
                        anyFloat(),
                        eq(false));

        // Trigger show iph the second time.
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        // Verify iph on tab bubble is displayed.
        verify(mController)
                .showIphOnTabStrip(
                        eq(groupTitle1),
                        eq(tab),
                        any(),
                        eq(IphType.TAB_NOTIFICATION_BUBBLE),
                        anyFloat(),
                        eq(false));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.DATA_SHARING})
    public void testTabGroupSyncIph_NotShowForCollaboration() {
        // Setup tab strip and group the first tab group.
        setupTabGroup(3, 5, TAB_GROUP_ID_1);

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
                        eq(groupTitle),
                        eq(null),
                        any(),
                        eq(IphType.TAB_GROUP_SYNC),
                        anyFloat(),
                        eq(false));
    }

    @Test
    public void testTabGroupSyncIph_DismissOnOrientationChanged() {
        // Setup tab group and Tab Group Sync iph.
        setupTabGroup(4, 5, TAB_GROUP_ID_1);
        mStripLayoutHelper.setLastSyncedGroupIdForTesting(
                mModel.getTabAt(mModel.getCount() - 1).getTabGroupId());
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutGroupTitle groupTitle = ((StripLayoutGroupTitle) views[4]);

        // Trigger show iph.
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify iph is displayed at the correct horizontal position.
        verify(mController)
                .showIphOnTabStrip(
                        eq(groupTitle),
                        eq(null),
                        any(),
                        eq(IphType.TAB_GROUP_SYNC),
                        anyFloat(),
                        eq(false));

        // Change orientation.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_HEIGHT, SCREEN_WIDTH, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Verify iph text bubble is dismissed on screen size change.
        verify(mController, times(2)).dismissTextBubble();
    }

    @Test
    public void testTabTearingXrIph() {
        DeviceInfo.setIsXrForTesting(true);
        initializeTest(false, false, 0, 1);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Create a new tab.
        mModel.addTab("new tab");
        mStripLayoutHelper.tabCreated(TIMESTAMP, 1, 0, true, false, false);

        // Trigger show iph.
        mStripLayoutHelper.finishAnimations();
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        // Verify iph is displayed at the correct horizontal position.
        verify(mController)
                .showIphOnTabStrip(
                        eq(null),
                        notNull(),
                        any(),
                        eq(IphType.TAB_TEARING_XR),
                        anyFloat(),
                        eq(true));
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
                StripLayoutTabDelegate.TAB_OPACITY_VISIBLE,
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
                StripLayoutTabDelegate.TAB_OPACITY_VISIBLE,
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
        groupTabs(1, 3, TAB_GROUP_ID_1);

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
        groupTabs(1, 3, TAB_GROUP_ID_1);

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
    public void testOpenContextMenu_notApplicable() {
        initializeTest(false, false, 0);
        setupForIndividualTabContextMenu();
        assertFalse(
                "If nothing is keyboard focused, expect context menu to not open",
                mStripLayoutHelper.openKeyboardFocusedContextMenu());
    }

    @Test
    public void testOpenContextMenu_tab() {
        initializeTest(false, false, 0);
        setupForIndividualTabContextMenu();
        StripLayoutTab tabToFocus = mStripLayoutHelper.getStripLayoutTabsForTesting()[0];
        tabToFocus.setKeyboardFocused(true);
        assertTrue(
                "Expected openKeyboardFocusedContextMenu to return true if tab context menu opened",
                mStripLayoutHelper.openKeyboardFocusedContextMenu());
        verify(mTabContextMenuCoordinator, times(1)).showMenu(any(), any());
    }

    @Test
    public void testOpenContextMenu_tabGroup() {
        initializeTest(false, false, 0);
        groupTabs(0, 1, TAB_GROUP_ID_1);
        setupForGroupContextMenu();
        StripLayoutGroupTitle groupTitle =
                (StripLayoutGroupTitle) mStripLayoutHelper.getStripLayoutViewsForTesting()[0];
        groupTitle.setKeyboardFocused(true);
        assertTrue(
                "Expected openKeyboardFocusedContextMenu to return true if tab context menu opened",
                mStripLayoutHelper.openKeyboardFocusedContextMenu());
        verify(mTabGroupContextMenuCoordinator, times(1)).showMenu(any(), any());
    }

    @Test
    public void testOpenContextMenu_closeButton() {
        initializeTest(false, false, 0);
        // Set up a view for ListMenu to use (otherwise constructing the ListMenu will fail).
        View tabView = new View(mActivity);
        when(mModel.getTabAt(anyInt())).thenReturn(mTab);
        when(mTab.getView()).thenReturn(tabView);
        StripLayoutTab parentTab = mStripLayoutHelper.getStripLayoutTabsForTesting()[0];
        parentTab.getCloseButton().setKeyboardFocused(true);
        assertTrue(
                "Expected openKeyboardFocusedContextMenu to return true if tab context menu opened",
                mStripLayoutHelper.openKeyboardFocusedContextMenu());
        assertTrue(
                "Expected close button context menu to be showing",
                mStripLayoutHelper.isCloseButtonMenuShowingForTesting());
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

        // Act: Fake the tab closure and end the animation, so the tab is removed from the model.
        Tab closingTab = mModel.getTabAt(9);
        mStripLayoutHelper.finishAnimations();
        mStripLayoutHelper.tabClosed(closingTab);

        // Assert: Tab is closed.
        int expectedTabCount = 9;
        assertEquals(
                "Unexpected tabs count",
                expectedTabCount,
                mStripLayoutHelper.getStripLayoutTabsForTesting().length);

        // Assert: There should only be one set of animations.
        assertFalse(mStripLayoutHelper.getRunningAnimatorForTesting().isRunning());
    }

    @Test
    public void testWidthCalculated_withNullTabGroupModelFilter() {
        initializeTest(false, false, 0, 1, null);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        assertNotEquals(0, mStripLayoutHelper.getUnpinnedTabWidthForTesting(), EPSILON);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testMultiSelect_CtrlClick_SelectsAndActivatesTab() {
        initializeTest(false, false, 0, 5);
        // Update layout to set view draw properties
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        // Arrange
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutView[] stripViews = mStripLayoutHelper.getStripLayoutViewsForTesting();

        // Act
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 2),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_CTRL_ON);

        // Verify the clicked tab becomes the active tab.
        verify(mModel).setIndex(eq(2), anyInt());
        // Assert
        assertTrue(
                "Clicked tab should be in the multi-select set.",
                mModel.isTabMultiSelected(tabs[2].getTabId()));
        assertTrue(
                "Previously selected tab should also be in the multi-select set",
                mModel.isTabMultiSelected(tabs[0].getTabId()));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testMultiSelect_CtrlClick_TogglesSelection() {
        initializeTest(false, false, 0, 5);
        // Update layout to set view draw properties
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        // Arrange
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        int clickedTabId = tabs[2].getTabId();
        StripLayoutView[] stripViews = mStripLayoutHelper.getStripLayoutViewsForTesting();

        // Act: First click to select a different tab.
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 2),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_CTRL_ON);
        assertTrue(
                "Tab should be selected after first Ctrl+Click.",
                mModel.isTabMultiSelected(clickedTabId));

        // Act: Second click to deselect the other tab.
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 0),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_CTRL_ON);

        // Assert
        assertFalse(
                "Tab should be deselected after second Ctrl+Click.",
                mModel.isTabMultiSelected(tabs[0].getTabId()));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testMultiSelect_ShiftClick_SelectsRange() {
        initializeTest(false, false, 1, 5); // Start with Tab 1 active (this is the anchor).
        // Update layout to set view draw properties
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        // Arrange
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutView[] stripViews = mStripLayoutHelper.getStripLayoutViewsForTesting();

        // Act: Shift+Click Tab 3.
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 3),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_SHIFT_ON);

        // Assert: Tabs 1, 2, and 3 should be selected.
        assertEquals(
                "Should be 3 tabs selected in the range.", 3, mModel.getMultiSelectedTabsCount());
        assertTrue("Tab 1 should be selected.", mModel.isTabMultiSelected(tabs[1].getTabId()));
        assertTrue("Tab 2 should be selected.", mModel.isTabMultiSelected(tabs[2].getTabId()));
        assertTrue("Tab 3 should be selected.", mModel.isTabMultiSelected(tabs[3].getTabId()));

        // Verify the clicked tab becomes the active tab.
        verify(mModel).setIndex(eq(3), anyInt());
        // Verify the anchor tab is set to 1, and has not been reset.
        assertEquals(
                "Anchor tab should not change during a Shift+Click sequence.",
                tabs[1].getTabId(),
                mStripLayoutHelper.getAnchorTabIdForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testMultiSelect_ShiftClick_IsDestructive() {
        initializeTest(false, false, 2, 5);
        // Update layout to set view draw properties
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        // Arrange
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutView[] stripViews = mStripLayoutHelper.getStripLayoutViewsForTesting();

        // Act: First, Shift+Click tab 3.
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 3),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_SHIFT_ON);
        assertEquals("Initial selection should have 2 tab.", 2, mModel.getMultiSelectedTabsCount());
        assertEquals(
                "Anchor should be tab 2.",
                tabs[2].getTabId(),
                mStripLayoutHelper.getAnchorTabIdForTesting());
        // Verify the clicked tab becomes the active tab.
        verify(mModel).setIndex(eq(3), anyInt());

        // Act: Now, Shift+Click tab 0. This should clear the old selection.
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 0),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_SHIFT_ON);

        // Assert: The old selection {3} is gone, and the new range {0, 1, 2} is selected.
        assertEquals(
                "Should be 3 tabs selected in the new range.",
                3,
                mModel.getMultiSelectedTabsCount());
        assertFalse("Tab 3 should not be selected.", mModel.isTabMultiSelected(tabs[3].getTabId()));
        assertTrue("Tab 0 should be selected.", mModel.isTabMultiSelected(tabs[0].getTabId()));
        assertTrue("Tab 1 should be selected.", mModel.isTabMultiSelected(tabs[1].getTabId()));
        assertTrue("Tab 2 should be selected.", mModel.isTabMultiSelected(tabs[2].getTabId()));
        // Verify the clicked tab becomes the active tab.
        verify(mModel).setIndex(eq(0), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testMultiSelect_ShiftCtrlClick_IsAdditive() {
        initializeTest(false, false, 0, 5);
        // Update layout to set view draw properties
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        // Arrange
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutView[] stripViews = mStripLayoutHelper.getStripLayoutViewsForTesting();

        // Act: First, Ctrl+Click tab 4.
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 4),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_CTRL_ON);
        assertTrue("Tab 0 should be selected.", mModel.isTabMultiSelected(tabs[0].getTabId()));
        assertTrue("Tab 4 should be selected.", mModel.isTabMultiSelected(tabs[4].getTabId()));

        // Act: Now, Shift+Ctrl+Click tab 2.
        // This should add the range {2, 3, 4} to the selection {0, 4}.
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 2),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_SHIFT_ON | KeyEvent.META_CTRL_ON);

        // Assert: The final selection should be {0, 2, 3, 4}.
        assertEquals(
                "Should be 4 tabs in the final selection.", 4, mModel.getMultiSelectedTabsCount());
        assertTrue(
                "Tab 0 should still be selected.", mModel.isTabMultiSelected(tabs[0].getTabId()));
        assertFalse("Tab 1 should not be selected.", mModel.isTabMultiSelected(tabs[1].getTabId()));
        assertTrue("Tab 2 should be selected.", mModel.isTabMultiSelected(tabs[2].getTabId()));
        assertTrue("Tab 3 should be selected.", mModel.isTabMultiSelected(tabs[3].getTabId()));
        assertTrue("Tab 4 should be selected.", mModel.isTabMultiSelected(tabs[4].getTabId()));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testMultiSelect_StandardClick_ClearsSelection() {
        initializeTest(false, false, 0, 5);
        // Update layout to set view draw properties
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        // Arrange
        StripLayoutView[] stripViews = mStripLayoutHelper.getStripLayoutViewsForTesting();

        // Act: First, select a few tabs.
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 1),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_CTRL_ON);
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 3),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_CTRL_ON);
        assertEquals(
                "Initial selection should have 3 tabs.", 3, mModel.getMultiSelectedTabsCount());

        // Act: Now, perform a standard click on another tab.
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 0),
                0,
                MotionEvent.BUTTON_PRIMARY,
                0);

        // The active tab is always considered selected.
        assertEquals(
                "Selection should be empty after a standard click.",
                1,
                mModel.getMultiSelectedTabsCount());
        // Verify the clicked tab becomes the active tab.
        verify(mModel, times(3)).setIndex(anyInt(), anyInt()); // 2 for Ctrl, 1 for standard click
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testMultiSelect_ShiftClick_ThroughCollapsedGroup_ExpandsGroup() {
        initializeTest(false, false, 0, 5);
        groupTabs(1, 4, TAB_GROUP_ID_1);
        when(mTabGroupModelFilter.getTabGroupCollapsed(TAB_GROUP_ID_1)).thenReturn(true);
        // Update layout to set view draw properties
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        StripLayoutView[] stripViews = mStripLayoutHelper.getStripLayoutViewsForTesting();

        // Shift+Click a tab across the collapsed group. Anchor is tab 0.
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 4),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_SHIFT_ON);

        verify(mTabGroupModelFilter)
                .setTabGroupCollapsed(eq(TAB_GROUP_ID_1), eq(false), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testMultiSelect_CtrlClick_OnActiveTab_SelectsLeftmost() {
        initializeTest(false, false, 0, 5);
        // Update layout to set view draw properties
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutView[] stripViews = mStripLayoutHelper.getStripLayoutViewsForTesting();

        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 4),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_CTRL_ON);
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 2),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_CTRL_ON);
        assertEquals("Initial selection should have 3 tabs", 3, mModel.getMultiSelectedTabsCount());

        // Ctrl+Click the active tab (tab 2) to deselect it.
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 2),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_CTRL_ON);

        // The new active tab is the leftmost tab (tab 0) and tab 2 is deselected.
        verify(mModel).setIndex(eq(0), anyInt());
        assertEquals("Final selection should have 2 tabs", 2, mModel.getMultiSelectedTabsCount());
        assertTrue("Tab 0 should be selected.", mModel.isTabMultiSelected(tabs[0].getTabId()));
        assertTrue("Tab 4 should be selected.", mModel.isTabMultiSelected(tabs[4].getTabId()));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
    public void testMultiSelect_CtrlClick_ResetsAnchorTab() {
        initializeTest(false, false, 1, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutView[] stripViews = mStripLayoutHelper.getStripLayoutViewsForTesting();

        // Shift+Click Tab 3 to establish an anchor tab (which will be Tab 1).
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 3),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_SHIFT_ON);

        // Anchor should be tab 1.
        assertEquals(
                "Anchor tab should be set after Shift+Click.",
                tabs[1].getTabId(),
                mStripLayoutHelper.getAnchorTabIdForTesting());

        // Ctrl+Click any other tab (Tab 0).
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 0),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_CTRL_ON);

        // The anchor tab should now be reset.
        assertEquals(
                "Anchor tab should be reset after a Ctrl+Click.",
                Tab.INVALID_TAB_ID,
                mStripLayoutHelper.getAnchorTabIdForTesting());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING})
    public void testTabContextMenu_MultipleTabsSelected() {
        // Setup
        initializeTest(false, false, 0, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        mStripLayoutHelper.setTabContextMenuCoordinatorForTesting(mTabContextMenuCoordinator);
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        StripLayoutView[] stripViews = mStripLayoutHelper.getStripLayoutViewsForTesting();

        // Ctrl+Click tab 1 and 3 to multi-select them along with the current tab (0).
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 1),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_CTRL_ON);
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 3),
                0,
                MotionEvent.BUTTON_PRIMARY,
                KeyEvent.META_CTRL_ON);

        // Right-click on one of the selected tabs to open the context menu.
        mStripLayoutHelper.click(
                TIMESTAMP,
                getClickCoordinateForTabAtIndex(stripViews, 1),
                0,
                MotionEvent.BUTTON_SECONDARY,
                0);

        // Verify
        List<Integer> expectedTabIds =
                List.of(tabs[0].getTabId(), tabs[1].getTabId(), tabs[3].getTabId());
        verify(mTabContextMenuCoordinator)
                .showMenu(
                        any(),
                        argThat(anchorInfo -> anchorInfo.getAllTabIds().equals(expectedTabIds)));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING})
    public void testTabContextMenu_MultipleTabsSelected_WithGroup() {
        // Setup
        initializeTest(false, false, 0, 5);
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        StripLayoutView[] stripViews = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutTab tab = (StripLayoutTab) stripViews[4];
        tab.setKeyboardFocused(true);
        // Action: Multiselect the keyboard focused tab.
        mStripLayoutHelper.multiselectKeyboardFocusedItem();
        // Verify
        assertTrue(mModel.isTabMultiSelected(tab.getTabId()));
        // Action : Multiselect the keyboard focused tab again to deselect it.
        mStripLayoutHelper.multiselectKeyboardFocusedItem();
        // Verify
        assertFalse(mModel.isTabMultiSelected(tab.getTabId()));
    }

    // 1. Moving a tab to higher indices, leaving a tab group
    @Test
    public void testKeyboardShortcut_MoveTabToHigherIndex_LeavingGroup() {
        // Setup: 5 tabs, with tabs 1, 2, 3 in a group: 0, [1, 2, 3], 4
        initializeTest(false, false, 0, 5);
        groupTabs(1, 4, TAB_GROUP_ID_1);
        mStripLayoutHelper.onTabStateInitialized();

        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutTab tab = (StripLayoutTab) views[4]; // Tab with ID 3 (skip group indicator)
        tab.setKeyboardFocused(true);

        // Action: Move tab 3 "to the right".
        mStripLayoutHelper.moveSelectedStripView(false);

        // Verify: Tab 3 has left the group but is still at index 3.
        // Original model: 0, [1, 2, 3], 4
        // Expected model: 0, [1, 2], 3, 4
        verify(mTabUngrouper, times(1))
                .ungroupTabs(eq(List.of(mModel.getTabAt(3))), eq(true), eq(true), any());
    }

    // 2. Moving a tab to higher indices, leaving a tab group, when the tab is at the end
    @Test
    public void testKeyboardShortcut_MoveTabToHigherIndex_LeavingGroup_AtEnd() {
        // Setup: 5 tabs, with tabs 2, 3, 4 in a group: 0, 1, [2, 3, 4]
        initializeTest(false, false, 0, 5);
        groupTabs(2, 5, TAB_GROUP_ID_1);
        mStripLayoutHelper.onTabStateInitialized();

        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutTab tab = (StripLayoutTab) views[5]; // Tab with ID 4 (skip group indicator)
        tab.setKeyboardFocused(true);

        // Action: Move tab 4 "to the right".
        mStripLayoutHelper.moveSelectedStripView(false);

        // Verify: Tab 3 has left the group but is still at index 3.
        // Original model: 0, 1, [2, 3, 4]
        // Expected model: 0, 1, [2, 3], 4
        verify(mTabUngrouper, times(1))
                .ungroupTabs(eq(List.of(mModel.getTabAt(4))), eq(true), eq(true), any());
        verify(mTabGroupModelFilter, never())
                .mergeListOfTabsToGroup(anyList(), any(), anyInt(), anyInt());
        verify(mModel, never()).moveTab(anyInt(), anyInt());
    }

    // 3. Moving a tab to lower indices, joining a tab group
    @Test
    public void testKeyboardShortcut_MoveTabToLowerIndex_JoiningGroup() {
        // Setup: 5 tabs, with tabs 2, 3 in a group: 0, 1, [2, 3], 4
        initializeTest(false, false, 0, 5);
        groupTabs(2, 4, TAB_GROUP_ID_1);
        mStripLayoutHelper.onTabStateInitialized();

        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutTab tab = (StripLayoutTab) views[5]; // Tab with ID 4 (skip group indicator)
        tab.setKeyboardFocused(true);

        // Action: Move tab 4 to the left
        mStripLayoutHelper.moveSelectedStripView(true);

        // Verify: Tab 4 has joined the group.
        // Original model: 0, 1, [2, 3], 4
        // Expected model: 0, 1, [2, 3, 4]
        Tab expectedDestinationTab = mModel.getTabById(3);
        verify(mTabGroupModelFilter, times(1))
                .mergeListOfTabsToGroup(
                        mTabListCaptor.capture(),
                        eq(expectedDestinationTab),
                        eq(null),
                        eq(MergeNotificationType.DONT_NOTIFY));
        assertTrue(mTabListCaptor.getValue().contains(mModel.getTabById(4)));
        verify(mTabUngrouper, never()).ungroupTabs(any(), anyBoolean(), anyBoolean(), any());
        verify(mModel, never()).moveTab(anyInt(), anyInt());
    }

    // 4. Moving a tab to higher indices, joining a tab group
    @Test
    public void testKeyboardShortcut_MoveTabToHigherIndex_JoiningGroup() {
        // Setup: 5 tabs, with tabs 2, 3 in a group: 0, 1, [2, 3], 4
        initializeTest(false, false, 0, 5);
        groupTabs(2, 4, TAB_GROUP_ID_1);
        mStripLayoutHelper.onTabStateInitialized();

        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutTab tab = (StripLayoutTab) views[1]; // Tab with ID 1
        tab.setKeyboardFocused(true);

        // Action: Move tab 1 to the right, in which case it will cross the group indicator.
        mStripLayoutHelper.moveSelectedStripView(false);

        // Verify: Tab 1 has joined the group.
        // Original model: 0, 1, [2, 3], 4
        // Expected model: 0, [1, 2, 3], 4
        Tab expectedDestinationTab = mModel.getTabById(2);
        verify(mTabGroupModelFilter, times(1))
                .mergeListOfTabsToGroup(
                        mTabListCaptor.capture(),
                        eq(expectedDestinationTab),
                        eq(0),
                        eq(MergeNotificationType.DONT_NOTIFY));
        assertTrue(mTabListCaptor.getValue().contains(mModel.getTabById(1)));
        verify(mTabUngrouper, never()).ungroupTabs(any(), anyBoolean(), anyBoolean(), any());
        verify(mModel, never()).moveTab(anyInt(), anyInt());
    }

    // 5. Moving a tab to lower indices, leaving a tab group, when the tab is at the start
    @Test
    public void testKeyboardShortcut_MoveTabToLowerIndex_LeavingGroup_AtStart() {
        // Setup: 5 tabs, with tabs 0, 1, 2 in a group: [0, 1, 2], 3, 4
        initializeTest(false, false, 0, 5);
        groupTabs(0, 3, TAB_GROUP_ID_1);
        mStripLayoutHelper.onTabStateInitialized();

        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutTab tab = (StripLayoutTab) views[1]; // Tab with ID 0 (skip group indicator)
        tab.setKeyboardFocused(true);

        // Action: Move tab 1 to the left, in which case it will cross the group indicator
        mStripLayoutHelper.moveSelectedStripView(true);

        // Verify: Tab 1 has left the group and is now at index 0.
        verify(mTabUngrouper, times(1))
                .ungroupTabs(eq(List.of(mModel.getTabAt(0))), eq(false), eq(true), any());
        verify(mTabGroupModelFilter, never())
                .mergeListOfTabsToGroup(anyList(), any(), anyInt(), anyInt());
        verify(mModel, never()).moveTab(anyInt(), anyInt());
    }

    // 6. Moving a tab within a tab group
    @Test
    public void testKeyboardShortcut_MoveTab_WithinGroup() {
        // Setup: 5 tabs, with tabs 0, 1, 2 in a group: [0, 1, 2], 3, 4
        initializeTest(false, false, 0, 5);
        groupTabs(0, 3, TAB_GROUP_ID_1);
        mStripLayoutHelper.onTabStateInitialized();

        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutTab tab = (StripLayoutTab) views[1]; // Tab with ID 0
        tab.setKeyboardFocused(true);

        // Action: Drag tab 0 to the right, where it remains in the group.
        mStripLayoutHelper.moveSelectedStripView(false);

        // Verify: The order within the group has changed.
        verify(mModel, times(1)).moveTab(0, 1);
        verify(mTabGroupModelFilter, never())
                .mergeListOfTabsToGroup(anyList(), any(), anyInt(), anyInt());
        verify(mTabUngrouper, never()).ungroupTabs(any(), anyBoolean(), anyBoolean(), any());
    }

    // 7. Moving a tab but it is not interacting with tab groups
    @Test
    public void testKeyboardShortcut_MoveTab_NoGroupInteraction() {
        // Setup: 5 tabs, all ungrouped.
        initializeTest(false, false, 0, 5);
        mStripLayoutHelper.onTabStateInitialized();

        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutTab tab = (StripLayoutTab) views[1]; // Tab with ID 1
        tab.setKeyboardFocused(true);

        // Action: Move tab 1 to the right.
        mStripLayoutHelper.moveSelectedStripView(false);

        // Verify: The tab order has changed.
        // Original model: 0, 1, 2, 3, 4
        // Expected model: 0, 2, 1, 3, 4
        verify(mModel, times(1)).moveTab(1, 2);
        verify(mTabGroupModelFilter, never())
                .mergeListOfTabsToGroup(anyList(), any(), anyInt(), anyInt());
        verify(mTabUngrouper, never()).ungroupTabs(any(), anyBoolean(), anyBoolean(), any());
    }

    // 8. Moving a tab to the right, past a collapsed tab group
    @Test
    public void testKeyboardShortcut_MoveTab_ToTheRightOfCollapsedGroup() {
        // Setup: 5 tabs, with tabs 1, 2 in a group: 0, [1, 2], 3, 4
        initializeTest(false, false, 0, 5);
        groupTabs(1, 3, TAB_GROUP_ID_1);
        mStripLayoutHelper.onTabStateInitialized();

        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutTab tab = (StripLayoutTab) views[0]; // Tab with ID 0
        tab.setKeyboardFocused(true);
        StripLayoutGroupTitle groupTitle = (StripLayoutGroupTitle) views[1]; // Tab group indicator
        mStripLayoutHelper.collapseTabGroupForTesting(groupTitle, true);

        // Action: Move tab 0 to the right.
        mStripLayoutHelper.moveSelectedStripView(false);

        // Verify: The tab order has changed.
        verify(mModel, times(1)).moveTab(0, 2);
        verify(mTabGroupModelFilter, never())
                .mergeListOfTabsToGroup(anyList(), any(), anyInt(), anyInt());
        verify(mTabUngrouper, never()).ungroupTabs(any(), anyBoolean(), anyBoolean(), any());
    }

    // 9. Moving a tab to the left, past a collapsed tab group
    @Test
    public void testKeyboardShortcut_MoveTab_ToTheLeftOfCollapsedGroup() {
        // Setup: 5 tabs, with tabs 1, 2 in a group: 0, [1, 2], 3, 4
        initializeTest(false, false, 0, 5);
        groupTabs(1, 3, TAB_GROUP_ID_1);
        mStripLayoutHelper.onTabStateInitialized();

        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutTab tab = (StripLayoutTab) views[4]; // Tab with ID 3 (skip group indicator)
        tab.setKeyboardFocused(true);
        StripLayoutGroupTitle groupTitle = (StripLayoutGroupTitle) views[1]; // Tab group indicator
        mStripLayoutHelper.collapseTabGroupForTesting(groupTitle, true);

        // Action: Move tab 3 to the left.
        mStripLayoutHelper.moveSelectedStripView(true);

        // Verify: The tab order has changed.
        verify(mModel, times(1)).moveTab(3, 1);
        verify(mTabGroupModelFilter, never())
                .mergeListOfTabsToGroup(anyList(), any(), anyInt(), anyInt());
        verify(mTabUngrouper, never()).ungroupTabs(any(), anyBoolean(), anyBoolean(), any());
    }

    // 10. Moving a tab group to higher indices, past one tab
    @Test
    public void testKeyboardShortcut_MoveTabGroupToHigherIndex_PastOneTab() {
        // Setup: 5 tabs, with tabs 1, 2 in a group: 0, [1, 2], 3, 4
        initializeTest(false, false, 0, 5);
        groupTabs(1, 3, TAB_GROUP_ID_1);
        mStripLayoutHelper.onTabStateInitialized();

        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutGroupTitle group = (StripLayoutGroupTitle) views[1]; // Group 1
        group.setKeyboardFocused(true);
        int lastShownTabId = ((StripLayoutTab) views[2]).getTabId();
        when(mTabGroupModelFilter.getGroupLastShownTabId(group.getTabGroupId()))
                .thenReturn(lastShownTabId);

        // Action: Move Group 1 to the right, past tab 3.
        mStripLayoutHelper.moveSelectedStripView(false);

        // Verify: The group has moved.
        verify(mTabGroupModelFilter, times(1)).moveRelatedTabs(lastShownTabId, 3);
        verify(mTabUngrouper, never()).ungroupTabs(any(), anyBoolean(), anyBoolean(), any());
    }

    // 11. Moving a tab group to higher indices, past a tab group
    @Test
    public void testKeyboardShortcut_MoveTabGroupToHigherIndex_PastGroup() {
        // Setup: 4 tabs, Group A (0, 1), Group B (2, 3): [0, 1], [2, 3]
        initializeTest(false, false, 0, 4);
        groupTabs(0, 2, TAB_GROUP_ID_1);
        groupTabs(2, 4, TAB_GROUP_ID_2);
        mStripLayoutHelper.onTabStateInitialized();

        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutGroupTitle group = (StripLayoutGroupTitle) views[0]; // Group A
        group.setKeyboardFocused(true);
        int lastShownTabId = ((StripLayoutTab) views[1]).getTabId();
        when(mTabGroupModelFilter.getGroupLastShownTabId(group.getTabGroupId()))
                .thenReturn(lastShownTabId);

        // Action: Move Group A to the right, past Group B.
        mStripLayoutHelper.moveSelectedStripView(false);

        // Verify: The groups have swapped positions.
        verify(mTabGroupModelFilter, times(1)).moveRelatedTabs(lastShownTabId, 3);
        verify(mTabUngrouper, never()).ungroupTabs(any(), anyBoolean(), anyBoolean(), any());
    }

    // 12. Moving a tab group to lower indices, past one tab
    @Test
    public void testKeyboardShortcut_MoveTabGroupToLowerIndex_PastOneTab() {
        // Setup: 5 tabs, with tabs 2, 3 in a group: 0, 1, [2, 3], 4
        initializeTest(false, false, 0, 5);
        groupTabs(2, 4, TAB_GROUP_ID_1);
        mStripLayoutHelper.onTabStateInitialized();

        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutGroupTitle group = (StripLayoutGroupTitle) views[2]; // Group 1
        group.setKeyboardFocused(true);
        int lastShownTabId = ((StripLayoutTab) views[3]).getTabId();
        when(mTabGroupModelFilter.getGroupLastShownTabId(group.getTabGroupId()))
                .thenReturn(lastShownTabId);

        // Action: Move Group 1 to the left, past tab 1.
        mStripLayoutHelper.moveSelectedStripView(true);

        // Verify: The group has moved.
        verify(mTabGroupModelFilter, times(1)).moveRelatedTabs(lastShownTabId, 1);
        verify(mTabUngrouper, never()).ungroupTabs(any(), anyBoolean(), anyBoolean(), any());
    }

    // 13. Moving a tab group to lower indices, past a tab group
    @Test
    public void testKeyboardShortcut_MoveTabGroupToLowerIndex_PastGroup() {
        // Setup: 4 tabs, Group A (0, 1), Group B (2, 3): [0, 1], [2, 3]
        initializeTest(false, false, 0, 4);
        groupTabs(0, 2, TAB_GROUP_ID_1);
        groupTabs(2, 4, TAB_GROUP_ID_2);
        mStripLayoutHelper.onTabStateInitialized();

        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutGroupTitle group = (StripLayoutGroupTitle) views[3]; // Group B
        group.setKeyboardFocused(true);
        int lastShownTabId = ((StripLayoutTab) views[4]).getTabId();
        when(mTabGroupModelFilter.getGroupLastShownTabId(group.getTabGroupId()))
                .thenReturn(lastShownTabId);

        // Action: Move Group B to the left, past Group A.
        mStripLayoutHelper.moveSelectedStripView(true);

        // Verify: The groups have swapped positions.
        verify(mTabGroupModelFilter, times(1)).moveRelatedTabs(lastShownTabId, 0);
        verify(mTabUngrouper, never()).ungroupTabs(any(), anyBoolean(), anyBoolean(), any());
    }

    // 14. Moving a tab group to higher indices, past a collapsed tab group
    @Test
    public void testKeyboardShortcut_MoveTabGroupToHigherIndex_PastCollapsedGroup() {
        // Setup: 4 tabs, Group A (0, 1), Group B (2, 3): [0, 1], [2, 3]
        initializeTest(false, false, 0, 4);
        groupTabs(0, 2, TAB_GROUP_ID_1);
        groupTabs(2, 4, TAB_GROUP_ID_2);
        mStripLayoutHelper.onTabStateInitialized();

        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutGroupTitle group = (StripLayoutGroupTitle) views[0]; // Group A
        mStripLayoutHelper.collapseTabGroupForTesting((StripLayoutGroupTitle) views[3], true);
        group.setKeyboardFocused(true);
        int lastShownTabId = ((StripLayoutTab) views[1]).getTabId();
        when(mTabGroupModelFilter.getGroupLastShownTabId(group.getTabGroupId()))
                .thenReturn(lastShownTabId);

        // Action: Move Group A to the right, past Group B.
        mStripLayoutHelper.moveSelectedStripView(false);

        // Verify: The groups have swapped positions.
        verify(mTabGroupModelFilter, times(1)).moveRelatedTabs(lastShownTabId, 3);
        verify(mTabUngrouper, never()).ungroupTabs(any(), anyBoolean(), anyBoolean(), any());
    }

    // 15. Moving a tab group to lower indices, past a collapsed tab group
    @Test
    public void testKeyboardShortcut_MoveTabGroupToLowerIndex_PastCollapsedGroup() {
        // Setup: 4 tabs, Group A (0, 1), Group B (2, 3): [0, 1], [2, 3]
        initializeTest(false, false, 0, 4);
        groupTabs(0, 2, TAB_GROUP_ID_1);
        groupTabs(2, 4, TAB_GROUP_ID_2);
        mStripLayoutHelper.onTabStateInitialized();

        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        StripLayoutGroupTitle group = (StripLayoutGroupTitle) views[3]; // Group B
        group.setKeyboardFocused(true);
        mStripLayoutHelper.collapseTabGroupForTesting((StripLayoutGroupTitle) views[0], true);
        int lastShownTabId = ((StripLayoutTab) views[4]).getTabId();
        when(mTabGroupModelFilter.getGroupLastShownTabId(group.getTabGroupId()))
                .thenReturn(lastShownTabId);

        // Action: Move Group B to the left, past Group A.
        mStripLayoutHelper.moveSelectedStripView(true);

        // Verify: The groups have swapped positions.
        verify(mTabGroupModelFilter, times(1)).moveRelatedTabs(lastShownTabId, 0);
        verify(mTabUngrouper, never()).ungroupTabs(any(), anyBoolean(), anyBoolean(), any());
    }

    @Test
    @Feature("Pinned Tabs")
    @EnableFeatures({ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP})
    public void testTabsDrawXAndWidth_PinnedTabs() {
        final int numTabs = 5;
        initializeTest(false, false, 0, numTabs);

        // Trigger a size change so the strip layout tab heights and widths get set.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_LEFT, PADDING_RIGHT, 0f);

        // Set the initial scroll offset to trigger an update to draw X positions.
        mStripLayoutHelper.setScrollOffsetForTesting(0);

        // Setup tab model and pin first tab.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        MockTabModel tabModel = new MockTabModel(mProfile, null);
        tabModel.addTab(0);
        tabModel.addTab(1);
        tabModel.addTab(2);
        tabModel.addTab(3);
        tabModel.addTab(4);
        tabModel.setIndex(0, TabSelectionType.FROM_NEW);
        tabModel.setActive(true);
        tabModel.getTabAt(0).setIsPinned(true);
        tabs[0].setIsPinned(true);

        // Trigger an update to re-compute tabs widths.
        mStripLayoutHelper.setTabModel(tabModel, mTabCreator, true);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        float expectedDrawXWithPinnedTab = PADDING_LEFT;

        // 191.5(tabWidth) = (800(screenWidth) - 10(leftPadding) - 60(rightPadding) -
        // 48(pinnedTabWidth) + (28(overlapWidth) * 3) / 4(numTab).
        float expectedTabWidthWithPinnedTab = 191.5f;

        // Verify the tabs are resized and positioned correctly after pinning.
        for (int i = 0; i < tabs.length; i++) {
            assertEquals(
                    "The tab's drawX is incorrect",
                    expectedDrawXWithPinnedTab,
                    tabs[i].getDrawX(),
                    0.1f);
            if (i == 0) {
                assertTrue("The tab should be pinned", tabs[i].getIsPinned());
                assertEquals(
                        "The pinned tab's width is incorrect",
                        PINNED_TAB_WIDTH_DP,
                        tabs[i].getWidth(),
                        0.1f);
                assertFalse(
                        "The pinned tab's close button should hide", tabs[i].canShowCloseButton());
            } else {
                assertFalse("The tab should not be pinned", tabs[i].getIsPinned());
                assertEquals(
                        "The tab's width is incorrect",
                        expectedTabWidthWithPinnedTab,
                        tabs[i].getWidth(),
                        0.1f);
                assertTrue(
                        "The unpinned tab's close button should show",
                        tabs[i].canShowCloseButton());
            }
            expectedDrawXWithPinnedTab += tabs[i].getWidth() - TAB_OVERLAP_WIDTH_DP;
        }

        // Unpin first tab and trigger an update.
        tabModel.getTabAt(0).setIsPinned(false);
        tabs[0].setIsPinned(false);
        mStripLayoutHelper.setStripLayoutTabsForTesting(new StripLayoutTab[0]);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        float expectedDrawXNoPinnedTab = PADDING_LEFT;
        // 168.4(tabWidth) = (800(screenWidth) - 10(leftPadding) - 60(rightPadding) +
        // 28(overlapWidth) * 4) / 5(numTab).
        float expectedWidthNoPinnedTab = 168.4f;

        // Verify the tabs are resized and positioned correctly after unpinning.
        tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        expectedDrawXNoPinnedTab = PADDING_LEFT;
        for (StripLayoutTab tab : tabs) {
            assertEquals(
                    "The tab's drawX is incorrect", expectedDrawXNoPinnedTab, tab.getDrawX(), 0.1f);
            assertEquals(
                    "The tab's width is incorrect", expectedWidthNoPinnedTab, tab.getWidth(), 0.1f);
            expectedDrawXNoPinnedTab += tab.getWidth() - TAB_OVERLAP_WIDTH_DP;
        }
    }

    @Test
    @Feature("Pinned Tabs")
    @EnableFeatures({ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP})
    public void testTabsDrawXAndWidth_PinnedTabs_Rtl() {
        LocalizationUtils.setRtlForTesting(true);
        final int numTabs = 5;
        initializeTest(true, false, 0, numTabs);

        // Trigger a size change so the strip layout tab heights and widths get set.
        mStripLayoutHelper.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, false, TIMESTAMP, PADDING_RIGHT, PADDING_LEFT, 0f);

        // Set the initial scroll offset to trigger an update to draw X positions.
        mStripLayoutHelper.setScrollOffsetForTesting(0);

        // Setup tab model and pin first tab.
        StripLayoutTab[] tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        MockTabModel tabModel = new MockTabModel(mProfile, null);
        tabModel.addTab(0);
        tabModel.addTab(1);
        tabModel.addTab(2);
        tabModel.addTab(3);
        tabModel.addTab(4);
        tabModel.setIndex(0, TabSelectionType.FROM_NEW);
        tabModel.setActive(true);
        tabModel.getTabAt(0).setIsPinned(true);
        tabs[0].setIsPinned(true);

        // Trigger an update to re-compute tabs widths.
        mStripLayoutHelper.setTabModel(tabModel, mTabCreator, true);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        float expectedDrawXWithPinnedTab = SCREEN_WIDTH - PADDING_LEFT - TAB_OVERLAP_WIDTH_DP;

        // 191.5(tabWidth) = (800(screenWidth) - 10(leftPadding) - 60(rightPadding) -
        // 48(pinnedTabWidth) + (28(overlapWidth) * 3) / 4(numTab).
        float expectedTabWidthWithPinnedTab = 191.5f;

        // Verify the tabs are resized and positioned correctly after pinning.
        for (int i = 0; i < tabs.length; i++) {
            if (i == 0) {
                assertTrue("The tab should be pinned", tabs[i].getIsPinned());
                assertEquals(
                        "The pinned tab's width is incorrect",
                        PINNED_TAB_WIDTH_DP,
                        tabs[i].getWidth(),
                        0.1f);
                assertFalse(
                        "The pinned tab's close button should hide", tabs[i].canShowCloseButton());
            } else {
                assertFalse("The tab should not be pinned", tabs[i].getIsPinned());
                assertEquals(
                        "The tab's width is incorrect",
                        expectedTabWidthWithPinnedTab,
                        tabs[i].getWidth(),
                        0.1f);
                assertTrue(
                        "The unpinned tab's close button should show",
                        tabs[i].canShowCloseButton());
            }
            expectedDrawXWithPinnedTab -= (tabs[i].getWidth() - TAB_OVERLAP_WIDTH_DP);
            assertEquals(
                    "The tab's drawX is incorrect",
                    expectedDrawXWithPinnedTab,
                    tabs[i].getDrawX(),
                    0.1f);
        }

        // Unpin first tab and trigger an update.
        tabModel.getTabAt(0).setIsPinned(false);
        tabs[0].setIsPinned(false);
        mStripLayoutHelper.setStripLayoutTabsForTesting(new StripLayoutTab[0]);
        mStripLayoutHelper.updateLayout(TIMESTAMP);

        float expectedDrawXNoPinnedTab = SCREEN_WIDTH - PADDING_LEFT - TAB_OVERLAP_WIDTH_DP;
        // 168.4(tabWidth) = (800(screenWidth) - 60(leftPadding) - 10(rightPadding) +
        // 28(overlapWidth) * 4) / 5(numTab).
        float expectedWidthNoPinnedTab = 168.4f;

        // Verify the tabs are resized and positioned correctly after unpinning.
        tabs = mStripLayoutHelper.getStripLayoutTabsForTesting();
        expectedDrawXNoPinnedTab = SCREEN_WIDTH - PADDING_LEFT - TAB_OVERLAP_WIDTH_DP;
        for (StripLayoutTab tab : tabs) {
            assertEquals(
                    "The tab's width is incorrect", expectedWidthNoPinnedTab, tab.getWidth(), 0.1f);
            expectedDrawXNoPinnedTab -= (tab.getWidth() - TAB_OVERLAP_WIDTH_DP);
            assertEquals(
                    "The tab's drawX is incorrect", expectedDrawXNoPinnedTab, tab.getDrawX(), 0.1f);
        }
    }

    private float getClickCoordinateForTabAtIndex(StripLayoutView[] stripViews, int i) {
        return stripViews[i].getTouchTargetBounds().left
                + (stripViews[i].getTouchTargetBounds().right
                        - stripViews[i].getTouchTargetBounds().left);
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
        TabDragHandlerBase.setDragTrackerTokenForTesting(dragTrackerToken);
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
            throw new AssertionError("Not reached.");
        }
    }
}
