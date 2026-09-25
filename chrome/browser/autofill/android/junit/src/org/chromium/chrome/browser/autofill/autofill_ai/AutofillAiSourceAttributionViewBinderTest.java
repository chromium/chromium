// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiSourceAttributionProperties.HeaderProperties;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiSourceAttributionProperties.ItemType;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiSourceAttributionProperties.SourceCardProperties;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link AutofillAiSourceAttributionViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillAiSourceAttributionViewBinderTest {
    private Activity mActivity;
    private AutofillAiSourceAttributionView mView;
    private ModelList mModelList;
    private SimpleRecyclerViewAdapter mAdapter;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mView = new AutofillAiSourceAttributionView(mActivity);
        mModelList = new ModelList();
        mAdapter = new SimpleRecyclerViewAdapter(mModelList);
        mAdapter.registerType(
                ItemType.HEADER,
                new LayoutViewBuilder<View>(R.layout.autofill_ai_attribution_header_item),
                AutofillAiSourceAttributionViewBinder::bindHeader);
        mAdapter.registerType(
                ItemType.SOURCE_CARD,
                new LayoutViewBuilder<View>(R.layout.autofill_ai_source_card_item),
                AutofillAiSourceAttributionViewBinder::bindSourceCard);
        mView.setAdapter(mAdapter);
    }

    private RecyclerView getRecyclerView() {
        return (RecyclerView) mView.getContentView();
    }

    private void layoutRecyclerView() {
        getRecyclerView()
                .measure(
                        View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY),
                        View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        getRecyclerView().layout(0, 0, 1000, 1000);
    }

    @Test
    public void testBindHeader() {
        PropertyModel headerModel =
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.SUBTITLE, "Vehicle · AN-147338")
                        .build();
        mModelList.add(new ListItem(ItemType.HEADER, headerModel));
        layoutRecyclerView();

        assertEquals(1, getRecyclerView().getChildCount());
        View headerView = getRecyclerView().getChildAt(0);
        assertNotNull(headerView);

        TextView titleView = headerView.findViewById(R.id.autofill_ai_attribution_sheet_title);
        TextView subtitleView =
                headerView.findViewById(R.id.autofill_ai_attribution_sheet_subtitle);
        assertNotNull(titleView);
        assertNotNull(subtitleView);

        assertEquals("Sources", titleView.getText().toString());
        assertEquals("Vehicle · AN-147338", subtitleView.getText().toString());

        headerModel.set(HeaderProperties.SUBTITLE, "Passport · John Doe");
        assertEquals("Passport · John Doe", subtitleView.getText().toString());
    }

    @Test
    public void testBindSourceCard() {
        AtomicBoolean clicked = new AtomicBoolean(false);
        PropertyModel cardModel =
                new PropertyModel.Builder(SourceCardProperties.ALL_KEYS)
                        .with(SourceCardProperties.ICON_RES_ID, R.drawable.ic_outline_email_24dp)
                        .with(SourceCardProperties.TITLE, "Flight to San Francisco")
                        .with(
                                SourceCardProperties.CONTENT_DESCRIPTION,
                                "Open Gmail source: Flight to San Francisco")
                        .with(SourceCardProperties.ON_CLICK_LISTENER, () -> clicked.set(true))
                        .build();
        mModelList.add(new ListItem(ItemType.SOURCE_CARD, cardModel));
        layoutRecyclerView();

        assertEquals(1, getRecyclerView().getChildCount());
        View cardView = getRecyclerView().getChildAt(0);
        assertNotNull(cardView);

        ImageView iconView = cardView.findViewById(R.id.source_icon);
        TextView titleView = cardView.findViewById(R.id.source_title);
        assertNotNull(iconView);
        assertNotNull(titleView);

        assertEquals(
                R.drawable.ic_outline_email_24dp,
                shadowOf(iconView.getDrawable()).getCreatedFromResId());
        assertEquals("Flight to San Francisco", titleView.getText().toString());
        assertEquals(
                "Open Gmail source: Flight to San Francisco",
                cardView.getContentDescription().toString());

        cardView.performClick();
        assertTrue(clicked.get());
    }

    @Test
    public void testMultipleListItems() {
        PropertyModel headerModel =
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(HeaderProperties.SUBTITLE, "Vehicle · AN-147338")
                        .build();
        PropertyModel card1 =
                new PropertyModel.Builder(SourceCardProperties.ALL_KEYS)
                        .with(SourceCardProperties.ICON_RES_ID, R.drawable.ic_outline_email_24dp)
                        .with(SourceCardProperties.TITLE, "Flight to San Francisco")
                        .with(
                                SourceCardProperties.CONTENT_DESCRIPTION,
                                "Open Gmail source: Flight to San Francisco")
                        .build();
        PropertyModel card2 =
                new PropertyModel.Builder(SourceCardProperties.ALL_KEYS)
                        .with(
                                SourceCardProperties.ICON_RES_ID,
                                R.drawable.ic_photo_library_fill_24dp)
                        .with(SourceCardProperties.TITLE, "Photos · album")
                        .with(
                                SourceCardProperties.CONTENT_DESCRIPTION,
                                "Open Photos source: Photos · album")
                        .build();

        mModelList.add(new ListItem(ItemType.HEADER, headerModel));
        mModelList.add(new ListItem(ItemType.SOURCE_CARD, card1));
        mModelList.add(new ListItem(ItemType.SOURCE_CARD, card2));
        layoutRecyclerView();

        assertEquals(3, getRecyclerView().getChildCount());
    }
}
