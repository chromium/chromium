// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.content.Intent;
import android.widget.TextView;

import androidx.appcompat.widget.Toolbar;
import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.scheduler.TipsNotificationsFeatureType;
import org.chromium.chrome.browser.notifications.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.chrome.browser.notifications.tips.TipsUtils;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** Integration tests for tips notifications. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.ANDROID_TIPS_NOTIFICATIONS})
@Batch(Batch.PER_CLASS)
public class TipsNotificationsTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Activity mActivity;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
    }

    @Test
    @MediumTest
    public void testBottomSheetBackButtonAndDismiss() {
        @TipsNotificationsFeatureType
        int featureType = TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING;
        FeatureTipPromoData data = TipsUtils.getFeatureTipPromoDataForType(mActivity, featureType);
        showFeatureTipBottomSheet(featureType, data);

        // Check that clicking the details button on the main page shows the detail page.
        onView(
                        allOf(
                                withId(R.id.tips_promo_details_button),
                                withText(R.string.tips_promo_bottom_sheet_negative_button_text)))
                .check(matches(isDisplayed()))
                .perform(click());
        onViewWaiting(allOf(withId(R.id.details_page_title_text), withText(data.detailPageTitle)))
                .check(matches(isDisplayed()));

        // Check that clicking the back button on the detail page brings back the main page.
        onView(withId(R.id.details_page_back_button))
                .check(matches(isDisplayed()))
                .perform(click());
        onViewWaiting(allOf(withId(R.id.main_page_title_text), withText(data.mainPageTitle)))
                .check(matches(isDisplayed()));

        // Check that clicking the details button on the main page shows the detail page.
        onView(
                        allOf(
                                withId(R.id.tips_promo_details_button),
                                withText(R.string.tips_promo_bottom_sheet_negative_button_text)))
                .check(matches(isDisplayed()))
                .perform(click());
        onViewWaiting(allOf(withId(R.id.details_page_title_text), withText(data.detailPageTitle)))
                .check(matches(isDisplayed()));

        // Check that backpress brings back the main page.
        Espresso.pressBack();
        onViewWaiting(allOf(withId(R.id.main_page_title_text), withText(data.mainPageTitle)))
                .check(matches(isDisplayed()));

        // Check that backpress dismisses the bottom sheet.
        Espresso.pressBack();
        onView(withId(R.id.bottom_sheet)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testESBBottomSheetMainPageAccept() {
        @TipsNotificationsFeatureType
        int featureType = TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING;
        FeatureTipPromoData data = TipsUtils.getFeatureTipPromoDataForType(mActivity, featureType);
        showFeatureTipBottomSheet(featureType, data);

        // Check that clicking the positive button on the main page opens up the safe browsing
        // settings page.
        onView(allOf(withId(R.id.tips_promo_settings_button), withText(data.positiveButtonText)))
                .check(matches(isDisplayed()))
                .perform(click());
        onViewWaiting(
                        allOf(
                                withText(R.string.prefs_section_safe_browsing_title),
                                isAssignableFrom(TextView.class),
                                withParent(isAssignableFrom(Toolbar.class))))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testESBBottomSheetDetailPageAccept() {
        @TipsNotificationsFeatureType
        int featureType = TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING;
        FeatureTipPromoData data = TipsUtils.getFeatureTipPromoDataForType(mActivity, featureType);
        showFeatureTipBottomSheet(featureType, data);

        // Check that clicking the details button on the main page shows the detail page.
        onView(
                        allOf(
                                withId(R.id.tips_promo_details_button),
                                withText(R.string.tips_promo_bottom_sheet_negative_button_text)))
                .check(matches(isDisplayed()))
                .perform(click());
        onViewWaiting(allOf(withId(R.id.details_page_title_text), withText(data.detailPageTitle)))
                .check(matches(isDisplayed()));

        // Check that clicking the positive button on the detail page opens up the safe browsing
        // settings page.
        onView(
                        allOf(
                                withId(R.id.tips_promo_details_settings_button),
                                withText(data.positiveButtonText)))
                .check(matches(isDisplayed()))
                .perform(click());
        onViewWaiting(
                        allOf(
                                withText(R.string.prefs_section_safe_browsing_title),
                                isAssignableFrom(TextView.class),
                                withParent(isAssignableFrom(Toolbar.class))))
                .check(matches(isDisplayed()));
    }

    private void showFeatureTipBottomSheet(
            @TipsNotificationsFeatureType int featureType, FeatureTipPromoData data) {
        Intent intent = IntentHandler.createTrustedOpenNewTabIntent(mActivity, false);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(IntentHandler.EXTRA_TIPS_NOTIFICATION_FEATURE_TYPE, featureType);
        IntentHandler.setTabLaunchType(intent, TabLaunchType.FROM_TIPS_NOTIFICATIONS);
        mActivity.startActivity(intent);

        onViewWaiting(withId(R.id.bottom_sheet)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.main_page_title_text), withText(data.mainPageTitle)))
                .check(matches(isDisplayed()));
        onView(allOf(withId(R.id.main_page_description_text), withText(data.mainPageDescription)))
                .check(matches(isDisplayed()));
    }
}
