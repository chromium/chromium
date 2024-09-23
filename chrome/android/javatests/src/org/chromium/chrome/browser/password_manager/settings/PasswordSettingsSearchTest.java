// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.openActionBarOverflowOrOptionsMenu;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.closeSoftKeyboard;
import static androidx.test.espresso.action.ViewActions.typeText;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anyOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.nullValue;
import static org.hamcrest.Matchers.sameInstance;

import static org.chromium.chrome.browser.password_manager.settings.PasswordSettingsTestHelper.ARES_AT_OLYMP;
import static org.chromium.chrome.browser.password_manager.settings.PasswordSettingsTestHelper.DEIMOS_AT_OLYMP;
import static org.chromium.chrome.browser.password_manager.settings.PasswordSettingsTestHelper.GREEK_GODS;
import static org.chromium.chrome.browser.password_manager.settings.PasswordSettingsTestHelper.HADES_AT_UNDERWORLD;
import static org.chromium.chrome.browser.password_manager.settings.PasswordSettingsTestHelper.PHOBOS_AT_OLYMP;
import static org.chromium.chrome.browser.password_manager.settings.PasswordSettingsTestHelper.ZEUS_ON_EARTH;
import static org.chromium.ui.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.ui.test.util.ViewUtils.VIEW_INVISIBLE;
import static org.chromium.ui.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Instrumentation;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.ColorFilter;
import android.graphics.drawable.Drawable;
import android.view.MenuItem;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.IdRes;
import androidx.annotation.StringRes;
import androidx.appcompat.view.menu.ActionMenuItemView;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.test.espresso.Espresso;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.history.HistoryActivity;
import org.chromium.chrome.browser.history.HistoryContentManager;
import org.chromium.chrome.browser.history.StubbedHistoryProvider;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.ui.test.util.ViewUtils;

