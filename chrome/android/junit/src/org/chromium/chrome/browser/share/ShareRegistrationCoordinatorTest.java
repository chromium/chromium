// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareRegistrationCoordinator.ShareBroadcastReceiver;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridge;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridgeJni;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.JUnitTestGURLs;

/** Tests for ShareRegistrationCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ShareRegistrationCoordinatorTest {
    @Rule
    public TestRule mProcessorRule = new Features.JUnitProcessor();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    SendTabToSelfAndroidBridge.Natives mNativeMock;

    @Mock
    private Profile mProfile;
    @Mock
    private Tab mTab;
    @Mock
    private NavigationEntry mNavigationEntry;
    @Mock
    private BottomSheetController mBottomSheetController;
    @Mock
    private SyncService mSyncService;
    @Mock
    private Context mContext;

    @Captor
    private ArgumentCaptor<Intent> mIntentCaptor;

    @Spy
    private Activity mActivity;

    private Supplier<Tab> mCurrentTabSupplier;
    private ShareRegistrationCoordinator mShareRegistrationCoordinator;

    @Before
    public void setUp() {
        mocker.mock(SendTabToSelfAndroidBridgeJni.TEST_HOOKS, mNativeMock);
        Profile.setLastUsedProfileForTesting(mProfile);
        mActivity = Mockito.spy(Robolectric.buildActivity(Activity.class).create().get());

        mCurrentTabSupplier = () -> mTab;
        mShareRegistrationCoordinator = new ShareRegistrationCoordinator(
                mActivity, mCurrentTabSupplier, mBottomSheetController);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_WHEN_SIGNED_IN)
    @SmallTest
    public void doSendTabToSelfShare() {
        // Setup the mocked object chain to get to the url, title and timestamp.
        when(mNavigationEntry.getUrl())
                .thenReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL));

        // Setup the mocked object for sync settings.
        when(mSyncService.isSyncRequested()).thenReturn(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> SyncService.overrideForTests(mSyncService));

        mShareRegistrationCoordinator.doSendTabToSelfShare(
                mActivity, mNavigationEntry, mBottomSheetController);
        verify(mBottomSheetController).requestShowContent(any(BottomSheetContent.class), eq(true));
    }

    @Test
    @SmallTest
    public void sendShareBroadcastWithAction() {
        ShareBroadcastReceiver receiver = new ShareBroadcastReceiver(mActivity);
        Runnable runnable = Mockito.mock(Runnable.class);
        receiver.registerShareType("foobar", runnable);

        ShareBroadcastReceiver.sendShareBroadcastWithAction(mActivity.getTaskId(), "foobar");
        verify(runnable).run();

        receiver.destroy();
    }

    @Test
    @SmallTest
    public void sendShareBroadcastWithAction_NoRegisteredReceiver() {
        ShareBroadcastReceiver.sendShareBroadcastWithAction(mActivity.getTaskId(), "foobar");
        verify(mContext, times(0)).sendBroadcast(mIntentCaptor.capture());
    }

    @Test
    @SmallTest
    public void sendShareBroadcastWithAction_ReplaceShareTypeRunnable() {
        ShareBroadcastReceiver receiver = new ShareBroadcastReceiver(mActivity);
        Runnable runnable1 = Mockito.mock(Runnable.class);
        Runnable runnable2 = Mockito.mock(Runnable.class);
        receiver.registerShareType("foobar", runnable1);
        try {
            receiver.registerShareType("foobar", runnable2);
            Assert.assertTrue("Expected exception to be thrown", false);
        } catch (IllegalStateException e) {
        }
    }

    @Test
    @SmallTest
    public void sendShareBroadcastWithAction_RegisterShareTypeAfterDestruction() {
        ShareBroadcastReceiver receiver = new ShareBroadcastReceiver(mActivity);
        receiver.destroy();
        Runnable runnable = Mockito.mock(Runnable.class);
        receiver.registerShareType("foobar", runnable);

        ShareBroadcastReceiver.sendShareBroadcastWithAction(mActivity.getTaskId(), "foobar");
        verify(runnable, times(0)).run();
    }

    @Test
    @SmallTest
    public void sendShareBroadcastWithAction_SendBroadcastAfterDestruction() {
        ShareBroadcastReceiver receiver = new ShareBroadcastReceiver(mActivity);
        Runnable runnable = Mockito.mock(Runnable.class);
        receiver.registerShareType("foobar", runnable);
        receiver.destroy();

        ShareBroadcastReceiver.sendShareBroadcastWithAction(mActivity.getTaskId(), "foobar");
        verify(runnable, times(0)).run();
    }

    @Test
    @SmallTest
    public void sendShareBroadcastWithAction_DifferentActivity() {
        ShareBroadcastReceiver receiver1 = new ShareBroadcastReceiver(mActivity);
        Runnable runnable1 = Mockito.mock(Runnable.class);
        receiver1.registerShareType("foobar", runnable1);

        doReturn(123).when(mActivity).getTaskId();
        ShareBroadcastReceiver receiver2 = new ShareBroadcastReceiver(mActivity);
        Runnable runnable2 = Mockito.mock(Runnable.class);
        receiver2.registerShareType("foobar", runnable2);

        ShareBroadcastReceiver.sendShareBroadcastWithAction(123, "foobar");
        verify(runnable1, times(0)).run();
        verify(runnable2).run();

        receiver1.destroy();
        receiver2.destroy();
    }
}
