// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withSpinnerText;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anything;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.lessThan;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.collection.IsMapContaining.hasEntry;

import static org.chromium.android_webview.test.devui.DeveloperUiTestUtils.withCount;

import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.text.SpannableString;
import android.text.style.BackgroundColorSpan;
import android.view.MotionEvent;
import android.view.View;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.test.espresso.DataInteraction;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.NoMatchingRootException;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.DeveloperModeUtils;
import org.chromium.android_webview.common.Flag;
import org.chromium.android_webview.devui.FlagsFragment;
import org.chromium.android_webview.devui.MainActivity;
import org.chromium.android_webview.devui.R;
import org.chromium.android_webview.nonembedded_util.WebViewPackageHelper;
import org.chromium.android_webview.services.DeveloperUiService;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.ui.test.util.ViewUtils;

import java.util.Arrays;
import java.util.Map;

/**
 * UI tests for {@link FlagsFragment}.
 *
 * <p>These tests should not be batched to make sure that the DeveloperUiService is killed after
 * each test, leaving a clean state.
 */
@RunWith(AwJUnit4ClassRunner.class)
@DoNotBatch(reason = "Clean up DeveloperUiService after each test")
public class FlagsFragmentTest {
    @Rule
    public BaseActivityTestRule<MainActivity> mRule =
            new BaseActivityTestRule<>(MainActivity.class);

    private static final Flag[] sMockFlagList = {
        Flag.commandLine(
                "first-switch-for-testing",
                "Fake switch for testing. This is at the start of the mock flag list."),
        Flag.commandLine(
                AwSwitches.HIGHLIGHT_ALL_WEBVIEWS,
                "Highlight the contents (including web contents) of all WebViews with a yellow "
                        + "tint. This is useful for identifying WebViews in an Android "
                        + "application."),
        Flag.commandLine(
                AwSwitches.WEBVIEW_VERBOSE_LOGGING,
                "WebView will log additional debugging information to logcat, such as "
                        + "variations and commandline state."),
        // Validity check: make sure omitting the description doesn't cause any crashes
        Flag.baseFeature("FeatureWithoutDescription"),
        Flag.baseFeature("FakeWebViewFeatureForTesting", "Fake base::Feature for testing."),
        Flag.commandLine(
                "last-switch-for-testing",
                "Fake switch for testing. This is at the end of the mock flag list."),
        // Add new commandline switches and features above. The final entry should have a
        // trailing comma for cleaner diffs.
    };

