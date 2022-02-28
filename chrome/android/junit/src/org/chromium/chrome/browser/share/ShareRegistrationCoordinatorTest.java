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
import org.robolectric.annotation.LooperMode;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridge;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridgeJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.JUnitTestGURLs;

/** Tests for ShareRegistrationCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class ShareRegistrationCoordinatorTest {
    @Rule
    public Features.JUnitProcessor mProcessorRule = new Features.JUnitProcessor();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public JniMocker mocker = new JniMocker();
    @Rule
    public AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

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
    private Context mContext;
    @Mock
    private WindowAndroid mWindowAndroid;

    @Captor
    private ArgumentCaptor<Intent> mIntentCaptor;

    @Spy
    private Activity mActivity;

    private Supplier<Tab> mCurrentTabSupplier;

    @Before
    public void setUp() {
        mocker.mock(SendTabToSelfAndroidBridgeJni.TEST_HOOKS, mNativeMock);
        Profile.setLastUsedProfileForTesting(mProfile);
        mActivity = Mockito.spy(Robolectric.buildActivity(Activity.class).create().get());

        mCurrentTabSupplier = () -> mTab;
    }

    @Test
    @SmallTest
    @Features.DisableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_SIGNIN_PROMO)
    public void doSendTabToSelfShare() {
        ShareRegistrationCoordinator shareCoordinator = new ShareRegistrationCoordinator(
                mActivity, mWindowAndroid, mCurrentTabSupplier, mBottomSheetController);
        // Setup the mocked object chain to get to the url, title and timestamp.
        when(mNavigationEntry.getUrl())
                .thenReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL));

        shareCoordinator.doSendTabToSelfShare(
                mActivity, mWindowAndroid, mNavigationEntry, mBottomSheetController);
        verify(mBottomSheetController).requestShowContent(any(BottomSheetContent.class), eq(true));

        shareCoordinator.destroy();
    }

    @Test
    @SmallTest
    public void onShareActionChosen() {
        ShareRegistrationCoordinator shareCoordinator = new ShareRegistrationCoordinator(
                mActivity, mWindowAndroid, mCurrentTabSupplier, mBottomSheetController);
        Runnable runnable = Mockito.mock(Runnable.class);
        shareCoordinator.registerShareType("foobar", runnable);

        ShareRegistrationCoordinator.onShareActionChosen(mActivity.getTaskId(), "foobar");
        verify(runnable).run();

        shareCoordinator.destroy();
    }

    @Test
    @SmallTest
    public void onShareActionChosen_NoRegisteredReceiver() {
        ShareRegistrationCoordinator.onShareActionChosen(mActivity.getTaskId(), "foobar");
        // No crash observed.
    }

    @Test
    @SmallTest
    public void registerShareType_ReusingShareType() {
        ShareRegistrationCoordinator shareCoordinator = new ShareRegistrationCoordinator(
                mActivity, mWindowAndroid, mCurrentTabSupplier, mBottomSheetController);
        Runnable runnable1 = Mockito.mock(Runnable.class);
        Runnable runnable2 = Mockito.mock(Runnable.class);
        shareCoordinator.registerShareType("foobar", runnable1);
        try {
            shareCoordinator.registerShareType("foobar", runnable2);
            Assert.assertTrue("Expected exception to be thrown", false);
        } catch (IllegalStateException e) {
        }
        shareCoordinator.destroy();
    }

    @Test
    @SmallTest
    public void onShareActionChosen_RegisterShareTypeAfterDestruction() {
        ShareRegistrationCoordinator shareCoordinator = new ShareRegistrationCoordinator(
                mActivity, mWindowAndroid, mCurrentTabSupplier, mBottomSheetController);
        shareCoordinator.destroy();
        Runnable runnable = Mockito.mock(Runnable.class);
        shareCoordinator.registerShareType("foobar", runnable);

        ShareRegistrationCoordinator.onShareActionChosen(mActivity.getTaskId(), "foobar");
        verify(runnable, times(0)).run();
    }

    @Test
    @SmallTest
    public void onShareActionChosen_SendBroadcastAfterDestruction() {
        ShareRegistrationCoordinator shareCoordinator = new ShareRegistrationCoordinator(
                mActivity, mWindowAndroid, mCurrentTabSupplier, mBottomSheetController);
        Runnable runnable = Mockito.mock(Runnable.class);
        shareCoordinator.registerShareType("foobar", runnable);
        shareCoordinator.destroy();

        ShareRegistrationCoordinator.onShareActionChosen(mActivity.getTaskId(), "foobar");
        verify(runnable, times(0)).run();
    }

    @Test
    @SmallTest
    public void sendShareBroadcastWithAction_DifferentActivity() {
        ShareRegistrationCoordinator shareCoordinator1 = new ShareRegistrationCoordinator(
                mActivity, mWindowAndroid, mCurrentTabSupplier, mBottomSheetController);
        Runnable runnable1 = Mockito.mock(Runnable.class);
        shareCoordinator1.registerShareType("foobar", runnable1);

        doReturn(123).when(mActivity).getTaskId();
        ShareRegistrationCoordinator shareCoordinator2 = new ShareRegistrationCoordinator(
                mActivity, mWindowAndroid, mCurrentTabSupplier, mBottomSheetController);
        Runnable runnable2 = Mockito.mock(Runnable.class);
        shareCoordinator2.registerShareType("foobar", runnable2);

        ShareRegistrationCoordinator.onShareActionChosen(123, "foobar");
        verify(runnable1, times(0)).run();
        verify(runnable2).run();

        shareCoordinator1.destroy();
        shareCoordinator2.destroy();
    }
}
