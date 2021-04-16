// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isNotChecked;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;

import android.content.res.Configuration;
import android.os.Build;
import android.provider.Settings;
import android.support.test.InstrumentationRegistry;
import android.support.test.uiautomator.UiDevice;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.test.espresso.NoMatchingRootException;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.rule.IntentsTestRule;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;

/** End-to-end tests for PriceTrackingDialog component. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study",
        "force-fieldtrials=Study/Group"})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.DisableFeatures({ChromeFeatureList.START_SURFACE_ANDROID})
public class PriceTrackingDialogTest {
    // clang-format on
    private static final String BASE_PARAMS =
            "force-fieldtrial-params=Study.Group:enable_price_tracking/true";
    private static final String ACTION_APP_NOTIFICATION_SETTINGS =
            "android.settings.APP_NOTIFICATION_SETTINGS";

    private ModalDialogManager mModalDialogManager;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Rule
    public IntentsTestRule<ChromeActivity> mIntentTestRule =
            new IntentsTestRule<>(ChromeActivity.class, false, false);

    @Before
    public void setUp() throws Exception {
        PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(true);
        mActivityTestRule.startMainActivityOnBlankPage();
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
        mModalDialogManager = TestThreadUtils.runOnUiThreadBlockingNoException(
                mActivityTestRule.getActivity()::getModalDialogManager);
        enterTabSwitcher(mActivityTestRule.getActivity());
    }

    @After
    public void tearDown() {
        PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(null);
        ActivityUtils.clearActivityOrientation(mActivityTestRule.getActivity());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testShowAndHidePriceTrackingDialog() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), cta, R.id.track_prices_row_menu_id);
        verifyDialogShowing(cta, false);

        // Press back should dismiss the dialog.
        pressBack();
        verifyDialogHiding(cta);

        // Open the price tracking dialog.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), cta, R.id.track_prices_row_menu_id);
        verifyDialogShowing(cta, false);

        // Click outside of the dialog area to close the Price tracking dialog.
        View dialogView = mModalDialogManager.getCurrentDialogForTest().get(
                ModalDialogProperties.CUSTOM_VIEW);
        int[] location = new int[2];
        // Get the position of the dialog view and click slightly above so that we essentially click
        // on the scrim.
        dialogView.getLocationOnScreen(location);
        UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
                .click(location[0], location[1] / 2);
        verifyDialogHiding(cta);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testTrackPricesSwitch() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), cta, R.id.track_prices_row_menu_id);
        verifyDialogShowing(cta, false);

        onView(withId(R.id.track_prices_switch)).check(matches(isChecked()));
        assertTrue(PriceTrackingUtilities.isTrackPricesOnTabsEnabled());
        onView(withId(R.id.track_prices_switch)).perform(click());
        onView(withId(R.id.track_prices_switch)).check(matches(isNotChecked()));
        assertFalse(PriceTrackingUtilities.isTrackPricesOnTabsEnabled());
        onView(withId(R.id.track_prices_switch)).perform(click());
        onView(withId(R.id.track_prices_switch)).check(matches(isChecked()));
        assertTrue(PriceTrackingUtilities.isTrackPricesOnTabsEnabled());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS + "/enable_price_notification/true"})
    public void testPriceAlertsButton() {
        Intents.init();
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), cta, R.id.track_prices_row_menu_id);
        verifyDialogShowing(cta, true);
        onView(withId(R.id.price_alerts_row_menu_id)).check(matches(isDisplayed()));
        onView(withId(R.id.price_alerts_arrow)).perform(click());

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            intended(hasAction(ACTION_APP_NOTIFICATION_SETTINGS));
        } else {
            intended(hasAction(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS));
        }
        Intents.release();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS + "/enable_price_notification/true"})
    public void testPriceAlertsRowMenuVisibility() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        // The price alerts row menu should be invisible if user doesn't sign in.
        PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(false);
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), cta, R.id.track_prices_row_menu_id);
        verifyDialogShowing(cta, false);

        pressBack();
        verifyDialogHiding(cta);

        // When user signs in, the price alerts row menu should be visible.
        PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(true);
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), cta, R.id.track_prices_row_menu_id);
        verifyDialogShowing(cta, true);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @CommandLineFlags.Add({BASE_PARAMS + "/enable_price_notification/true"})
    public void testRenderPriceTrackingDialog_Portrait() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), cta, R.id.track_prices_row_menu_id);
        verifyDialogShowing(cta, true);

        View priceTrackingDialogView = mModalDialogManager.getCurrentDialogForTest().get(
                ModalDialogProperties.CUSTOM_VIEW);
        mRenderTestRule.render(priceTrackingDialogView, "price_tracking_dialog_portrait");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @CommandLineFlags.Add({BASE_PARAMS + "/enable_price_notification/true"})
    public void testRenderPriceTrackingDialog_Landscape() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        ActivityUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), cta, R.id.track_prices_row_menu_id);
        verifyDialogShowing(cta, true);

        View priceTrackingDialogView = mModalDialogManager.getCurrentDialogForTest().get(
                ModalDialogProperties.CUSTOM_VIEW);
        mRenderTestRule.render(priceTrackingDialogView, "price_tracking_dialog_landscape");
    }

    private void verifyDialogShowing(
            ChromeTabbedActivity cta, boolean isPriceAlertsRowMenuVisible) {
        // Verify price tracking dialog view.
        onView(withId(R.id.price_tracking_dialog))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    String title = cta.getString(R.string.price_tracking_settings);
                    assertEquals(title, ((TextView) v.findViewById(R.id.title)).getText());

                    String trackPricesTitle = cta.getString(R.string.track_prices_on_tabs);
                    assertEquals(trackPricesTitle,
                            ((TextView) v.findViewById(R.id.track_prices_title)).getText());
                    String trackPricesDescription =
                            cta.getString(R.string.track_prices_on_tabs_description);
                    assertEquals(trackPricesDescription,
                            ((TextView) v.findViewById(R.id.track_prices_description)).getText());

                    String priceAlertsTitle = cta.getString(R.string.price_drop_alerts);
                    assertEquals(priceAlertsTitle,
                            ((TextView) v.findViewById(R.id.price_alerts_title)).getText());
                    String priceAlertsDescription =
                            cta.getString(R.string.price_drop_alerts_description);
                    assertEquals(priceAlertsDescription,
                            ((TextView) v.findViewById(R.id.price_alerts_description)).getText());

                    if (isPriceAlertsRowMenuVisible) {
                        assertEquals(View.VISIBLE,
                                ((ViewGroup) v.findViewById(R.id.price_alerts_row_menu_id))
                                        .getVisibility());
                    } else {
                        assertEquals(View.GONE,
                                ((ViewGroup) v.findViewById(R.id.price_alerts_row_menu_id))
                                        .getVisibility());
                    }
                });
    }

    private void verifyDialogHiding(ChromeTabbedActivity cta) {
        boolean isShowing = true;
        try {
            onView(withId(R.id.price_tracking_dialog))
                    .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                    .check(matches(isDisplayed()));
        } catch (NoMatchingRootException e) {
            isShowing = false;
        } catch (Exception e) {
            assert false : "error when inspecting price tracking dialog.";
        }
        assertFalse(isShowing);
    }
}
