// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link CrossDeviceListMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CrossDeviceListMediatorUnitTest {
    private ModelList mModelList;
    private PropertyModel mModel;
    private CrossDeviceListMediator mMediator;

    @Before
    public void setUp() {
        mModelList = new ModelList();
        mModel = new PropertyModel(CrossDeviceListProperties.ALL_KEYS);
        mMediator = new CrossDeviceListMediator(mModelList, mModel);
    }

    @After
    public void tearDown() {
        mMediator.destroy();
        assertEquals(0, mModelList.size());
    }

    @Test
    public void testClearData() {
        mMediator.clearModelList();
        assertEquals(0, mModelList.size());
    }

    @Test
    public void testNoGroupItems() {
        mMediator.buildModelList();
        assertEquals(0, mModelList.size());
        assertTrue(mModel.get(CrossDeviceListProperties.EMPTY_STATE_VISIBLE));
    }
}