    @Before
    public void setUp() throws Exception {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(context, MainActivity.class);
        MainActivity.markPopupPermissionRequestedInPrefsForTesting();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FlagsFragment.setFlagListForTesting(sMockFlagList);
                    DeveloperUiService.setFlagListForTesting(sMockFlagList);
                });
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(
                WebViewPackageHelper.getContextPackageInfo(context));

        intent.putExtra(MainActivity.FRAGMENT_ID_INTENT_EXTRA, MainActivity.FRAGMENT_ID_FLAGS);
        mRule.launchActivity(intent);

        waitForInflatedFlagFragment();
    }

    @After
    public void tearDown() {
        // Make sure to clear shared preferences between tests to avoid any saved state.
        DeveloperUiService.clearSharedPrefsForTesting(ContextUtils.getApplicationContext());
    }

    private void waitForInflatedFlagFragment() {
        // Espresso is normally configured to automatically wait for the main thread to go idle, but
        // BaseActivityTestRule turns that behavior off so we must explicitly wait for the View
        // hierarchy to inflate.
        ViewUtils.waitForVisibleView(withId(R.id.navigation_flags_ui));
        ViewUtils.waitForVisibleView(withId(R.id.navigation_home));
        ViewUtils.waitForVisibleView(withId(R.id.flag_search_bar));
        ViewUtils.waitForVisibleView(withId(R.id.flags_list));
        ViewUtils.waitForVisibleView(withId(R.id.reset_flags_button));

        // For some reasons, the blinking Text Cursor can make the UI thread very busy.
        // This leads to AppNotIdleException and flaky tests, because Espresso could not find a 15ms
        // gap between calls to update UI thread. To fix this, we should just hide the edit text
        // cursor. It does not change the test functionality, but will eliminate one source of
        // flakiness.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    EditText searchBar = mRule.getActivity().findViewById(R.id.flag_search_bar);
                    searchBar.setCursorVisible(false);
                });

        // Always close the soft keyboard when the activity is launched which is sometimes shown
        // because flags search TextView has input focus by default. The keyboard may cover up some
        // Views causing test flakiness/failures.
        Espresso.closeSoftKeyboard();
    }

    private CallbackHelper getFlagUiSearchBarListener() {
        final CallbackHelper helper = new CallbackHelper();
        FlagsFragment.setFilterListenerForTesting(
                () -> {
                    helper.notifyCalled();
                });
        return helper;
    }

    private static Matcher<View> withHintText(final Matcher<String> stringMatcher) {
        return new TypeSafeMatcher<View>() {
            @Override
            public boolean matchesSafely(View view) {
                if (!(view instanceof EditText)) {
                    return false;
                }
                String hint = ((EditText) view).getHint().toString();
                return stringMatcher.matches(hint);
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("with hint text: ");
                stringMatcher.describeTo(description);
            }
        };
    }

    private static Matcher<View> withHintText(final String expectedHint) {
        return withHintText(is(expectedHint));
    }

    private static Matcher<View> containingHighlightSpan() {
        return new TypeSafeMatcher<View>() {
            @Override
            public boolean matchesSafely(View view) {
                if (!(view instanceof TextView)) {
                    return false;
                }
                CharSequence text = ((TextView) view).getText();
                if (!(text instanceof SpannableString)) {
                    return false;
                }
                BackgroundColorSpan[] spans =
                        ((SpannableString) text)
                                .getSpans(0, text.length(), BackgroundColorSpan.class);
                return Arrays.stream(spans)
                        .anyMatch(span -> span.getBackgroundColor() == Color.YELLOW);
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("containing highlight span");
            }
        };
    }

    @IntDef({
        CompoundDrawable.START,
        CompoundDrawable.TOP,
        CompoundDrawable.END,
        CompoundDrawable.BOTTOM
    })
    private @interface CompoundDrawable {
        int START = 0;
        int TOP = 1;
        int END = 2;
        int BOTTOM = 3;
    }

    private static Matcher<View> compoundDrawableVisible(@CompoundDrawable int position) {
        return new TypeSafeMatcher<View>() {
            @Override
            public boolean matchesSafely(View view) {
                if (!(view instanceof TextView)) {
                    return false;
                }
                Drawable[] compoundDrawables = ((TextView) view).getCompoundDrawablesRelative();
                Drawable endDrawable = compoundDrawables[position];
                return endDrawable != null;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("with drawable in position " + position);
            }
        };
    }

    // Click a TextView at the start/end/top/bottom. Does not check if any CompoundDrawable drawable
    // is in that position, it just sends a touch event for those coordinates.
    private static void tapCompoundDrawableOnUiThread(
            TextView view, @CompoundDrawable int position) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    long downTime = SystemClock.uptimeMillis();
                    long eventTime = downTime + 50;
                    float x = view.getWidth() / 2.0f;
                    float y = view.getHeight() / 2.0f;
                    if (position == CompoundDrawable.START) {
                        x = 0.0f;
                    } else if (position == CompoundDrawable.END) {
                        x = view.getWidth();
                    } else if (position == CompoundDrawable.TOP) {
                        y = 0.0f;
                    } else if (position == CompoundDrawable.BOTTOM) {
                        y = view.getHeight();
                    }

                    int metaState =
                            0; // no modifier keys (ex. alt/control), this is just a touch event
                    view.dispatchTouchEvent(
                            MotionEvent.obtain(
                                    downTime, eventTime, MotionEvent.ACTION_UP, x, y, metaState));
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHasPublicNoArgsConstructor() throws Throwable {
        FlagsFragment fragment = new FlagsFragment();
        Assert.assertNotNull(fragment);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSearchEmptyByDefault() throws Throwable {
        onView(withId(R.id.flag_search_bar)).check(matches(withText("")));
        onView(withId(R.id.flag_search_bar)).check(matches(withHintText("Search flags")));

        // Magnifier should always be visible, "clear text" icon should be hidden
        onView(withId(R.id.flag_search_bar))
                .check(matches(compoundDrawableVisible(CompoundDrawable.START)));
        onView(withId(R.id.flag_search_bar))
                .check(matches(not(compoundDrawableVisible(CompoundDrawable.END))));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSearchByName() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("verbose-logging"));
        helper.waitForCallback(searchBarChangeCount, 1);
        onView(allOf(withId(R.id.flag_name), withText(AwSwitches.WEBVIEW_VERBOSE_LOGGING)))
                .check(matches(isDisplayed()));
        onView(withId(R.id.flags_list)).check(matches(withCount(1)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSearchByDescription() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("highlight the contents"));
        helper.waitForCallback(searchBarChangeCount, 1);
        onView(allOf(withId(R.id.flag_name), withText(AwSwitches.HIGHLIGHT_ALL_WEBVIEWS)))
                .check(matches(isDisplayed()));
        onView(withId(R.id.flags_list)).check(matches(withCount(1)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSearchMatchingNameAndDescriptionWithIndividualWordsInQuery() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        int searchBarChangeCount = helper.getCallCount();
        // The "verbose" part matches the name, and the "logcat" part matches the description.
        onView(withId(R.id.flag_search_bar)).perform(replaceText("verbose logcat"));
        helper.waitForCallback(searchBarChangeCount, 1);
        onView(allOf(withId(R.id.flag_name), withText(AwSwitches.WEBVIEW_VERBOSE_LOGGING)))
                .check(matches(isDisplayed()));
        onView(withId(R.id.flags_list)).check(matches(withCount(1)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSearchHighlightingQueryWordsInFlagName() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();
        int searchBarChangeCount = helper.getCallCount();
        // "verbose" appears in the flag name, but not the description
        onView(withId(R.id.flag_search_bar)).perform(replaceText("verbose"));
        helper.waitForCallback(searchBarChangeCount, 1);

        Matcher<View> flagNameMatcher =
                allOf(withId(R.id.flag_name), withText(AwSwitches.WEBVIEW_VERBOSE_LOGGING));
        onView(flagNameMatcher).check(matches(containingHighlightSpan()));
        onView(allOf(withId(R.id.flag_description), hasSibling(flagNameMatcher)))
                .check(matches(not(containingHighlightSpan())));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSearchHighlightingQueryWordsInFlagDescription() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();
        int searchBarChangeCount = helper.getCallCount();
        // "logcat" appears in the flag description, but not the name
        onView(withId(R.id.flag_search_bar)).perform(replaceText("logcat"));
        helper.waitForCallback(searchBarChangeCount, 1);

        Matcher<View> flagNameMatcher =
                allOf(withId(R.id.flag_name), withText(AwSwitches.WEBVIEW_VERBOSE_LOGGING));
        onView(flagNameMatcher).check(matches(not(containingHighlightSpan())));
        onView(allOf(withId(R.id.flag_description), hasSibling(flagNameMatcher)))
                .check(matches(containingHighlightSpan()));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSearchHighlightingQueryWordsInFlagNameAndDescription() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();
        int searchBarChangeCount = helper.getCallCount();
        // "log" appears in both the flag name and the description
        onView(withId(R.id.flag_search_bar)).perform(replaceText("log"));
        helper.waitForCallback(searchBarChangeCount, 1);

        Matcher<View> flagNameMatcher =
                allOf(withId(R.id.flag_name), withText(AwSwitches.WEBVIEW_VERBOSE_LOGGING));
        onView(flagNameMatcher).check(matches(containingHighlightSpan()));
        onView(allOf(withId(R.id.flag_description), hasSibling(flagNameMatcher)))
                .check(matches(containingHighlightSpan()));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCaseInsensitive() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("VERBOSE-LOGGING"));
        helper.waitForCallback(searchBarChangeCount, 1);
        onView(allOf(withId(R.id.flag_name), withText(AwSwitches.WEBVIEW_VERBOSE_LOGGING)))
                .check(matches(isDisplayed()));
        onView(withId(R.id.flags_list)).check(matches(withCount(1)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMultipleResults() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        ListView flagsList = mRule.getActivity().findViewById(R.id.flags_list);
        int totalNumFlags = flagsList.getCount();

        // This assumes:
        //  * There will always be > 1 flag which mentions WebView explicitly (ex.
        //    HIGHLIGHT_ALL_WEBVIEWS and WEBVIEW_VERBOSE_LOGGING)
        //  * There will always be >= 1 flag which does not mention WebView in its description (ex.
        //    --show-composited-layer-borders).
        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("webview"));
        helper.waitForCallback(searchBarChangeCount, 1);
        onView(withId(R.id.flags_list))
                .check(matches(withCount(allOf(greaterThan(1), lessThan(totalNumFlags)))));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testClearingSearchShowsAllFlags() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        ListView flagsList = mRule.getActivity().findViewById(R.id.flags_list);
        int totalNumFlags = flagsList.getCount();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("verbose-logging"));
        helper.waitForCallback(searchBarChangeCount, 1);
        onView(withId(R.id.flags_list)).check(matches(withCount(1)));

        onView(withId(R.id.flag_search_bar)).perform(replaceText(""));
        helper.waitForCallback(searchBarChangeCount, 2);
        onView(withId(R.id.flags_list)).check(matches(withCount(totalNumFlags)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testTappingClearButtonClearsText() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("verbose-logging"));
        helper.waitForCallback(searchBarChangeCount, 1);

        // "x" icon should visible if there's some text
        onView(withId(R.id.flag_search_bar))
                .check(matches(compoundDrawableVisible(CompoundDrawable.END)));

        EditText searchBar = mRule.getActivity().findViewById(R.id.flag_search_bar);
        tapCompoundDrawableOnUiThread(searchBar, CompoundDrawable.END);

        // "x" icon disappears when text is cleared
        onView(withId(R.id.flag_search_bar)).check(matches(withText("")));
        onView(withId(R.id.flag_search_bar))
                .check(matches(not(compoundDrawableVisible(CompoundDrawable.END))));

        // Magnifier icon should still be visible
        onView(withId(R.id.flag_search_bar))
                .check(matches(compoundDrawableVisible(CompoundDrawable.START)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testDeletingTextHidesClearTextButton() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("verbose-logging"));
        helper.waitForCallback(searchBarChangeCount, 1);

        // "x" icon should visible if there's some text
        onView(withId(R.id.flag_search_bar))
                .check(matches(compoundDrawableVisible(CompoundDrawable.END)));

        onView(withId(R.id.flag_search_bar)).perform(replaceText(""));

        // "x" icon disappears when text is cleared
        onView(withId(R.id.flag_search_bar)).check(matches(withText("")));
        onView(withId(R.id.flag_search_bar))
                .check(matches(not(compoundDrawableVisible(CompoundDrawable.END))));

        // Magnifier icon should still be visible
        onView(withId(R.id.flag_search_bar))
                .check(matches(compoundDrawableVisible(CompoundDrawable.START)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testElsewhereOnSearchBarDoesNotClearText() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("verbose-logging"));
        helper.waitForCallback(searchBarChangeCount, 1);

        EditText searchBar = mRule.getActivity().findViewById(R.id.flag_search_bar);
        tapCompoundDrawableOnUiThread(searchBar, CompoundDrawable.TOP);

        // EditText should not be cleared
        onView(withId(R.id.flag_search_bar)).check(matches(withText("verbose-logging")));

        // "x" icon is still visible
        onView(withId(R.id.flag_search_bar))
                .check(matches(compoundDrawableVisible(CompoundDrawable.END)));
    }

    /**
     * Toggle a flag's spinner with the given state value, and check that text changes to the
     * correct value.
     *
     * @param flagInteraction the {@link DataInteraction} object representing a flag View item via
     *         {@code onData()}.
     * @param state {@code true} for "enabled", {@code false} for "disabled", {@code null} for
     *         Default.
     * @return the same {@code flagInteraction} passed param for the ease of chaining.
     */
    private DataInteraction toggleFlag(DataInteraction flagInteraction, Boolean state) {
        String stateText = state == null ? "Default" : state ? "Enabled" : "Disabled";
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        // We first select the spinner on the list of flags.
                        // That will make a dialog appear witch the option we wish to select.
                        flagInteraction.onChildView(withId(R.id.flag_toggle)).perform(click());
                        // We then select the state we want from the dialog.
                        onView(withText(stateText)).inRoot(isDialog()).perform(click());
                    } catch (NoMatchingRootException noMatchException) {
                        // Espresso is flaky with dialogs from Spinners.
                        // It seems to rarely not open the dialog.
                        // To avoid this happening, the tests will re-attempt
                        // to select a flag if the root (ie the dialog), was not
                        // found.
                        // This is safe to do because the first click is explicitly on
                        // a flag toggle, and the second click is explicitly in a dialog.
                        // See crbug.com/1400515 for more details.
                        throw new CriteriaNotSatisfiedException(noMatchException);
                    }
                });
        // Finally we confirm that the original spinner was updated after the dialog option has
        // been selected.
        flagInteraction
                .onChildView(withId(R.id.flag_toggle))
                .check(matches(withSpinnerText(containsString(stateText))));

        return flagInteraction;
    }

    /** Verify if the baseFeature flag contains only "Default", "Enabled" , "Disabled" states. */
    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testFlagStates_baseFeature() throws Throwable {
        ListView flagsList = mRule.getActivity().findViewById(R.id.flags_list);

        int firstBaseFeaturePosition = -1;
        for (int i = 1; i < flagsList.getCount(); i++) {
            if (((Flag) flagsList.getAdapter().getItem(i)).isBaseFeature()) {
                firstBaseFeaturePosition = i;
                break;
            }
        }
        Assert.assertNotEquals(
                "Flags list should have at least one base feature flag",
                -1,
                firstBaseFeaturePosition);

        testFlagStatesHelper(firstBaseFeaturePosition);
    }

    /** Verify if the commandline flag contains only "Default", "Enabled" states. */
    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testFlagStates_commandLineFlag() throws Throwable {
        ListView flagsList = mRule.getActivity().findViewById(R.id.flags_list);

        int firstCommandLineFlagPosition = -1;
        for (int i = 1; i < flagsList.getCount(); i++) {
            if (!((Flag) flagsList.getAdapter().getItem(i)).isBaseFeature()) {
                firstCommandLineFlagPosition = i;
                break;
            }
        }
        Assert.assertNotEquals(
                "Flags list should have at least one commandline flag",
                -1,
                firstCommandLineFlagPosition);

        testFlagStatesHelper(firstCommandLineFlagPosition);
    }

    /** Helper method to verify that flag only contains the appropriate states */
    private void testFlagStatesHelper(int flagPosition) {
        ListView flagsList = mRule.getActivity().findViewById(R.id.flags_list);
        Flag flag = (Flag) flagsList.getAdapter().getItem(flagPosition);
        DataInteraction flagInteraction =
                onData(anything()).inAdapterView(withId(R.id.flags_list)).atPosition(flagPosition);

        // click open the spinner containing the states of the commandline flag
        flagInteraction.onChildView(withId(R.id.flag_toggle)).perform(click());

        // assert that the flag contains only the two states
        onView(withText("Default")).check(matches(isDisplayed()));
        onView(withText("Enabled")).check(matches(isDisplayed()));

        if (flag.isBaseFeature()) {
            onView(withText("Disabled")).check(matches(isDisplayed()));
        } else {
            onView(withText("Disabled")).check(doesNotExist());
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testTogglingFlagShowsBlueDot_baseFeature() throws Throwable {
        ListView flagsList = mRule.getActivity().findViewById(R.id.flags_list);

        int firstBaseFeaturePosition = -1;
        for (int i = 1; i < flagsList.getCount(); i++) {
            if (((Flag) flagsList.getAdapter().getItem(i)).isBaseFeature()) {
                firstBaseFeaturePosition = i;
                break;
            }
        }
        Assert.assertNotEquals(
                "Flags list should have at least one baseFeature Flag",
                -1,
                firstBaseFeaturePosition);

        testTogglingFlagShowsBlueDotHelper(firstBaseFeaturePosition);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testTogglingFlagShowsBlueDot_commandLineFlag() throws Throwable {
        ListView flagsList = mRule.getActivity().findViewById(R.id.flags_list);

        int firstCommandLineFlagPosition = -1;
        for (int i = 1; i < flagsList.getCount(); i++) {
            if (!((Flag) flagsList.getAdapter().getItem(i)).isBaseFeature()) {
                firstCommandLineFlagPosition = i;
                break;
            }
        }
        Assert.assertNotEquals(
                "Flags list should have at least one commandline flag",
                -1,
                firstCommandLineFlagPosition);

        testTogglingFlagShowsBlueDotHelper(firstCommandLineFlagPosition);
    }

    private void testTogglingFlagShowsBlueDotHelper(int flagPosition) {
        ListView flagsList = mRule.getActivity().findViewById(R.id.flags_list);
        Flag flag = (Flag) flagsList.getAdapter().getItem(flagPosition);

        DataInteraction flagInteraction =
                onData(anything()).inAdapterView(withId(R.id.flags_list)).atPosition(flagPosition);

        // blue dot should be hidden by default
        flagInteraction
                .onChildView(withId(R.id.flag_name))
                .check(matches(not(compoundDrawableVisible(CompoundDrawable.START))));

        // Test enabling flags shows a bluedot next to flag name
        toggleFlag(flagInteraction, true);
        flagInteraction
                .onChildView(withId(R.id.flag_name))
                .check(matches(compoundDrawableVisible(CompoundDrawable.START)));

        // Test setting to default hide the blue dot
        toggleFlag(flagInteraction, null);
        flagInteraction
                .onChildView(withId(R.id.flag_name))
                .check(matches(not(compoundDrawableVisible(CompoundDrawable.START))));

        // Test disabling flags shows a bluedot next to flag name, only applies to BaseFeatures
        if (flag.isBaseFeature()) {
            toggleFlag(flagInteraction, false);
            flagInteraction
                    .onChildView(withId(R.id.flag_name))
                    .check(matches(compoundDrawableVisible(CompoundDrawable.START)));
        }
        // Test setting to default again hide the blue dot
        toggleFlag(flagInteraction, null);
        flagInteraction
                .onChildView(withId(R.id.flag_name))
                .check(matches(not(compoundDrawableVisible(CompoundDrawable.START))));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testToggledFlagsFloatToTop() throws Throwable {
        ListView flagsList = mRule.getActivity().findViewById(R.id.flags_list);
        int totalNumFlags = flagsList.getCount();
        String lastFlagName = ((Flag) flagsList.getAdapter().getItem(totalNumFlags - 1)).getName();

        // Toggle the last flag in the list.
        toggleFlag(
                onData(anything())
                        .inAdapterView(withId(R.id.flags_list))
                        .atPosition(totalNumFlags - 1),
                true);
        // Navigate from the flags UI then back to it to trigger list sorting.
        onView(withId(R.id.navigation_home)).perform(click());
        onView(withId(R.id.navigation_flags_ui)).perform(click());

        // Check that the toggled flag is now at the top of the list. This assumes that the flags
        // list has > 2 items.
        onData(anything())
                .inAdapterView(withId(R.id.flags_list))
                .atPosition(1)
                .onChildView(withId(R.id.flag_name))
                .check(matches(withText(lastFlagName)));

        // Reset to default.
        toggleFlag(onData(anything()).inAdapterView(withId(R.id.flags_list)).atPosition(1), null);
        // Navigate from the flags UI then back to it to trigger list sorting.
        onView(withId(R.id.navigation_home)).perform(click());
        onView(withId(R.id.navigation_flags_ui)).perform(click());
        // Check that flags goes back to the end of the list when untoggled.
        onData(anything())
                .inAdapterView(withId(R.id.flags_list))
                .atPosition(totalNumFlags - 1)
                .onChildView(withId(R.id.flag_name))
                .check(matches(withText(lastFlagName)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testResetFlags() throws Throwable {
        ListView flagsList = mRule.getActivity().findViewById(R.id.flags_list);
        String firstFlagName = ((Flag) flagsList.getAdapter().getItem(1)).getName();

        toggleFlag(onData(anything()).inAdapterView(withId(R.id.flags_list)).atPosition(1), true);
        Map<String, Boolean> flagOverrides =
                DeveloperModeUtils.getFlagOverrides(DeveloperUiTest.TEST_WEBVIEW_PACKAGE_NAME);
        assertThat(
                "flagOverrides map should contain exactly one entry", flagOverrides.size(), is(1));
        assertThat(flagOverrides, hasEntry(firstFlagName, true));

        onView(withId(R.id.reset_flags_button)).perform(click());

        DataInteraction flagInteraction =
                onData(anything()).inAdapterView(withId(R.id.flags_list)).atPosition(1);
        flagInteraction
                .onChildView(withId(R.id.flag_name))
                .check(matches(not(compoundDrawableVisible(CompoundDrawable.START))));
        flagInteraction
                .onChildView(withId(R.id.flag_toggle))
                .check(matches(withSpinnerText(containsString("Default"))));
        Assert.assertTrue(
                DeveloperModeUtils.getFlagOverrides(DeveloperUiTest.TEST_WEBVIEW_PACKAGE_NAME)
                        .isEmpty());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testResetFlagsByIntent() throws Throwable {
        // 1. First test that the intent resets the flags
        // Given one flag is set
        toggleFlag(onData(anything()).inAdapterView(withId(R.id.flags_list)).atPosition(1), true);
        Assert.assertFalse(
                DeveloperModeUtils.getFlagOverrides(DeveloperUiTest.TEST_WEBVIEW_PACKAGE_NAME)
                        .isEmpty());

        // Close the activity
        ApplicationTestUtils.finishActivity(mRule.getActivity());

        // And when the activity is relaunched with the reset flag
        // which should clear the flags
        Intent intent = new Intent(ContextUtils.getApplicationContext(), MainActivity.class);
        intent.putExtra(MainActivity.FRAGMENT_ID_INTENT_EXTRA, MainActivity.FRAGMENT_ID_FLAGS);
        intent.putExtra(MainActivity.RESET_FLAGS_INTENT_EXTRA, true);
        mRule.launchActivity(intent);
        waitForInflatedFlagFragment();

        // Then the flags will be empty
        DataInteraction flagInteraction =
                onData(anything()).inAdapterView(withId(R.id.flags_list)).atPosition(1);
        flagInteraction
                .onChildView(withId(R.id.flag_name))
                .check(matches(not(compoundDrawableVisible(CompoundDrawable.START))));
        flagInteraction
                .onChildView(withId(R.id.flag_toggle))
                .check(matches(withSpinnerText(containsString("Default"))));
        Assert.assertTrue(
                DeveloperModeUtils.getFlagOverrides(DeveloperUiTest.TEST_WEBVIEW_PACKAGE_NAME)
                        .isEmpty());

        // 2. Then test that the intent will not keep causing resets
        // Given a flag is set again
        toggleFlag(onData(anything()).inAdapterView(withId(R.id.flags_list)).atPosition(1), true);
        Assert.assertFalse(
                DeveloperModeUtils.getFlagOverrides(DeveloperUiTest.TEST_WEBVIEW_PACKAGE_NAME)
                        .isEmpty());

        // And navigate away from flags
        onView(withId(R.id.navigation_home)).perform(click());
        onView(withId(R.id.fragment_home)).check(matches(isDisplayed()));

        // When navigating back to the flags fragment
        onView(withId(R.id.navigation_flags_ui)).perform(click());
        waitForInflatedFlagFragment();

        // Then the flags should still be there
        Assert.assertFalse(
                DeveloperModeUtils.getFlagOverrides(DeveloperUiTest.TEST_WEBVIEW_PACKAGE_NAME)
                        .isEmpty());
    }
}
