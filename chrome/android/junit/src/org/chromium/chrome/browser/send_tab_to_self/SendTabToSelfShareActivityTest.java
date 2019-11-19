// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.WebContents;

/** Tests for SendTabToSelfShareActivityTest */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SendTabToSelfShareActivityTest {
    @Rule
    public JniMocker mocker = new JniMocker();

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

    private Profile mProfile;

    private class SendTabToSelfShareActivityForTest extends SendTabToSelfShareActivity {
        @Override
        BottomSheetContent createBottomSheetContent(
                ChromeActivity activity, NavigationEntry entry) {
            return mBottomSheetContent;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(SendTabToSelfAndroidBridgeJni.TEST_HOOKS, mNativeMock);
        RecordHistogram.setDisabledForTests(true);
    }

    @After
    public void tearDown() {
        RecordHistogram.setDisabledForTests(false);
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
        when(mTab.getProfile()).thenReturn(mProfile);

        // Setup the mocked object chain to get to the url, title and timestamp.
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mNavigationController.getVisibleEntry()).thenReturn(mNavigationEntry);

        // Setup the mocked object chain to get the bottom controller.
        when(mChromeActivity.getBottomSheetController()).thenReturn(mBottomSheetController);

        SendTabToSelfShareActivityForTest shareActivity = new SendTabToSelfShareActivityForTest();
        shareActivity.handleShareAction(mChromeActivity);
        verify(mBottomSheetController).requestShowContent(any(BottomSheetContent.class), eq(true));
    }
}
