// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;

/** Tests for {@link TabGroupCreationDialogManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupCreationDialogManagerUnitTest {
    private static final Token TAB_GROUP_ID = new Token(378L, 48739L);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Profile mProfile;
    @Mock private TabModel mTabModel;
    @Mock private TabGroupVisualDataDialogManager mTabGroupVisualDataDialogManager;
    @Mock private Runnable mOnTabGroupCreation;

    private TabGroupCreationDialogManager mTabGroupCreationDialogManager;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mTabGroupCreationDialogManager =
                new TabGroupCreationDialogManager(
                        activity, mModalDialogManager, mOnTabGroupCreation);

        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
    }

    @Test
    public void testShowOnWillMergingCreateNewGroup() {
        mTabGroupCreationDialogManager.setDialogManagerForTesting(mTabGroupVisualDataDialogManager);

        mTabGroupCreationDialogManager.showDialog(TAB_GROUP_ID, mTabModel);
        ModalDialogProperties.Controller controller =
                mTabGroupCreationDialogManager.getDialogControllerForTesting();
        verify(mTabGroupVisualDataDialogManager).showDialog(TAB_GROUP_ID, mTabModel, controller);
    }

    @Test
    public void testOnDismiss() {
        mTabGroupCreationDialogManager.setDialogManagerForTesting(mTabGroupVisualDataDialogManager);

        mTabGroupCreationDialogManager.showDialog(TAB_GROUP_ID, mTabModel);
        when(mTabGroupVisualDataDialogManager.getCurrentGroupTitle()).thenReturn("abcd");
        ModalDialogProperties.Controller controller =
                mTabGroupCreationDialogManager.getDialogControllerForTesting();

        controller.onDismiss(null, DialogDismissalCause.UNKNOWN);
        verify(mTabModel).setTabGroupColor(eq(TAB_GROUP_ID), anyInt());
        verify(mTabModel).setTabGroupTitle(eq(TAB_GROUP_ID), any());
        verify(mOnTabGroupCreation).run();
    }

    @Test
    public void testOnDismiss_deletedGroup() {
        mTabGroupCreationDialogManager.setDialogManagerForTesting(mTabGroupVisualDataDialogManager);

        mTabGroupCreationDialogManager.showDialog(TAB_GROUP_ID, mTabModel);
        when(mTabGroupVisualDataDialogManager.getCurrentGroupTitle()).thenReturn("abcd");
        ModalDialogProperties.Controller controller =
                mTabGroupCreationDialogManager.getDialogControllerForTesting();

        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(false);
        controller.onDismiss(null, DialogDismissalCause.UNKNOWN);

        verify(mTabModel, never()).setTabGroupColor(any(), anyInt());
        verify(mTabModel, never()).setTabGroupTitle(any(), any());
        verify(mOnTabGroupCreation).run();
    }
}
