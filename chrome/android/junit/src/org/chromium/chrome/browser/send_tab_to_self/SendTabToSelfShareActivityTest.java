// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridge;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridgeJni;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfCoordinator;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.JUnitTestGURLs;

/** Tests for SendTabToSelfShareActivityTest */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SendTabToSelfShareActivityTest {
    @Rule
    public JniMocker mocker = new JniMocker();
    @Mock
    public Profile.Natives mMockProfileNatives;

    @Mock
    SendTabToSelfAndroidBridge.Natives mNativeMock;
    @Mock
    private Tab mTab;
    @Mock
    private ChromeActivity mChromeActivity;
    @Mock
    private ActivityTabProvider mActivityTabProvider;
    @Mock
    private WebContents mWebContents;
    @Mock
    private NavigationController mNavigationController;
    @Mock
    private NavigationEntry mNavigationEntry;
    @Mock
    private BottomSheetContent mBottomSheetContent;
    @Mock
    private BottomSheetController mBottomSheetController;

    @Mock
    private ProfileSyncService mProfileSyncService;

    private Profile mProfile;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(SendTabToSelfAndroidBridgeJni.TEST_HOOKS, mNativeMock);
        mocker.mock(ProfileJni.TEST_HOOKS, mMockProfileNatives);
    }

    @Test
    @SmallTest
    public void testIsFeatureAvailable() {
        boolean expected = true;
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mNativeMock.isFeatureAvailable(eq(mWebContents))).thenReturn(expected);

        boolean actual = SendTabToSelfShareActivity.featureIsAvailable(mTab);
        Assert.assertEquals(expected, actual);
    }

    @Test
    @SmallTest
    public void testHandleShareAction() {
        // Setup the mocked object chain to get to the profile.
        when(mChromeActivity.getActivityTabProvider()).thenReturn(mActivityTabProvider);
        when(mActivityTabProvider.get()).thenReturn(mTab);
        when(mMockProfileNatives.fromWebContents(eq(mWebContents))).thenReturn(mProfile);

        // Setup the mocked object chain to get to the url, title and timestamp.
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mNavigationController.getVisibleEntry()).thenReturn(mNavigationEntry);
        when(mNavigationEntry.getUrl())
                .thenReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL));

        // Setup the mocked object for sync settings.
        when(mProfileSyncService.isSyncRequested()).thenReturn(true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ProfileSyncService.overrideForTests(mProfileSyncService));

        // Setup the mocked object chain to get the bottom controller.
        SendTabToSelfShareActivity shareActivity = new SendTabToSelfShareActivity();
        SendTabToSelfCoordinator.setBottomSheetContentForTesting(mBottomSheetContent);
        SendTabToSelfShareActivity.setBottomSheetControllerForTesting(mBottomSheetController);
        shareActivity.handleAction(/* triggeringActivity= */ mChromeActivity,
                /* menuOrKeyboardActionController= */ mChromeActivity);
        verify(mBottomSheetController).requestShowContent(any(BottomSheetContent.class), eq(true));
    }
}
