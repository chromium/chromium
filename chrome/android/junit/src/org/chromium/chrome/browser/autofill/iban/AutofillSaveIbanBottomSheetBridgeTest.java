// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.layouts.LayoutManagerAppUtils;
import org.chromium.chrome.browser.layouts.ManagedLayoutManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.autofill.payments.AutofillSaveIbanUiInfo;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.ui.base.WindowAndroid;

import java.util.Collections;

/** Unit tests for {@link AutofillSaveIbanBottomSheetBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@SmallTest
public final class AutofillSaveIbanBottomSheetBridgeTest {
    private static final long MOCK_POINTER = 0xb00fb00f;
    private static final String USER_PROVIDED_NICKNAME = "My Doctor's IBAN";
    private static final AutofillSaveIbanUiInfo TEST_IBAN_UI_INFO =
            new AutofillSaveIbanUiInfo.Builder()
                    .withAcceptText("Save")
                    .withCancelText("No thanks")
                    .withDescriptionText("")
                    .withIbanValue("CH5604835012345678009")
                    .withTitleText("Save IBAN?")
                    .withLegalMessageLines(Collections.EMPTY_LIST)
                    .withLogoIcon(0)
                    .withTitleText("")
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private AutofillSaveIbanBottomSheetBridge.Natives mBridgeNatives;
    @Mock private ManagedBottomSheetController mBottomSheetController;
    @Mock private ManagedLayoutManager mLayoutManager;
    @Mock private Profile mProfile;

    private AutofillSaveIbanBottomSheetBridge mAutofillSaveIbanBottomSheetBridge;
    private WindowAndroid mWindow;

    @Before
    public void setUp() {
        mJniMocker.mock(AutofillSaveIbanBottomSheetBridgeJni.TEST_HOOKS, mBridgeNatives);
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        // set a MaterialComponents theme which is required for the `OutlinedBox` text field.
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mWindow = new WindowAndroid(activity);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        LayoutManagerAppUtils.attach(mWindow, mLayoutManager);
        MockTabModel tabModel = new MockTabModel(mProfile, /* delegate= */ null);
        mAutofillSaveIbanBottomSheetBridge =
                new AutofillSaveIbanBottomSheetBridge(MOCK_POINTER, mWindow, tabModel);
    }

    @After
    public void tearDown() {
        BottomSheetControllerFactory.detach(mBottomSheetController);
        LayoutManagerAppUtils.detach(mLayoutManager);
        mWindow.destroy();
    }

    @Test
    public void testRequestShowContent() {
        mAutofillSaveIbanBottomSheetBridge.requestShowContent(TEST_IBAN_UI_INFO);

        verify(mBottomSheetController)
                .requestShowContent(
                        any(AutofillSaveIbanBottomSheetContent.class), /* animate= */ eq(true));
    }

    @Test
    public void testDestroy() {
        mAutofillSaveIbanBottomSheetBridge.requestShowContent(TEST_IBAN_UI_INFO);
        mAutofillSaveIbanBottomSheetBridge.destroy();

        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillSaveIbanBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(StateChangeReason.NONE));
    }

    @Test
    public void testDestroy_whenCoordinatorHasNotBeenCreated() {
        mAutofillSaveIbanBottomSheetBridge.destroy();

        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testDestroy_whenDestroyed() {
        mAutofillSaveIbanBottomSheetBridge.requestShowContent(TEST_IBAN_UI_INFO);

        mAutofillSaveIbanBottomSheetBridge.destroy();
        clearInvocations(mBottomSheetController);

        mAutofillSaveIbanBottomSheetBridge.destroy();
        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testOnUiAccepted_callsNativeOnUiAccepted() {
        mAutofillSaveIbanBottomSheetBridge.onUiAccepted(USER_PROVIDED_NICKNAME);

        verify(mBridgeNatives).onUiAccepted(MOCK_POINTER, USER_PROVIDED_NICKNAME);
    }

    @Test
    public void testOnUiAccepted_doesNotCallNative_afterDestroy() {
        mAutofillSaveIbanBottomSheetBridge.destroy();

        mAutofillSaveIbanBottomSheetBridge.onUiAccepted(USER_PROVIDED_NICKNAME);

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testOnUiCanceled_callsNativeOnUiCanceled() {
        mAutofillSaveIbanBottomSheetBridge.onUiCanceled();

        verify(mBridgeNatives).onUiCanceled(MOCK_POINTER);
    }

    @Test
    public void testOnUiCanceled_doesNotCallNative_afterDestroy() {
        mAutofillSaveIbanBottomSheetBridge.destroy();

        mAutofillSaveIbanBottomSheetBridge.onUiCanceled();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testOnUiIgnored_callsNativeOnUiIgnored() {
        mAutofillSaveIbanBottomSheetBridge.onUiIgnored();

        verify(mBridgeNatives).onUiIgnored(MOCK_POINTER);
    }

    @Test
    public void testOnUiIgnored_doesNotCallNative_afterDestroy() {
        mAutofillSaveIbanBottomSheetBridge.destroy();

        mAutofillSaveIbanBottomSheetBridge.onUiIgnored();

        verifyNoInteractions(mBridgeNatives);
    }
}
