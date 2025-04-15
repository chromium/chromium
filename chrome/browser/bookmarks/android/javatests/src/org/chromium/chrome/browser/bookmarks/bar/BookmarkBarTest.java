// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static android.view.KeyEvent.KEYCODE_ENTER;
import static android.view.KeyEvent.META_ALT_ON;
import static android.view.KeyEvent.META_CTRL_ON;
import static android.view.KeyEvent.META_SHIFT_ON;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isFocusable;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.endsWith;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.notNullValue;
import static org.hamcrest.Matchers.nullValue;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.res.Configuration;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewStub;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.action.EspressoKey;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Optional;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

/** Integration tests for the bookmark bar feature. */
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
@Restriction({DeviceFormFactor.TABLET, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
@RunWith(ChromeJUnit4ClassRunner.class)
public class BookmarkBarTest {

    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private BookmarkModel mModel;
    private BookmarkId mDesktopFolderId;
    private @Nullable List<BookmarkId> mItemIds;

    @Before
    public void setUp() {
        mCtaTestRule.startOnBlankPage();

        BookmarkBarUtils.setFeatureAllowedForTesting(true);
        ThreadUtils.runOnUiThreadBlocking(() -> setBookmarkBarSetting(/* enabled= */ true));
        waitForBookmarkBarVisibility(/* visible= */ true);
        BookmarkTestUtil.waitForBookmarkModelLoaded();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel = mCtaTestRule.getActivity().getBookmarkModelForTesting();
                    mModel.removeAllUserBookmarks();
                    mDesktopFolderId = mModel.getDesktopFolderId();
                });
    }

    @After
    public void tearDown() {
        if (mItemIds != null && !mItemIds.isEmpty()) {
            ThreadUtils.runOnUiThreadBlocking(() -> mItemIds.forEach(mModel::deleteBookmark));
            mItemIds = null;
        }
    }

    @Test
    @MediumTest
    public void testOnAllBookmarksButtonClick() {
        onViewDisplayed(bookmarkBarItemWithText("All Bookmarks")).perform(click());
        onViewDisplayed(bookmarkManagerToolbarWithText("Bookmarks"));
    }

    @Test
    @MediumTest
    public void testOnBookmarkBarToggledViaKeyboard() {
        final var activity = mCtaTestRule.getActivity();
        final var evt =
                new KeyEvent(
                        /* downTime= */ SystemClock.uptimeMillis(),
                        /* eventTime= */ SystemClock.uptimeMillis(),
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_B,
                        /* repeat= */ 0,
                        KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON);

        // Set up.
        ThreadUtils.runOnUiThreadBlocking(() -> setBookmarkBarSetting(/* enabled= */ false));
        waitForBookmarkBarVisibility(/* visible= */ false);

        // Case: Toggle w/ feature allowed.
        ThreadUtils.runOnUiThreadBlocking(() -> activity.onKeyDown(evt.getKeyCode(), evt));
        waitForBookmarkBarVisibility(/* visible= */ true);
        ThreadUtils.runOnUiThreadBlocking(() -> activity.onKeyDown(evt.getKeyCode(), evt));
        waitForBookmarkBarVisibility(/* visible= */ false);

        // Case: Toggle w/ feature disallowed.
        BookmarkBarUtils.setFeatureAllowedForTesting(false);
        ThreadUtils.runOnUiThreadBlocking(() -> activity.onKeyDown(evt.getKeyCode(), evt));
        waitForBookmarkBarVisibility(/* visible= */ false);
    }

    @Test
    @MediumTest
    public void testOnBookmarkFolderClick() throws ExecutionException {
        final String title = "Folder";
        mItemIds = List.of(addFolder(title));
        onViewWaiting(bookmarkBarItemWithText(title)).perform(click());
        onViewWaiting(bookmarkManagerToolbarWithText(title)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testOnBookmarkItemClick() throws ExecutionException {
        final Tab originalTab = getCurrentTab();
        final String title = "Google";
        final GURL url = getTestServerUrl("/chrome/test/data/android/google.html");
        mItemIds = List.of(addBookmark(/* index= */ 0, title, url));
        onViewWaiting(bookmarkBarItemWithText(title)).perform(click());
        CriteriaHelper.pollUiThread(
                () -> {
                    final Tab currentTab = getCurrentTab();
                    Criteria.checkThat(currentTab, is(originalTab));
                    Criteria.checkThat(currentTab, notNullValue());
                    Criteria.checkThat(currentTab.getUrl(), notNullValue());
                    Criteria.checkThat(currentTab.getUrl(), is(url));
                });
    }

    @Test
    @MediumTest
    public void testOnBookmarkItemControlClick() throws ExecutionException {
        // Set up.
        final String title = "Google";
        final GURL url = getTestServerUrl("/chrome/test/data/android/google.html");
        mItemIds = List.of(addBookmark(/* index= */ 0, title, url));

        {
            // Case: perform control-click via touch.
            final Tab originalTab = getCurrentTab();
            final GURL originalUrl = originalTab.getUrl();
            onViewWaiting(bookmarkBarItemWithText(title)).perform(clickWith(META_CTRL_ON));
            CriteriaHelper.pollUiThread(
                    () -> {
                        final Tab currentTab = getCurrentTab();
                        Criteria.checkThat(currentTab, is(originalTab));
                        Criteria.checkThat(currentTab, notNullValue());
                        Criteria.checkThat(currentTab.getUrl(), notNullValue());
                        Criteria.checkThat(currentTab.getUrl(), is(originalUrl));

                        final Tab lastTab = getLastTab();
                        Criteria.checkThat(lastTab, is(not(currentTab)));
                        Criteria.checkThat(lastTab, notNullValue());
                        Criteria.checkThat(lastTab.getUrl(), notNullValue());
                        Criteria.checkThat(lastTab.getUrl(), is(url));
                    });
        }

        {
            // Case: perform ENTER-click to ensure control-click via touch is not sticky.
            final Tab originalTab = getCurrentTab();
            onViewWaiting(bookmarkBarItemWithText(title)).perform(focus(), pressKey(KEYCODE_ENTER));
            CriteriaHelper.pollUiThread(
                    () -> {
                        final Tab currentTab = getCurrentTab();
                        Criteria.checkThat(currentTab, is(originalTab));
                        Criteria.checkThat(currentTab, notNullValue());
                        Criteria.checkThat(currentTab.getUrl(), notNullValue());
                        Criteria.checkThat(currentTab.getUrl(), is(url));
                    });
        }
    }

    @Test
    @MediumTest
    public void testOnConfigurationChange() {
        waitForBookmarkBarVisibility(/* visible= */ true);

        // Case: Configuration changed to disallow feature.
        BookmarkBarUtils.setFeatureAllowedForTesting(false);
        ThreadUtils.runOnUiThreadBlocking(this::notifyConfigurationChanged);
        waitForBookmarkBarVisibility(/* visible= */ false);

        // Case: Configuration changed to allow feature.
        BookmarkBarUtils.setFeatureAllowedForTesting(true);
        ThreadUtils.runOnUiThreadBlocking(this::notifyConfigurationChanged);
        waitForBookmarkBarVisibility(/* visible= */ true);
    }

    @Test
    @MediumTest
    public void testOnOverflowButtonClick() {
        final GURL url = getTestServerUrl("/chrome/test/data/android/google.html");
        mItemIds =
                IntStream.range(0, 100)
                        .mapToObj(i -> optionalOfThrowable(() -> addBookmark(i, "" + i, url)))
                        .map(Optional::get)
                        .collect(Collectors.toList());
        onViewWaiting(bookmarkBarOverflowButton()).check(matches(isDisplayed())).perform(click());
        onViewWaiting(bookmarkManagerToolbarWithText("Bookmarks bar"))
                .check(matches(isDisplayed()));
    }

    private @Nullable BookmarkId addBookmark(int index, @NonNull String title, @NonNull GURL url)
            throws ExecutionException {
        return BookmarkTestUtil.addBookmark(
                mCtaTestRule.getActivityTestRule(),
                mModel,
                index,
                title,
                url,
                /* parent= */ mDesktopFolderId);
    }

    private @Nullable BookmarkId addFolder(@NonNull String title) throws ExecutionException {
        return BookmarkTestUtil.addFolder(
                mCtaTestRule.getActivityTestRule(), mModel, title, /* parent= */ mDesktopFolderId);
    }

    private @NonNull Matcher<View> bookmarkBarItemWithText(@NonNull String text) {
        return allOf(
                isDescendantOfA(withClassName(endsWith("BookmarkBar"))),
                withClassName(endsWith("BookmarkBarButton")),
                hasDescendant(withText(text)));
    }

    private @NonNull Matcher<View> bookmarkBarOverflowButton() {
        return allOf(
                isDescendantOfA(withClassName(endsWith("BookmarkBar"))),
                withId(R.id.bookmark_bar_overflow_button));
    }

    private @NonNull Matcher<View> bookmarkManagerToolbarWithText(@NonNull String text) {
        return allOf(isDescendantOfA(withClassName(endsWith("BookmarkToolbar"))), withText(text));
    }

    private @NonNull ViewAction clickWith(int metaState) {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return isDisplayed();
            }

            @Override
            public String getDescription() {
                return String.format("clickWith(metaState=%d)", metaState);
            }

            @Override
            public void perform(@NonNull UiController uiController, @NonNull View view) {
                TouchCommon.singleClickView(view, metaState);
            }
        };
    }

    private @NonNull ViewAction focus() {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return allOf(isDisplayed(), isFocusable());
            }

            @Override
            public String getDescription() {
                return "focus";
            }

            @Override
            public void perform(@NonNull UiController uiController, @NonNull View view) {
                // NOTE: Focus doesn't exist in touch mode except under special circumstances.
                // Temporarily enable focusability to ensure the focus request can succeed.
                // See https://android-developers.googleblog.com/2008/12/touch-mode.html.
                final boolean isFocusableInTouchMode = view.isFocusableInTouchMode();
                view.setFocusableInTouchMode(true);
                view.requestFocus();
                CriteriaHelper.pollUiThreadNested(view::isFocused);
                view.setFocusableInTouchMode(isFocusableInTouchMode);
            }
        };
    }

    private @Nullable Tab getCurrentTab() {
        return mCtaTestRule.getActivity().getActivityTab();
    }

    private @Nullable Tab getLastTab() {
        final var tabModel = mCtaTestRule.getActivity().getCurrentTabModel();
        return tabModel.getTabAt(tabModel.getCount() - 1);
    }

    private @NonNull GURL getTestServerUrl(@NonNull String relativeUrl) {
        return new GURL(mCtaTestRule.getTestServer().getURL(relativeUrl));
    }

    private void notifyConfigurationChanged() {
        final var activity = mCtaTestRule.getActivity();
        final var newConfig = new Configuration(activity.getSavedConfigurationForTesting());
        activity.onConfigurationChanged(newConfig);
    }

    private ViewInteraction onViewDisplayed(@NonNull Matcher<View> viewMatcher) {
        return onViewWaiting(allOf(viewMatcher, isDisplayed()));
    }

    private <T> @NonNull Optional<T> optionalOfThrowable(@NonNull Callable<T> callable) {
        try {
            return Optional.of(callable.call());
        } catch (@NonNull Throwable e) {
            return Optional.empty();
        }
    }

    private @NonNull ViewAction pressKey(int keyCode) {
        return pressKey(keyCode, /* metaState= */ 0);
    }

    private @NonNull ViewAction pressKey(int keyCode, int metaState) {
        final var isAltPressed = (metaState & META_ALT_ON) != 0;
        final var isCtrlPressed = (metaState & META_CTRL_ON) != 0;
        final var isShiftPressed = (metaState & META_SHIFT_ON) != 0;
        return androidx.test.espresso.action.ViewActions.pressKey(
                new EspressoKey.Builder()
                        .withAltPressed(isAltPressed)
                        .withCtrlPressed(isCtrlPressed)
                        .withKeyCode(keyCode)
                        .withShiftPressed(isShiftPressed)
                        .build());
    }

    private void setBookmarkBarSetting(boolean enabled) {
        final var activity = mCtaTestRule.getActivity();
        final var profile = activity.getProfileProviderSupplier().get().getOriginalProfile();
        BookmarkBarUtils.setSettingEnabled(profile, enabled);
    }

    private void waitForBookmarkBarVisibility(boolean visible) {
        CriteriaHelper.pollUiThread(
                () -> {
                    final var activity = mCtaTestRule.getActivity();
                    final var view = activity.<BookmarkBar>findViewById(R.id.bookmark_bar);
                    final var viewStub = activity.<ViewStub>findViewById(R.id.bookmark_bar_stub);
                    if (visible) {
                        Criteria.checkThat(view, is(notNullValue()));
                        Criteria.checkThat(view.getVisibility(), is(View.VISIBLE));
                        Criteria.checkThat(view.isLaidOut(), is(true));
                        Criteria.checkThat(viewStub, is(nullValue()));
                    } else {
                        Criteria.checkThat(view, is(nullValue()));
                        Criteria.checkThat(viewStub, is(notNullValue()));
                    }
                });
    }
}
