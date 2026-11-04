// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;

import android.content.Context;
import android.graphics.Canvas;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceGroupAdapter;
import androidx.preference.PreferenceManager;
import androidx.preference.PreferenceScreen;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.ChromeBasePreferenceCategory;

import java.util.List;
import java.util.Map;

/** Unit test for {@link SelectionDecoration}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SelectionDecorationTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_Chromium_Settings);
    }

    @Test
    public void testSetChildViewColor_preferenceCategoryHasCategoryStyle() {
        PreferenceManager preferenceManager = new PreferenceManager(mContext);
        PreferenceScreen preferenceScreen = preferenceManager.createPreferenceScreen(mContext);

        var category = new ChromeBasePreferenceCategory(mContext);
        category.setKey("category_key");
        category.setTitle("Category Header");
        preferenceScreen.addPreference(category);

        Preference preference = new Preference(mContext);
        preference.setKey("item_key");
        preference.setTitle("Menu Item");
        preferenceScreen.addPreference(preference);

        PreferenceGroupAdapter adapter = new PreferenceGroupAdapter(preferenceScreen);

        RecyclerView recyclerView = new TestRecyclerView(mContext);
        recyclerView.setLayoutManager(new LinearLayoutManager(mContext));
        recyclerView.setAdapter(adapter);

        // View for ChromeBasePreferenceCategory
        FrameLayout categoryView = new FrameLayout(mContext);
        TextView categoryTitle = new TextView(mContext);
        categoryTitle.setId(android.R.id.title);
        categoryView.addView(categoryTitle);

        // View for Preference
        FrameLayout itemContainer = new FrameLayout(mContext);
        TextView itemTitle = new TextView(mContext);
        itemTitle.setId(android.R.id.title);
        itemContainer.addView(itemTitle);

        recyclerView.addView(categoryView);
        recyclerView.addView(itemContainer);

        SelectionDecoration decoration =
                new SelectionDecoration(
                        0, 0, 0f, 0, /* sectionGapPx= */ 0, /* sectionIdProvider= */ pref -> null);

        // Trigger onDraw which runs setChildViewColor
        decoration.onDraw(new Canvas(), recyclerView, mock(RecyclerView.State.class));

        // Category view background should be cleared (null)
        assertNull("Category background should be null", categoryView.getBackground());
    }

    @Test
    public void testGetItemOffsets_addsSectionGapAboveFirstRowOfSection() {
        PreferenceManager preferenceManager = new PreferenceManager(mContext);
        PreferenceScreen preferenceScreen = preferenceManager.createPreferenceScreen(mContext);
        Map<String, String> sectionByKey = Map.of("a1", "a", "a2", "a", "b1", "b");
        for (String key : List.of("a1", "a2", "b1")) {
            Preference preference = new Preference(mContext);
            preference.setKey(key);
            preference.setTitle(key);
            preferenceScreen.addPreference(preference);
        }

        RecyclerView recyclerView = new RecyclerView(mContext);
        recyclerView.setLayoutManager(new LinearLayoutManager(mContext));
        recyclerView.setAdapter(new PreferenceGroupAdapter(preferenceScreen));

        int verticalMargin = 4;
        int sectionGap = 20;
        SelectionDecoration decoration =
                new SelectionDecoration(
                        verticalMargin,
                        0,
                        0f,
                        0,
                        sectionGap,
                        pref -> sectionByKey.get(pref.getKey()));
        recyclerView.addItemDecoration(decoration);

        // Lay out the list so the adapter binds the rows and the decoration sets their margins.
        recyclerView.measure(
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(2000, View.MeasureSpec.EXACTLY));
        recyclerView.layout(0, 0, 1000, 2000);

        assertEquals(3, recyclerView.getChildCount());
        assertEquals(verticalMargin, topMargin(recyclerView.getChildAt(0)));
        assertEquals(verticalMargin, topMargin(recyclerView.getChildAt(1)));
        assertEquals(verticalMargin + sectionGap, topMargin(recyclerView.getChildAt(2)));
    }

    private static int topMargin(View view) {
        return ((ViewGroup.MarginLayoutParams) view.getLayoutParams()).topMargin;
    }

    private static class TestRecyclerView extends RecyclerView {
        TestRecyclerView(Context context) {
            super(context);
        }

        @Override
        public int getChildAdapterPosition(android.view.View child) {
            return indexOfChild(child);
        }
    }
}
