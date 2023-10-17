// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_TITLE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.DEVICE_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.HOME_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.VISIBLE;

import android.app.Activity;
import android.widget.ViewFlipper;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;

/** Unit tests for RestoreTabsCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RestoreTabsCoordinatorUnitTest {
    @Rule public JniMocker jniMocker = new JniMocker();

    @Mock FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock private RestoreTabsMediator mMediator;
    @Mock private Profile mProfile;
    @Mock private RestoreTabsControllerDelegate mDelegate;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ForeignSessionHelper mForeignSessionHelper;

    private RestoreTabsCoordinator mCoordinator;
    private Activity mActivity;
    private PropertyModel mModel;
    private ViewFlipper mViewFlipperView;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        jniMocker.mock(FaviconHelperJni.TEST_HOOKS, mFaviconHelperJniMock);
        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mCoordinator =
                new RestoreTabsCoordinator(
                        mActivity, mProfile, mMediator, mTabCreatorManager, mBottomSheetController);
        mModel = mCoordinator.getPropertyModelForTesting();
        mViewFlipperView = mCoordinator.getViewFlipperForTesting();
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
    }

    @Test
    public void testRestoreTabsCoordinator_showHomeScreen() {
        mCoordinator.showHomeScreen(mForeignSessionHelper, new ArrayList<>(), mDelegate);
        verify(mMediator, times(1))
                .showHomeScreen(
                        any(ForeignSessionHelper.class),
                        anyList(),
                        any(RestoreTabsControllerDelegate.class));
    }

    @Test
    public void testRestoreTabsCoordinator_viewFlipperScreenChange() {
        mModel.set(CURRENT_SCREEN, HOME_SCREEN);
        Assert.assertEquals(0, mViewFlipperView.getDisplayedChild());

        mModel.set(DETAIL_SCREEN_TITLE, R.string.restore_tabs_device_screen_sheet_title);
        mModel.set(CURRENT_SCREEN, DEVICE_SCREEN);
        Assert.assertEquals(1, mViewFlipperView.getDisplayedChild());
    }

    @Test
    public void testRestoreTabsCoordinator_unsuccessfulVisibilityChange() {
        mModel.set(VISIBLE, true);
        when(mMediator.setVisible(anyBoolean(), any(RestoreTabsPromoSheetContent.class)))
                .thenReturn(false);
        verify(mMediator).dismiss();
    }
}
