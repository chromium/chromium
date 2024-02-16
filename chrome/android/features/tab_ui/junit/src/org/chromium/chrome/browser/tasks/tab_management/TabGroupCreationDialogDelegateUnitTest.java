// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
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

import org.chromium.base.supplier.ObservableSupplierImpl;
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

/** Tests for TabGroupCreationDialogDelegate. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupCreationDialogDelegateUnitTest {
    private static final int TAB_COUNT = 3;

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Mock private TabGroupModelFilter mRegularTabGroupModelFilter;
    @Mock private TabGroupModelFilter mIncognitoTabGroupModelFilter;
    @Captor private ArgumentCaptor<PropertyModel> mModelCaptor;
    @Captor private ArgumentCaptor<TabGroupModelFilterObserver> mObserverCaptor;

    private Activity mActivity;
    private TabGroupCreationDialogDelegate mTabGroupCreationDialogDelegate;
    private ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier =
            new ObservableSupplierImpl<>();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mTabModelSelectorSupplier.set(mTabModelSelector);
        mTabGroupCreationDialogDelegate =
                spy(
                        new TabGroupCreationDialogDelegate(
                                mActivity, mModalDialogManager, mTabModelSelectorSupplier));
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getTabModelFilter(false))
                .thenReturn(mRegularTabGroupModelFilter);
        when(mTabModelFilterProvider.getTabModelFilter(true))
                .thenReturn(mIncognitoTabGroupModelFilter);
    }

    @After
    public void tearDown() {
        mTabGroupCreationDialogDelegate.destroy();
    }

    @Test
    public void testCreationDialogDelegate_addObservers() {
        mTabGroupCreationDialogDelegate.addObservers();

        verify(mRegularTabGroupModelFilter)
                .addTabGroupObserver(any(TabGroupModelFilterObserver.class));
        verify(mIncognitoTabGroupModelFilter)
                .addTabGroupObserver(any(TabGroupModelFilterObserver.class));
    }

    @Test
    public void testShowOnDidCreateGroup() {
        mTabGroupCreationDialogDelegate.addObservers();
        verify(mRegularTabGroupModelFilter).addTabGroupObserver(mObserverCaptor.capture());
        TabGroupModelFilterObserver observer = mObserverCaptor.getValue();

        int rootId = 1;
        int tabCount = 5;
        when(mRegularTabGroupModelFilter.getRelatedTabCountForRootId(rootId)).thenReturn(tabCount);
        observer.didCreateNewGroup(rootId);

        verify(mTabGroupCreationDialogDelegate).showDialog(tabCount, false);
    }

    @Test
    public void testShowOnWillMergeTabToGroup() {
        mTabGroupCreationDialogDelegate.addObservers();
        verify(mRegularTabGroupModelFilter).addTabGroupObserver(mObserverCaptor.capture());
        TabGroupModelFilterObserver observer = mObserverCaptor.getValue();

        int newRootId = 2;
        int newRootIdTabCount = 10;
        when(mRegularTabGroupModelFilter.getRelatedTabCountForRootId(newRootId))
                .thenReturn(newRootIdTabCount);

        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class);
        when(mRegularTabGroupModelFilter.getGroupLastShownTab(newRootId)).thenReturn(tab2);

        when(mRegularTabGroupModelFilter.isTabInTabGroup(tab1)).thenReturn(true);
        when(mRegularTabGroupModelFilter.isTabInTabGroup(tab2)).thenReturn(true);
        observer.willMergeTabToGroup(tab1, newRootId);
        verify(mTabGroupCreationDialogDelegate, never()).showDialog(anyInt(), anyBoolean());

        when(mRegularTabGroupModelFilter.isTabInTabGroup(tab1)).thenReturn(false);
        when(mRegularTabGroupModelFilter.isTabInTabGroup(tab2)).thenReturn(true);
        observer.willMergeTabToGroup(tab1, newRootId);
        verify(mTabGroupCreationDialogDelegate, never()).showDialog(anyInt(), anyBoolean());

        when(mRegularTabGroupModelFilter.isTabInTabGroup(tab1)).thenReturn(true);
        when(mRegularTabGroupModelFilter.isTabInTabGroup(tab2)).thenReturn(false);
        observer.willMergeTabToGroup(tab1, newRootId);
        verify(mTabGroupCreationDialogDelegate, never()).showDialog(anyInt(), anyBoolean());

        when(mRegularTabGroupModelFilter.isTabInTabGroup(tab1)).thenReturn(false);
        when(mRegularTabGroupModelFilter.isTabInTabGroup(tab2)).thenReturn(false);
        observer.willMergeTabToGroup(tab1, newRootId);
        verify(mTabGroupCreationDialogDelegate).showDialog(newRootIdTabCount + 1, false);
    }

    @Test
    public void testCreationDialogDelegate_showDialog() {
        mTabGroupCreationDialogDelegate.showDialog(TAB_COUNT, false);
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