import java.util.Date;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for the search feature of the "Passwords" settings screen. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordSettingsSearchTest {
    private static final long UI_UPDATING_TIMEOUT_MS = 3000;

    @Rule
    public BaseActivityTestRule<HistoryActivity> mHistoryActivityTestRule =
            new BaseActivityTestRule<>(HistoryActivity.class);

    @Rule
    public SettingsActivityTestRule<PasswordSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PasswordSettings.class);

    @Mock private PasswordCheck mPasswordCheck;

    private final PasswordSettingsTestHelper mTestHelper = new PasswordSettingsTestHelper();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PasswordCheckFactory.setPasswordCheckForTesting(mPasswordCheck);
    }

    @After
    public void tearDown() {
        mTestHelper.tearDown();
    }

    /** Check that the search item is visible in the action bar. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @SuppressWarnings("AlwaysShowAction") // We need to ensure the icon is in the action bar.
    public void testSearchIconVisibleInActionBar() {
        mTestHelper.setPasswordSource(null); // Initialize empty preferences.
        mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);
        onViewWaiting(withText(R.string.password_manager_settings_title));
        PasswordSettings f = mSettingsActivityTestRule.getFragment();

        // Force the search option into the action bar.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    f.getMenuForTesting()
                            .findItem(R.id.menu_id_search)
                            .setShowAsAction(MenuItem.SHOW_AS_ACTION_ALWAYS);
                });

        onView(withId(R.id.menu_id_search)).check(matches(isDisplayed()));
    }

    /** Check that the search item is visible in the overflow menu. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchTextInOverflowMenuVisible() {
        mTestHelper.setPasswordSource(
                null); // Initialize empty preferences.mSettingsActivityTestRule
        mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);
        onViewWaiting(withText(R.string.password_manager_settings_title));
        PasswordSettings f = mSettingsActivityTestRule.getFragment();

        // Force the search option into the overflow menu.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    f.getMenuForTesting()
                            .findItem(R.id.menu_id_search)
                            .setShowAsAction(MenuItem.SHOW_AS_ACTION_NEVER);
                });

        // Open the overflow menu.
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        onView(withText(R.string.search)).check(matches(isDisplayed()));
    }

    /**
     * Check that searching doesn't push the help icon into the overflow menu permanently. On screen
     * sizes where the help item starts out in the overflow menu, ensure it stays there.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testTriggeringSearchRestoresHelpIcon() {
        mTestHelper.setPasswordSource(null);
        mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);
        onViewWaiting(withText(R.string.password_manager_settings_title));

        // Retrieve the initial status and ensure that the help option is there at all.
        final AtomicReference<Boolean> helpInOverflowMenu = new AtomicReference<>(false);
        onView(withId(R.id.menu_id_targeted_help))
                .check(
                        (helpMenuItem, e) -> {
                            ActionMenuItemView view = (ActionMenuItemView) helpMenuItem;
                            helpInOverflowMenu.set(view == null || !view.showsIcon());
                        });
        if (helpInOverflowMenu.get()) {
            openActionBarOverflowOrOptionsMenu(
                    InstrumentationRegistry.getInstrumentation().getTargetContext());
            onView(withText(R.string.menu_help)).check(matches(isDisplayed()));
            Espresso.pressBack(); // to close the Overflow menu.
        } else {
            onView(withId(R.id.menu_id_targeted_help)).check(matches(isDisplayed()));
        }

        // Trigger the search, close it and wait for UI to be restored.
        onView(withSearchMenuIdOrText()).perform(click());
        onView(withContentDescription(R.string.abc_action_bar_up_description)).perform(click());
        onViewWaiting(withText(R.string.password_manager_settings_title));

        // Check that the help option is exactly where it was to begin with.
        if (helpInOverflowMenu.get()) {
            openActionBarOverflowOrOptionsMenu(
                    InstrumentationRegistry.getInstrumentation().getTargetContext());
            onView(withText(R.string.menu_help)).check(matches(isDisplayed()));
            onView(withId(R.id.menu_id_targeted_help)).check(doesNotExist());
        } else {
            onView(withText(R.string.menu_help)).check(doesNotExist());
            onView(withId(R.id.menu_id_targeted_help)).check(matches(isDisplayed()));
        }
    }

    /** Check that the search filters the list by name. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchFiltersByUserName() {
        mTestHelper.setPasswordSourceWithMultipleEntries(GREEK_GODS);
        mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        // Search for a string matching multiple user names. Case doesn't need to match.
        onView(withSearchMenuIdOrText()).perform(click());
        onView(withId(R.id.search_src_text))
                .perform(click(), typeText("aREs"), closeSoftKeyboard());

        onView(withText(ARES_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        onView(withText(PHOBOS_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        onView(withText(DEIMOS_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        onView(withText(ZEUS_ON_EARTH.getUserName())).check(doesNotExist());
        onView(withText(HADES_AT_UNDERWORLD.getUrl())).check(doesNotExist());
    }

    /** Check that the search filters the list by URL. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchFiltersByUrl() {
        mTestHelper.setPasswordSourceWithMultipleEntries(GREEK_GODS);
        mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        // Search for a string that matches multiple URLs. Case doesn't need to match.
        onView(withSearchMenuIdOrText()).perform(click());
        onView(withId(R.id.search_src_text))
                .perform(click(), typeText("Olymp"), closeSoftKeyboard());

        onView(withText(ARES_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        onView(withText(PHOBOS_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        onView(withText(DEIMOS_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        onView(withText(ZEUS_ON_EARTH.getUserName())).check(doesNotExist());
        onView(withText(HADES_AT_UNDERWORLD.getUrl())).check(doesNotExist());
    }

    /** Check that the search filters the list by URL. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchDisplaysNoResultMessageIfSearchTurnsUpEmpty() {
        mTestHelper.setPasswordSourceWithMultipleEntries(GREEK_GODS);
        mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        // Open the search which should hide the Account link.
        onView(withSearchMenuIdOrText()).perform(click());

        // Search for a string that matches nothing which should leave the results entirely blank.
        onView(withId(R.id.search_src_text))
                .perform(click(), typeText("Mars"), closeSoftKeyboard());

        for (SavedPasswordEntry god : GREEK_GODS) {
            onView(allOf(withText(god.getUserName()), withText(god.getUrl())))
                    .check(doesNotExist());
        }
        onView(withText(R.string.saved_passwords_none_text)).check(doesNotExist());
        // Check that the section header for saved passwords is not present. Do not confuse it with
        // the toolbar label which contains the same string, look for the one inside a linear
        // layout.
        onView(
                        allOf(
                                withParent(isAssignableFrom(LinearLayout.class)),
                                withText(R.string.password_manager_settings_title)))
                .check(doesNotExist());
        // Check the message for no result.
        onView(withText(R.string.password_no_result)).check(matches(isDisplayed()));
    }

    /** Check that triggering the search hides all non-password prefs. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchIconClickedHidesExceptionsTemporarily() {
        mTestHelper.setPasswordExceptions(new String[] {"http://exclu.de", "http://not-inclu.de"});
        mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        onView(withText(R.string.section_saved_passwords_exceptions)).check(matches(isDisplayed()));

        onView(withSearchMenuIdOrText()).perform(click());
        onView(withId(R.id.search_src_text)).perform(click(), closeSoftKeyboard());

        onView(withText(R.string.section_saved_passwords_exceptions)).check(doesNotExist());

        onView(withContentDescription(R.string.abc_action_bar_up_description)).perform(click());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync(); // Close search view.

        onView(withText(R.string.section_saved_passwords_exceptions)).check(matches(isDisplayed()));
    }

    /** Check that triggering the search hides all non-password prefs. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchIconClickedHidesGeneralPrefs() {
        mTestHelper.setPasswordSource(ZEUS_ON_EARTH);
        mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);
        final PasswordSettings prefs = mSettingsActivityTestRule.getFragment();
        final AtomicReference<Boolean> menuInitiallyVisible = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        menuInitiallyVisible.set(
                                prefs.getToolbarForTesting().isOverflowMenuShowing()));

        onView(withText(R.string.password_settings_save_passwords)).check(matches(isDisplayed()));

        if (menuInitiallyVisible.get()) { // Check overflow menu only on large screens that have it.
            onView(withContentDescription(R.string.abc_action_menu_overflow_description))
                    .check(matches(isDisplayed()));
        }

        onView(withSearchMenuIdOrText()).perform(click());

        onView(withText(R.string.password_settings_save_passwords)).check(doesNotExist());
        ViewUtils.waitForViewCheckingState(
                withParent(withContentDescription(R.string.abc_action_menu_overflow_description)),
                VIEW_INVISIBLE | VIEW_GONE | VIEW_NULL);

        onView(withContentDescription(R.string.abc_action_bar_up_description)).perform(click());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        if (menuInitiallyVisible.get()) { // If the overflow menu was there, it should be restored.
            onView(withContentDescription(R.string.abc_action_menu_overflow_description))
                    .check(matches(isDisplayed()));
        }
    }

    /** Check that closing the search via back button brings back all non-password prefs. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchBarBackButtonRestoresGeneralPrefs() {
        mTestHelper.setPasswordSourceWithMultipleEntries(GREEK_GODS);
        mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        onView(withSearchMenuIdOrText()).perform(click());
        onView(withId(R.id.search_src_text)).perform(click(), typeText("Zeu"), closeSoftKeyboard());

        onView(withText(R.string.password_settings_save_passwords)).check(doesNotExist());

        onView(withContentDescription(R.string.abc_action_bar_up_description)).perform(click());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        onView(withText(R.string.password_settings_save_passwords)).check(matches(isDisplayed()));

        onView(withId(R.id.menu_id_search)).check(matches(isDisplayed()));
    }

    /** Check that clearing the search also hides the clear button. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchViewCloseIconExistsOnlyToClearQueries() {
        mTestHelper.setPasswordSourceWithMultipleEntries(GREEK_GODS);
        mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        // Trigger search which shouldn't have the button yet.
        onView(withSearchMenuIdOrText()).perform(click());
        ViewUtils.waitForViewCheckingState(
                withId(R.id.search_close_btn), VIEW_INVISIBLE | VIEW_GONE | VIEW_NULL);

        // Type something and see the button appear.
        onView(withId(R.id.search_src_text))
                // Trigger search which shouldn't have the button yet.
                .perform(click(), typeText("Zeu"), closeSoftKeyboard());
        onView(withId(R.id.search_close_btn)).check(matches(isDisplayed()));

        // Clear the search which should hide the button again.
        onView(withId(R.id.search_close_btn)).perform(click()); // Clear search.
        ViewUtils.waitForViewCheckingState(
                withId(R.id.search_close_btn), VIEW_INVISIBLE | VIEW_GONE | VIEW_NULL);
    }

    /**
     * Check that the changed color of the loaded Drawable does not persist for other uses of the
     * drawable. This is not implicitly true as a loaded Drawable is by default only a reference to
     * the globally defined resource.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchIconColorAffectsOnlyLocalSearchDrawable() {
        // Open the password preferences and remember the applied color filter.
        mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);
        final PasswordSettings f = mSettingsActivityTestRule.getFragment();
        onView(withId(R.id.search_button)).check(matches(isDisplayed()));
        final AtomicReference<ColorFilter> passwordSearchFilter = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Drawable drawable =
                            f.getMenuForTesting().findItem(R.id.menu_id_search).getIcon();
                    passwordSearchFilter.set(DrawableCompat.getColorFilter(drawable));
                });

        // Now launch a non-empty History activity.
        StubbedHistoryProvider mHistoryProvider = new StubbedHistoryProvider();
        mHistoryProvider.addItem(StubbedHistoryProvider.createHistoryItem(0, new Date().getTime()));
        mHistoryProvider.addItem(StubbedHistoryProvider.createHistoryItem(1, new Date().getTime()));
        HistoryContentManager.setProviderForTests(mHistoryProvider);
        mHistoryActivityTestRule.launchActivity(null);

        // Find the search view to ensure that the set color filter is different from the saved one.
        final AtomicReference<ColorFilter> historySearchFilter = new AtomicReference<>();
        onView(withId(R.id.search_menu_id)).check(matches(isDisplayed()));
        onView(withId(R.id.search_menu_id))
                .check(
                        (searchMenuItem, e) -> {
                            Drawable drawable =
                                    ((ActionMenuItemView) searchMenuItem).getItemData().getIcon();
                            historySearchFilter.set(DrawableCompat.getColorFilter(drawable));
                            assertThat(
                                    historySearchFilter.get(),
                                    anyOf(
                                            is(nullValue()),
                                            is(not(sameInstance(passwordSearchFilter.get())))));
                        });

        // Close the activity and check that the icon in the password preferences has not changed.
        mHistoryActivityTestRule.getActivity().finish();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ColorFilter colorFilter =
                            DrawableCompat.getColorFilter(
                                    f.getMenuForTesting().findItem(R.id.menu_id_search).getIcon());
                    assertThat(
                            colorFilter,
                            anyOf(is(nullValue()), is(sameInstance(passwordSearchFilter.get()))));
                    assertThat(
                            colorFilter,
                            anyOf(
                                    is(nullValue()),
                                    is(not(sameInstance(historySearchFilter.get())))));
                });
    }

    /**
     * Check that the filtered password list persists after the user had inspected a single result.
     *
     * <p>TODO(crbug.com/40763233): Move this test to a full integration test which spins up native
     * and actually has stored passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisabledTest(message = "crbug/1202907 - The edit UI is now launched via native.")
    @RequiresRestart("crbug/1137002 - Figure out why this flakes as a batched test.")
    public void testSearchResultsPersistAfterEntryInspection() {
        mTestHelper.setPasswordSourceWithMultipleEntries(GREEK_GODS);
        mTestHelper.setPasswordExceptions(new String[] {"http://exclu.de", "http://not-inclu.de"});
        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);
        mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        // Open the search and filter all but "Zeus".
        onView(withSearchMenuIdOrText()).perform(click());

        onViewWaiting(withId(R.id.search_src_text));
        onView(withId(R.id.search_src_text)).perform(click(), typeText("Zeu"), closeSoftKeyboard());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        onView(withText(R.string.passwords_auto_signin_title)).check(doesNotExist());
        onView(withText(ZEUS_ON_EARTH.getUserName())).check(matches(isDisplayed()));
        onView(withText(PHOBOS_AT_OLYMP.getUserName())).check(doesNotExist());
        onView(withText(HADES_AT_UNDERWORLD.getUrl())).check(doesNotExist());

        // Click "Zeus" to open edit field and verify the password. Pretend the user just passed the
        // reauthentication challenge.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.ReauthScope.ONE_AT_A_TIME);
        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(new IntentFilter(Intent.ACTION_VIEW), null, false);
        onView(withText(ZEUS_ON_EARTH.getUserName())).perform(click());
        monitor.waitForActivityWithTimeout(UI_UPDATING_TIMEOUT_MS);
        Assert.assertEquals("Monitor for has not been called", 1, monitor.getHits());
        InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
        onView(withContentDescription(R.string.password_entry_viewer_show_stored_password))
                .perform(click());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        onView(withText(ZEUS_ON_EARTH.getPassword())).check(matches(isDisplayed()));
        onView(withContentDescription(R.string.abc_action_bar_up_description))
                .perform(click()); // Go back to the search list.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        onView(withText(R.string.passwords_auto_signin_title)).check(doesNotExist());
        onView(withText(ZEUS_ON_EARTH.getUserName())).check(matches(isDisplayed()));
        onView(withText(PHOBOS_AT_OLYMP.getUserName())).check(doesNotExist());
        onView(withText(HADES_AT_UNDERWORLD.getUrl())).check(doesNotExist());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // The search bar should still be open and still display the search query.
        onViewWaiting(allOf(withId(R.id.search_src_text), withText("Zeu")));
        onView(withId(R.id.search_src_text)).check(matches(withText("Zeu")));
    }

    /**
     * Looks for the search icon by id or by its title.
     *
     * @return Returns either the icon button or the menu option.
     */
    private static Matcher<View> withSearchMenuIdOrText() {
        return withMenuIdOrText(R.id.menu_id_search, R.string.search);
    }

    /**
     * Looks for the icon by id. If it cannot be found, it's probably hidden in the overflow menu.
     * In that case, open the menu and search for its title.
     *
     * @return Returns either the icon button or the menu option.
     */
    private static Matcher<View> withMenuIdOrText(@IdRes int actionId, @StringRes int actionLabel) {
        Matcher<View> matcher = withId(actionId);
        try {
            Espresso.onView(matcher).check(matches(isDisplayed()));
            return matcher;
        } catch (Exception NoMatchingViewException) {
            openActionBarOverflowOrOptionsMenu(
                    InstrumentationRegistry.getInstrumentation().getTargetContext());
            return withText(actionLabel);
        }
    }
}
