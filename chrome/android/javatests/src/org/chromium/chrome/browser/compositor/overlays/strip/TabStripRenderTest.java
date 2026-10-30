// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import android.graphics.Bitmap;
import android.graphics.Color;

import androidx.annotation.Nullable;
import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.TabStripUtils;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.SharedGroupTestHelper;
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.test.util.RenderTestRule.Component;

/**
 * Render tests for the Tab Strip.
 *
 * <p>TODO(crbug.com/574104536): Add render tests for other tab strip behaviors.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@DisableFeatures({ChromeFeatureList.USE_WEB_UI_NTP_ANDROID})
public class TabStripRenderTest {
    private static final String RENDER_TEST_ID_TWO_MEMBERS = "shared_tab_group_avatar";
    private static final String RENDER_TEST_ID_SINGLE_MEMBER =
            "shared_tab_group_avatar_single_member";
    private static final String RENDER_TEST_ID_TRAILING_COUNT =
            "shared_tab_group_avatar_trailing_count";
    private static final String RENDER_TEST_ID_MEMBER_JOINED =
            "shared_tab_group_avatar_member_joined";
    private static final String RENDER_TEST_ID_GROUP_UNSHARED =
            "shared_tab_group_avatar_group_unshared";
    private static final String RENDER_TEST_ID_AVATAR_CHANGED =
            "shared_tab_group_avatar_color_changed";
    private static final String RENDER_TEST_ID_WITH_NOTIFICATION_BUBBLE =
            "shared_tab_group_avatar_notification_bubble";
    private static final String RENDER_TEST_ID_RTL = "shared_tab_group_avatar_rtl";

    private static final int AVATAR_BITMAP_SIZE_PX = 50;

    private static final GroupMember GROUP_MEMBER1 = SharedGroupTestHelper.GROUP_MEMBER1;
    private static final GroupMember GROUP_MEMBER2 = SharedGroupTestHelper.GROUP_MEMBER2;
    private static final GroupMember GROUP_MEMBER3 =
            createGroupMember("gaiaId3", "Member3", "three@gmail.com");
    private static final GroupMember GROUP_MEMBER4 =
            createGroupMember("gaiaId4", "Member4", "four@gmail.com");
    private static final GroupMember GROUP_MEMBER5 =
            createGroupMember("gaiaId5", "Member5", "five@gmail.com");

    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(
                            Component.UI_BROWSER_CHROME_TABS_CHROME_TAB_GROUPS_SHARED_TAB_GROUPS)
                    .setRevision(2)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DataSharingService mDataSharingService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ServiceStatus mServiceStatus;
    @Mock private DataSharingUIDelegate mDataSharingUiDelegate;

    private SharedGroupTestHelper mSharedGroupTestHelper;
    private int mAvatarColor = Color.RED;

    @Before
    public void setUp() {
        CompositorAnimationHandler.setTestingMode(true);
        mAvatarColor = Color.RED;
        DataSharingServiceFactory.setForTesting(mDataSharingService);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        doReturn(mServiceStatus).when(mCollaborationService).getServiceStatus();
        doReturn(true).when(mServiceStatus).isAllowedToJoin();
        doReturn(mDataSharingUiDelegate).when(mDataSharingService).getUiDelegate();

        mSharedGroupTestHelper = new SharedGroupTestHelper(mCollaborationService);

        doAnswer(
                        invocation -> {
                            DataSharingAvatarBitmapConfig config = invocation.getArgument(0);
                            Bitmap avatarBitmap =
                                    Bitmap.createBitmap(
                                            AVATAR_BITMAP_SIZE_PX,
                                            AVATAR_BITMAP_SIZE_PX,
                                            Bitmap.Config.ARGB_8888);
                            avatarBitmap.eraseColor(mAvatarColor);
                            config.getDataSharingAvatarCallback().onAvatarLoaded(avatarBitmap);
                            return null;
                        })
                .when(mDataSharingUiDelegate)
                .getAvatarBitmap(any());
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testSharedGroupAvatar() throws Exception {
        setupSharedGroup(GROUP_MEMBER1, GROUP_MEMBER2);
        mRenderTestRule.compareForResult(captureBitmap(), RENDER_TEST_ID_TWO_MEMBERS);
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testSharedGroupAvatar_singleMember() throws Exception {
        setupSharedGroup(GROUP_MEMBER1);
        mRenderTestRule.compareForResult(captureBitmap(), RENDER_TEST_ID_SINGLE_MEMBER);
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testSharedGroupAvatar_trailingCount() throws Exception {
        setupSharedGroup(GROUP_MEMBER1, GROUP_MEMBER2, GROUP_MEMBER3, GROUP_MEMBER4, GROUP_MEMBER5);
        mRenderTestRule.compareForResult(captureBitmap(), RENDER_TEST_ID_TRAILING_COUNT);
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testSharedGroupAvatar_memberJoined() throws Exception {
        // Start with 2 members.
        StripLayoutHelper stripLayoutHelper = setupSharedGroup(GROUP_MEMBER1, GROUP_MEMBER2);

        // 3 more members join dynamically (expanding the pill with a +3 count bubble).
        updateGroupMembers(
                stripLayoutHelper,
                GROUP_MEMBER1,
                GROUP_MEMBER2,
                GROUP_MEMBER3,
                GROUP_MEMBER4,
                GROUP_MEMBER5);
        mRenderTestRule.compareForResult(captureBitmap(), RENDER_TEST_ID_MEMBER_JOINED);
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testSharedGroupAvatar_groupUnshared() throws Exception {
        // Start with 2 members.
        StripLayoutHelper stripLayoutHelper = setupSharedGroup(GROUP_MEMBER1, GROUP_MEMBER2);

        // Unshare group: clear avatar and shared state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    StripLayoutGroupTitle groupTitle = getFirstGroupTitle(stripLayoutHelper);
                    groupTitle.clearSharedTabGroup();
                    stripLayoutHelper
                            .getLayerTitleCacheForTesting()
                            .removeSharedGroupAvatar(groupTitle.getTabGroupId());
                    stripLayoutHelper.updateGroupTextAndSharedState(groupTitle.getTabGroupId());
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    StripLayoutGroupTitle groupTitle = findFirstGroupTitle(stripLayoutHelper);
                    return groupTitle == null || groupTitle.getAvatarResourceForTesting() == null;
                });
        settleLayout(stripLayoutHelper);
        mRenderTestRule.compareForResult(captureBitmap(), RENDER_TEST_ID_GROUP_UNSHARED);
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testSharedGroupAvatar_avatarBitmapChanged() throws Exception {
        // Start with Red avatar.
        mAvatarColor = Color.RED;
        StripLayoutHelper stripLayoutHelper = setupSharedGroup(GROUP_MEMBER1, GROUP_MEMBER2);

        // Avatar bitmap changes to Blue.
        mAvatarColor = Color.BLUE;
        updateGroupMembers(stripLayoutHelper, GROUP_MEMBER1, GROUP_MEMBER2);
        mRenderTestRule.compareForResult(captureBitmap(), RENDER_TEST_ID_AVATAR_CHANGED);
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testSharedGroupAvatar_withNotificationBubble() throws Exception {
        StripLayoutHelper stripLayoutHelper = setupSharedGroup(GROUP_MEMBER1, GROUP_MEMBER2);

        // Show notification bubble on the group title.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    StripLayoutGroupTitle groupTitle = getFirstGroupTitle(stripLayoutHelper);
                    groupTitle.setNotificationBubbleShown(true);
                    stripLayoutHelper.updateGroupTextAndSharedState(groupTitle.getTabGroupId());
                });
        settleLayout(stripLayoutHelper);
        mRenderTestRule.compareForResult(captureBitmap(), RENDER_TEST_ID_WITH_NOTIFICATION_BUBBLE);
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testSharedGroupAvatar_rtl() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> LocalizationUtils.setRtlForTesting(true));
        StripLayoutHelper stripLayoutHelper = setupSharedGroup(GROUP_MEMBER1, GROUP_MEMBER2);

        // Show notification bubble to verify full mirroring: [ Bubble | Title | Avatars ].
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    StripLayoutGroupTitle groupTitle = getFirstGroupTitle(stripLayoutHelper);
                    groupTitle.setNotificationBubbleShown(true);
                    stripLayoutHelper.updateGroupTextAndSharedState(groupTitle.getTabGroupId());
                });
        settleLayout(stripLayoutHelper);
        mRenderTestRule.compareForResult(captureBitmap(), RENDER_TEST_ID_RTL);
    }

    private StripLayoutHelper setupSharedGroup(GroupMember... members) throws Exception {
        // Start on blank page and add two more tabs.
        CtaPageStation page = mCtaTestRule.startOnBlankPage();
        page = page.openNewTabFast();
        page = page.openNewTabFast();
        ChromeTabbedActivity activity = page.getActivity();

        StripLayoutHelper stripLayoutHelper =
                TabStripTestUtils.getActiveStripLayoutHelper(activity);
        settleLayout(stripLayoutHelper);

        // Group the first two tabs, leaving the third tab ungrouped.
        TabStripTestUtils.createTabGroup(activity, /* isIncognito= */ false, 0, 1);
        // Wait for views to be created.
        CriteriaHelper.pollUiThread(() -> findFirstGroupTitle(stripLayoutHelper) != null);
        settleLayout(stripLayoutHelper);

        updateGroupMembers(stripLayoutHelper, members);
        return stripLayoutHelper;
    }

    private void updateGroupMembers(StripLayoutHelper stripLayoutHelper, GroupMember... members) {
        // Setup shared state.
        mSharedGroupTestHelper.mockGetGroupData(SharedGroupTestHelper.COLLABORATION_ID1, members);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    StripLayoutGroupTitle groupTitle = getFirstGroupTitle(stripLayoutHelper);

                    groupTitle.updateSharedTabGroup(
                            SharedGroupTestHelper.COLLABORATION_ID1,
                            mDataSharingService,
                            mCollaborationService,
                            (avatarRes) -> {
                                stripLayoutHelper
                                        .getLayerTitleCacheForTesting()
                                        .registerSharedGroupAvatar(
                                                groupTitle.getTabGroupId(), avatarRes);
                            },
                            () -> {
                                stripLayoutHelper.updateGroupTextAndSharedState(
                                        groupTitle.getTabGroupId());
                            });
                });

        // Wait for a frame to render the avatar.
        CriteriaHelper.pollUiThread(
                () -> {
                    StripLayoutGroupTitle groupTitle = getFirstGroupTitle(stripLayoutHelper);
                    return groupTitle.getAvatarResourceForTesting() != null;
                });
        settleLayout(stripLayoutHelper);
    }

    private void settleLayout(StripLayoutHelper stripLayoutHelper) {
        TabStripUtils.settleDownCompositor(stripLayoutHelper);
        boolean hasGroupTitle =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            mCtaTestRule
                                    .getActivity()
                                    .getLayoutManager()
                                    .getStripLayoutHelperManager()
                                    .updateHelperEndMargins();
                            stripLayoutHelper.finishAnimations();
                            stripLayoutHelper.finishScrollForTesting();
                            stripLayoutHelper.updateLayout(0);
                            stripLayoutHelper.finishAnimations();
                            stripLayoutHelper.updateLayout(0);
                            return findFirstGroupTitle(stripLayoutHelper) != null;
                        });
        if (hasGroupTitle) {
            waitForFrameSwapped(stripLayoutHelper);
        }
    }

    private void waitForFrameSwapped(StripLayoutHelper stripLayoutHelper) {
        CallbackHelper onFrameSwapped = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    StripLayoutGroupTitle groupTitle = findFirstGroupTitle(stripLayoutHelper);
                    if (groupTitle != null) {
                        stripLayoutHelper.updateGroupTextAndSharedState(groupTitle.getTabGroupId());
                    }
                    mCtaTestRule
                            .getActivity()
                            .getLayoutManager()
                            .requestUpdate(onFrameSwapped::notifyCalled);
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    if (onFrameSwapped.getCallCount() > 0) return true;
                    StripLayoutGroupTitle groupTitle = findFirstGroupTitle(stripLayoutHelper);
                    if (groupTitle != null) {
                        stripLayoutHelper.updateGroupTextAndSharedState(groupTitle.getTabGroupId());
                    }
                    mCtaTestRule.getActivity().getLayoutManager().requestUpdate();
                    return false;
                });
    }

    private Bitmap captureBitmap() {
        StripLayoutHelper stripLayoutHelper =
                TabStripTestUtils.getActiveStripLayoutHelper(mCtaTestRule.getActivity());
        // Wait for two sequential compositor swaps so any in-flight pipelined frame drains in Viz
        // and SurfaceFlinger latches the final frame before UiAutomation captures the display.
        waitForFrameSwapped(stripLayoutHelper);
        waitForFrameSwapped(stripLayoutHelper);
        Bitmap fullScreen =
                InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot();
        Assert.assertNotNull("Screenshot from UiAutomation was null", fullScreen);
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    CompositorViewHolder compositorViewHolder =
                            mCtaTestRule.getActivity().findViewById(R.id.compositor_view_holder);
                    int[] location = new int[2];
                    compositorViewHolder.getLocationOnScreen(location);
                    int tabStripHeightPx =
                            mCtaTestRule
                                    .getActivity()
                                    .getToolbarManager()
                                    .getTopToolbarCoordinator()
                                    .getTabStripHeight();
                    Assert.assertTrue("Expected positive tab strip height", tabStripHeightPx > 0);
                    return Bitmap.createBitmap(
                            fullScreen,
                            location[0],
                            location[1],
                            compositorViewHolder.getWidth(),
                            tabStripHeightPx);
                });
    }

    private static GroupMember createGroupMember(String gaiaId, String displayName, String email) {
        return new GroupMember(
                new GaiaId(gaiaId),
                displayName,
                email,
                MemberRole.MEMBER,
                /* avatarUrl= */ null,
                displayName);
    }

    private static @Nullable StripLayoutGroupTitle findFirstGroupTitle(
            StripLayoutHelper stripLayoutHelper) {
        for (StripLayoutView view : stripLayoutHelper.getStripLayoutViewsForTesting()) {
            if (view instanceof StripLayoutGroupTitle groupTitle) {
                return groupTitle;
            }
        }
        return null;
    }

    private static StripLayoutGroupTitle getFirstGroupTitle(StripLayoutHelper stripLayoutHelper) {
        StripLayoutGroupTitle title = findFirstGroupTitle(stripLayoutHelper);
        Assert.assertNotNull("Expected at least one StripLayoutGroupTitle", title);
        return title;
    }
}
