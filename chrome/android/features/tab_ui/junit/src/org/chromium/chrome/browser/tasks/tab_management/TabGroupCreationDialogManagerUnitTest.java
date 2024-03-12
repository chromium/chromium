// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.appcompat.widget.DialogTitle;

import org.junit.After;
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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for TabGroupCreationDialogManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupCreationDialogManagerUnitTest {
    private static final int TAB_COUNT = 3;
    private static final String TAB1_TITLE = "Tab1";
    private static final int TAB1_ID = 456;

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Mock private TabGroupModelFilter mRegularTabGroupModelFilter;
    @Mock private TabGroupModelFilter mIncognitoTabGroupModelFilter;
    @Mock private TabGroupCreationDialogManager.ShowDialogDelegate mShowDialogDelegate;
    @Captor private ArgumentCaptor<PropertyModel> mModelCaptor;
    @Captor private ArgumentCaptor<TabGroupModelFilterObserver> mObserverCaptor;

    private Activity mActivity;
    private TabGroupCreationDialogManager mTabGroupCreationDialogManager;
    private Tab mTab1;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTab1 = TabUiUnitTestUtils.prepareTab(TAB1_ID, TAB1_TITLE);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getTabModelFilter(false))
                .thenReturn(mRegularTabGroupModelFilter);
        when(mTabModelFilterProvider.getTabModelFilter(true))
                .thenReturn(mIncognitoTabGroupModelFilter);
        mTabGroupCreationDialogManager =
                new TabGroupCreationDialogManager(
                        mActivity, mModalDialogManager, mTabModelSelector);
    }

    @After
    public void tearDown() {
        mTabGroupCreationDialogManager.destroy();
    }

    @Test
    public void testShowOnDidCreateGroup() {
        mTabGroupCreationDialogManager.setShowDialogDelegateForTesting(mShowDialogDelegate);

        verify(mRegularTabGroupModelFilter).addTabGroupObserver(mObserverCaptor.capture());
        TabGroupModelFilterObserver observer = mObserverCaptor.getValue();

        int tabCount = 5;
        when(mRegularTabGroupModelFilter.getRelatedTabCountForRootId(TAB1_ID)).thenReturn(tabCount);
        observer.didCreateNewGroup(mTab1, mRegularTabGroupModelFilter);

        verify(mShowDialogDelegate).showDialog(tabCount, false);
    }

    @Test
    public void testCreationDialogDelegate_showDialog() {
        mTabGroupCreationDialogManager
                .getShowDialogDelegateForTesting()
                .showDialog(TAB_COUNT, false);
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP));

        PropertyModel model = mModelCaptor.getValue();
        Assert.assertEquals(
                mActivity
                        .getResources()
                        .getString(R.string.tab_group_creation_positive_button_text),
                model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        Assert.assertEquals(
                mActivity.getResources().getString(R.string.cancel),
                model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));

        DialogTitle title =
                model.get(ModalDialogProperties.CUSTOM_VIEW)
                        .findViewById(R.id.creation_dialog_title);
        Assert.assertEquals(
                mActivity.getResources().getString(R.string.tab_group_creation_dialog_title),
                title.getText());

        TabGroupCreationTextInputLayout groupTitle =
                model.get(ModalDialogProperties.CUSTOM_VIEW).findViewById(R.id.tab_group_title);
        groupTitle.getEditText().setText("user title");

        ModalDialogProperties.Controller dialogController =
                model.get(ModalDialogProperties.CONTROLLER);
        dialogController.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);

        verify(mModalDialogManager)
                .dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }
}
