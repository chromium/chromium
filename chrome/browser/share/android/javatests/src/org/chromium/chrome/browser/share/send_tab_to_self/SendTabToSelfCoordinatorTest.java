// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.chromium.url.JUnitTestGURLs.HTTP_URL;

import android.view.View;

import androidx.annotation.IdRes;
import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.sync.protocol.DeviceInfoSpecifics;
import org.chromium.components.sync.protocol.EntitySpecifics;
import org.chromium.components.sync.protocol.FeatureSpecificFields;
import org.chromium.components.sync.protocol.SyncEnums.DeviceType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.WindowAndroid;

import java.util.Optional;

/** Tests for SendTabToSelfCoordinator */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SendTabToSelfCoordinatorTest {
    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule();

    @Before
    public void setUp() {
        addTargetDeviceToSyncServer();
    }

    @Test
    @LargeTest
    @DisabledTest(message = "https://crbug.com/1299410")
    public void testShowDeviceListIfSignedIn() {
        // Sign in and wait for the device list to be downloaded.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        CriteriaHelper.pollUiThread(() -> {
            return SendTabToSelfAndroidBridge
                    .getEntryPointDisplayReason(
                            Profile.getLastUsedRegularProfile(), HTTP_URL.getSpec())
                    .equals(Optional.of(EntryPointDisplayReason.OFFER_FEATURE));
        });

        buildAndShowCoordinator();

        waitForViewShown(R.id.device_picker_list);
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_SIGNIN_PROMO)
    public void testShowFeatureUnavailablePromptIfSignedOutAndFeatureDisabled() {
        // Set up a user satisfying all the preconditions for a sign-in promo, except having the
        // feature enabled.
        mSyncTestRule.addTestAccount();
        buildAndShowCoordinator();

        waitForViewShown(R.id.send_tab_to_self_feature_unavailable_prompt);
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_SIGNIN_PROMO)
    @DisabledTest(message = "https://crbug.com/1302062")
    public void testShowSigninPromoIfSignedOutAndFeatureEnabled() {
        // An account must be added to the device so the promo is offered.
        mSyncTestRule.addTestAccount();
        buildAndShowCoordinator();

        // Check the promo is displayed, in particular the sign-in button.
        waitForViewShown(R.id.account_picker_continue_as_button);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getBottomSheetView()
                    .findViewById(R.id.account_picker_continue_as_button)
                    .performClick();
        });

        waitForViewShown(R.id.device_picker_list);
    }

    private void addTargetDeviceToSyncServer() {
        FeatureSpecificFields features =
                FeatureSpecificFields.newBuilder().setSendTabToSelfReceivingEnabled(true).build();
        // Setting a recent timestamp here is necessary, otherwise the device will be considered
        // expired and won't be displayed.
        DeviceInfoSpecifics deviceInfo =
                DeviceInfoSpecifics.newBuilder()
                        .setCacheGuid("CacheGuid")
                        .setClientName("Device")
                        .setDeviceType(DeviceType.TYPE_PHONE)
                        .setSyncUserAgent("UserAgent")
                        .setChromeVersion("1.0")
                        .setSigninScopedDeviceId("Id")
                        .setLastUpdatedTimestamp(System.currentTimeMillis())
                        .setFeatureFields(features)
                        .build();
        EntitySpecifics specifics = EntitySpecifics.newBuilder().setDeviceInfo(deviceInfo).build();
        // TODO(crbug.com/1219434): Don't duplicate DeviceInfo's SpecificsToTag() logic for the
        // clientTag value, instead expose the function to Java and call it here.
        mSyncTestRule.getFakeServerHelper().injectUniqueClientEntity(
                "nonUniqueName", /*clientTag=*/"DeviceInfo_CacheGuid", specifics);
    }

    private void buildAndShowCoordinator() {
        ChromeTabbedActivity activity = mSyncTestRule.getActivity();
        WindowAndroid windowAndroid = activity.getWindowAndroid();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SendTabToSelfCoordinator coordinator =
                    new SendTabToSelfCoordinator(activity, windowAndroid, HTTP_URL.getSpec(),
                            "Page", BottomSheetControllerProvider.from(windowAndroid),
                            Profile.getLastUsedRegularProfile(), null);
            coordinator.show();
        });
    }

    private View getBottomSheetView() {
        WindowAndroid windowAndroid = mSyncTestRule.getActivity().getWindowAndroid();
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return BottomSheetControllerProvider.from(windowAndroid)
                    .getCurrentSheetContent()
                    .getContentView();
        });
    }

    private void waitForViewShown(@IdRes int id) {
        CriteriaHelper.pollUiThread(() -> {
            View view = getBottomSheetView().findViewById(id);
            return view != null && view.getVisibility() == View.VISIBLE;
        });
    }
}
