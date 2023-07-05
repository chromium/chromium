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
import android.view.ViewGroup;
import android.widget.TextView;

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
import org.chromium.base.CollectionUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.SyncService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;
import org.chromium.ui.widget.TextViewWithClickableSpans;

import java.util.HashSet;

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
    private SyncService mSyncServiceMock;

    private FakeModalDialogManager mModalDialogManager;

    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mModalDialogManager = new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);

        when(mIdentityServicesProviderMock.getIdentityManager(mProfileMock))
                .thenReturn(mIdentityManagerMock);
        when(mTabModelSelectorMock.getCurrentTab()).thenReturn(mTabMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);

        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);
    }

    @After
    public void tearDown() {
        verifyNoMoreInteractions(mOnDismissCallbackMock);
    }

    private void setSignedInStatus(boolean isSignedIn) {
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(isSignedIn);
    }

    private void setHistorySyncStatus(boolean isSyncing) {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(isSyncing);
        when(mSyncServiceMock.getActiveDataTypes())
                .thenReturn(isSyncing
                                ? CollectionUtil.newHashSet(ModelType.HISTORY_DELETE_DIRECTIVES)
                                : new HashSet<Integer>());
    }

    @Test
    @SmallTest
    public void testCancelQuickDelete() {
        setSignedInStatus(false);
        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mProfileMock)
                .showDialog(new QuickDeleteDialogDelegate.QuickDeleteDialogData());

        mModalDialogManager.clickNegativeButton();
        verify(mOnDismissCallbackMock, times(1))
                .onResult(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Test
    @SmallTest
    public void testConfirmQuickDelete() {
        setSignedInStatus(false);
        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mProfileMock)
                .showDialog(new QuickDeleteDialogDelegate.QuickDeleteDialogData());

        mModalDialogManager.clickPositiveButton();
        verify(mOnDismissCallbackMock, times(1))
                .onResult(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    @Test
    @SmallTest
    public void testSearchHistoryDisambiguationNotShown_WhenUserIsSignedOut() {
        setSignedInStatus(false);
        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mProfileMock)
                .showDialog(new QuickDeleteDialogDelegate.QuickDeleteDialogData());

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
                mTabModelSelectorMock, mProfileMock)
                .showDialog(new QuickDeleteDialogDelegate.QuickDeleteDialogData());

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
                mTabModelSelectorMock, mProfileMock)
                .showDialog(new QuickDeleteDialogDelegate.QuickDeleteDialogData());

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
        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mProfileMock)
                .showDialog(new QuickDeleteDialogDelegate.QuickDeleteDialogData());

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

        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mProfileMock)
                .showDialog(new QuickDeleteDialogDelegate.QuickDeleteDialogData(tabsToBeClosed));

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

        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mProfileMock)
                .showDialog(new QuickDeleteDialogDelegate.QuickDeleteDialogData(tabsToBeClosed));

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
    public void testBrowsingHistory_ZeroDomains_RemovesTheBrowsingHistoryRow() {
        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mProfileMock)
                .showDialog(new QuickDeleteDialogDelegate.QuickDeleteDialogData());

        View dialogView =
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.CUSTOM_VIEW);
        ViewGroup quickDeleteBrowsingHistoryRow =
                dialogView.findViewById(R.id.quick_delete_history_row);

        assertEquals(View.GONE, quickDeleteBrowsingHistoryRow.getVisibility());
    }

    @Test
    @SmallTest
    public void testBrowsingHistory_OneDomain_OnlyDisplaysLastVisitedDomain() {
        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mProfileMock)
                .showDialog(new QuickDeleteDialogDelegate.QuickDeleteDialogData("example.com", 1));

        View dialogView =
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.CUSTOM_VIEW);
        ViewGroup quickDeleteBrowsingHistoryRow =
                dialogView.findViewById(R.id.quick_delete_history_row);
        TextView quickDeleteBrowsingHistoryRowTitle =
                dialogView.findViewById(R.id.quick_delete_history_row_title);

        String expected = "example.com";
        assertEquals(expected, quickDeleteBrowsingHistoryRowTitle.getText().toString());
        assertEquals(View.VISIBLE, quickDeleteBrowsingHistoryRow.getVisibility());
    }

    @Test
    @SmallTest
    public void testBrowsingHistory_TwoDomains_UpdatesHistoryText_Singular() {
        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mProfileMock)
                .showDialog(new QuickDeleteDialogDelegate.QuickDeleteDialogData("example.com", 2));

        View dialogView =
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.CUSTOM_VIEW);
        ViewGroup quickDeleteBrowsingHistoryRow =
                dialogView.findViewById(R.id.quick_delete_history_row);
        TextView quickDeleteBrowsingHistoryRowTitle =
                dialogView.findViewById(R.id.quick_delete_history_row_title);

        String expected = "example.com + 1 site";
        assertEquals(expected, quickDeleteBrowsingHistoryRowTitle.getText().toString());
        assertEquals(View.VISIBLE, quickDeleteBrowsingHistoryRow.getVisibility());
    }

    @Test
    @SmallTest
    public void testBrowsingHistory_MultipleDomains_UpdatesHistoryText_Plural() {
        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mProfileMock)
                .showDialog(new QuickDeleteDialogDelegate.QuickDeleteDialogData("example.com", 5));

        View dialogView =
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.CUSTOM_VIEW);
        ViewGroup quickDeleteBrowsingHistoryRow =
                dialogView.findViewById(R.id.quick_delete_history_row);
        TextView quickDeleteBrowsingHistoryRowTitle =
                dialogView.findViewById(R.id.quick_delete_history_row_title);

        String expected = "example.com + 4 sites";
        assertEquals(expected, quickDeleteBrowsingHistoryRowTitle.getText().toString());
        assertEquals(View.VISIBLE, quickDeleteBrowsingHistoryRow.getVisibility());
    }

    @Test
    @SmallTest
    public void testBrowsingHistory_HistorySyncDisabled_HidesMoreOnSyncedDevicesText() {
        setHistorySyncStatus(false);
        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mProfileMock)
                .showDialog(new QuickDeleteDialogDelegate.QuickDeleteDialogData("example.com", 1));

        View dialogView =
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.CUSTOM_VIEW);
        ViewGroup quickDeleteBrowsingHistoryRow =
                dialogView.findViewById(R.id.quick_delete_history_row);
        TextView quickDeleteBrowsingHistoryRowSubtitle =
                dialogView.findViewById(R.id.quick_delete_history_row_subtitle);

        assertEquals(View.VISIBLE, quickDeleteBrowsingHistoryRow.getVisibility());
        assertEquals(View.GONE, quickDeleteBrowsingHistoryRowSubtitle.getVisibility());
    }

    @Test
    @SmallTest
    public void testBrowsingHistory_HistorySyncEnabled_DisplaysMoreOnSyncedDevicesText() {
        setHistorySyncStatus(true);
        new QuickDeleteDialogDelegate(mActivity, mModalDialogManager, mOnDismissCallbackMock,
                mTabModelSelectorMock, mProfileMock)
                .showDialog(new QuickDeleteDialogDelegate.QuickDeleteDialogData("example.com", 1));

        View dialogView =
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.CUSTOM_VIEW);
        ViewGroup quickDeleteBrowsingHistoryRow =
                dialogView.findViewById(R.id.quick_delete_history_row);
        TextView quickDeleteBrowsingHistoryRowSubtitle =
                dialogView.findViewById(R.id.quick_delete_history_row_subtitle);

        assertEquals(View.VISIBLE, quickDeleteBrowsingHistoryRow.getVisibility());
        assertEquals(View.VISIBLE, quickDeleteBrowsingHistoryRowSubtitle.getVisibility());
    }

    @Test
    @SmallTest
    public void testQuickDeleteDialog_TimePeriod_Binding() {
        QuickDeleteDialogDelegate dialog = new QuickDeleteDialogDelegate(mActivity,
                mModalDialogManager, mOnDismissCallbackMock, mTabModelSelectorMock, mProfileMock);
        QuickDeleteDialogDelegate.TimePeriodSpinnerOption options[] =
                dialog.getTimePeriodSpinnerOptions();

        assertEquals(6, options.length);
        assertEquals(TimePeriod.LAST_15_MINUTES, options[0].getTimePeriod());
        assertEquals(TimePeriod.LAST_HOUR, options[1].getTimePeriod());
        assertEquals(TimePeriod.LAST_DAY, options[2].getTimePeriod());
        assertEquals(TimePeriod.LAST_WEEK, options[3].getTimePeriod());
        assertEquals(TimePeriod.FOUR_WEEKS, options[4].getTimePeriod());
        assertEquals(TimePeriod.ALL_TIME, options[5].getTimePeriod());
    }
}
