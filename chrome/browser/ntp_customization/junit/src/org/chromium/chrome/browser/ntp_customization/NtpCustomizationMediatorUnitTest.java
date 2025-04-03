// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.DISCOVER_FEED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LAYOUT_TO_DISPLAY;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_VIEW_DELEGATE;

import android.content.Context;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.feed.FeedFeatures;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridgeJni;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.Map;

/** Unit tests for {@link NtpCustomizationMediator} */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpCustomizationMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private NtpCustomizationBottomSheetContent mBottomSheetContent;
    @Mock private PropertyModel mViewFlipperPropertyModel;
    @Mock private PropertyModel mContainerPropertyModel;
    @Mock private PrefService mPrefService;
    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock private Profile mProfile;
    @Mock private ProfileProvider mProfileProvider;

    private NtpCustomizationMediator mMediator;
    private Map<Integer, Integer> mViewFlipperMap;
    private ListContainerViewDelegate mListDelegate;
    private Context mContext;
    private OneshotSupplierImpl<ProfileProvider> mSupplier;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mSupplier = new OneshotSupplierImpl<>();
        mMediator =
                new NtpCustomizationMediator(
                        mBottomSheetController,
                        mBottomSheetContent,
                        mViewFlipperPropertyModel,
                        mContainerPropertyModel,
                        mSupplier);
        mViewFlipperMap = mMediator.getViewFlipperMapForTesting();
        mListDelegate = mMediator.createListDelegate();
    }

    @Test
    @SmallTest
    public void testMaybeAddOneBottomSheetLayout() {
        // Verifies that the value and the key added to mViewFlipperMap are correct when adding
        // the key to mViewFlipperMap at the first time.
        mMediator.registerBottomSheetLayout(BottomSheetType.NTP_CARDS);

        assertEquals(1, mViewFlipperMap.size());
        assertEquals(0, (int) mViewFlipperMap.get(BottomSheetType.NTP_CARDS));

        // Verifies that adding the same key again won't change mViewFlipperMap and
        // mViewFlipperView.
        mMediator.registerBottomSheetLayout(BottomSheetType.NTP_CARDS);

        assertEquals(1, mViewFlipperMap.size());
        assertEquals(0, (int) mViewFlipperMap.get(BottomSheetType.NTP_CARDS));
    }

    @Test
    @SmallTest
    public void testMaybeAddTwoBottomSheetLayout() {
        // Verifies that the value and the key added to mViewFlipperMap are correct when first
        // adding the key.
        mMediator.registerBottomSheetLayout(BottomSheetType.NTP_CARDS);
        mMediator.registerBottomSheetLayout(BottomSheetType.MAIN);

        assertEquals(2, mViewFlipperMap.size());
        assertEquals(0, (int) mViewFlipperMap.get(BottomSheetType.NTP_CARDS));
        assertEquals(1, (int) mViewFlipperMap.get(BottomSheetType.MAIN));

        // Verifies that calling maybeAddBottomSheetLayout() with same type multiple times won't
        // change mViewFlipperMap and mViewFlipperView.
        mMediator.registerBottomSheetLayout(BottomSheetType.NTP_CARDS);
        mMediator.registerBottomSheetLayout(BottomSheetType.NTP_CARDS);
        mMediator.registerBottomSheetLayout(BottomSheetType.MAIN);
        mMediator.registerBottomSheetLayout(BottomSheetType.NTP_CARDS);
        mMediator.registerBottomSheetLayout(BottomSheetType.MAIN);

        assertEquals(2, mViewFlipperMap.size());
        assertEquals(0, (int) mViewFlipperMap.get(BottomSheetType.NTP_CARDS));
        assertEquals(1, (int) mViewFlipperMap.get(BottomSheetType.MAIN));
    }

    @Test
    @SmallTest
    public void testRequestShowContentCalledOnlyOnce() {
        // Verifies that requestShowContent() is called only when showBottomSheet() is called at the
        // first time.
        mViewFlipperMap.put(BottomSheetType.NTP_CARDS, 5);

        mMediator.showBottomSheet(BottomSheetType.NTP_CARDS);

        verify(mBottomSheetController).requestShowContent(eq(mBottomSheetContent), eq(true));
        clearInvocations(mBottomSheetController);

        mViewFlipperMap.put(11, 101);
        mMediator.showBottomSheet(11);
        verify(mBottomSheetController, never())
                .requestShowContent(eq(mBottomSheetContent), eq(true));
    }

    @Test
    @SmallTest
    public void testShowBottomSheet() {
        // Verifies that setDisplayChild() is called and mCurrentBottomSheet is set correctly.
        @BottomSheetType int bottomSheetType = NTP_CARDS;
        int viewFlipperIndex = 2;
        mViewFlipperMap.put(bottomSheetType, viewFlipperIndex);

        mMediator.showBottomSheet(bottomSheetType);

        verify(mViewFlipperPropertyModel).set(eq(LAYOUT_TO_DISPLAY), eq(viewFlipperIndex));
        assertEquals(bottomSheetType, (int) mMediator.getCurrentBottomSheetForTesting());
    }

    @Test
    @SmallTest
    public void testMetricsInShowBottomSheet() {
        String histogramName = "NewTabPage.Customization.BottomSheet.Shown";
        @BottomSheetType int[] bottomSheetTypes = new int[] {NTP_CARDS, MAIN};

        for (int i = 0; i < bottomSheetTypes.length; i++) {
            @BottomSheetType int type = bottomSheetTypes[i];
            mViewFlipperMap.put(type, i);
            mMediator.showBottomSheet(type);

            HistogramWatcher histogramWatcher =
                    HistogramWatcher.newSingleRecordWatcher(histogramName, type);
            NtpCustomizationMetricsUtils.recordBottomSheetShown(type);
            histogramWatcher.assertExpected();
        }
    }

    @Test
    @SmallTest
    public void testShowBottomSheetAssertionError() {
        // Verifies that AssertionError will be raised if maybeAddBottomSheetLayout() is not
        // called before calling showBottomSheet()
        assertThrows(AssertionError.class, () -> mMediator.showBottomSheet(BottomSheetType.MAIN));
        assertThrows(
                AssertionError.class, () -> mMediator.showBottomSheet(BottomSheetType.NTP_CARDS));
    }

    @Test
    @SmallTest
    public void testBackPressNotInitialized() {
        // Verifies that backPressOnCurrentBottomSheet() will do nothing if mCurrentBottomSheet is
        // not initialized.
        mMediator.backPressOnCurrentBottomSheet();

        verify(mBottomSheetController, never())
                .hideContent(any(BottomSheetContent.class), anyBoolean());
        verify(mViewFlipperPropertyModel, never()).set(eq(LAYOUT_TO_DISPLAY), anyInt());
    }

    @Test
    @SmallTest
    public void testBackPressOnMainBottomSheet() {
        mMediator.setCurrentBottomSheetForTesting(BottomSheetType.MAIN);

        mMediator.backPressOnCurrentBottomSheet();

        // Verifies that hideContent() is called and mCurrentBottomSheet is set to null.
        verify(mBottomSheetController).hideContent(eq(mBottomSheetContent), eq(true));
        assertNull(mMediator.getCurrentBottomSheetForTesting());

        // Verifies that showBottomSheet() is not called.
        verify(mViewFlipperPropertyModel, never()).set(eq(LAYOUT_TO_DISPLAY), anyInt());
    }

    @Test
    @SmallTest
    public void testBackPressOnNtpCardsBottomSheet() {
        mViewFlipperMap.put(BottomSheetType.MAIN, 10);
        mMediator.setCurrentBottomSheetForTesting(BottomSheetType.NTP_CARDS);

        mMediator.backPressOnCurrentBottomSheet();

        // Verifies that hideContent() is not called and showBottomSheet() is called to change the
        // value of mCurrentBottomSheet and to set the value of mPropertyModel.
        verify(mBottomSheetController, never())
                .hideContent(any(BottomSheetContent.class), anyBoolean());
        assertEquals(BottomSheetType.MAIN, (int) mMediator.getCurrentBottomSheetForTesting());
        verify(mViewFlipperPropertyModel).set(eq(LAYOUT_TO_DISPLAY), eq(10));
    }

    @Test
    @SmallTest
    public void testDestroy() {
        // Verifies mViewFlipperMap is cleared.
        mViewFlipperMap.put(BottomSheetType.NTP_CARDS, 9);
        mViewFlipperMap.put(BottomSheetType.MAIN, 10);

        assertEquals(2, mViewFlipperMap.size());
        mMediator.destroy();
        assertEquals(0, mViewFlipperMap.size());

        // Verifies mTypeToListenerMap is cleared.
        Map<Integer, View.OnClickListener> typeToListenerMap =
                mMediator.getTypeToListenersForTesting();
        typeToListenerMap.put(BottomSheetType.NTP_CARDS, view -> {});
        assertEquals(1, typeToListenerMap.size());
        mMediator.destroy();
        assertEquals(0, typeToListenerMap.size());
    }

    @Test
    @SmallTest
    public void testBottomSheetObserver() {
        // Verifies the supplier is set to true when the sheet opens.
        BottomSheetObserver observer = mMediator.getBottomSheetObserverForTesting();
        observer.onSheetOpened(0);
        verify(mBottomSheetContent).onSheetOpened();

        // Verifies the supplier is set to false when the sheet closes and the observer is removed.
        observer.onSheetClosed(3); // Closes the sheet by clicking the trim.
        verify(mBottomSheetContent).onSheetClosed();
        verify(mBottomSheetController).removeObserver(eq(observer));
        clearInvocations(mBottomSheetContent);
        clearInvocations(mBottomSheetController);

        observer.onSheetClosed(0); // Closes the sheet by clicking the system back button.
        verify(mBottomSheetContent).onSheetClosed();
        verify(mBottomSheetController).removeObserver(eq(observer));
    }

    @Test
    @SmallTest
    public void testListContainerViewDelegate() {
        // Verifies the subtitle of the "feeds" list item is "On" and is null for other list item.
        assertEquals("On", mListDelegate.getListItemSubtitle(DISCOVER_FEED, mContext));
        assertNull(mListDelegate.getListItemSubtitle(MAIN, mContext));

        // Verifies the listener returned from the delegate is in mTypeToListeners map.
        Map<Integer, View.OnClickListener> typeToListenerMap =
                mMediator.getTypeToListenersForTesting();
        View.OnClickListener ntpListener = mock(View.OnClickListener.class);
        View.OnClickListener feedsListener = mock(View.OnClickListener.class);
        typeToListenerMap.put(NTP_CARDS, ntpListener);
        typeToListenerMap.put(DISCOVER_FEED, feedsListener);
        assertEquals(ntpListener, mListDelegate.getListener(NTP_CARDS));
        assertEquals(feedsListener, mListDelegate.getListener(DISCOVER_FEED));
    }

    @Test
    @SmallTest
    public void testRegisterClickListener() {
        View.OnClickListener listener = mock(View.OnClickListener.class);
        mMediator.registerClickListener(10, listener);
        assertEquals(listener, mMediator.getTypeToListenersForTesting().get(10));
    }

    @Test
    @SmallTest
    public void testRenderContent() {
        mMediator.renderListContent();
        verify(mContainerPropertyModel)
                .set(eq(LIST_CONTAINER_VIEW_DELEGATE), any(ListContainerViewDelegate.class));
    }

    @Test
    @SmallTest
    public void testBuildListContentWhenProfileIsNotReady() {
        List<Integer> listContent = mMediator.buildListContent();
        assertEquals(List.of(NTP_CARDS), listContent);
    }

    @Test
    @SmallTest
    public void testBuildListContent() {
        // Verifies that "DISCOVER_FEED" misses from the result list if and if only isFeedEnabled()
        // is false;
        FeedServiceBridgeJni.setInstanceForTesting(mFeedServiceBridgeJniMock);
        mSupplier.set(mProfileProvider);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        FeedFeatures.setFakePrefsForTest(mPrefService);

        // Mock dependencies to enable FeedFeatures.isFeedEnabled(profile) to return true.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(true);
        when(mFeedServiceBridgeJniMock.isEnabled()).thenReturn(true);

        assertTrue(FeedFeatures.isFeedEnabled(mProfile));
        assertEquals(List.of(NTP_CARDS, DISCOVER_FEED), mMediator.buildListContent());

        // Mock dependencies to enable FeedFeatures.isFeedEnabled(profile) to return false.
        when(mPrefService.getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE)).thenReturn(false);

        assertFalse(FeedFeatures.isFeedEnabled(mProfile));
        assertEquals(List.of(NTP_CARDS), mMediator.buildListContent());
    }
}
