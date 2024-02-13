// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import static org.chromium.chrome.browser.recent_tabs.ui.TabItemProperties.FOREIGN_SESSION_TAB;
import static org.chromium.chrome.browser.recent_tabs.ui.TabItemProperties.IS_SELECTED;
import static org.chromium.chrome.browser.recent_tabs.ui.TabItemProperties.ON_CLICK_LISTENER;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

/** Tests for the TabItem model. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabItemPropertiesUnitTest {
    private ForeignSessionTab mTab;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mTab = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        mModel = TabItemProperties.create(/* tab= */ mTab, /* isSelected= */ true);
    }

    @After
    public void tearDown() {
        mModel = null;
    }

    @Test
    public void testForeignSessionItemProperties_initCreatesValidDefaultModel() {
        Assert.assertEquals(mModel.get(FOREIGN_SESSION_TAB), mTab);
        Assert.assertEquals(mModel.get(IS_SELECTED), true);
        Assert.assertNull(mModel.get(ON_CLICK_LISTENER));
    }

    @Test
    public void testForeignSessionItemProperties_toggleTabSelectedState() {
        mModel.set(ON_CLICK_LISTENER, () -> {});
        Assert.assertNotNull(mModel.get(ON_CLICK_LISTENER));

        boolean isSelected = mModel.get(IS_SELECTED);
        Assert.assertEquals(isSelected, true);
        mModel.set(IS_SELECTED, !isSelected);
        Assert.assertEquals(mModel.get(IS_SELECTED), false);
    }
}
