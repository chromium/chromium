// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.io.IOException;

/**
 * Tests for quick delete dialog view.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.QUICK_DELETE_FOR_ANDROID})
@Batch(Batch.PER_CLASS)
public class QuickDeleteDialogDelegateTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.PRIVACY)
                    .build();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testQuickDeleteDialogView() throws IOException {
        openQuickDeleteDialog();

        onView(withText(R.string.quick_delete_dialog_title)).check(matches(isDisplayed()));
        onView(withText(R.string.quick_delete_dialog_description)).check(matches(isDisplayed()));
        onView(withText(R.string.clear_history_title)).check(matches(isDisplayed()));
        onView(withText(R.string.clear_cookies_and_site_data_title)).check(matches(isDisplayed()));
        onView(withText(R.string.clear_cache_title)).check(matches(isDisplayed()));

        // TODO(crbug.com/1412087): Get the full dialog for render test instead of just the custom
        // view.
        View dialogView = mActivityTestRule.getActivity()
                                  .getModalDialogManager()
                                  .getCurrentDialogForTest()
                                  .get(ModalDialogProperties.CUSTOM_VIEW);
        mRenderTestRule.render(dialogView, "quick_delete_dialog");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testSearchHistoryDisambiguationTextShown_WhenUserIsSignedIn() throws IOException {
        mSigninTestRule.addTestAccountThenSignin();
        openQuickDeleteDialog();

        onView(withId(R.id.search_history_disambiguation)).check(matches(isDisplayed()));

        View dialogView = mActivityTestRule.getActivity()
                                  .getModalDialogManager()
                                  .getCurrentDialogForTest()
                                  .get(ModalDialogProperties.CUSTOM_VIEW);

        mRenderTestRule.render(dialogView, "quick_delete_dialog-signed-in");
    }

    private void openQuickDeleteDialog() {
        // Open 3 dot menu.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });
        onViewWaiting(withId(R.id.app_menu_list))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        // Click on quick delete menu item.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.callOnItemClick(
                    mActivityTestRule.getAppMenuCoordinator(), R.id.quick_delete_menu_id);
        });
    }
}
