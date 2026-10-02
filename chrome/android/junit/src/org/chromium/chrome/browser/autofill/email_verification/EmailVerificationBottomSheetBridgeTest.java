// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.email_verification;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;

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
    public void testRequestShowContent_whenAlreadyShowing_isNoOp() {
        mBridge.requestShowContent(TEST_TITLE, TEST_DESCRIPTION);
        clearInvocations(mBottomSheetController, mBridgeNatives);

        mBridge.requestShowContent(TEST_TITLE, TEST_DESCRIPTION);

        verifyNoInteractions(mBottomSheetController);
        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testRequestShowContent_whenNullProvider_callsOnUiDismissedOther() {
        WindowAndroid unattachedWindow =
                new WindowAndroid(
                        Robolectric.buildActivity(Activity.class).create().get(),
                        /* occlusionTrackingAllowed= */ true);
        MockTabModel tabModel = new MockTabModel(mProfile, /* delegate= */ null);
        EmailVerificationBottomSheetBridge bridge =
                new EmailVerificationBottomSheetBridge(MOCK_POINTER, unattachedWindow, tabModel);

        bridge.requestShowContent(TEST_TITLE, TEST_DESCRIPTION);

        verify(mBridgeNatives)
                .onUiDismissed(MOCK_POINTER, EmailVerificationPermissionUiStatus.OTHER);
        unattachedWindow.destroy();
    }

    @Test
    public void testHide() {
        mBridge.requestShowContent(TEST_TITLE, TEST_DESCRIPTION);
        clearInvocations(mBridgeNatives);
        mBridge.hide();

        verify(mBottomSheetController)
                .hideContent(
                        any(EmailVerificationBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(StateChangeReason.INTERACTION_COMPLETE));
        verify(mBridgeNatives)
                .onUiDismissed(MOCK_POINTER, EmailVerificationPermissionUiStatus.OTHER);
    }

    @Test
    public void testDestroy() {
        mBridge.requestShowContent(TEST_TITLE, TEST_DESCRIPTION);
        mBridge.destroy();

        verify(mBottomSheetController)
                .hideContent(
                        any(EmailVerificationBottomSheetContent.class),
                        /* animate= */ eq(false),
                        eq(StateChangeReason.NONE));
        verify(mBridgeNatives, never()).onUiDismissed(eq(MOCK_POINTER), anyInt());
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
    public void testOnUiAccepted_callsNativeOnUiAccepted() {
        mBridge.onUiAccepted();

        verify(mBridgeNatives).onUiAccepted(MOCK_POINTER);
    }

    @Test
    public void testOnUiAccepted_doesNotCallNative_afterDestroy() {
        mBridge.destroy();

        mBridge.onUiAccepted();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testOnUiDismissed_clearsCoordinator() {
        mBridge.requestShowContent(TEST_TITLE, TEST_DESCRIPTION);
        mBridge.onUiDismissed(EmailVerificationPermissionUiStatus.OTHER);

        // Coordinator should be cleared so subsequent requests can be shown.
        clearInvocations(mBottomSheetController, mBridgeNatives);
        mBridge.requestShowContent(TEST_TITLE, TEST_DESCRIPTION);
        verify(mBottomSheetController)
                .requestShowContent(
                        any(EmailVerificationBottomSheetContent.class), /* animate= */ eq(true));
    }

    @Test
    public void testOnUiDismissed_callsNativeOnUiDismissedWithReason() {
        mBridge.requestShowContent(TEST_TITLE, TEST_DESCRIPTION);
        clearInvocations(mBridgeNatives);

        mBridge.onUiDismissed(EmailVerificationPermissionUiStatus.USER_ABORTED);
        verify(mBridgeNatives)
                .onUiDismissed(MOCK_POINTER, EmailVerificationPermissionUiStatus.USER_ABORTED);
    }

    @Test
    public void testOnUiDismissed_doesNotCallNative_afterDestroy() {
        mBridge.requestShowContent(TEST_TITLE, TEST_DESCRIPTION);
        clearInvocations(mBridgeNatives);

        mBridge.destroy();
        mBridge.onUiDismissed(EmailVerificationPermissionUiStatus.USER_ABORTED);

        verifyNoInteractions(mBridgeNatives);
    }
}
