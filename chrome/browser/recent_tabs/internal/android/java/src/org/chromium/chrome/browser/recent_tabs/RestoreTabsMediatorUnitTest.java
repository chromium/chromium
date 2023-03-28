// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.hamcrest.Matchers.instanceOf;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_BACK_CLICK_HANDLER;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DEVICE_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.HOME_SCREEN_DELEGATE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.REVIEW_TABS_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.REVIEW_TABS_SCREEN_DELEGATE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.SELECTED_DEVICE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.DEVICE_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.REVIEW_TABS_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.VISIBLE;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsDetailScreenCoordinator;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsPromoScreenCoordinator;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Tests for RestoreTabsMediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RestoreTabsMediatorUnitTest {
    private PropertyModel mModel = RestoreTabsProperties.createDefaultModel();
    private RestoreTabsMediator mMediator = new RestoreTabsMediator();

    @Before
    public void setUp() {
        mMediator.initialize(mModel, new RestoreTabsControllerFactory.ControllerListener() {
            @Override
            public void onDismissed() {
                mMediator.destroy();
            }
        });
    }

    @After
    public void tearDown() {
        mModel = null;
    }

    @Test
    public void testRestoreTabsMediator_initCreatesValidDefaultModel() {
        Assert.assertEquals(mModel.get(VISIBLE), false);
        Assert.assertNotNull(mModel.get(HOME_SCREEN_DELEGATE));
        Assert.assertThat(mModel.get(HOME_SCREEN_DELEGATE),
                instanceOf(RestoreTabsPromoScreenCoordinator.Delegate.class));
        Assert.assertNotNull(mModel.get(DETAIL_SCREEN_BACK_CLICK_HANDLER));
        Assert.assertEquals(
                mModel.get(CURRENT_SCREEN), RestoreTabsProperties.ScreenType.HOME_SCREEN);
    }

    @Test
    public void testRestoreTabsMediator_onDismissed() {
        ForeignSession session = new ForeignSession("tag", "John's iPhone 6", 32L);
        List<ForeignSession> testSessions = new ArrayList<>();
        testSessions.add(session);

        mMediator.showOptions(testSessions);
        Assert.assertEquals(mModel.get(VISIBLE), true);
        mMediator.dismiss();
        Assert.assertEquals(mModel.get(VISIBLE), false);
    }

    @Test
    public void testRestoreTabsMediator_showOptionsUpdatesModel() {
        ForeignSession session = new ForeignSession("tag", "John's iPhone 6", 32L);
        List<ForeignSession> testSessions = new ArrayList<>();
        testSessions.add(session);

        mMediator.showOptions(testSessions);

        Assert.assertEquals(mModel.get(VISIBLE), true);
        Assert.assertEquals(
                mModel.get(CURRENT_SCREEN), RestoreTabsProperties.ScreenType.HOME_SCREEN);
        Assert.assertEquals(mModel.get(SELECTED_DEVICE), testSessions.get(0));
    }

    @Test
    public void testRestoreTabsMediator_setDeviceListItemsNoSelection() {
        ForeignSession session1 = new ForeignSession("tag1", "John's iPhone 6", 32L);
        ForeignSession session2 = new ForeignSession("tag2", "John's iPhone 7", 33L);
        List<ForeignSession> testSessions = new ArrayList<>();
        testSessions.add(session1);
        testSessions.add(session2);

        mMediator.setDeviceListItems(testSessions);

        Assert.assertEquals(mModel.get(SELECTED_DEVICE), testSessions.get(0));
    }

    @Test
    public void testRestoreTabsMediator_setDeviceListItemsSelection() {
        ForeignSession session1 = new ForeignSession("tag1", "John's iPhone 6", 32L);
        ForeignSession session2 = new ForeignSession("tag2", "John's iPhone 7", 33L);
        List<ForeignSession> testSessions = new ArrayList<>();
        testSessions.add(session1);
        testSessions.add(session2);

        mModel.set(SELECTED_DEVICE, session2);

        mMediator.setDeviceListItems(testSessions);

        Assert.assertEquals(mModel.get(SELECTED_DEVICE), testSessions.get(1));
    }

    @Test
    public void testRestoreTabsMediator_setSelectedDeviceItem() {
        ForeignSession session = new ForeignSession("tag", "John's iPhone 6", 32L);
        mMediator.setSelectedDeviceItem(session);

        Assert.assertEquals(mModel.get(SELECTED_DEVICE), session);
    }

    @Test
    public void testRestoreTabsMediator_setCurrentScreenDevices() {
        mMediator.setCurrentScreen(DEVICE_SCREEN);

        Assert.assertEquals(mModel.get(DETAIL_SCREEN_MODEL_LIST), mModel.get(DEVICE_MODEL_LIST));
        Assert.assertEquals(
                mModel.get(CURRENT_SCREEN), RestoreTabsProperties.ScreenType.DEVICE_SCREEN);
    }

    @Test
    public void testRestoreTabsMediator_setCurrentScreenReviewTabs() {
        mMediator.setCurrentScreen(REVIEW_TABS_SCREEN);

        Assert.assertEquals(
                mModel.get(DETAIL_SCREEN_MODEL_LIST), mModel.get(REVIEW_TABS_MODEL_LIST));
        Assert.assertNotNull(mModel.get(REVIEW_TABS_SCREEN_DELEGATE));
        Assert.assertThat(mModel.get(REVIEW_TABS_SCREEN_DELEGATE),
                instanceOf(RestoreTabsDetailScreenCoordinator.Delegate.class));
        Assert.assertEquals(
                mModel.get(CURRENT_SCREEN), RestoreTabsProperties.ScreenType.REVIEW_TABS_SCREEN);
    }
}
