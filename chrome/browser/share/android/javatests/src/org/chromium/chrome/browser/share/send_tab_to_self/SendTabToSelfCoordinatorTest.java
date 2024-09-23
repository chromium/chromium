// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doAnswer;

import static org.chromium.url.JUnitTestGURLs.HTTP_URL;

import android.app.Activity;
import android.view.View;

import androidx.annotation.IdRes;
import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.ui.base.WindowAndroid;

import java.util.Optional;

/** Tests for SendTabToSelfCoordinator */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SendTabToSelfCoordinatorTest {
    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();

    @Mock private DeviceLockActivityLauncher mDeviceLockActivityLauncher;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);

        // Skip device lock UI on automotive.
        doAnswer(
                        invocation -> {
                            WindowAndroid.IntentCallback callback =
                                    (WindowAndroid.IntentCallback) invocation.getArguments()[4];
                            callback.onIntentCompleted(Activity.RESULT_OK, null);
                            return null;
                        })
                .when(mDeviceLockActivityLauncher)
                .launchDeviceLockActivity(any(), any(), anyBoolean(), any(), any(), any());

        // Setting a recent timestamp here is necessary, otherwise the device will be considered
        // expired and won't be displayed.
        long now = System.currentTimeMillis();
        mSyncTestRule.getFakeServerHelper().injectDeviceInfoEntity("CacheGuid", "Device", now, now);
    }

    @Test
    @LargeTest
    @DisabledTest(message = "https://crbug.com/1299410")
    public void testShowDeviceListIfSignedIn() {
        // Sign in and wait for the device list to be downloaded.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        CriteriaHelper.pollUiThread(
                () -> {
                    return SendTabToSelfAndroidBridge.getEntryPointDisplayReason(
                                    ProfileManager.getLastUsedRegularProfile(), HTTP_URL.getSpec())
                            .equals(Optional.of(EntryPointDisplayReason.OFFER_FEATURE));
                });

        buildAndShowCoordinator();

        waitForViewShown(R.id.device_picker_list);
    }

    @Test
    @LargeTest
    // TODO(crbug.com/40825119): Flaky on Nexus 5x (bullhead).
    @DisableIf.Build(hardware_is = "bullhead")
    public void testShowSigninPromoIfSignedOut() {
        // An account must be added to the device so the promo is offered.
        mSyncTestRule.addTestAccount();
        buildAndShowCoordinator();

        // Check the promo is displayed, in particular the sign-in button.
        waitForViewShown(R.id.account_picker_continue_as_button);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getBottomSheetView()
                            .findViewById(R.id.account_picker_continue_as_button)
                            .performClick();
                });

        waitForViewShown(R.id.device_picker_list);
    }

    private void buildAndShowCoordinator() {
        ChromeTabbedActivity activity = mSyncTestRule.getActivity();
        WindowAndroid windowAndroid = activity.getWindowAndroid();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SendTabToSelfCoordinator coordinator =
                            new SendTabToSelfCoordinator(
                                    activity,
                                    windowAndroid,
                                    HTTP_URL.getSpec(),
                                    "Page",
                                    BottomSheetControllerProvider.from(windowAndroid),
                                    ProfileManager.getLastUsedRegularProfile(),
                                    mDeviceLockActivityLauncher);
                    coordinator.show();
                });
    }

    private View getBottomSheetView() {
        WindowAndroid windowAndroid = mSyncTestRule.getActivity().getWindowAndroid();
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return BottomSheetControllerProvider.from(windowAndroid)
                            .getCurrentSheetContent()
                            .getContentView();
                });
    }

    private void waitForViewShown(@IdRes int id) {
        CriteriaHelper.pollUiThread(
                () -> {
                    View view = getBottomSheetView().findViewById(id);
                    return view != null && view.getVisibility() == View.VISIBLE;
                });
    }
}
