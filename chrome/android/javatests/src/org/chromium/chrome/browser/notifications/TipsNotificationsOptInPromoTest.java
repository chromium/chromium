// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasExtra;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.Matchers.allOf;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Context;
import android.content.Intent;
import android.provider.Settings;
import android.util.Pair;
import android.view.View;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.notifications.TipsOptInBottomSheetFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.ui.widget.ButtonCompat;

import java.io.IOException;

// TODO(crbug.com/478907175): Remove casting when value returns the view type.
/** Integration and render tests for the tips notifications opt in promo. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.ANDROID_TIPS_NOTIFICATIONS + ":always_show_opt_in_promo/true"})
@Batch(Batch.PER_CLASS)
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
@DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/488115473
public class TipsNotificationsOptInPromoTest {
    @Rule
    public FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(Component.UI_NOTIFICATIONS)
                    .build();

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testOptInBottomSheetDismiss() throws IOException {
        var tripResult = showOptInBottomSheet();
        TipsOptInBottomSheetFacility bottomSheet = tripResult.first;
        RegularNewTabPageStation openedNtp = tripResult.second;

        mRenderTestRule.render(
                ((View) bottomSheet.bottomSheetElement.value()), "opt_in_bottom_sheet");

        // Check that backpress dismisses the opt in bottom sheet.
        bottomSheet.dismiss();
        assertFinalDestination(openedNtp);
    }

    @Test
    @MediumTest
    public void testOptInBottomSheetClose() {
        var tripResult = showOptInBottomSheet();
        TipsOptInBottomSheetFacility bottomSheet = tripResult.first;
        RegularNewTabPageStation openedNtp = tripResult.second;

        // Check that close dismisses the opt in bottom sheet.
        bottomSheet.clickCloseButton();
        assertFinalDestination(openedNtp);
    }

    @Test
    @MediumTest
    public void testOptInBottomSheetAccept() {
        var tripResult = showOptInBottomSheet();
        TipsOptInBottomSheetFacility bottomSheet = tripResult.first;
        RegularNewTabPageStation openedNtp = tripResult.second;
        assertThat(((TextView) bottomSheet.titleElement.value()).getText())
                .isEqualTo(mContext.getString(R.string.tips_opt_in_bottom_sheet_title));
        assertThat(((TextView) bottomSheet.descriptionElement.value()).getText())
                .isEqualTo(mContext.getString(R.string.tips_opt_in_bottom_sheet_description));
        assertThat(((ButtonCompat) bottomSheet.positiveButtonElement.value()).getText())
                .isEqualTo(
                        mContext.getString(R.string.tips_opt_in_bottom_sheet_positive_button_text));
        assertThat(((ButtonCompat) bottomSheet.negativeButtonElement.value()).getText())
                .isEqualTo(
                        mContext.getString(R.string.tips_opt_in_bottom_sheet_negative_button_text));

        // Stub the intent to open the notification settings page.
        Intents.init();
        ActivityResult intentResult = new ActivityResult(Activity.RESULT_OK, null);
        intending(
                        allOf(
                                hasAction(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS),
                                hasExtra(
                                        Settings.EXTRA_CHANNEL_ID,
                                        ChromeChannelDefinitions.ChannelId.TIPS)))
                .respondWith(intentResult);

        // Check that accept dismisses the opt in bottom sheet.
        bottomSheet.clickAcceptButton();
        assertFinalDestination(openedNtp);
        Intents.release();
    }

    private Pair<TipsOptInBottomSheetFacility, RegularNewTabPageStation> showOptInBottomSheet() {
        // Setup the NTP that will be opened with the custom tips intent.
        RegularNewTabPageStation openedNtp =
                RegularNewTabPageStation.newBuilder().withEntryPoint().build();

        Intent intent = IntentHandler.createTrustedOpenNewTabIntent(mContext, false);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        return new Pair<>(
                mCtaTestRule
                        .startWithIntentTo(intent)
                        .arriveAtAnd(openedNtp)
                        .enterFacility(new TipsOptInBottomSheetFacility<>()),
                openedNtp);
    }
}
