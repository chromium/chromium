// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.DEVICE_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.HOME_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.REVIEW_TABS_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.VISIBLE;

import android.content.Context;
import android.view.View;

import androidx.activity.OnBackPressedCallback;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Tests for RestoreTabsDialogMediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RestoreTabsDialogMediatorUnitTest {
    private static final String RESTORE_TABS_USED = EventConstants.RESTORE_TABS_PROMO_USED;
    private static final String TEST_CONTENT_DESRIPTION = "Test content description";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private RestoreTabsControllerDelegate mDelegate;
    @Mock private ForeignSessionHelper mForeignSessionHelper;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private Context mContext;
    @Mock private View mContent;

    private PropertyModel mModel = RestoreTabsProperties.createDefaultModel();
    private PropertyModel mDialogModel;
    private final RestoreTabsDialogMediator mMediator = new RestoreTabsDialogMediator();

    @Before
    public void setUp() {
        DeviceInfo.setIsXrForTesting(true);
        TrackerFactory.setTrackerForTests(mTracker);
        Supplier<ModalDialogManager> modalDialogManagerSupplier = () -> mModalDialogManager;
        when(mContext.getString(R.string.restore_tabs_content_description))
                .thenReturn(TEST_CONTENT_DESRIPTION);
        mMediator.initialize(
                mModel, mProfile, mTabCreatorManager, mContext, modalDialogManagerSupplier);
        mDialogModel = mMediator.getHostDialogModelForTesting();
    }

    @After
    public void tearDown() {
        mMediator.destroy();
        TrackerFactory.setTrackerForTests(null);
        mModel = null;
        mDialogModel = null;
    }

    @Test
    public void testRestoreTabsMediator_dialogModelInitialized() {
        Assert.assertTrue(mDialogModel.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));
        // This feature is only available on XR.
        Assert.assertTrue(mDialogModel.get(ModalDialogProperties.DISABLE_SCRIM));
        Assert.assertEquals(
                TEST_CONTENT_DESRIPTION,
                mDialogModel.get(ModalDialogProperties.CONTENT_DESCRIPTION));
        Assert.assertNotNull(mDialogModel.get(ModalDialogProperties.CONTROLLER));
        Assert.assertNotNull(
                mDialogModel.get(ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER));
    }

    @Test
    public void testRestoreTabsMediator_isVisible() {
        Assert.assertTrue(mMediator.setVisible(true, mContent));
        Assert.assertEquals(mDialogModel.get(ModalDialogProperties.CUSTOM_VIEW), mContent);
        verify(mModalDialogManager)
                .showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    @Test
    public void testRestoreTabsMediator_isNotVisible() {
        Assert.assertTrue(mMediator.setVisible(false, mContent));
        verify(mModalDialogManager)
                .dismissDialog(mDialogModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
    }

    @Test
    public void testRestoreTabsMediator_onBackPressedHomeScreen() {
        verifyBackPressedOnScreen(HOME_SCREEN);
    }

    @Test
    public void testRestoreTabsMediator_onBackPressedReviewTabsScreen() {
        verifyBackPressedOnScreen(REVIEW_TABS_SCREEN);
    }

    @Test
    public void testRestoreTabsMediator_onBackPressedDeviceScreen() {
        verifyBackPressedOnScreen(DEVICE_SCREEN);
    }

    private void verifyBackPressedOnScreen(@ScreenType int screenType) {
        List<ForeignSessionTab> testTabs = new ArrayList<>();
        testTabs.add(new ForeignSessionTab(JUnitTestGURLs.URL_1, "title1", 32L, 32L, 0));
        testTabs.add(new ForeignSessionTab(JUnitTestGURLs.URL_2, "title2", 32L, 32L, 0));

        List<ForeignSessionWindow> testWindows = new ArrayList<>();
        testWindows.add(new ForeignSessionWindow(31L, 1, testTabs));

        List<ForeignSession> testSessions = new ArrayList<>();
        testSessions.add(
                new ForeignSession("tag", "John's iPhone 6", 32L, testWindows, FormFactor.PHONE));
        testSessions.add(
                new ForeignSession("tag", "John's iPhone 7", 32L, testWindows, FormFactor.PHONE));

        RestoreTabsMetricsHelper.setPromoShownCount(1);
        mMediator.showHomeScreen(mForeignSessionHelper, testSessions, mDelegate);
        Assert.assertTrue(mModel.get(VISIBLE));
        Assert.assertEquals(HOME_SCREEN, mModel.get(CURRENT_SCREEN));
        mModel.set(CURRENT_SCREEN, screenType);

        OnBackPressedCallback backPressedCallback =
                (OnBackPressedCallback)
                        mDialogModel.get(ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER);
        backPressedCallback.handleOnBackPressed();

        if (screenType == HOME_SCREEN) {
            Assert.assertFalse(mModel.get(VISIBLE));
        } else {
            Assert.assertEquals(mModel.get(CURRENT_SCREEN), HOME_SCREEN);
        }
        RestoreTabsMetricsHelper.setPromoShownCount(0);
    }
}
