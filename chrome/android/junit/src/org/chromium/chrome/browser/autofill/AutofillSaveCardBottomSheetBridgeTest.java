// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Robolectric.buildActivity;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.autofill.payments.AutofillSaveCardUiInfo;
import org.chromium.components.autofill.payments.CardDetail;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link AutofillSaveCardBottomSheetBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@SmallTest
public final class AutofillSaveCardBottomSheetBridgeTest {
    private static final long MOCK_POINTER = 0xb00fb00f;

    private static final String HTTPS_EXAMPLE_TEST = "https://example.test";

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    private AutofillSaveCardBottomSheetBridge mAutofillSaveCardBottomSheetBridge;

    @Mock
    private AutofillSaveCardBottomSheetBridge.Natives mBridgeNatives;

    private ShadowActivity mShadowActivity;

    private WindowAndroid mWindow;

    @Mock
    private ManagedBottomSheetController mBottomSheetController;

    private AutofillSaveCardUiInfo mUiInfo;

    @Before
    public void setUp() {
        mJniMocker.mock(AutofillSaveCardBottomSheetBridgeJni.TEST_HOOKS, mBridgeNatives);
        mUiInfo = new AutofillSaveCardUiInfo.Builder()
                          .withCardDetail(new CardDetail(/*iconId*/ 0, "label", "subLabel"))
                          .build();
        Activity activity = buildActivity(Activity.class).create().get();
        mShadowActivity = shadowOf(activity);
        mWindow = new WindowAndroid(activity);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        mAutofillSaveCardBottomSheetBridge =
                new AutofillSaveCardBottomSheetBridge(MOCK_POINTER, mWindow);
    }

    @After
    public void tearDown() {
        BottomSheetControllerFactory.detach(mBottomSheetController);
        mWindow.destroy();
    }

    @Test
    public void testRequestShowContent_callsControllerRequestShowContent() {
        requestShowContent(/*didShow=*/true);

        verify(mBottomSheetController)
                .requestShowContent(any(AutofillSaveCardBottomSheetContent.class),
                        /* animate= */ eq(true));
    }

    @Test
    public void testRequestShowContent_callsNativeOnUiShown_whenShown() {
        requestShowContent(/*didShow=*/true);

        verify(mBridgeNatives).onUiShown(MOCK_POINTER);
    }

    @Test
    public void testRequestShowContent_doesNotCallNativeOnUiShown_whenNotShown() {
        requestShowContent(/*didShow=*/false);

        verify(mBridgeNatives, never()).onUiShown(MOCK_POINTER);
    }

    @Test
    public void testDidClickLegalMessageUrl_launchesCustomTabIntent() {
        AutofillSaveCardBottomSheetContent bottomSheetContent =
                requestShowContent(/*didShow=*/true);

        bottomSheetContent.getDelegate().didClickLegalMessageUrl(HTTPS_EXAMPLE_TEST);

        Intent intent = mShadowActivity.getNextStartedActivity();
        assertThat(intent.getData(), equalTo(Uri.parse(HTTPS_EXAMPLE_TEST)));
        assertThat(intent.getAction(), equalTo(Intent.ACTION_VIEW));
        assertThat(intent.getExtras().get(CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE),
                equalTo(CustomTabsIntent.SHOW_PAGE_TITLE));
    }

    @Test
    public void testDidClickConfirm_hidesBottomSheetContent() {
        AutofillSaveCardBottomSheetContent bottomSheetContent =
                requestShowContent(/*didShow=*/true);

        bottomSheetContent.getDelegate().didClickConfirm();

        verify(mBottomSheetController)
                .hideContent(bottomSheetContent, /*animate=*/true,
                        StateChangeReason.INTERACTION_COMPLETE);
    }

    @Test
    public void testDidClickConfirm_callsNativeOnUiAccepted() {
        AutofillSaveCardBottomSheetContent bottomSheetContent =
                requestShowContent(/*didShow=*/true);

        bottomSheetContent.getDelegate().didClickConfirm();

        verify(mBridgeNatives).onUiAccepted(MOCK_POINTER);
    }

    @Test
    public void testDidClickConfirm_doesNotCallBridgeNatives_whenAlreadyClickedConfirm() {
        AutofillSaveCardBottomSheetContent bottomSheetContent =
                requestShowContent(/*didShow=*/true);
        bottomSheetContent.getDelegate().didClickConfirm();
        clearInvocations(mBridgeNatives);

        bottomSheetContent.getDelegate().didClickConfirm();

        verify(mBridgeNatives, never()).onUiAccepted(MOCK_POINTER);
    }

    @Test
    public void testDidClickConfirm_doesNotCallBridgeNatives_afterDestroy() {
        requestShowContent(/*didShow=*/true);
        mAutofillSaveCardBottomSheetBridge.destroy();

        mAutofillSaveCardBottomSheetBridge.didClickConfirm();

        verify(mBridgeNatives, never())
                .onUiAccepted(/*nativeAutofillSaveCardBottomSheetBridge=*/anyLong());
    }

    @Test
    public void testDidClickCancel_hidesBottomSheetContent() {
        AutofillSaveCardBottomSheetContent bottomSheetContent =
                requestShowContent(/*didShow=*/true);

        bottomSheetContent.getDelegate().didClickCancel();

        verify(mBottomSheetController)
                .hideContent(bottomSheetContent, /*animate=*/true,
                        StateChangeReason.INTERACTION_COMPLETE);
    }

