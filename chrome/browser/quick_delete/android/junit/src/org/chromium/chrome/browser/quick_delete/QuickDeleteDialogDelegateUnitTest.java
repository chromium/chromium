// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;
import org.chromium.ui.widget.TextViewWithClickableSpans;

import java.util.List;

/**
 * Unit tests for Quick Delete dialog.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class QuickDeleteDialogDelegateUnitTest {
    @Mock
    private Callback<Integer> mOnDismissCallbackMock;
    @Mock
    private TabModelSelector mTabModelSelectorMock;
    @Mock
    private IdentityServicesProvider mIdentityServicesProviderMock;
    @Mock
    private Profile mProfileMock;
    @Mock
    private Tab mTabMock;
    @Mock
    private TabModel mTabModelMock;
    @Mock
    private IdentityManager mIdentityManagerMock;
    @Mock
    private QuickDeleteTabsFilter mQuickDeleteTabsFilterMock;
    @Mock
    private List<Tab> mClosedTabListMock;

    private FakeModalDialogManager mModalDialogManager;

    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mModalDialogManager = new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);

        when(mTabModelSelectorMock.getCurrentModel()).thenReturn(mTabModelMock);
        when(mTabModelMock.getProfile()).thenReturn(mProfileMock);

        when(mIdentityServicesProviderMock.getIdentityManager(mProfileMock))
                .thenReturn(mIdentityManagerMock);
        when(mTabModelSelectorMock.getCurrentTab()).thenReturn(mTabMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
    }

    @After
    public void tearDown() {
        verifyNoMoreInteractions(mOnDismissCallbackMock);
    }

    private void setSignedInStatus(boolean isSignedIn) {
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(isSignedIn);
    }

    @Test
    @SmallTest
    public void testCancelQuickDelete() {
        setSignedInStatus(false);
        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mQuickDeleteTabsFilterMock)
                .showDialog();

        mModalDialogManager.clickNegativeButton();
        verify(mOnDismissCallbackMock, times(1))
                .onResult(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Test
    @SmallTest
    public void testConfirmQuickDelete() {
        setSignedInStatus(false);
        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mQuickDeleteTabsFilterMock)
                .showDialog();

        mModalDialogManager.clickPositiveButton();
        verify(mOnDismissCallbackMock, times(1))
                .onResult(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    @Test
    @SmallTest
    public void testSearchHistoryDisambiguationNotShown_WhenUserIsSignedOut() {
        setSignedInStatus(false);
        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mQuickDeleteTabsFilterMock)
                .showDialog();

        View dialogView =
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.CUSTOM_VIEW);

        TextViewWithClickableSpans searchHistoryDisambiguation =
                dialogView.findViewById(R.id.search_history_disambiguation);

        assertEquals(searchHistoryDisambiguation.getVisibility(), View.GONE);
    }

    @Test
    @SmallTest
    public void testSearchHistoryDisambiguation_SearchHistoryLink() {
        setSignedInStatus(true);
        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mQuickDeleteTabsFilterMock)
                .showDialog();

        View dialogView =
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.CUSTOM_VIEW);

        TextViewWithClickableSpans searchHistoryDisambiguation =
                dialogView.findViewById(R.id.search_history_disambiguation);

        assertEquals(searchHistoryDisambiguation.getClickableSpans().length, 2);
        searchHistoryDisambiguation.getClickableSpans()[0].onClick(searchHistoryDisambiguation);

        ArgumentCaptor<LoadUrlParams> argument = ArgumentCaptor.forClass(LoadUrlParams.class);

        verify(mTabModelSelectorMock, times(1))
                .openNewTab(argument.capture(), eq(TabLaunchType.FROM_CHROME_UI), eq(mTabMock),
                        eq(false));
        assertEquals(UrlConstants.GOOGLE_SEARCH_HISTORY_URL_IN_QD, argument.getValue().getUrl());
        verify(mOnDismissCallbackMock, times(1)).onResult(DialogDismissalCause.ACTION_ON_CONTENT);
    }

    @Test
    @SmallTest
    public void testSearchHistoryDisambiguation_OtherActivityLink() {
        setSignedInStatus(true);
        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mQuickDeleteTabsFilterMock)
                .showDialog();

        View dialogView =
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.CUSTOM_VIEW);

        TextViewWithClickableSpans searchHistoryDisambiguation =
                dialogView.findViewById(R.id.search_history_disambiguation);

        assertEquals(searchHistoryDisambiguation.getClickableSpans().length, 2);
        searchHistoryDisambiguation.getClickableSpans()[1].onClick(searchHistoryDisambiguation);

        ArgumentCaptor<LoadUrlParams> argument = ArgumentCaptor.forClass(LoadUrlParams.class);

        verify(mTabModelSelectorMock, times(1))
                .openNewTab(argument.capture(), eq(TabLaunchType.FROM_CHROME_UI), eq(mTabMock),
                        eq(false));
        assertEquals(UrlConstants.MY_ACTIVITY_URL_IN_QD, argument.getValue().getUrl());
        verify(mOnDismissCallbackMock, times(1)).onResult(DialogDismissalCause.ACTION_ON_CONTENT);
    }

    @Test
    @SmallTest
    public void testTabsToBeClosed_ZeroTabs_RemovesTheTabsClosedText() {
        when(mClosedTabListMock.size()).thenReturn(0);
        when(mQuickDeleteTabsFilterMock.getListOfTabsToBeClosed()).thenReturn(mClosedTabListMock);

        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mQuickDeleteTabsFilterMock)
                .showDialog();

        View dialogView =
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.CUSTOM_VIEW);
        TextViewWithCompoundDrawables quickDeleteTabsCloseRowTextView =
                dialogView.findViewById(R.id.quick_delete_tabs_close_row);
        assertEquals(View.GONE, quickDeleteTabsCloseRowTextView.getVisibility());
    }

    @Test
    @SmallTest
    public void testTabsToBeClosed_OneTab_UpdatesTabsClosedText_Singular() {
        final int tabsToBeClosed = 1;
        when(mClosedTabListMock.size()).thenReturn(tabsToBeClosed);
        when(mQuickDeleteTabsFilterMock.getListOfTabsToBeClosed()).thenReturn(mClosedTabListMock);

        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mQuickDeleteTabsFilterMock)
                .showDialog();

        View dialogView =
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.CUSTOM_VIEW);
        TextViewWithCompoundDrawables quickDeleteTabsCloseRowTextView =
                dialogView.findViewById(R.id.quick_delete_tabs_close_row);

        String expected = mActivity.getResources().getQuantityString(
                R.plurals.quick_delete_dialog_tabs_closed_text, tabsToBeClosed, tabsToBeClosed);
        assertEquals(expected, quickDeleteTabsCloseRowTextView.getText());
        assertEquals(View.VISIBLE, quickDeleteTabsCloseRowTextView.getVisibility());
    }

    @Test
    @SmallTest
    public void testTabsToBeClosed_MultipleTab_UpdatesTabsClosedText_Plural() {
        final int tabsToBeClosed = 2;
        when(mClosedTabListMock.size()).thenReturn(tabsToBeClosed);
        when(mQuickDeleteTabsFilterMock.getListOfTabsToBeClosed()).thenReturn(mClosedTabListMock);

        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mQuickDeleteTabsFilterMock)
                .showDialog();

        View dialogView =
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.CUSTOM_VIEW);
        TextViewWithCompoundDrawables quickDeleteTabsCloseRowTextView =
                dialogView.findViewById(R.id.quick_delete_tabs_close_row);

        String expected = mActivity.getResources().getQuantityString(
                R.plurals.quick_delete_dialog_tabs_closed_text, tabsToBeClosed, tabsToBeClosed);
        assertEquals(expected, quickDeleteTabsCloseRowTextView.getText());
        assertEquals(View.VISIBLE, quickDeleteTabsCloseRowTextView.getVisibility());
    }
}
