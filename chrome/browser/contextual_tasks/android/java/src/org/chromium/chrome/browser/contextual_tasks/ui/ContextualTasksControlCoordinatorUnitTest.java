// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks.ui;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetPeekProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link ContextualTasksControlCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContextualTasksControlCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabBottomSheetManager mTabBottomSheetManager;

    private ContextualTasksControlCoordinator mCoordinator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mCoordinator = new ContextualTasksControlCoordinator(mTabBottomSheetManager);
        mModel = mCoordinator.getModel();
    }

    @Test
    public void testInitialization() {
        assertNotNull(mModel);
        assertNotNull(mModel.get(TabBottomSheetPeekProperties.ON_ACTION_BUTTON_CLICKED));
        assertNotNull(mModel.get(TabBottomSheetPeekProperties.ON_CLOSE_CLICKED));
        assertNotNull(mModel.get(TabBottomSheetPeekProperties.ON_PEEK_VIEW_CLICKED));
    }

    @Test
    public void testCloseClicked_closesBottomSheet() {
        mModel.get(TabBottomSheetPeekProperties.ON_CLOSE_CLICKED).run();
        verify(mTabBottomSheetManager).tryToCloseBottomSheet(true);
    }

    @Test
    public void testActionClicked_expandsBottomSheet() {
        mModel.get(TabBottomSheetPeekProperties.ON_ACTION_BUTTON_CLICKED).run();
        verify(mTabBottomSheetManager).setSheetExpanded(true);
    }

    @Test
    public void testPeekViewClicked_expandsBottomSheet() {
        mModel.get(TabBottomSheetPeekProperties.ON_PEEK_VIEW_CLICKED).run();
        verify(mTabBottomSheetManager).setSheetExpanded(true);
    }
}
