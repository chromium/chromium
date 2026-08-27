// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.wallet_reminder_notice;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.notNullValue;
import static org.hamcrest.Matchers.nullValue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link AutofillWalletReminderNoticeBottomSheetBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillWalletReminderNoticeBottomSheetBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private WindowAndroid mWindow;
    @Mock private ManagedBottomSheetController mBottomSheetController;
    private AutofillWalletReminderNoticeBottomSheetBridge mBridge;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        mWindow = new WindowAndroid(activity, /* occlusionTrackingAllowed= */ true);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        mBridge = new AutofillWalletReminderNoticeBottomSheetBridge(mWindow);
    }

    @After
    public void tearDown() {
        BottomSheetControllerFactory.detach(mBottomSheetController);
        mWindow.destroy();
    }

    @Test
    public void testRequestShowContent() {
        mBridge.requestShowContent();

        verify(mBottomSheetController)
                .requestShowContent(
                        any(AutofillWalletReminderNoticeBottomSheetContent.class),
                        /* animate= */ eq(true));
        assertThat(mBridge.getCoordinatorForTesting(), notNullValue());
    }

    @Test
    public void testRequestShowContent_calledMultipleTimes_destroysPreviousCoordinator() {
        mBridge.requestShowContent();
        AutofillWalletReminderNoticeBottomSheetCoordinator firstCoordinator =
                mBridge.getCoordinatorForTesting();
        assertThat(firstCoordinator, notNullValue());

        mBridge.requestShowContent();
        AutofillWalletReminderNoticeBottomSheetCoordinator secondCoordinator =
                mBridge.getCoordinatorForTesting();
        assertThat(secondCoordinator, notNullValue());

        // Verify hideContent was called for destroying the first coordinator
        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillWalletReminderNoticeBottomSheetContent.class),
                        /* animate= */ eq(false),
                        eq(BottomSheetController.StateChangeReason.NONE));
    }

    @Test
    public void testRequestShowContent_whenBottomSheetControllerNull() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        WindowAndroid windowWithoutController =
                new WindowAndroid(activity, /* occlusionTrackingAllowed= */ true);
        AutofillWalletReminderNoticeBottomSheetBridge bridge =
                new AutofillWalletReminderNoticeBottomSheetBridge(windowWithoutController);

        bridge.requestShowContent();

        assertThat(bridge.getCoordinatorForTesting(), nullValue());
        windowWithoutController.destroy();
    }

    @Test
    public void testRequestShowContent_whenContextNull() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        WindowAndroid window = new WindowAndroid(activity, /* occlusionTrackingAllowed= */ true);
        BottomSheetControllerFactory.attach(window, mBottomSheetController);
        AutofillWalletReminderNoticeBottomSheetBridge bridge =
                new AutofillWalletReminderNoticeBottomSheetBridge(window);
        window.destroy();

        bridge.requestShowContent();

        assertThat(bridge.getCoordinatorForTesting(), nullValue());
    }

    @Test
    public void testDestroy() {
        mBridge.requestShowContent();
        assertThat(mBridge.getCoordinatorForTesting(), notNullValue());

        mBridge.destroy();

        assertThat(mBridge.getCoordinatorForTesting(), nullValue());
        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillWalletReminderNoticeBottomSheetContent.class),
                        /* animate= */ eq(false),
                        eq(BottomSheetController.StateChangeReason.NONE));
        verify(mBottomSheetController)
                .removeObserver(any(AutofillWalletReminderNoticeBottomSheetMediator.class));
    }

    @Test
    public void testDestroy_whenCoordinatorNotCreated() {
        mBridge.destroy();

        verifyNoInteractions(mBottomSheetController);
    }
}
