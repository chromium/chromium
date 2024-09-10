// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

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
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.layouts.LayoutManagerAppUtils;
import org.chromium.chrome.browser.layouts.ManagedLayoutManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.autofill.payments.AutofillSaveCardUiInfo;
import org.chromium.components.autofill.payments.CardDetail;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.ui.base.WindowAndroid;

import java.util.Collections;

/** Unit tests for {@link AutofillSaveCardBottomSheetBridge}. */
@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
public final class AutofillSaveCardBottomSheetBridgeTest {
    private static final long NATIVE_AUTOFILL_SAVE_CARD_BOTTOM_SHEET_BRIDGE = 0xb00fb00fL;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private AutofillSaveCardBottomSheetBridge.Natives mBridgeNatives;
    private WindowAndroid mWindow;
    @Mock private ManagedBottomSheetController mBottomSheetController;
    @Mock private ManagedLayoutManager mLayoutManager;
    @Mock private Profile mProfile;
    private AutofillSaveCardBottomSheetBridge mBridge;

    @Before
    public void setUp() {
        mJniMocker.mock(AutofillSaveCardBottomSheetBridgeJni.TEST_HOOKS, mBridgeNatives);
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        mWindow = new WindowAndroid(activity);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        LayoutManagerAppUtils.attach(mWindow, mLayoutManager);
        MockTabModel tabModel = new MockTabModel(mProfile, /* delegate= */ null);
        mBridge =
                new AutofillSaveCardBottomSheetBridge(
                        NATIVE_AUTOFILL_SAVE_CARD_BOTTOM_SHEET_BRIDGE, mWindow, tabModel);
    }

    @After
    public void tearDown() {
        LayoutManagerAppUtils.detach(mLayoutManager);
        BottomSheetControllerFactory.detach(mBottomSheetController);
        mWindow.destroy();
    }

    private void requestShowContent() {
        mBridge.requestShowContent(
                new AutofillSaveCardUiInfo.Builder()
                        .withCardDetail(new CardDetail(/* iconId= */ 0, "label", "subLabel"))
                        .withCardDescription("Card description")
                        .withLegalMessageLines(Collections.EMPTY_LIST)
                        .withTitleText("Title")
                        .withConfirmText("Confirm")
                        .withCancelText("Cancel")
                        .withDescriptionText("Description")
                        .withLoadingDescription("Loading description")
                        .build(),
                /* skipLoadingForFixFlow= */ false);
    }

    @Test
    public void testRequestShowContent() {
        requestShowContent();

        verify(mBottomSheetController)
                .requestShowContent(
                        any(AutofillSaveCardBottomSheetContent.class), /* animate= */ eq(true));
    }

    @Test
    public void testHide() {
        requestShowContent();
        mBridge.hide();

        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillSaveCardBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testDestroy() {
        requestShowContent();
        mBridge.destroy();

        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillSaveCardBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(StateChangeReason.NONE));
    }

    @Test
    public void testDestroy_whenCoordinatorHasNotBeenCreated() {
        mBridge.destroy();

        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testDestroy_whenDestroyed() {
        requestShowContent();

        mBridge.destroy();
        clearInvocations(mBottomSheetController);

        mBridge.destroy();
        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testOnUiShown() {
        mBridge.onUiShown();

        verify(mBridgeNatives).onUiShown(eq(NATIVE_AUTOFILL_SAVE_CARD_BOTTOM_SHEET_BRIDGE));
    }

    @Test
    public void testOnUiShown_whenDestroyed() {
        mBridge.destroy();

        mBridge.onUiShown();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testOnUiAccepted() {
        mBridge.onUiAccepted();

        verify(mBridgeNatives).onUiAccepted(eq(NATIVE_AUTOFILL_SAVE_CARD_BOTTOM_SHEET_BRIDGE));
    }

    @Test
    public void testOnUiAccepted_whenDestroyed() {
        mBridge.destroy();

        mBridge.onUiAccepted();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testOnUiCanceled() {
        mBridge.onUiCanceled();

        verify(mBridgeNatives).onUiCanceled(eq(NATIVE_AUTOFILL_SAVE_CARD_BOTTOM_SHEET_BRIDGE));
    }

    @Test
    public void testOnUiCanceled_whenDestroyed() {
        mBridge.destroy();

        mBridge.onUiCanceled();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testOnUiIgnored() {
        mBridge.onUiIgnored();

        verify(mBridgeNatives).onUiIgnored(eq(NATIVE_AUTOFILL_SAVE_CARD_BOTTOM_SHEET_BRIDGE));
    }

    @Test
    public void testOnUiIgnored_whenDestroyed() {
        mBridge.destroy();

        mBridge.onUiIgnored();

        verifyNoInteractions(mBridgeNatives);
    }
}
