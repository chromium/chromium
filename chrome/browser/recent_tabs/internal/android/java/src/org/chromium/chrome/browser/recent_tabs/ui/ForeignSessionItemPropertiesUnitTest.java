// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import static org.chromium.chrome.browser.recent_tabs.ui.ForeignSessionItemProperties.IS_SELECTED;
import static org.chromium.chrome.browser.recent_tabs.ui.ForeignSessionItemProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.recent_tabs.ui.ForeignSessionItemProperties.SESSION_PROFILE;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;

/** Tests for the ForeignSessionItem model. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ForeignSessionItemPropertiesUnitTest {
    private ForeignSession mSession;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mSession = new ForeignSession("tag", "John's iPhone 6", 32L, new ArrayList<>(), 2);
        mModel =
                ForeignSessionItemProperties.create(
                        /* device= */ mSession,
                        /* isSelected= */ false,
                        /* onClickListener= */ () -> {});
    }

    @After
    public void tearDown() {
        mModel = null;
    }

    @Test
    public void testForeignSessionItemProperties_initCreatesValidDefaultModel() {
        Assert.assertEquals(mModel.get(SESSION_PROFILE), mSession);
        Assert.assertEquals(mModel.get(IS_SELECTED), false);
        Assert.assertNotNull(mModel.get(ON_CLICK_LISTENER));
    }

    @Test
    public void testForeignSessionItemProperties_setSelectedDeviceItem() {
        mModel.set(IS_SELECTED, true);
        Assert.assertEquals(mModel.get(IS_SELECTED), true);
    }
}
