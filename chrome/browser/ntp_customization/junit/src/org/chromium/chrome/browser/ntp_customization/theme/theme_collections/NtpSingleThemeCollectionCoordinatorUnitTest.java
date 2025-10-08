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
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.SINGLE_THEME_COLLECTION;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_COLLECTIONS;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
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
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeBridge;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeBridge.ThemeCollectionSelectionListener;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link NtpSingleThemeCollectionCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpSingleThemeCollectionCoordinatorUnitTest {

    private static final String TEST_COLLECTION_ID = "Test Collection Id";
    private static final String TEST_COLLECTION_TITLE = "Test Collection";
    private static final String NEW_TEST_COLLECTION_ID = "New Test Collection Id";
    private static final String NEW_TEST_COLLECTION_TITLE = "New Test Collection";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private NtpThemeBridge mNtpThemeBridge;
    @Mock private ImageFetcher mImageFetcher;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Runnable mOnThemeImageSelectedCallback;
    @Captor private ArgumentCaptor<Callback<List<CollectionImage>>> mCallbackCaptor;
    @Captor private ArgumentCaptor<ThemeCollectionSelectionListener> mListenerCaptor;
    @Captor private ArgumentCaptor<ComponentCallbacks> mComponentCallbacksCaptor;

    private NtpSingleThemeCollectionCoordinator mCoordinator;
    private Context mContext;
    private Context mContextSpy;
    private View mBottomSheetView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mContextSpy = spy(mContext);

        when(mBottomSheetDelegate.getBottomSheetController()).thenReturn(mBottomSheetController);

        mCoordinator =
                new NtpSingleThemeCollectionCoordinator(
                        mContextSpy,
                        mBottomSheetDelegate,
                        mNtpThemeBridge,
                        mImageFetcher,
                        TEST_COLLECTION_ID,
                        TEST_COLLECTION_TITLE,
                        SheetState.FULL,
                        mOnThemeImageSelectedCallback);

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
        verify(mNtpThemeBridge)
                .getBackgroundImages(eq(TEST_COLLECTION_ID), mCallbackCaptor.capture());
        verify(mNtpThemeBridge).addListener(any());

        NtpThemeCollectionsAdapter adapter = mCoordinator.getNtpThemeCollectionsAdapterForTesting();
        NtpThemeCollectionsAdapter adapterSpy = spy(adapter);
        mCoordinator.setNtpThemeCollectionsAdapterForTesting(adapterSpy);

        List<CollectionImage> images = new ArrayList<>();
        images.add(
                new CollectionImage(
                        TEST_COLLECTION_ID,
                        JUnitTestGURLs.URL_1,
                        JUnitTestGURLs.URL_1,
                        new ArrayList<>(),
                        JUnitTestGURLs.URL_1));
        mCallbackCaptor.getValue().onResult(images);

        verify(adapterSpy).setItems(eq(images));
        verify(adapterSpy).setSelection(any(), any());
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
        verify(mContextSpy).registerComponentCallbacks(mComponentCallbacksCaptor.capture());
        ComponentCallbacks componentCallbacks = mComponentCallbacksCaptor.getValue();

        View backButton = mBottomSheetView.findViewById(R.id.back_button);
        ImageView learnMoreButton = mBottomSheetView.findViewById(R.id.learn_more_button);
        NtpThemeCollectionsAdapter adapter = mCoordinator.getNtpThemeCollectionsAdapterForTesting();
        NtpThemeCollectionsAdapter adapterSpy = spy(adapter);
        mCoordinator.setNtpThemeCollectionsAdapterForTesting(adapterSpy);

        assertTrue(backButton.hasOnClickListeners());
        assertTrue(learnMoreButton.hasOnClickListeners());

        mCoordinator.destroy();

        assertFalse(backButton.hasOnClickListeners());
        assertFalse(learnMoreButton.hasOnClickListeners());
        verify(adapterSpy).clearOnClickListeners();
        verify(mNtpThemeBridge).removeListener(any());
        verify(mContextSpy).unregisterComponentCallbacks(eq(componentCallbacks));
    }

    @Test
    public void testUpdateThemeCollection() {
        verify(mNtpThemeBridge).getBackgroundImages(eq(TEST_COLLECTION_ID), any());
        TextView title = mCoordinator.getTitleForTesting();
        NtpThemeCollectionsAdapter adapter = mCoordinator.getNtpThemeCollectionsAdapterForTesting();
        NtpThemeCollectionsAdapter adapterSpy = spy(adapter);
        mCoordinator.setNtpThemeCollectionsAdapterForTesting(adapterSpy);

        // Title should not be updated with the same title.
        mCoordinator.updateThemeCollection(
                TEST_COLLECTION_ID, TEST_COLLECTION_TITLE, SheetState.FULL);
        // `getBackgroundImages` is called once in `setUp()`. No new call should be made.
        verify(mNtpThemeBridge, times(1)).getBackgroundImages(any(), any());
        verify(adapterSpy, times(0)).setItems(any());

        // Title should be updated with a new title.
        mCoordinator.updateThemeCollection(
                NEW_TEST_COLLECTION_ID, NEW_TEST_COLLECTION_TITLE, SheetState.FULL);
        assertEquals(NEW_TEST_COLLECTION_TITLE, title.getText().toString());
        verify(mNtpThemeBridge)
                .getBackgroundImages(eq(NEW_TEST_COLLECTION_ID), mCallbackCaptor.capture());

        List<CollectionImage> images = new ArrayList<>();
        images.add(
                new CollectionImage(
                        NEW_TEST_COLLECTION_ID,
                        JUnitTestGURLs.URL_1,
                        JUnitTestGURLs.URL_1,
                        new ArrayList<>(),
                        JUnitTestGURLs.URL_1));
        mCallbackCaptor.getValue().onResult(images);
        verify(adapterSpy).setItems(eq(images));
    }

    @Test
    public void testFetchImagesForCollection_expandSheet() {
        // Case 1: isInitiative is true.
        verify(mNtpThemeBridge)
                .getBackgroundImages(eq(TEST_COLLECTION_ID), mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onResult(new ArrayList<>());
        verify(mBottomSheetController).expandSheet();

        // Case 2: previous bottom sheet state is HALF.
        mCoordinator.updateThemeCollection(
                NEW_TEST_COLLECTION_ID, NEW_TEST_COLLECTION_TITLE, SheetState.HALF);
        verify(mNtpThemeBridge)
                .getBackgroundImages(eq(NEW_TEST_COLLECTION_ID), mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onResult(new ArrayList<>());
        verify(mBottomSheetController, times(2)).expandSheet();

        // Case 3: previous bottom sheet state is not HALF and not initiative.
        mCoordinator.updateThemeCollection(
                TEST_COLLECTION_ID, TEST_COLLECTION_TITLE, SheetState.FULL);
        verify(mNtpThemeBridge, times(2))
                .getBackgroundImages(eq(TEST_COLLECTION_ID), mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onResult(new ArrayList<>());
        // expandSheet should still be called only twice from previous cases.
        verify(mBottomSheetController, times(2)).expandSheet();
    }

    @Test
    public void testHandleThemeCollectionImageClick() {
        // Provide data to the adapter.
        verify(mNtpThemeBridge)
                .getBackgroundImages(eq(TEST_COLLECTION_ID), mCallbackCaptor.capture());
        List<CollectionImage> images = new ArrayList<>();
        CollectionImage imageToClick =
                new CollectionImage(
                        TEST_COLLECTION_ID,
                        JUnitTestGURLs.URL_1,
                        JUnitTestGURLs.URL_1,
                        new ArrayList<>(),
                        JUnitTestGURLs.URL_1);
        images.add(imageToClick);
        mCallbackCaptor.getValue().onResult(images);

        // Force the RecyclerView to create and bind views.
        RecyclerView recyclerView =
                mBottomSheetView.findViewById(R.id.single_theme_collection_recycler_view);
        recyclerView.measure(
                View.MeasureSpec.makeMeasureSpec(400, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.EXACTLY));
        recyclerView.layout(0, 0, 400, 800);

        // Get the view for the first item.
        View themeCollectionView = recyclerView.getChildAt(0);
        assertNotNull(themeCollectionView);

        themeCollectionView.performClick();
        verify(mNtpThemeBridge)
                .setSelectedTheme(eq(imageToClick.collectionId), eq(imageToClick.imageUrl));
        verify(mOnThemeImageSelectedCallback).run();
    }

    @Test
    public void testOnThemeSelectionChanged() {
        NtpThemeCollectionsAdapter adapter = mCoordinator.getNtpThemeCollectionsAdapterForTesting();
        NtpThemeCollectionsAdapter adapterSpy = spy(adapter);
        mCoordinator.setNtpThemeCollectionsAdapterForTesting(adapterSpy);

        verify(mNtpThemeBridge).addListener(mListenerCaptor.capture());
        ThemeCollectionSelectionListener listener = mListenerCaptor.getValue();

        String collectionId = "test_id";
        GURL imageUrl = JUnitTestGURLs.URL_2;
        listener.onThemeCollectionSelectionChanged(collectionId, imageUrl);

        verify(adapterSpy).setSelection(eq(collectionId), eq(imageUrl));
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
}
