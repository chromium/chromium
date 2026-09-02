// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.CallbackUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator.AvatarProvider;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator.FaviconProvider;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager.ScrimClient;
import org.chromium.components.collaboration.messaging.ActivityLogItem;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.MessageAttribution;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.TabMessageMetadata;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Render tests for the Recent Activity bottom sheet. */
@DoNotBatch(reason = "Night mode requires clean activity launch.")
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class RecentActivityBottomSheetRenderTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false).name("Default"),
                    new ParameterSet().value(true).name("NightMode"));

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(Component.UI_BROWSER_MOBILE)
                    .build();

    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mBottomSheetTestSupport;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private FaviconProvider mFaviconProvider;
    @Mock private AvatarProvider mAvatarProvider;
    @Mock private RecentActivityActionHandler mRecentActivityActionHandler;

    public RecentActivityBottomSheetRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController =
                            createBottomSheetController(mActivityTestRule.getActivity());
                    mBottomSheetTestSupport = new BottomSheetTestSupport(mBottomSheetController);
                });
    }

    @After
    public void tearDown() {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    private BottomSheetController createBottomSheetController(Activity activity) {
        ViewGroup contentView = activity.findViewById(android.R.id.content);
        ScrimManager scrimManager = new ScrimManager(activity, contentView, ScrimClient.NONE);
        InsetObserver insetObserver =
                new InsetObserver(
                        new ImmutableWeakReference<>(activity.getWindow().getDecorView()),
                        new ImmutableWeakReference<>(activity.getApplicationContext()),
                        /* enableKeyboardOverlayMode= */ false,
                        /* enableExtraEdgeToEdgeLogging= */ false);
        return BottomSheetControllerFactory.createBottomSheetController(
                () -> scrimManager,
                activity.getWindow(),
                KeyboardVisibilityDelegate.getInstance(),
                () -> contentView,
                () -> 0,
                /* desktopWindowStateManager= */ null,
                insetObserver,
                /* enableLargeFormFactorUi= */ false);
    }

    private ActivityLogItem createLog(String user, String title, String desc, String time) {
        ActivityLogItem logItem = new ActivityLogItem();
        logItem.collaborationEvent = CollaborationEvent.TAB_ADDED;
        logItem.titleText = title;
        logItem.descriptionText = desc;
        logItem.timeDeltaText = time;
        GroupMember triggeringUser = new GroupMember(null, user, null, MemberRole.MEMBER, null, "");
        logItem.activityMetadata = new MessageAttribution();
        logItem.activityMetadata.triggeringUser = triggeringUser;
        logItem.activityMetadata.tabMetadata = new TabMessageMetadata();
        logItem.activityMetadata.tabMetadata.lastKnownUrl = "https://google.com";
        logItem.activityMetadata.tabMetadata.localTabId = 1;
        return logItem;
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRecentActivityBottomSheet() throws IOException {
        List<ActivityLogItem> logItems = new ArrayList<>();
        logItems.add(createLog("Alice", "Alice added a tab", "chromium.org", "Just now"));
        logItems.add(createLog("Bob", "Bob changed a tab", "google.com", "15m ago"));
        when(mMessagingBackendService.getActivityLog(any())).thenReturn(logItems);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecentActivityListCoordinator coordinator =
                            new RecentActivityListCoordinator(
                                    "test_collab",
                                    mActivityTestRule.getActivity(),
                                    mBottomSheetController,
                                    mMessagingBackendService,
                                    mTabGroupSyncService,
                                    mFaviconProvider,
                                    mAvatarProvider,
                                    mRecentActivityActionHandler,
                                    CallbackUtils.emptyRunnable());
                    coordinator.requestShowUI();
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetContainer =
                mActivityTestRule
                        .getActivity()
                        .findViewById(
                                org.chromium.components.browser_ui.bottomsheet.R.id.bottom_sheet);
        CriteriaHelper.pollUiThread(bottomSheetContainer::isLaidOut);

        mRenderTestRule.render(bottomSheetContainer, "recent_activity_bottom_sheet");
    }
}
