// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Spinner;

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
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.quick_delete.QuickDeleteDialogDelegate.TimePeriodChangeObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.TextViewWithClickableSpans;

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
    private Tab mTabMock;
    @Mock
    private SettingsLauncher mSettingsLauncherMock;
    @Mock
    private TimePeriodChangeObserver mTimePeriodChangeObserverMock;

    private FakeModalDialogManager mModalDialogManager;

    private Activity mActivity;
    private View mQuickDeleteView;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mTabModelSelectorMock.getCurrentTab()).thenReturn(mTabMock);

        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mQuickDeleteView =
                LayoutInflater.from(mActivity).inflate(R.layout.quick_delete_dialog, null);
        mModalDialogManager = new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
    }

    @After
    public void tearDown() {
        verifyNoMoreInteractions(mOnDismissCallbackMock);
    }

    @Test
    @SmallTest
    public void testObserverFired_OnSpinnerChanges() {
        new QuickDeleteDialogDelegate(mActivity, mQuickDeleteView, mModalDialogManager,
                mOnDismissCallbackMock, mTabModelSelectorMock, mSettingsLauncherMock,
                mTimePeriodChangeObserverMock)
                .showDialog();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Spinner spinnerView = mQuickDeleteView.findViewById(R.id.quick_delete_spinner);
            // Set the time selection for LAST_HOUR.
            spinnerView.setSelection(1);
            verify(mTimePeriodChangeObserverMock).onTimePeriodChanged(eq(TimePeriod.LAST_HOUR));
        });
    }

    @Test
    @SmallTest
    public void testClickingOnMoreOptions_LaunchesClearBrowsingData() {
        doNothing()
                .when(mSettingsLauncherMock)
                .launchSettingsActivity(eq(mActivity),
                        eq(SettingsLauncher.SettingsFragment.CLEAR_BROWSING_DATA_ADVANCED_PAGE));
        new QuickDeleteDialogDelegate(mActivity, mQuickDeleteView, mModalDialogManager,
                mOnDismissCallbackMock, mTabModelSelectorMock, mSettingsLauncherMock,
                mTimePeriodChangeObserverMock)
                .showDialog();

        ButtonCompat moreOptionsView =
                mQuickDeleteView.findViewById(R.id.quick_delete_more_options);
        moreOptionsView.performClick();

        verify(mSettingsLauncherMock)
                .launchSettingsActivity(eq(mActivity),
                        eq(SettingsLauncher.SettingsFragment.CLEAR_BROWSING_DATA_ADVANCED_PAGE));
    }

    @Test
    @SmallTest
    public void testCancelQuickDelete() {
        new QuickDeleteDialogDelegate(mActivity, mQuickDeleteView, mModalDialogManager,
                mOnDismissCallbackMock, mTabModelSelectorMock, mSettingsLauncherMock,
                mTimePeriodChangeObserverMock)
                .showDialog();

        mModalDialogManager.clickNegativeButton();
        verify(mOnDismissCallbackMock, times(1))
                .onResult(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Test
    @SmallTest
    public void testConfirmQuickDelete() {
        new QuickDeleteDialogDelegate(mActivity, mQuickDeleteView, mModalDialogManager,
                mOnDismissCallbackMock, mTabModelSelectorMock, mSettingsLauncherMock,
                mTimePeriodChangeObserverMock)
                .showDialog();

        mModalDialogManager.clickPositiveButton();
        verify(mOnDismissCallbackMock, times(1))
                .onResult(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    @Test
    @SmallTest
    public void testSearchHistoryDisambiguation_SearchHistoryLink() {
        new QuickDeleteDialogDelegate(mActivity, mQuickDeleteView, mModalDialogManager,
                mOnDismissCallbackMock, mTabModelSelectorMock, mSettingsLauncherMock,
                mTimePeriodChangeObserverMock)
                .showDialog();

        TextViewWithClickableSpans searchHistoryDisambiguation =
                mQuickDeleteView.findViewById(R.id.search_history_disambiguation);

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
        new QuickDeleteDialogDelegate(mActivity, mQuickDeleteView, mModalDialogManager,
                mOnDismissCallbackMock, mTabModelSelectorMock, mSettingsLauncherMock,
                mTimePeriodChangeObserverMock)
                .showDialog();

        TextViewWithClickableSpans searchHistoryDisambiguation =
                mQuickDeleteView.findViewById(R.id.search_history_disambiguation);

        assertEquals(searchHistoryDisambiguation.getClickableSpans().length, 2);
        searchHistoryDisambiguation.getClickableSpans()[1].onClick(searchHistoryDisambiguation);

        ArgumentCaptor<LoadUrlParams> argument = ArgumentCaptor.forClass(LoadUrlParams.class);

        verify(mTabModelSelectorMock, times(1))
                .openNewTab(argument.capture(), eq(TabLaunchType.FROM_CHROME_UI), eq(mTabMock),
                        eq(false));
        assertEquals(UrlConstants.MY_ACTIVITY_URL_IN_QD, argument.getValue().getUrl());
        verify(mOnDismissCallbackMock, times(1)).onResult(DialogDismissalCause.ACTION_ON_CONTENT);
    }
}
