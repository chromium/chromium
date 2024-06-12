// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.os.Looper;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.commerce.PriceTrackingUtilsJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider.ButtonDataObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Unit test for {@link PriceTrackingButtonController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2})
public class PriceTrackingButtonControllerUnitTest {
    @Rule public JniMocker mJniMocker = new JniMocker();

    private Activity mActivity;
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private ObservableSupplier<BookmarkModel> mBookmarkModelSupplier;
    private ObservableSupplierImpl<Tab> mTabSupplier;
    private ObservableSupplierImpl<Boolean> mPriceTrackingStateSupplier;
    @Mock private Tab mMockTab;
    @Mock private Supplier<TabBookmarker> mMockTabBookmarkerSupplier;
    @Mock private TabBookmarker mMockTabBookmarker;
    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private ModalDialogManager mMockModalDialogManager;
    @Mock private SnackbarManager mMockSnackbarManager;
    @Mock private Profile mMockProfile;
    @Mock private BookmarkModel mMockBookmarkModel;
    @Mock PriceTrackingUtils.Natives mMockPriceTrackingUtilsJni;
    @Captor private ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(PriceTrackingUtilsJni.TEST_HOOKS, mMockPriceTrackingUtilsJni);
        mPriceTrackingStateSupplier = new ObservableSupplierImpl<>(false);
        mProfileSupplier = new ObservableSupplierImpl<>(mMockProfile);
        mBookmarkModelSupplier = new ObservableSupplierImpl<>(mMockBookmarkModel);
        mTabSupplier = new ObservableSupplierImpl<>(mMockTab);
        when(mMockTab.getContext()).thenReturn(mActivity);
        when(mMockTabBookmarkerSupplier.get()).thenReturn(mMockTabBookmarker);
    }

    private PriceTrackingButtonController createButtonController() {
        return new PriceTrackingButtonController(
                mActivity,
                mTabSupplier,
                mMockModalDialogManager,
                mMockBottomSheetController,
                mMockSnackbarManager,
                mMockTabBookmarkerSupplier,
                mProfileSupplier,
                mBookmarkModelSupplier,
                mPriceTrackingStateSupplier);
    }

    @Test
    public void testButtonShouldUpdateOnPriceTrackingChange() {
        ButtonDataObserver mockButtonObserver = mock(ButtonDataObserver.class);
        PriceTrackingButtonController priceTrackingButtonController = createButtonController();
        priceTrackingButtonController.addObserver(mockButtonObserver);

        ButtonSpec originalButtonSpec = priceTrackingButtonController.get(mMockTab).getButtonSpec();

        mPriceTrackingStateSupplier.set(true);
        verify(mockButtonObserver).buttonDataChanged(true);
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        ButtonSpec updatedButtonSpec = priceTrackingButtonController.get(mMockTab).getButtonSpec();

        Assert.assertNotEquals(
                originalButtonSpec.getContentDescription(),
                updatedButtonSpec.getContentDescription());
        Assert.assertNotEquals(originalButtonSpec.getDrawable(), updatedButtonSpec.getDrawable());
    }

    @Test
    public void testPriceTrackingButtonClick() {
        PriceTrackingButtonController priceTrackingButtonController = createButtonController();
        ButtonData buttonData = priceTrackingButtonController.get(mMockTab);

        buttonData.getButtonSpec().getOnClickListener().onClick(null);

        verify(mMockTabBookmarker).startOrModifyPriceTracking(mMockTab);
    }

    @Test
    public void testPriceTrackingButtonClick_shouldRemoveTrackingWhenAlreadyTracking() {
        View mockView = Mockito.mock(View.class);
        Resources mockResources = Mockito.mock(Resources.class);
        when(mockView.getResources()).thenReturn(mockResources);

        BookmarkId bookmarkId = new BookmarkId(1234, BookmarkType.NORMAL);
        when(mMockBookmarkModel.getUserBookmarkIdForTab(mMockTab)).thenReturn(bookmarkId);
        ArgumentCaptor<Callback<Boolean>> jniCallbackArgumentCaptor =
                ArgumentCaptor.forClass(Callback.class);

        PriceTrackingButtonController priceTrackingButtonController = createButtonController();
        mPriceTrackingStateSupplier.set(true);

        ButtonData buttonData = priceTrackingButtonController.get(mMockTab);

        buttonData.getButtonSpec().getOnClickListener().onClick(mockView);

        verify(mMockTabBookmarker, never()).startOrModifyPriceTracking(mMockTab);
        verify(mMockPriceTrackingUtilsJni)
                .setPriceTrackingStateForBookmark(
                        eq(mMockProfile),
                        eq(bookmarkId.getId()),
                        eq(false),
                        jniCallbackArgumentCaptor.capture(),
                        eq(false));

        jniCallbackArgumentCaptor.getValue().onResult(true);

        verify(mMockSnackbarManager).showSnackbar(any());
    }

    @Test
    public void testPriceTrackingButton_IsDisabledWhenBottomSheetAppears() {
        PriceTrackingButtonController priceTrackingButtonController = createButtonController();
        ButtonDataProvider.ButtonDataObserver buttonDataObserver =
                Mockito.mock(ButtonDataProvider.ButtonDataObserver.class);
        priceTrackingButtonController.addObserver(buttonDataObserver);
        ButtonData buttonData = priceTrackingButtonController.get(mMockTab);

        // The controller should have registered an observer to listen to bottom sheet events.
        verify(mMockBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());

        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);

        Assert.assertFalse(buttonData.isEnabled());
        verify(buttonDataObserver).buttonDataChanged(true);
    }

    @Test
    public void testPriceTrackingButton_IsReenabledWhenBottomSheetDismissed() {
        PriceTrackingButtonController priceTrackingButtonController = createButtonController();

        ButtonDataProvider.ButtonDataObserver buttonDataObserver =
                Mockito.mock(ButtonDataProvider.ButtonDataObserver.class);
        priceTrackingButtonController.addObserver(buttonDataObserver);
        ButtonData buttonData = priceTrackingButtonController.get(mMockTab);

        // The controller should have registered an observer to listen to bottom sheet events.
        verify(mMockBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());

        // Show bottom sheet to disable button.
        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);

        // Close the bottom sheet, button should be enabled again.
        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.NONE);

        // After the bottom sheet is closed the button should be enabled.
        Assert.assertTrue(buttonData.isEnabled());
        // We should have notified of changes twice (when disabled and when enabled again).
        verify(buttonDataObserver, times(2)).buttonDataChanged(true);
    }
}
