// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import android.content.Context;
import android.content.Intent;
import android.util.Pair;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.scheduler.TipsNotificationsFeatureType;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.notifications.TipsPromoDetailsPageBottomSheetFacility;
import org.chromium.chrome.test.transit.notifications.TipsPromoMainPageBottomSheetFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.settings.SettingsStation;
import org.chromium.ui.widget.ButtonCompat;

import java.util.List;

/** Integration tests for the tips notifications feature promo. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.ANDROID_TIPS_NOTIFICATIONS})
@Batch(Batch.PER_CLASS)
public class TipsNotificationsFeaturePromoTest {
    @Rule
    public FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    @MediumTest
    public void testBottomSheetBackButtonAndDismiss() {
        @TipsNotificationsFeatureType
        int featureType = TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING;

        // Check that clicking the details button on the main page shows the detail page.
        var trip = showFeatureTipBottomSheet(featureType);
        TipsPromoMainPageBottomSheetFacility mainPageBottomSheet = trip.first;
        RegularNewTabPageStation openedNtp = trip.second;
        TipsPromoDetailsPageBottomSheetFacility detailsPageBottomSheet =
                mainPageBottomSheet.clickDetailsButton(null);

        // Check that clicking the back button on the detail page brings back the main page.
        mainPageBottomSheet = detailsPageBottomSheet.clickBackButton();

        // Check that the system backpress brings back the main page from the details page.
        detailsPageBottomSheet = mainPageBottomSheet.clickDetailsButton(null);
        mainPageBottomSheet = detailsPageBottomSheet.pressBack();

        // Check that backpress dismisses the main page bottom sheet.
        mainPageBottomSheet.dismiss();
    }

    @Test
    @MediumTest
    public void testESBBottomSheetMainPageAccept() {
        @TipsNotificationsFeatureType
        int featureType = TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING;

        // Check that clicking the settings button on the main page opens the safe browsing page.
        var trip = showFeatureTipBottomSheet(featureType);
        TipsPromoMainPageBottomSheetFacility mainPageBottomSheet = trip.first;
        RegularNewTabPageStation openedNtp = trip.second;
        assertThat(((TextView) mainPageBottomSheet.mainPageTitleElement.value()).getText())
                .isEqualTo(mContext.getString(R.string.tips_promo_bottom_sheet_title_esb));
        assertThat(((TextView) mainPageBottomSheet.mainPageDescriptionElement.value()).getText())
                .isEqualTo(mContext.getString(R.string.tips_promo_bottom_sheet_description_esb));
        assertThat(((ButtonCompat) mainPageBottomSheet.settingsButtonElement.value()).getText())
                .isEqualTo(
                        mContext.getString(R.string.tips_promo_bottom_sheet_positive_button_text));
        assertThat(((ButtonCompat) mainPageBottomSheet.detailsButtonElement.value()).getText())
                .isEqualTo(
                        mContext.getString(R.string.tips_promo_bottom_sheet_negative_button_text));
        SettingsStation<SafeBrowsingSettingsFragment> safeBrowsingSettings =
                mainPageBottomSheet.clickESBSettingsButton();
        assertFinalDestination(safeBrowsingSettings);

        // Return to a PageStation for InitialStateRule to reset properly.
        safeBrowsingSettings
                .pressBackTo()
                .arriveAt(RegularNewTabPageStation.newBuilder().initFrom(openedNtp).build());
    }

    @Test
    @MediumTest
    public void testESBBottomSheetDetailPageAccept() {
        @TipsNotificationsFeatureType
        int featureType = TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING;
        List<Integer> detailPageStepsRes =
                List.of(
                        R.string.tips_promo_bottom_sheet_first_step_esb,
                        R.string.tips_promo_bottom_sheet_second_step_esb,
                        R.string.tips_promo_bottom_sheet_third_step_esb);

        // Check that clicking the settings button on the detail page opens the safe browsing page.
        var trip = showFeatureTipBottomSheet(featureType);
        TipsPromoMainPageBottomSheetFacility mainPageBottomSheet = trip.first;
        RegularNewTabPageStation openedNtp = trip.second;
        TipsPromoDetailsPageBottomSheetFacility detailsPageBottomSheet =
                mainPageBottomSheet.clickDetailsButton(detailPageStepsRes);
        assertThat(((TextView) detailsPageBottomSheet.detailPageTitleElement.value()).getText())
                .isEqualTo(mContext.getString(R.string.tips_promo_bottom_sheet_title_esb));
        assertThat(((ButtonCompat) detailsPageBottomSheet.settingsButtonElement.value()).getText())
                .isEqualTo(
                        mContext.getString(R.string.tips_promo_bottom_sheet_positive_button_text));
        SettingsStation<SafeBrowsingSettingsFragment> safeBrowsingSettings =
                detailsPageBottomSheet.clickESBSettingsButton();
        assertFinalDestination(safeBrowsingSettings);

        // Return to a PageStation for InitialStateRule to reset properly.
        safeBrowsingSettings
                .pressBackTo()
                .arriveAt(RegularNewTabPageStation.newBuilder().initFrom(openedNtp).build());
    }

    private Pair<TipsPromoMainPageBottomSheetFacility, RegularNewTabPageStation>
            showFeatureTipBottomSheet(@TipsNotificationsFeatureType int featureType) {
        // Setup the NTP that will be opened with the custom tips intent.
        RegularNewTabPageStation openedNtp =
                RegularNewTabPageStation.newBuilder().withEntryPoint().build();

        Intent intent = IntentHandler.createTrustedOpenNewTabIntent(mContext, false);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(IntentHandler.EXTRA_TIPS_NOTIFICATION_FEATURE_TYPE, featureType);
        IntentHandler.setTabLaunchType(intent, TabLaunchType.FROM_TIPS_NOTIFICATIONS);

        return new Pair<>(
                mCtaTestRule
                        .startWithIntentTo(intent)
                        .arriveAtAnd(openedNtp)
                        .enterFacility(new TipsPromoMainPageBottomSheetFacility<>()),
                openedNtp);
    }
}
