// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.appcompat.widget.DialogTitle;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for TabGroupVisualDataDialogManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupVisualDataDialogManagerUnitTest {
    private static final int TAB1_ID = 456;

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private ModalDialogProperties.Controller mDialogController;
    @Captor private ArgumentCaptor<PropertyModel> mModelCaptor;

    private Activity mActivity;
    private TabGroupVisualDataDialogManager mTabGroupVisualDataDialogManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mTabGroupVisualDataDialogManager =
                new TabGroupVisualDataDialogManager(
                        mActivity,
                        mModalDialogManager,
                        TabGroupVisualDataDialogManager.DialogType.TAB_GROUP_CREATION,
                        R.string.tab_group_creation_dialog_title);
    }

    @Test
    public void testVisualDataDialogDelegate_showDialog() {
        mTabGroupVisualDataDialogManager.showDialog(
                TAB1_ID, mTabGroupModelFilter, mDialogController);
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP));

        PropertyModel model = mModelCaptor.getValue();
        Assert.assertEquals(
                mActivity
                        .getResources()
                        .getString(R.string.tab_group_creation_positive_button_text),
                model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));

        DialogTitle title =
                model.get(ModalDialogProperties.CUSTOM_VIEW)
                        .findViewById(R.id.visual_data_dialog_title);
        Assert.assertEquals(
                mActivity.getResources().getString(R.string.tab_group_creation_dialog_title),
                title.getText());
    }

    @Test
    public void testVisualDataDialogDelegate_doubleShowDismissed() {
        // Mock a double trigger for the creation dialog observer method for the same group action,
        // but show dialog is only called once.
        mTabGroupVisualDataDialogManager.showDialog(
                TAB1_ID, mTabGroupModelFilter, mDialogController);
        mTabGroupVisualDataDialogManager.showDialog(
                TAB1_ID, mTabGroupModelFilter, mDialogController);
        verify(mModalDialogManager, times(1))
                .showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP));
    }
}