    @Test
    public void testDidClickCancel_callsNativeOnUiCanceled() {
        AutofillSaveCardBottomSheetContent bottomSheetContent =
                requestShowContent(/*didShow=*/true);

        bottomSheetContent.getDelegate().didClickCancel();

        verify(mBridgeNatives).onUiCanceled(MOCK_POINTER);
    }

    @Test
    public void testDidClickCancel_doesNotCallBridgeNatives_whenAlreadyClickedCancel() {
        AutofillSaveCardBottomSheetContent bottomSheetContent =
                requestShowContent(/*didShow=*/true);
        bottomSheetContent.getDelegate().didClickCancel();
        clearInvocations(mBridgeNatives);

        bottomSheetContent.getDelegate().didClickCancel();

        verify(mBridgeNatives, never())
                .onUiIgnored(/*nativeAutofillSaveCardBottomSheetBridge=*/anyLong());
    }

    @Test
    public void testDidClickCancel_doesNotCallBridgeNatives_afterDestroy() {
        requestShowContent(/*didShow=*/true);
        mAutofillSaveCardBottomSheetBridge.destroy();

        mAutofillSaveCardBottomSheetBridge.didClickCancel();

        verify(mBridgeNatives, never())
                .onUiCanceled(/*nativeAutofillSaveCardBottomSheetBridge=*/anyLong());
    }

    @Test
    public void testSheetClosedOnBackPress_callsNativeOnUiCanceled() {
        requestShowContent(/*didShow=*/true);

        mAutofillSaveCardBottomSheetBridge.onSheetClosed(StateChangeReason.BACK_PRESS);

        verify(mBridgeNatives).onUiCanceled(MOCK_POINTER);
    }

    @Test
    public void testSheetClosedOnSwipe_callsNativeOnUiCanceled() {
        requestShowContent(/*didShow=*/true);

        mAutofillSaveCardBottomSheetBridge.onSheetClosed(StateChangeReason.SWIPE);

        verify(mBridgeNatives).onUiCanceled(MOCK_POINTER);
    }

    @Test
    public void testSheetClosedOnTapScrim_callsNativeOnUiCanceled() {
        requestShowContent(/*didShow=*/true);

        mAutofillSaveCardBottomSheetBridge.onSheetClosed(StateChangeReason.TAP_SCRIM);

        verify(mBridgeNatives).onUiCanceled(MOCK_POINTER);
    }

    @Test
    public void testSheetClosedOnInteractionComplete_doesNotCallOnUiCanceledNorOnUiAccepted() {
        requestShowContent(/*didShow=*/true);

        mAutofillSaveCardBottomSheetBridge.onSheetClosed(StateChangeReason.INTERACTION_COMPLETE);

        verify(mBridgeNatives, never())
                .onUiCanceled(/*nativeAutofillSaveCardBottomSheetBridge=*/anyLong());
        verify(mBridgeNatives, never())
                .onUiAccepted(/*nativeAutofillSaveCardBottomSheetBridge=*/anyLong());
    }

    @Test
    public void testSheetClosedWithoutUserInteraction_callsNativeOnUiIgnored() {
        requestShowContent(/*didShow=*/true);

        mAutofillSaveCardBottomSheetBridge.onSheetClosed(StateChangeReason.PROMOTE_TAB);

        verify(mBridgeNatives).onUiIgnored(MOCK_POINTER);
    }

    @Test
    public void testDestroy_stopsObservingTheBottomSheetController() {
        mAutofillSaveCardBottomSheetBridge.destroy();

        verify(mBottomSheetController).removeObserver(mAutofillSaveCardBottomSheetBridge);
    }

    @Test
    public void testDestroy_callsNativeOnUiIgnored_withoutOtherInteraction() {
        requestShowContent(/*didShow=*/true);

        mAutofillSaveCardBottomSheetBridge.destroy();

        verify(mBridgeNatives).onUiIgnored(MOCK_POINTER);
    }

    @Test
    public void testDestroy_doesNotCallBridgeNatives_withoutBottomSheetShown() {
        requestShowContent(/*didShow=*/false);

        mAutofillSaveCardBottomSheetBridge.destroy();

        verify(mBridgeNatives, never())
                .onUiIgnored(/*nativeAutofillSaveCardBottomSheetBridge=*/anyLong());
    }

    @Test
    public void testRequestShowContent_doesNotCreateABottomSheet_afterDestroy() {
        mAutofillSaveCardBottomSheetBridge.destroy();

        mAutofillSaveCardBottomSheetBridge.requestShowContent(mUiInfo);

        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    /**
     * Calls the bridge's requestShowContent while capturing the bottom sheet content.
     *
     * The bridge must be initialized; otherwise, we fail to capture the BottomSheet with an
     * exception from Mockito.
     *
     * @param didShow The value returned by the mocked call
     *                {@link ManagedBottomSheetController#requestShowContent}.
     * @return The captured value, a AutofillSaveCardBottomSheetContent.
     */
    private AutofillSaveCardBottomSheetContent requestShowContent(boolean didShow) {
        ArgumentCaptor<AutofillSaveCardBottomSheetContent> bottomSheetContent =
                ArgumentCaptor.forClass(AutofillSaveCardBottomSheetContent.class);
        when(mBottomSheetController.requestShowContent(bottomSheetContent.capture(), anyBoolean()))
                .thenReturn(didShow);
        mAutofillSaveCardBottomSheetBridge.requestShowContent(mUiInfo);
        return bottomSheetContent.getValue();
    }
}
