// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
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
import android.widget.ImageView;

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
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeBridge;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link NtpThemeCollectionsCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeCollectionsCoordinatorUnitTest {

    private static final String TEST_COLLECTION_ID = "Test Collection Id";
    private static final String TEST_COLLECTION_TITLE = "Test Collection";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private NtpSingleThemeCollectionCoordinator mNtpSingleThemeCollectionCoordinator;
    @Mock private NtpThemeBridge.Natives mNtpThemeBridgeJniMock;
    @Mock private Runnable mOnThemeImageSelectedCallback;
    @Captor private ArgumentCaptor<Callback<Object[]>> mCallbackCaptor;
    @Captor private ArgumentCaptor<ComponentCallbacks> mComponentCallbacksCaptor;

    private NtpThemeCollectionsCoordinator mCoordinator;
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

        NtpThemeBridgeJni.setInstanceForTesting(mNtpThemeBridgeJniMock);
        when(mNtpThemeBridgeJniMock.init(mProfile)).thenReturn(1L);
        when(mBottomSheetDelegate.getBottomSheetController()).thenReturn(mBottomSheetController);

        mCoordinator =
                new NtpThemeCollectionsCoordinator(
                        mContextSpy, mBottomSheetDelegate, mProfile, mOnThemeImageSelectedCallback);

        ArgumentCaptor<View> viewCaptor = ArgumentCaptor.forClass(View.class);
        verify(mBottomSheetDelegate)
                .registerBottomSheetLayout(eq(THEME_COLLECTIONS), viewCaptor.capture());
        mBottomSheetView = viewCaptor.getValue();
    }

    @Test
    public void testConstructor() {
        assertNotNull(mBottomSheetView);
        verify(mNtpThemeBridgeJniMock).getBackgroundCollections(eq(1L), mCallbackCaptor.capture());

        RecyclerView recyclerView =
                mBottomSheetView.findViewById(R.id.theme_collections_recycler_view);
        NtpThemeCollectionsAdapter adapter = (NtpThemeCollectionsAdapter) recyclerView.getAdapter();
        NtpThemeCollectionsAdapter adapterSpy = spy(adapter);
        mCoordinator.setNtpThemeCollectionsAdapterForTesting(adapterSpy);

        Object[] collections = new Object[0];
        mCallbackCaptor.getValue().onResult(collections);

        verify(mBottomSheetController).expandSheet();
        verify(adapterSpy).setSelection(any(), any());
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
    public void testLearnMoreButton() {
        View learnMoreButton = mBottomSheetView.findViewById(R.id.learn_more_button);
        assertNotNull(learnMoreButton);
        assertTrue(learnMoreButton.hasOnClickListeners());
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
        ImageView learnMoreButton = mBottomSheetView.findViewById(R.id.learn_more_button);
        RecyclerView recyclerView =
                mBottomSheetView.findViewById(R.id.theme_collections_recycler_view);
        NtpThemeCollectionsAdapter adapter = (NtpThemeCollectionsAdapter) recyclerView.getAdapter();
        NtpThemeCollectionsAdapter adapterSpy = spy(adapter);
        mCoordinator.setNtpThemeCollectionsAdapterForTesting(adapterSpy);
        mCoordinator.setNtpSingleThemeCollectionCoordinatorForTesting(
                mNtpSingleThemeCollectionCoordinator);

        assertTrue(backButton.hasOnClickListeners());
        assertTrue(learnMoreButton.hasOnClickListeners());
        assertNotNull(mCoordinator.getNtpSingleThemeCollectionCoordinatorForTesting());

        mCoordinator.destroy();

        assertFalse(backButton.hasOnClickListeners());
        assertFalse(learnMoreButton.hasOnClickListeners());
        verify(adapterSpy).clearOnClickListeners();
        verify(mNtpSingleThemeCollectionCoordinator).destroy();
        verify(mNtpThemeBridgeJniMock).destroy(eq(1L));
        verify(mContextSpy).unregisterComponentCallbacks(eq(componentCallbacks));
    }

    @Test
    public void testHandleThemeCollectionClick() {
        // Populate mThemeCollectionsList in the coordinator.
        verify(mNtpThemeBridgeJniMock).getBackgroundCollections(eq(1L), mCallbackCaptor.capture());
        Object[] collections = new Object[1];
        collections[0] =
                new BackgroundCollection(
                        TEST_COLLECTION_ID, TEST_COLLECTION_TITLE, JUnitTestGURLs.EXAMPLE_URL);
        mCallbackCaptor.getValue().onResult(collections);
        verify(mBottomSheetController).expandSheet();

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
        themeCollectionView.performClick();
        assertNotNull(mCoordinator.getNtpSingleThemeCollectionCoordinatorForTesting());
        verify(mBottomSheetDelegate).showBottomSheet(eq(BottomSheetType.SINGLE_THEME_COLLECTION));

        // On second click, the existing single theme coordinator is updated and the sheet is shown.
        mCoordinator.setNtpSingleThemeCollectionCoordinatorForTesting(
                mNtpSingleThemeCollectionCoordinator);
        themeCollectionView.performClick();
        verify(mNtpSingleThemeCollectionCoordinator)
                .updateThemeCollection(
                        eq(TEST_COLLECTION_ID),
                        eq(TEST_COLLECTION_TITLE),
                        eq(BottomSheetController.SheetState.FULL));
        verify(mBottomSheetDelegate, times(2))
                .showBottomSheet(eq(BottomSheetType.SINGLE_THEME_COLLECTION));
    }

    @Test
    public void testOnThemeSelectionChanged() {
        RecyclerView recyclerView =
                mBottomSheetView.findViewById(R.id.theme_collections_recycler_view);
        NtpThemeCollectionsAdapter adapter = (NtpThemeCollectionsAdapter) recyclerView.getAdapter();
        NtpThemeCollectionsAdapter adapterSpy = spy(adapter);
        mCoordinator.setNtpThemeCollectionsAdapterForTesting(adapterSpy);

        NtpThemeBridge ntpThemeBridge = mCoordinator.getNtpThemeBridgeForTesting();

        String collectionId = "test_id";
        GURL imageUrl = JUnitTestGURLs.URL_2;
        ntpThemeBridge.setSelectedTheme(collectionId, imageUrl);

        verify(adapterSpy).setSelection(eq(collectionId), eq(imageUrl));
    }

    @Test
    public void testClearThemeSelection() {
        RecyclerView recyclerView =
                mBottomSheetView.findViewById(R.id.theme_collections_recycler_view);
        NtpThemeCollectionsAdapter adapter = (NtpThemeCollectionsAdapter) recyclerView.getAdapter();
        NtpThemeCollectionsAdapter adapterSpy = spy(adapter);
        mCoordinator.setNtpThemeCollectionsAdapterForTesting(adapterSpy);

        mCoordinator.clearThemeCollectionSelection();

        // Verify that the adapter's selection is cleared via the listener callback.
        verify(adapterSpy).setSelection(eq(null), eq(null));
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
