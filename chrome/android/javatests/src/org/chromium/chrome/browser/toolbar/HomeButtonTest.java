// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;
import static org.chromium.ui.test.util.ViewUtils.performAccessibilityAction;
import static org.chromium.ui.test.util.ViewUtils.waitForVisibleView;

import android.app.Activity;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.home_button.HomeButton;
import org.chromium.chrome.browser.toolbar.home_button.HomeButtonCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Test related to {@link HomeButton}. TODO: Add more test when features related has update. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class HomeButtonTest {
    private static final String ASSERT_MSG_MENU_IS_CREATED =
            "ContextMenu is not created after long press.";
    private static final String ASSERT_MSG_MENU_SIZE =
            "ContextMenu has a different size than test setting.";

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;

    private HomeButton mHomeButton;
    private HomeButtonCoordinator mHomeButtonCoordinator;
    private int mIdHomeButton;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
        ApplicationTestUtils.waitForActivityState(sActivity, Stage.RESUMED);
    }

    @Before
    public void setUp() {
        // Set the default test status for homepage button tests. By default, the homepage is
        // <b>enabled</b> and with customized URL.
        mHomepageTestRule.useCustomizedHomepageForTest("https://www.chromium.org/");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FrameLayout content = new FrameLayout(sActivity);
                    sActivity.setContentView(content);

                    mIdHomeButton = View.generateViewId();
                    mHomeButton = new HomeButton(sActivity, null);
                    // For a view created in a test, we can make the view not important for
                    // accessibility to prevent failures from AccessibilityChecks. Do not do this
                    // for views outside tests.
                    mHomeButton.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
                    NonNullObservableSupplier<Boolean> homepagePolicySupplier =
                            ObservableSuppliers.alwaysFalse();
                    mHomeButton.setId(mIdHomeButton);
                    mHomeButtonCoordinator =
                            new HomeButtonCoordinator(
                                    sActivity,
                                    mHomeButton,
                                    (metaState, buttonState) -> {},
                                    HomepageManager.getInstance()::onMenuClick,
                                    SupplierUtils.alwaysFalse(),
                                    mThemeColorProvider,
                                    mIncognitoStateProvider,
                                    /* actionRegistry= */ null);
                    SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);

                    content.addView(mHomeButton);
                });
    }

    @Test
    @SmallTest
    public void testContextMenu_AfterConversion() {
        waitForVisibleView(withId(mIdHomeButton));
        ThreadUtils.runOnUiThreadBlocking(() -> assertTrue(mHomeButton.performLongClick()));

        ModelList menu = mHomeButtonCoordinator.getMenuForTesting();
        assertNotNull(ASSERT_MSG_MENU_IS_CREATED, menu);
        assertEquals(ASSERT_MSG_MENU_SIZE, 1, menu.size());

        // Test click on context menu item
        onViewWaiting(
                        allOf(
                                withParent(withId(R.id.menu_list)),
                                hasDescendant(withText(R.string.options_homepage_edit_title))))
                .perform(performAccessibilityAction(AccessibilityNodeInfo.ACTION_CLICK));
        verify(mSettingsNavigation).startSettings(sActivity, HomepageSettings.class);
    }
}
