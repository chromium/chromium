// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_COLLECTIONS;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link NtpThemeCollectionsCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeCollectionsCoordinatorUnitTest {

    private static final String TEST_COLLECTION_ID = "Test Collection Id";
    private static final String TEST_COLLECTION_TITLE = "Test Collection";
    private static final int TEST_COLLECTION_HASH = 123; // Mock hash value for testing

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private NtpSingleThemeCollectionCoordinator mNtpSingleThemeCollectionCoordinator;
    @Mock private NtpThemeCollectionManager mNtpThemeCollectionManager;
    @Mock private Runnable mOnDailyUpdateCancelledCallback;
    @Captor private ArgumentCaptor<ComponentCallbacks> mComponentCallbacksCaptor;

    private NtpThemeCollectionsCoordinator mCoordinator;
    private Context mContext;
    private Context mContextSpy;
    private View mBottomSheetView;
    private List<BackgroundCollection> mThemeCollectionsList;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mContextSpy = spy(mContext);
        mThemeCollectionsList = new ArrayList<>();

        when(mBottomSheetDelegate.getBottomSheetController()).thenReturn(mBottomSheetController);

        mCoordinator =
                new NtpThemeCollectionsCoordinator(
                        mContextSpy,
                        mBottomSheetDelegate,
                        mProfile,
                        mNtpThemeCollectionManager,
                        mOnDailyUpdateCancelledCallback,
                        mThemeCollectionsList);

        ArgumentCaptor<View> viewCaptor = ArgumentCaptor.forClass(View.class);
        verify(mBottomSheetDelegate)
                .registerBottomSheetLayout(eq(THEME_COLLECTIONS), viewCaptor.capture());
        mBottomSheetView = viewCaptor.getValue();
    }

    @Test
    public void testConstructor() {
        assertNotNull(mBottomSheetView);

        RecyclerView recyclerView =
                mBottomSheetView.findViewById(R.id.theme_collections_recycler_view);
        NtpThemeCollectionsAdapter adapter = (NtpThemeCollectionsAdapter) recyclerView.getAdapter();
        assertEquals(mThemeCollectionsList.size(), adapter.getItemCount());
    }

    @Test
    public void testBackButton() {
        View backButton = mBottomSheetView.findViewById(R.id.back_button);
        assertNotNull(backButton);
        assertTrue(backButton.hasOnClickListeners());

        backButton.performClick();

        verify(mBottomSheetDelegate).showBottomSheet(eq(THEME));
    }

    @Test
    public void testBuildRecyclerView() {
        RecyclerView recyclerView =
                mBottomSheetView.findViewById(R.id.theme_collections_recycler_view);
        assertNotNull(recyclerView);

        // Verify LayoutManager
        assertTrue(recyclerView.getLayoutManager() instanceof GridLayoutManager);
        assertEquals(3, ((GridLayoutManager) recyclerView.getLayoutManager()).getSpanCount());

        // Verify Adapter
        assertTrue(recyclerView.getAdapter() instanceof NtpThemeCollectionsAdapter);
    }

    @Test
    public void testDestroy() {
        verify(mContextSpy).registerComponentCallbacks(mComponentCallbacksCaptor.capture());
        ComponentCallbacks componentCallbacks = mComponentCallbacksCaptor.getValue();

        View backButton = mBottomSheetView.findViewById(R.id.back_button);
        RecyclerView recyclerView =
                mBottomSheetView.findViewById(R.id.theme_collections_recycler_view);
        NtpThemeCollectionsAdapter adapter = (NtpThemeCollectionsAdapter) recyclerView.getAdapter();
        NtpThemeCollectionsAdapter adapterSpy = spy(adapter);
        mCoordinator.setNtpThemeCollectionsAdapterForTesting(adapterSpy);
        mCoordinator.setNtpSingleThemeCollectionCoordinatorForTesting(
                mNtpSingleThemeCollectionCoordinator);

        assertTrue(backButton.hasOnClickListeners());
        assertNotNull(mCoordinator.getNtpSingleThemeCollectionCoordinatorForTesting());

        mCoordinator.destroy();

        assertFalse(backButton.hasOnClickListeners());
        verify(adapterSpy).clearOnClickListeners();
        verify(mNtpSingleThemeCollectionCoordinator).destroy();
        verify(mContextSpy).unregisterComponentCallbacks(eq(componentCallbacks));
    }

    @Test
    public void testHandleThemeCollectionClick() {
        String histogramName = "NewTabPage.Customization.Theme.ThemeCollection.CollectionShow";

        // Create a list with one collection for this test.
        List<BackgroundCollection> collections = new ArrayList<>();
        collections.add(
                new BackgroundCollection(
                        TEST_COLLECTION_ID,
                        TEST_COLLECTION_TITLE,
                        JUnitTestGURLs.EXAMPLE_URL,
                        TEST_COLLECTION_HASH));
        mCoordinator =
                new NtpThemeCollectionsCoordinator(
                        mContextSpy,
                        mBottomSheetDelegate,
                        mProfile,
                        mNtpThemeCollectionManager,
                        mOnDailyUpdateCancelledCallback,
                        collections);
        ArgumentCaptor<View> viewCaptor = ArgumentCaptor.forClass(View.class);
        verify(mBottomSheetDelegate, times(2))
                .registerBottomSheetLayout(eq(THEME_COLLECTIONS), viewCaptor.capture());
        mBottomSheetView = viewCaptor.getValue();

        // Force the RecyclerView to create and bind views.
        RecyclerView recyclerView =
                mBottomSheetView.findViewById(R.id.theme_collections_recycler_view);
        recyclerView.measure(
                View.MeasureSpec.makeMeasureSpec(400, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.EXACTLY));
        recyclerView.layout(0, 0, 400, 800);

        // Get the view for the first item.
        View themeCollectionView = recyclerView.getChildAt(0);
        assertNotNull(themeCollectionView);

        // On first click, a new single theme coordinator is created and the sheet is shown.
        assertNull(mCoordinator.getNtpSingleThemeCollectionCoordinatorForTesting());
        when(mBottomSheetController.getSheetState())
                .thenReturn(BottomSheetController.SheetState.FULL);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, TEST_COLLECTION_HASH);
        themeCollectionView.performClick();
        assertNotNull(mCoordinator.getNtpSingleThemeCollectionCoordinatorForTesting());
        verify(mBottomSheetDelegate).showBottomSheet(eq(BottomSheetType.SINGLE_THEME_COLLECTION));
        histogramWatcher.assertExpected();

        // On second click, the existing single theme coordinator is updated and the sheet is shown.
        mCoordinator.setNtpSingleThemeCollectionCoordinatorForTesting(
                mNtpSingleThemeCollectionCoordinator);
        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, TEST_COLLECTION_HASH);
        themeCollectionView.performClick();
        verify(mNtpSingleThemeCollectionCoordinator)
                .updateThemeCollection(
                        eq(TEST_COLLECTION_ID),
                        eq(TEST_COLLECTION_TITLE),
                        eq(TEST_COLLECTION_HASH));
        verify(mBottomSheetDelegate, times(2))
                .showBottomSheet(eq(BottomSheetType.SINGLE_THEME_COLLECTION));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testConfigurationChanged() {
        verify(mContextSpy).registerComponentCallbacks(mComponentCallbacksCaptor.capture());
        ComponentCallbacks componentCallbacks = mComponentCallbacksCaptor.getValue();

        int initialScreenWidth = mCoordinator.getScreenWidthForTesting();

        // Test that screen width is updated on configuration change.
        Configuration newConfig = new Configuration(mContext.getResources().getConfiguration());
        newConfig.screenWidthDp = 1000;
        componentCallbacks.onConfigurationChanged(newConfig);

        int screenWidthAfterChange = mCoordinator.getScreenWidthForTesting();
        assertTrue(
                "Screen width should change on configuration change.",
                initialScreenWidth != screenWidthAfterChange);
        assertEquals(
                "Screen width should be updated to the new value.", 1000, screenWidthAfterChange);

        // Test that screen width is not updated if it is the same.
        componentCallbacks.onConfigurationChanged(newConfig);
        assertEquals(
                "Screen width should not change if configuration is the same.",
                screenWidthAfterChange,
                mCoordinator.getScreenWidthForTesting());

        // Test that screen width is updated again with a different value.
        newConfig.screenWidthDp = 500;
        componentCallbacks.onConfigurationChanged(newConfig);
        assertTrue(
                "Screen width should change on configuration change again.",
                screenWidthAfterChange != mCoordinator.getScreenWidthForTesting());
        assertEquals(
                "Screen width should be updated to the new value.",
                500,
                mCoordinator.getScreenWidthForTesting());
    }

    @Test
    public void testInitializeBottomSheetContent() {
        RecyclerView recyclerView =
                mBottomSheetView.findViewById(R.id.theme_collections_recycler_view);
        NtpThemeCollectionsAdapter adapter = (NtpThemeCollectionsAdapter) recyclerView.getAdapter();
        NtpThemeCollectionsAdapter adapterSpy = spy(adapter);
        mCoordinator.setNtpThemeCollectionsAdapterForTesting(adapterSpy);
        mCoordinator.setNtpSingleThemeCollectionCoordinatorForTesting(
                mNtpSingleThemeCollectionCoordinator);

        // Mock manager return values
        String collectionId = "test_id";
        GURL imageUrl = JUnitTestGURLs.URL_2;
        when(mNtpThemeCollectionManager.getSelectedThemeCollectionId()).thenReturn(collectionId);
        when(mNtpThemeCollectionManager.getSelectedThemeCollectionImageUrl()).thenReturn(imageUrl);

        // Test for THEME_COLLECTIONS
        mCoordinator.initializeBottomSheetContent(BottomSheetType.THEME_COLLECTIONS);
        verify(adapterSpy).setSelection(eq(collectionId), eq(imageUrl));

        // Test for SINGLE_THEME_COLLECTION
        mCoordinator.initializeBottomSheetContent(BottomSheetType.SINGLE_THEME_COLLECTION);
        verify(mNtpSingleThemeCollectionCoordinator).initializeBottomSheetContent();
    }
}
