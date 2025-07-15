// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.SINGLE_THEME_COLLECTION;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_COLLECTIONS;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.R;

/** Unit tests for {@link NtpSingleThemeCollectionCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpSingleThemeCollectionCoordinatorUnitTest {

    private static final String TEST_COLLECTION_TITLE = "Test Collection";
    private static final String NEW_TEST_COLLECTION_TITLE = "New Test Collection";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetDelegate mBottomSheetDelegate;

    private NtpSingleThemeCollectionCoordinator mCoordinator;
    private Context mContext;
    private View mBottomSheetView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        mCoordinator =
                new NtpSingleThemeCollectionCoordinator(
                        mContext, mBottomSheetDelegate, TEST_COLLECTION_TITLE);

        ArgumentCaptor<View> viewCaptor = ArgumentCaptor.forClass(View.class);
        verify(mBottomSheetDelegate)
                .registerBottomSheetLayout(eq(SINGLE_THEME_COLLECTION), viewCaptor.capture());
        mBottomSheetView = viewCaptor.getValue();
    }

    @Test
    public void testConstructor() {
        assertNotNull(mBottomSheetView);
        TextView title = mBottomSheetView.findViewById(R.id.bottom_sheet_title);
        assertEquals(TEST_COLLECTION_TITLE, title.getText().toString());
    }

    @Test
    public void testBackButton() {
        View backButton = mBottomSheetView.findViewById(R.id.back_button);
        assertNotNull(backButton);
        assertTrue(backButton.hasOnClickListeners());

        backButton.performClick();

        verify(mBottomSheetDelegate).showBottomSheet(eq(THEME_COLLECTIONS));
    }

    @Test
    public void testLearnMoreButton() {
        View learnMoreButton = mBottomSheetView.findViewById(R.id.learn_more_button);
        assertNotNull(learnMoreButton);
        assertTrue(learnMoreButton.hasOnClickListeners());
    }

    @Test
    public void testBuildRecyclerView() {
        RecyclerView recyclerView =
                mBottomSheetView.findViewById(R.id.single_theme_collection_recycler_view);
        assertNotNull(recyclerView);

        // Verify LayoutManager
        assertTrue(recyclerView.getLayoutManager() instanceof GridLayoutManager);
        assertEquals(3, ((GridLayoutManager) recyclerView.getLayoutManager()).getSpanCount());

        // Verify Adapter
        assertTrue(recyclerView.getAdapter() instanceof NtpThemeCollectionsAdapter);
    }

    @Test
    public void testDestroy() {
        View backButton = mBottomSheetView.findViewById(R.id.back_button);
        ImageView learnMoreButton = mBottomSheetView.findViewById(R.id.learn_more_button);
        NtpThemeCollectionsAdapter adapter = mCoordinator.getNtpThemeCollectionsAdapterForTesting();
        NtpThemeCollectionsAdapter spiedAdapter = spy(adapter);
        mCoordinator.setNtpThemeCollectionsAdapterForTesting(spiedAdapter);

        assertTrue(backButton.hasOnClickListeners());
        assertTrue(learnMoreButton.hasOnClickListeners());

        mCoordinator.destroy();

        assertFalse(backButton.hasOnClickListeners());
        assertFalse(learnMoreButton.hasOnClickListeners());
        verify(spiedAdapter).clearOnClickListeners();
    }

    @Test
    public void testUpdateThemeCollection() {
        TextView title = mCoordinator.getTitleForTesting();
        NtpThemeCollectionsAdapter adapter = mCoordinator.getNtpThemeCollectionsAdapterForTesting();
        NtpThemeCollectionsAdapter spiedAdapter = spy(adapter);
        mCoordinator.setNtpThemeCollectionsAdapterForTesting(spiedAdapter);

        // Title should not be updated with the same title.
        mCoordinator.updateThemeCollection(TEST_COLLECTION_TITLE);
        verify(spiedAdapter, times(0)).setItems(any());

        // Title should be updated with a new title.
        mCoordinator.updateThemeCollection(NEW_TEST_COLLECTION_TITLE);
        assertEquals(NEW_TEST_COLLECTION_TITLE, title.getText().toString());
        verify(spiedAdapter).setItems(any());
    }
}
