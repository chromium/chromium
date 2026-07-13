// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetPeekProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link ContextualTasksControlCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContextualTasksControlCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabBottomSheetManager mTabBottomSheetManager;
    @Mock private Profile mProfile;
    @Mock private ContextualTasksControlCoordinator.Natives mJniMock;

    private ContextualTasksControlCoordinator mCoordinator;
    private PropertyModel mModel;

    private static final long NATIVE_PTR = 12345L;

    @Before
    public void setUp() {
        ContextualTasksControlCoordinatorJni.setInstanceForTesting(mJniMock);
        when(mJniMock.init(any(), eq(mProfile))).thenReturn(NATIVE_PTR);

        mCoordinator = new ContextualTasksControlCoordinator(mTabBottomSheetManager, mProfile);
        mModel = mCoordinator.getModel();
    }

    @Test
    public void testInitialization() {
        assertNotNull(mModel);
        assertNotNull(mModel.get(TabBottomSheetPeekProperties.ON_ACTION_BUTTON_CLICKED));
        assertNotNull(mModel.get(TabBottomSheetPeekProperties.ON_CLOSE_CLICKED));
        assertNotNull(mModel.get(TabBottomSheetPeekProperties.ON_PEEK_VIEW_CLICKED));
        verify(mJniMock).init(mCoordinator, mProfile);
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

    @Test
    public void testTaskTitleChanged_updatesModel() {
        mCoordinator.onTaskChanged("", "task-1");
        mCoordinator.onTaskTitleChanged("task-1", "Task Title 1");
        assertEquals("Task Title 1", mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
    }

    @Test
    public void testDestroy_destroysNativeObject() {
        mCoordinator.destroy();
        verify(mJniMock).destroy(NATIVE_PTR);
    }
}
