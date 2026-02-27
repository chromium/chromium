// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.swipeUp;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import android.content.Context;
import android.content.Intent;
import android.util.Pair;
import android.view.View;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.notifications.scheduler.TipsNotificationsFeatureType;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.notifications.TipsPromoDetailsPageBottomSheetFacility;
import org.chromium.chrome.test.transit.notifications.TipsPromoMainPageBottomSheetFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.quick_delete.QuickDeleteDialogFacility;
import org.chromium.chrome.test.transit.settings.SettingsStation;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.ui.widget.ButtonCompat;

import java.io.IOException;
import java.util.List;

// TODO(crbug.com/478907175): Remove casting when value returns the view type.
/** Integration and render tests for the tips notifications feature promo. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({
    ChromeFeatureList.ANDROID_TIPS_NOTIFICATIONS,
    SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
})
@Batch(Batch.PER_CLASS)
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
@DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/488115473
public class TipsNotificationsFeaturePromoTest {
    @Rule
    public FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setBugComponent(Component.UI_NOTIFICATIONS)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LensController mLensController;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        LensController.setInstanceForTesting(mLensController);
    }

    @Test
    @MediumTest
    public void testBottomSheetBackButtonAndDismiss() {
        @TipsNotificationsFeatureType
        int featureType = TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING;

        // Check that clicking the details button on the main page shows the detail page.
        var tripResult = showFeatureTipBottomSheet(featureType);
        TipsPromoMainPageBottomSheetFacility mainPageBottomSheet = tripResult.first;
        RegularNewTabPageStation openedNtp = tripResult.second;
        TipsPromoDetailsPageBottomSheetFacility detailsPageBottomSheet =
                mainPageBottomSheet.clickDetailsButton();

        // Check that clicking the back button on the detail page brings back the main page.
        mainPageBottomSheet = detailsPageBottomSheet.clickBackButton();

        // Check that the system backpress brings back the main page from the details page.
        detailsPageBottomSheet = mainPageBottomSheet.clickDetailsButton();
        mainPageBottomSheet = detailsPageBottomSheet.pressBack();

        // Check that backpress dismisses the main page bottom sheet.
        mainPageBottomSheet.dismiss();
        assertFinalDestination(openedNtp);
    }

    @Test
    @MediumTest
    public void testESBBottomSheetMainPageAccept() {
        @TipsNotificationsFeatureType
        int featureType = TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING;

        // Check that clicking the settings button on the main page opens the safe browsing page.
        var tripResult = showFeatureTipBottomSheet(featureType);
        TipsPromoMainPageBottomSheetFacility mainPageBottomSheet = tripResult.first;
        RegularNewTabPageStation openedNtp = tripResult.second;
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
    @Feature({"RenderTest"})
    public void testESBBottomSheetDetailPageAccept() throws IOException {
        @TipsNotificationsFeatureType
        int featureType = TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING;
        List<Integer> detailPageStepsRes =
                List.of(
                        R.string.tips_promo_bottom_sheet_first_step_esb,
                        R.string.tips_promo_bottom_sheet_second_step_esb,
                        R.string.tips_promo_bottom_sheet_third_step_esb);

        // Check that clicking the settings button on the detail page opens the safe browsing page.
        var tripResult = showFeatureTipBottomSheet(featureType);
        TipsPromoMainPageBottomSheetFacility mainPageBottomSheet = tripResult.first;
        RegularNewTabPageStation openedNtp = tripResult.second;

        mRenderTestRule.render(
                ((View) mainPageBottomSheet.bottomSheetElement.value()),
                "esb_feature_promo_main_page");

        TipsPromoDetailsPageBottomSheetFacility detailsPageBottomSheet =
                mainPageBottomSheet.clickDetailsButton(detailPageStepsRes);
        assertThat(((TextView) detailsPageBottomSheet.detailPageTitleElement.value()).getText())
                .isEqualTo(mContext.getString(R.string.tips_promo_bottom_sheet_title_esb));
        assertThat(((ButtonCompat) detailsPageBottomSheet.settingsButtonElement.value()).getText())
                .isEqualTo(
                        mContext.getString(R.string.tips_promo_bottom_sheet_positive_button_text));

        mRenderTestRule.render(
                ((View) detailsPageBottomSheet.bottomSheetElement.value()),
                "esb_feature_promo_detail_page");

        SettingsStation<SafeBrowsingSettingsFragment> safeBrowsingSettings =
                detailsPageBottomSheet.clickESBSettingsButton();
        assertFinalDestination(safeBrowsingSettings);

        // Return to a PageStation for InitialStateRule to reset properly.
        safeBrowsingSettings
                .pressBackTo()
                .arriveAt(RegularNewTabPageStation.newBuilder().initFrom(openedNtp).build());
    }

    @Test
    @MediumTest
    public void testQuickDeleteBottomSheetMainPageAccept() {
        @TipsNotificationsFeatureType int featureType = TipsNotificationsFeatureType.QUICK_DELETE;

        // Check that clicking the settings button on the main page opens the quick delete page.
        var tripResult = showFeatureTipBottomSheet(featureType);
        TipsPromoMainPageBottomSheetFacility mainPageBottomSheet = tripResult.first;
        RegularNewTabPageStation openedNtp = tripResult.second;
        // TODO(crbug.com/467389502): Remove swipe up when layout bug is fixed to show fully.
        onView(ViewMatchers.withId(android.R.id.content)).perform(swipeUp());
        assertThat(((TextView) mainPageBottomSheet.mainPageTitleElement.value()).getText())
                .isEqualTo(mContext.getString(R.string.tips_promo_bottom_sheet_title_quick_delete));
        assertThat(((TextView) mainPageBottomSheet.mainPageDescriptionElement.value()).getText())
                .isEqualTo(
                        mContext.getString(
                                R.string.tips_promo_bottom_sheet_description_quick_delete));
        assertThat(((ButtonCompat) mainPageBottomSheet.settingsButtonElement.value()).getText())
                .isEqualTo(
                        mContext.getString(R.string.tips_promo_bottom_sheet_positive_button_text));
        assertThat(((ButtonCompat) mainPageBottomSheet.detailsButtonElement.value()).getText())
                .isEqualTo(
                        mContext.getString(R.string.tips_promo_bottom_sheet_negative_button_text));
        QuickDeleteDialogFacility quickDeleteDialog = mainPageBottomSheet.clickQuickDeleteButton();
        assertFinalDestination(openedNtp, quickDeleteDialog);

        // Return to a PageStation for InitialStateRule to reset properly.
        quickDeleteDialog.pressBackToDismiss();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testQuickDeleteBottomSheetDetailPageAccept() throws IOException {
        @TipsNotificationsFeatureType int featureType = TipsNotificationsFeatureType.QUICK_DELETE;
        List<Integer> detailPageStepsRes =
                List.of(
                        R.string.tips_promo_bottom_sheet_first_step_quick_delete,
                        R.string.tips_promo_bottom_sheet_second_step_quick_delete,
                        R.string.tips_promo_bottom_sheet_third_step_quick_delete);

        // Check that clicking the settings button on the detail page opens the quick delete page.
        var tripResult = showFeatureTipBottomSheet(featureType);
        TipsPromoMainPageBottomSheetFacility mainPageBottomSheet = tripResult.first;
        RegularNewTabPageStation openedNtp = tripResult.second;

        mRenderTestRule.render(
                ((View) mainPageBottomSheet.bottomSheetElement.value()),
                "quick_delete_feature_promo_main_page");

        // TODO(crbug.com/467389502): Remove swipe up when layout bug is fixed to show fully.
        onView(ViewMatchers.withId(android.R.id.content)).perform(swipeUp());
        TipsPromoDetailsPageBottomSheetFacility detailsPageBottomSheet =
                mainPageBottomSheet.clickDetailsButton(detailPageStepsRes);
        assertThat(((TextView) detailsPageBottomSheet.detailPageTitleElement.value()).getText())
                .isEqualTo(
                        mContext.getString(
                                R.string.tips_promo_bottom_sheet_title_quick_delete_short));
        assertThat(((ButtonCompat) detailsPageBottomSheet.settingsButtonElement.value()).getText())
                .isEqualTo(
                        mContext.getString(R.string.tips_promo_bottom_sheet_positive_button_text));

        mRenderTestRule.render(
                ((View) detailsPageBottomSheet.bottomSheetElement.value()),
                "quick_delete_feature_promo_detail_page");

        QuickDeleteDialogFacility quickDeleteDialog =
                detailsPageBottomSheet.clickQuickDeleteButton();
        assertFinalDestination(openedNtp, quickDeleteDialog);

        // Return to a PageStation for InitialStateRule to reset properly.
        quickDeleteDialog.pressBackToDismiss();
    }

    @Test
    @MediumTest
    public void testGoogleLensBottomSheetMainPageAccept() {
        @TipsNotificationsFeatureType int featureType = TipsNotificationsFeatureType.GOOGLE_LENS;

        // Check that clicking the settings button on the main page opens the quick delete page.
        var tripResult = showFeatureTipBottomSheet(featureType);
        TipsPromoMainPageBottomSheetFacility mainPageBottomSheet = tripResult.first;
        RegularNewTabPageStation openedNtp = tripResult.second;
        assertThat(((TextView) mainPageBottomSheet.mainPageTitleElement.value()).getText())
                .isEqualTo(mContext.getString(R.string.tips_promo_bottom_sheet_title_lens));
        assertThat(((TextView) mainPageBottomSheet.mainPageDescriptionElement.value()).getText())
                .isEqualTo(mContext.getString(R.string.tips_promo_bottom_sheet_description_lens));
        assertThat(((ButtonCompat) mainPageBottomSheet.settingsButtonElement.value()).getText())
                .isEqualTo(
                        mContext.getString(
                                R.string.tips_promo_bottom_sheet_positive_button_text_lens));
        assertThat(((ButtonCompat) mainPageBottomSheet.detailsButtonElement.value()).getText())
                .isEqualTo(
                        mContext.getString(R.string.tips_promo_bottom_sheet_negative_button_text));
        mainPageBottomSheet.clickGoogleLensButton(mLensController);

        // Return to a PageStation for InitialStateRule to reset properly, which clicking the bottom
        // sheet does since the Google Lens call is intercepted and does not show.
        assertFinalDestination(openedNtp);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testGoogleLensBottomSheetDetailPageAccept() throws IOException {
        @TipsNotificationsFeatureType int featureType = TipsNotificationsFeatureType.GOOGLE_LENS;
        List<Integer> detailPageStepsRes =
                List.of(
                        R.string.tips_promo_bottom_sheet_first_step_lens,
                        R.string.tips_promo_bottom_sheet_second_step_lens,
                        R.string.tips_promo_bottom_sheet_third_step_lens);

        // Check that clicking the settings button on the detail page opens the quick delete page.
        var tripResult = showFeatureTipBottomSheet(featureType);
        TipsPromoMainPageBottomSheetFacility mainPageBottomSheet = tripResult.first;
        RegularNewTabPageStation openedNtp = tripResult.second;

        mRenderTestRule.render(
                ((View) mainPageBottomSheet.bottomSheetElement.value()),
                "google_lens_feature_promo_main_page");

        TipsPromoDetailsPageBottomSheetFacility detailsPageBottomSheet =
                mainPageBottomSheet.clickDetailsButton(detailPageStepsRes);
        assertThat(((TextView) detailsPageBottomSheet.detailPageTitleElement.value()).getText())
                .isEqualTo(mContext.getString(R.string.tips_promo_bottom_sheet_title_lens));
        assertThat(((ButtonCompat) detailsPageBottomSheet.settingsButtonElement.value()).getText())
                .isEqualTo(
                        mContext.getString(
                                R.string.tips_promo_bottom_sheet_positive_button_text_lens));

        mRenderTestRule.render(
                ((View) detailsPageBottomSheet.bottomSheetElement.value()),
                "google_lens_feature_promo_detail_page");

        detailsPageBottomSheet.clickGoogleLensButton(mLensController);

        // Return to a PageStation for InitialStateRule to reset properly, which clicking the bottom
        // sheet does since the Google Lens call is intercepted and does not show.
        assertFinalDestination(openedNtp);
    }

    @Test
    @MediumTest
    public void testBottomOmniboxBottomSheetMainPageAccept() {
        @TipsNotificationsFeatureType int featureType = TipsNotificationsFeatureType.BOTTOM_OMNIBOX;

        // Check that clicking the settings button on the main page opens the bottom omnibox page.
        var tripResult = showFeatureTipBottomSheet(featureType);
        TipsPromoMainPageBottomSheetFacility mainPageBottomSheet = tripResult.first;
        RegularNewTabPageStation openedNtp = tripResult.second;
        assertThat(((TextView) mainPageBottomSheet.mainPageTitleElement.value()).getText())
                .isEqualTo(
                        mContext.getString(R.string.tips_promo_bottom_sheet_title_bottom_omnibox));
        assertThat(((TextView) mainPageBottomSheet.mainPageDescriptionElement.value()).getText())
                .isEqualTo(
                        mContext.getString(
                                R.string.tips_promo_bottom_sheet_description_bottom_omnibox));
        assertThat(((ButtonCompat) mainPageBottomSheet.settingsButtonElement.value()).getText())
                .isEqualTo(
                        mContext.getString(R.string.tips_promo_bottom_sheet_positive_button_text));
        assertThat(((ButtonCompat) mainPageBottomSheet.detailsButtonElement.value()).getText())
                .isEqualTo(
                        mContext.getString(R.string.tips_promo_bottom_sheet_negative_button_text));
        SettingsStation<AddressBarSettingsFragment> bottomOmniboxSettings =
                mainPageBottomSheet.clickBottomOmniboxSettingsButton();
        assertFinalDestination(bottomOmniboxSettings);

        // Return to a PageStation for InitialStateRule to reset properly.
        bottomOmniboxSettings
                .pressBackTo()
                .arriveAt(RegularNewTabPageStation.newBuilder().initFrom(openedNtp).build());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBottomOmniboxBottomSheetDetailPageAccept() throws IOException {
        @TipsNotificationsFeatureType int featureType = TipsNotificationsFeatureType.BOTTOM_OMNIBOX;
        List<Integer> detailPageStepsRes =
                List.of(
                        R.string.tips_promo_bottom_sheet_first_step_bottom_omnibox,
                        R.string.tips_promo_bottom_sheet_second_step_bottom_omnibox,
                        R.string.tips_promo_bottom_sheet_third_step_bottom_omnibox);

        // Check that clicking the settings button on the detail page opens the bottom omnibox page.
        var tripResult = showFeatureTipBottomSheet(featureType);
        TipsPromoMainPageBottomSheetFacility mainPageBottomSheet = tripResult.first;
        RegularNewTabPageStation openedNtp = tripResult.second;

        mRenderTestRule.render(
                ((View) mainPageBottomSheet.bottomSheetElement.value()),
                "bottom_omnibox_feature_promo_main_page");

        TipsPromoDetailsPageBottomSheetFacility detailsPageBottomSheet =
                mainPageBottomSheet.clickDetailsButton(detailPageStepsRes);
        assertThat(((TextView) detailsPageBottomSheet.detailPageTitleElement.value()).getText())
                .isEqualTo(
                        mContext.getString(
                                R.string.tips_promo_bottom_sheet_title_bottom_omnibox_short));
        assertThat(((ButtonCompat) detailsPageBottomSheet.settingsButtonElement.value()).getText())
                .isEqualTo(
                        mContext.getString(R.string.tips_promo_bottom_sheet_positive_button_text));

        mRenderTestRule.render(
                ((View) detailsPageBottomSheet.bottomSheetElement.value()),
                "bottom_omnibox_feature_promo_detail_page");

        SettingsStation<AddressBarSettingsFragment> bottomOmniboxSettings =
                detailsPageBottomSheet.clickBottomOmniboxSettingsButton();
        assertFinalDestination(bottomOmniboxSettings);

        // Return to a PageStation for InitialStateRule to reset properly.
        bottomOmniboxSettings
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
