// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.hamcrest.Matchers.instanceOf;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.HOME_SCREEN_DELEGATE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.SELECTED_DEVICE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.VISIBLE;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
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
        mMediator.initialize(mModel);
    }

    @After
    public void tearDown() {
        mModel = null;
    }

    @Test
    public void testRestoreTabsMediator_createsValidDefaultModel() {
        ForeignSession session = new ForeignSession("tag", "John's iPhone 6", 32L);
        List<ForeignSession> testSessions = new ArrayList<>();
        testSessions.add(session);

        Assert.assertEquals(mModel.get(VISIBLE), false);

        mMediator.showOptions(testSessions);

        Assert.assertEquals(mModel.get(VISIBLE), true);
        Assert.assertEquals(
                mModel.get(CURRENT_SCREEN), RestoreTabsProperties.ScreenType.HOME_SCREEN);
        Assert.assertEquals(mModel.get(SELECTED_DEVICE), testSessions.get(0));

        Assert.assertNotNull(mModel.get(HOME_SCREEN_DELEGATE));
        Assert.assertThat(mModel.get(HOME_SCREEN_DELEGATE),
                instanceOf(RestoreTabsPromoScreenCoordinator.Delegate.class));
    }
}
