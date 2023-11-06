// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;
import static org.robolectric.Robolectric.buildActivity;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.layouts.LayoutManagerAppUtils;
import org.chromium.chrome.browser.layouts.ManagedLayoutManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.autofill.payments.AutofillSaveCardUiInfo;
import org.chromium.components.autofill.payments.CardDetail;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link AutofillSaveCardBottomSheetBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@SmallTest
public final class AutofillSaveCardBottomSheetBridgeTest {
    private static final long MOCK_POINTER = 0xb00fb00f;

    private static final String HTTPS_EXAMPLE_TEST = "https://example.test";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public JniMocker mJniMocker = new JniMocker();

    private AutofillSaveCardBottomSheetBridge mAutofillSaveCardBottomSheetBridge;

    @Mock private AutofillSaveCardBottomSheetBridge.Natives mBridgeNatives;

    private Activity mActivity;
    private ShadowActivity mShadowActivity;

    private WindowAndroid mWindow;

    @Mock private Profile mProfile;

    private MockTabModel mTabModel;

    @Mock private ManagedBottomSheetController mBottomSheetController;

    @Mock private ManagedLayoutManager mLayoutManager;

    @Mock private AutofillSaveCardBottomSheetBridge.CoordinatorFactory mCoordinatorFactory;

    private AutofillSaveCardUiInfo mUiInfo;

    @Mock private AutofillSaveCardBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        mJniMocker.mock(AutofillSaveCardBottomSheetBridgeJni.TEST_HOOKS, mBridgeNatives);
        mUiInfo =
                new AutofillSaveCardUiInfo.Builder()
                        .withCardDetail(new CardDetail(/* iconId= */ 0, "label", "subLabel"))
                        .build();
        mActivity = buildActivity(Activity.class).create().get();
        mShadowActivity = shadowOf(mActivity);
        mWindow = new WindowAndroid(mActivity);
        mTabModel = new MockTabModel(mProfile, /* delegate= */ null);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        LayoutManagerAppUtils.attach(mWindow, mLayoutManager);
        mAutofillSaveCardBottomSheetBridge =
                new AutofillSaveCardBottomSheetBridge(
                        MOCK_POINTER, mWindow, mTabModel, mCoordinatorFactory);
    }

    private void setupCoordinatorFactory() {
        when(mCoordinatorFactory.create(
                        mActivity,
                        mBottomSheetController,
                        mLayoutManager,
                        mTabModel,
                        mUiInfo,
                        mAutofillSaveCardBottomSheetBridge))
                .thenReturn(mCoordinator);
    }

    @After
    public void tearDown() {
        BottomSheetControllerFactory.detach(mBottomSheetController);
        LayoutManagerAppUtils.detach(mLayoutManager);
        mWindow.destroy();
    }

    @Test
    public void testRequestShowContent_requestsShowOnCoordinator() {
        setupCoordinatorFactory();

        mAutofillSaveCardBottomSheetBridge.requestShowContent(mUiInfo);

        verify(mCoordinator).requestShowContent();
    }

    @Test
    public void testDestroy_callsCoordinatorDestroy() {
        setupCoordinatorFactory();
        mAutofillSaveCardBottomSheetBridge.requestShowContent(mUiInfo);

        mAutofillSaveCardBottomSheetBridge.destroy();

        verify(mCoordinator).destroy();
    }

    @Test
    public void testDestroy_whenCoordinatorHasNotBeenCreated() {
        mAutofillSaveCardBottomSheetBridge.destroy();

        verifyNoInteractions(mCoordinator);
    }

    @Test
    public void testDestroyTwice_destroysCoordinatorOnce() {
        setupCoordinatorFactory();
        mAutofillSaveCardBottomSheetBridge.requestShowContent(mUiInfo);

        mAutofillSaveCardBottomSheetBridge.destroy();
        mAutofillSaveCardBottomSheetBridge.destroy();

        verify(mCoordinator).destroy();
    }

    @Test
    public void testOnUiShown_callsNativeOnUiShown() {
        mAutofillSaveCardBottomSheetBridge.onUiShown();

        verify(mBridgeNatives).onUiShown(MOCK_POINTER);
    }

    @Test
    public void testOnUiShown_doesNotCallNative_afterDestroy() {
        mAutofillSaveCardBottomSheetBridge.destroy();

        mAutofillSaveCardBottomSheetBridge.onUiShown();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testOnUiAccepted_callsNativeOnUiAccepted() {
        mAutofillSaveCardBottomSheetBridge.onUiAccepted();

        verify(mBridgeNatives).onUiAccepted(MOCK_POINTER);
    }

    @Test
    public void testOnUiAccepted_doesNotCallNative_afterDestroy() {
        mAutofillSaveCardBottomSheetBridge.destroy();

        mAutofillSaveCardBottomSheetBridge.onUiAccepted();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testOnUiCanceled_callsNativeOnUiCanceled() {
        mAutofillSaveCardBottomSheetBridge.onUiCanceled();

        verify(mBridgeNatives).onUiCanceled(MOCK_POINTER);
    }

    @Test
    public void testOnUiCanceled_doesNotCallNative_afterDestroy() {
        mAutofillSaveCardBottomSheetBridge.destroy();

        mAutofillSaveCardBottomSheetBridge.onUiCanceled();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testOnUiIgnored_callsNativeOnUiIgnored() {
        mAutofillSaveCardBottomSheetBridge.onUiIgnored();

        verify(mBridgeNatives).onUiIgnored(MOCK_POINTER);
    }

    @Test
    public void testOnUiIgnored_doesNotCallNative_afterDestroy() {
        mAutofillSaveCardBottomSheetBridge.destroy();

        mAutofillSaveCardBottomSheetBridge.onUiIgnored();

        verifyNoInteractions(mBridgeNatives);
    }
}
