// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * Robolectric tests for {@link QuickDeleteViewBinder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
@Batch(Batch.UNIT_TESTS)
public class QuickDeleteViewBinderTest {
    private Activity mActivity;
    private View mQuickDeleteView;
    private PropertyModel mPropertyModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mQuickDeleteView =
                LayoutInflater.from(mActivity).inflate(R.layout.quick_delete_dialog, null);
        mPropertyModel = new PropertyModel.Builder(QuickDeleteProperties.ALL_KEYS)
                                 .with(QuickDeleteProperties.CONTEXT, mActivity)
                                 .build();
        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                mPropertyModel, mQuickDeleteView, QuickDeleteViewBinder::bind);
    }

    @After
    public void tearDown() {
        mPropertyModelChangeProcessor.destroy();
    }

    private void setSignedInStatus(boolean isSignedIn) {
        mPropertyModel.set(QuickDeleteProperties.IS_SIGNED_IN, isSignedIn);
    }

    private void setHistorySyncStatus(boolean isSyncing) {
        mPropertyModel.set(QuickDeleteProperties.IS_SYNCING_HISTORY, isSyncing);
    }

    @Test
    @SmallTest
    public void testBrowsingHistory_ZeroDomains_RemovesTheBrowsingHistoryRow() {
        var data = new QuickDeleteDelegate.DomainVisitsData("", 0);
        mPropertyModel.set(QuickDeleteProperties.DOMAIN_VISITED_DATA, data);
        ViewGroup quickDeleteBrowsingHistoryRow =
                mQuickDeleteView.findViewById(R.id.quick_delete_history_row);

        assertEquals(View.GONE, quickDeleteBrowsingHistoryRow.getVisibility());
    }

    @Test
    @SmallTest
    public void testBrowsingHistory_OneDomain_OnlyDisplaysLastVisitedDomain() {
        var data = new QuickDeleteDelegate.DomainVisitsData("example.com", 1);
        mPropertyModel.set(QuickDeleteProperties.DOMAIN_VISITED_DATA, data);

        ViewGroup quickDeleteBrowsingHistoryRow =
                mQuickDeleteView.findViewById(R.id.quick_delete_history_row);
        TextView quickDeleteBrowsingHistoryRowTitle =
                mQuickDeleteView.findViewById(R.id.quick_delete_history_row_title);

        String expected = "example.com";
        assertEquals(expected, quickDeleteBrowsingHistoryRowTitle.getText().toString());
        assertEquals(View.VISIBLE, quickDeleteBrowsingHistoryRow.getVisibility());
    }

    @Test
    @SmallTest
    public void testBrowsingHistory_TwoDomains_UpdatesHistoryText_Singular() {
        var data = new QuickDeleteDelegate.DomainVisitsData("example.com", 2);
        mPropertyModel.set(QuickDeleteProperties.DOMAIN_VISITED_DATA, data);

        ViewGroup quickDeleteBrowsingHistoryRow =
                mQuickDeleteView.findViewById(R.id.quick_delete_history_row);
        TextView quickDeleteBrowsingHistoryRowTitle =
                mQuickDeleteView.findViewById(R.id.quick_delete_history_row_title);

        String expected = "example.com + 1 site";
        assertEquals(expected, quickDeleteBrowsingHistoryRowTitle.getText().toString());
        assertEquals(View.VISIBLE, quickDeleteBrowsingHistoryRow.getVisibility());
    }

    @Test
    @SmallTest
    public void testBrowsingHistory_MultipleDomains_UpdatesHistoryText_Plural() {
        var data = new QuickDeleteDelegate.DomainVisitsData("example.com", 5);
        mPropertyModel.set(QuickDeleteProperties.DOMAIN_VISITED_DATA, data);

        ViewGroup quickDeleteBrowsingHistoryRow =
                mQuickDeleteView.findViewById(R.id.quick_delete_history_row);
        TextView quickDeleteBrowsingHistoryRowTitle =
                mQuickDeleteView.findViewById(R.id.quick_delete_history_row_title);

        String expected = "example.com + 4 sites";
        assertEquals(expected, quickDeleteBrowsingHistoryRowTitle.getText().toString());
        assertEquals(View.VISIBLE, quickDeleteBrowsingHistoryRow.getVisibility());
    }

    @Test
    @SmallTest
    public void testBrowsingHistory_HistorySyncDisabled_HidesMoreOnSyncedDevicesText() {
        setHistorySyncStatus(false);
        var data = new QuickDeleteDelegate.DomainVisitsData("example.com", 1);
        mPropertyModel.set(QuickDeleteProperties.DOMAIN_VISITED_DATA, data);

        ViewGroup quickDeleteBrowsingHistoryRow =
                mQuickDeleteView.findViewById(R.id.quick_delete_history_row);
        TextView quickDeleteBrowsingHistoryRowSubtitle =
                mQuickDeleteView.findViewById(R.id.quick_delete_history_row_subtitle);

        assertEquals(View.VISIBLE, quickDeleteBrowsingHistoryRow.getVisibility());
        assertEquals(View.GONE, quickDeleteBrowsingHistoryRowSubtitle.getVisibility());
    }

    @Test
    @SmallTest
    public void testBrowsingHistory_HistorySyncEnabled_DisplaysMoreOnSyncedDevicesText() {
        setHistorySyncStatus(true);
        var data = new QuickDeleteDelegate.DomainVisitsData("example.com", 1);
        mPropertyModel.set(QuickDeleteProperties.DOMAIN_VISITED_DATA, data);

        ViewGroup quickDeleteBrowsingHistoryRow =
                mQuickDeleteView.findViewById(R.id.quick_delete_history_row);
        TextView quickDeleteBrowsingHistoryRowSubtitle =
                mQuickDeleteView.findViewById(R.id.quick_delete_history_row_subtitle);

        assertEquals(View.VISIBLE, quickDeleteBrowsingHistoryRow.getVisibility());
        assertEquals(View.VISIBLE, quickDeleteBrowsingHistoryRowSubtitle.getVisibility());
    }

    @Test
    @SmallTest
    public void testTabsToBeClosed_OneTab_UpdatesTabsClosedText_Singular() {
        final int tabsToBeClosed = 1;
        mPropertyModel.set(QuickDeleteProperties.CLOSED_TABS_COUNT, tabsToBeClosed);
        TextViewWithCompoundDrawables quickDeleteTabsCloseRowTextView =
                mQuickDeleteView.findViewById(R.id.quick_delete_tabs_close_row);

        String expected = mActivity.getResources().getQuantityString(
                R.plurals.quick_delete_dialog_tabs_closed_text, tabsToBeClosed, tabsToBeClosed);
        assertEquals(expected, quickDeleteTabsCloseRowTextView.getText());
        assertEquals(View.VISIBLE, quickDeleteTabsCloseRowTextView.getVisibility());
    }

    @Test
    @SmallTest
    public void testTabsToBeClosed_MultipleTab_UpdatesTabsClosedText_Plural() {
        final int tabsToBeClosed = 2;
        mPropertyModel.set(QuickDeleteProperties.CLOSED_TABS_COUNT, tabsToBeClosed);
        TextViewWithCompoundDrawables quickDeleteTabsCloseRowTextView =
                mQuickDeleteView.findViewById(R.id.quick_delete_tabs_close_row);

        String expected = mActivity.getResources().getQuantityString(
                R.plurals.quick_delete_dialog_tabs_closed_text, tabsToBeClosed, tabsToBeClosed);
        assertEquals(expected, quickDeleteTabsCloseRowTextView.getText());
        assertEquals(View.VISIBLE, quickDeleteTabsCloseRowTextView.getVisibility());
    }

    @Test
    @SmallTest
    public void testSearchHistoryDisambiguationShown_WhenUserIsSignedOut() {
        setSignedInStatus(true);
        TextViewWithClickableSpans searchHistoryDisambiguation =
                mQuickDeleteView.findViewById(R.id.search_history_disambiguation);
        assertEquals(View.VISIBLE, searchHistoryDisambiguation.getVisibility());
    }

    @Test
    @SmallTest
    public void testSearchHistoryDisambiguationNotShown_WhenUserIsSignedOut() {
        setSignedInStatus(false);
        TextViewWithClickableSpans searchHistoryDisambiguation =
                mQuickDeleteView.findViewById(R.id.search_history_disambiguation);
        assertEquals(searchHistoryDisambiguation.getVisibility(), View.GONE);
    }
}
