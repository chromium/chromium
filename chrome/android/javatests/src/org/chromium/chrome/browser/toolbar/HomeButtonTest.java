// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.view.View;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.toolbar.home_button.HomeButton;
import org.chromium.chrome.browser.toolbar.home_button.HomeButtonCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/** Test related to {@link HomeButton}. TODO: Add more test when features related has update. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class HomeButtonTest extends BlankUiTestActivityTestCase {
    private static final String ASSERT_MSG_MENU_IS_CREATED =
            "ContextMenu is not created after long press.";
    private static final String ASSERT_MSG_MENU_SIZE =
            "ContextMenu has a different size than test setting.";

    @Rule public HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    @Mock private SettingsNavigation mSettingsNavigation;

    private HomeButtonCoordinator mHomeButtonCoordinator;
    private int mIdHomeButton;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        MockitoAnnotations.initMocks(this);

        // Set the default test status for homepage button tests.
        // By default, the homepage is <b>enabled</b> and with customized URL.
        mHomepageTestRule.useCustomizedHomepageForTest("https://www.chromium.org/");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FrameLayout content = new FrameLayout(getActivity());
                    getActivity().setContentView(content);

                    mIdHomeButton = View.generateViewId();
                    HomeButton homeButton = new HomeButton(getActivity(), null);
                    // For a view created in a test, we can make the view not important for
                    // accessibility to prevent failures from AccessibilityChecks. Do not do this
                    // for views outside tests.
                    homeButton.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
                    ObservableSupplierImpl<Boolean> homepagePolicySupplier =
                            new ObservableSupplierImpl<>();
                    homepagePolicySupplier.set(false);
                    homeButton.setId(mIdHomeButton);
                    mHomeButtonCoordinator =
                            new HomeButtonCoordinator(
                                    getActivity(),
                                    homeButton,
                                    HomepageManager.getInstance()::onMenuClick,
                                    () -> false);
                    SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);

                    content.addView(homeButton);
                });
    }

    @Test
    @SmallTest
    public void testContextMenu_AfterConversion() {
        onView(withId(mIdHomeButton)).perform(longClick());

        ModelList menu = mHomeButtonCoordinator.getMenuForTesting();
        Assert.assertNotNull(ASSERT_MSG_MENU_IS_CREATED, menu);
        Assert.assertEquals(ASSERT_MSG_MENU_SIZE, 1, menu.size());

        // Test click on context menu item
        onView(withText(R.string.options_homepage_edit_title)).perform(click());
        Mockito.verify(mSettingsNavigation).startSettings(getActivity(), HomepageSettings.class);
    }
}
