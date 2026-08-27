// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.email_verification;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

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
import org.chromium.chrome.browser.autofill.anchored_dialog.AnchoredDialogCoordinator;
import org.chromium.chrome.browser.autofill.anchored_dialog.AnchoredDialogCoordinatorProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.autofill.EmailVerificationPermissionUiStatus;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link EmailVerificationBottomSheetBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@SmallTest
public final class EmailVerificationBottomSheetBridgeTest {
    private static final long MOCK_POINTER = 0xb00fb00f;
    private static final String TEST_TITLE = "Verify this email automatically?";
    private static final String TEST_DESCRIPTION =
            "While you're signed in, google.com can confirm test@example.com on supported sites so"
                    + " you don't have to check your inbox";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private EmailVerificationBottomSheetBridge.Natives mBridgeNatives;
    @Mock private ManagedBottomSheetController mBottomSheetController;
    @Mock private AnchoredDialogCoordinator mAnchoredDialogCoordinator;
    @Mock private Profile mProfile;

    private EmailVerificationBottomSheetBridge mBridge;
    private WindowAndroid mWindow;

    @Before
    public void setUp() {
        EmailVerificationBottomSheetBridgeJni.setInstanceForTesting(mBridgeNatives);
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        Activity activity = Robolectric.setupActivity(TestActivity.class);
        mWindow = new WindowAndroid(activity, /* occlusionTrackingAllowed= */ true);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        AnchoredDialogCoordinatorProvider.attach(mWindow, mAnchoredDialogCoordinator);
        MockTabModel tabModel = new MockTabModel(mProfile, /* delegate= */ null);
        mBridge = new EmailVerificationBottomSheetBridge(MOCK_POINTER, mWindow, tabModel);
    }

    @After
    public void tearDown() {
        AnchoredDialogCoordinatorProvider.detach(mAnchoredDialogCoordinator);
        BottomSheetControllerFactory.detach(mBottomSheetController);
        mWindow.destroy();
    }

    @Test
    public void testRequestShowContent() {
        mBridge.requestShowContent(TEST_TITLE, TEST_DESCRIPTION);

        verify(mBottomSheetController)
                .requestShowContent(
                        any(EmailVerificationBottomSheetContent.class), /* animate= */ eq(true));
        verify(mBridgeNatives).onUiShown(MOCK_POINTER);
    }

    @Test
    public void testRequestShowContent_whenNullProvider_callsOnUiDecisionOther() {
        WindowAndroid unattachedWindow =
                new WindowAndroid(
                        Robolectric.buildActivity(Activity.class).create().get(),
                        /* occlusionTrackingAllowed= */ true);
        MockTabModel tabModel = new MockTabModel(mProfile, /* delegate= */ null);
        EmailVerificationBottomSheetBridge bridge =
                new EmailVerificationBottomSheetBridge(MOCK_POINTER, unattachedWindow, tabModel);

        bridge.requestShowContent(TEST_TITLE, TEST_DESCRIPTION);

        verify(mBridgeNatives)
                .onUiDecision(MOCK_POINTER, EmailVerificationPermissionUiStatus.OTHER);
        unattachedWindow.destroy();
    }

    @Test
    public void testHide() {
        mBridge.requestShowContent(TEST_TITLE, TEST_DESCRIPTION);
        mBridge.hide();

        verify(mBottomSheetController)
                .hideContent(
                        any(EmailVerificationBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testDestroy() {
        mBridge.requestShowContent(TEST_TITLE, TEST_DESCRIPTION);
        mBridge.destroy();

        verify(mBottomSheetController)
                .hideContent(
                        any(EmailVerificationBottomSheetContent.class),
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
        mBridge.requestShowContent(TEST_TITLE, TEST_DESCRIPTION);

        mBridge.destroy();
        clearInvocations(mBottomSheetController);

        mBridge.destroy();
        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testOnUiShown_callsNativeOnUiShown() {
        mBridge.onUiShown();

        verify(mBridgeNatives).onUiShown(MOCK_POINTER);
    }

    @Test
    public void testOnUiShown_doesNotCallNative_afterDestroy() {
        mBridge.destroy();

        mBridge.onUiShown();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testOnUiDecision_callsNativeOnUiDecision() {
        mBridge.onUiDecision(EmailVerificationPermissionUiStatus.ALLOWED);

        verify(mBridgeNatives)
                .onUiDecision(MOCK_POINTER, EmailVerificationPermissionUiStatus.ALLOWED);
    }

    @Test
    public void testOnUiDecision_doesNotCallNative_afterDestroy() {
        mBridge.destroy();

        mBridge.onUiDecision(EmailVerificationPermissionUiStatus.ALLOWED);

        verifyNoInteractions(mBridgeNatives);
    }
}
