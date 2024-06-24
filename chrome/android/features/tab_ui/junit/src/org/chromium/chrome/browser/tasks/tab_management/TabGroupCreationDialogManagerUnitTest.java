// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;

/** Tests for TabGroupCreationDialogManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupCreationDialogManagerUnitTest {
    private static final String TAB1_TITLE = "Tab1";
    private static final int TAB1_ID = 456;
    private static final int COLOR_1 = TabGroupColorId.BLUE;

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupVisualDataDialogManager mTabGroupVisualDataDialogManager;
    @Mock private Runnable mOnTabGroupCreation;

    private Activity mActivity;
    private TabGroupCreationDialogManager mTabGroupCreationDialogManager;
    private Tab mTab1;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTab1 = TabUiUnitTestUtils.prepareTab(TAB1_ID, TAB1_TITLE);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mTabGroupCreationDialogManager =
                new TabGroupCreationDialogManager(
                        mActivity, mModalDialogManager, mOnTabGroupCreation);
    }

    @Test
    public void testCreationDialogSkipped() {
        TabGroupModelFilter.SKIP_TAB_GROUP_CREATION_DIALOG.setForTesting(true);

        mTabGroupCreationDialogManager.setDialogManagerForTesting(mTabGroupVisualDataDialogManager);

        mTabGroupCreationDialogManager.showDialog(TAB1_ID, mTabGroupModelFilter);

        verify(mTabGroupVisualDataDialogManager, never()).showDialog(anyInt(), any(), any());
    }

    @Test
    public void testShowOnWillMergingCreateNewGroup() {
        mTabGroupCreationDialogManager.setDialogManagerForTesting(mTabGroupVisualDataDialogManager);

        mTabGroupCreationDialogManager.showDialog(TAB1_ID, mTabGroupModelFilter);
        ModalDialogProperties.Controller controller =
                mTabGroupCreationDialogManager.getDialogControllerForTesting();
        verify(mTabGroupVisualDataDialogManager)
                .showDialog(TAB1_ID, mTabGroupModelFilter, controller);
    }

    @Test
    public void testRunnableOnDismiss() {
        mTabGroupCreationDialogManager.setDialogManagerForTesting(mTabGroupVisualDataDialogManager);

        mTabGroupCreationDialogManager.showDialog(TAB1_ID, mTabGroupModelFilter);
        ModalDialogProperties.Controller controller =
                mTabGroupCreationDialogManager.getDialogControllerForTesting();
        controller.onDismiss(null, DialogDismissalCause.UNKNOWN);
        verify(mOnTabGroupCreation).run();
    }
}
