// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.os.Build.VERSION;
import android.support.test.InstrumentationRegistry;
import android.text.Spanned;
import android.text.SpannedString;
import android.text.style.ClickableSpan;
import android.text.style.StyleSpan;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.ImageView;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.Root;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.ViewAssertion;
import androidx.test.espresso.matcher.BoundedMatcher;
import androidx.test.espresso.matcher.ViewMatchers;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.hamcrest.TypeSafeMatcher;
import org.json.JSONArray;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipIcon;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.DrawableProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TriggerScriptProto.TriggerScriptAction;
import org.chromium.chrome.browser.autofill_assistant.proto.TriggerScriptUIProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TriggerScriptUIProto.TriggerChip;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.components.autofill_assistant.R;
import org.chromium.components.autofill_assistant.TriggerContext;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Callable;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * Contains utilities for testing Autofill Assistant.
 */
class AutofillAssistantUiTestUtil {
    /** Image fetcher which synchronously returns a preset image. */
    static class MockImageFetcher extends ImageFetcher.ImageFetcherForTesting {
        private final Bitmap mBitmapToFetch;
        private final BaseGifImage mGifToFetch;

        MockImageFetcher(@Nullable Bitmap bitmapToFetch, @Nullable BaseGifImage gifToFetch) {
            mBitmapToFetch = bitmapToFetch;
            mGifToFetch = gifToFetch;
        }

        @Override
        public void fetchGif(final ImageFetcher.Params params, Callback<BaseGifImage> callback) {
            callback.onResult(mGifToFetch);
        }

        @Override
        public void fetchImage(Params params, Callback<Bitmap> callback) {
            callback.onResult(mBitmapToFetch);
        }

        @Override
        public void clear() {}

        @Override
        public @ImageFetcherConfig int getConfig() {
            return ImageFetcherConfig.IN_MEMORY_ONLY;
        }

        @Override
        public void destroy() {}
    }

    /** Checks that a text view has a specific maximum number of lines to display. */
    static TypeSafeMatcher<View> isTextMaxLines(int maxLines) {
        return new TypeSafeMatcher<View>() {
            @Override
            protected boolean matchesSafely(View item) {
                if (!(item instanceof TextView)) {
                    return false;
                }
                return ((TextView) item).getMaxLines() == maxLines;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("isTextMaxLines");
            }
        };
    }

    /**
     * Checks that a text view has a specific typeface style. NOTE: this only works for views that
     * explicitly set the text style, *NOT* for text spans! @see {@link #hasTypefaceSpan(int, int,
     * int)}
     */
    static TypeSafeMatcher<View> hasTypefaceStyle(/*@Typeface.Style*/ int style) {
        return new TypeSafeMatcher<View>() {
            @Override
            protected boolean matchesSafely(View item) {
                if (!(item instanceof TextView)) {
                    return false;
                }
                Typeface typeface = ((TextView) item).getTypeface();
                if (typeface == null) {
                    return false;
                }
                return (typeface.getStyle() & style) == style;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("hasTypefaceStyle");
            }
        };
    }

    /**
     * Checks that a text view has a span with the specified style in the specified region.
     * @param start The start offset of the style span
     * @param end The end offset of the style span
     * @param style The style to check for
     * @return A matcher that returns true if the view satisfies the condition.
     */
    static TypeSafeMatcher<View> hasTypefaceSpan(
            int start, int end, /*@Typeface.Style*/ int style) {
        return new TypeSafeMatcher<View>() {
            @Override
            protected boolean matchesSafely(View item) {
                if (!(item instanceof TextView)) {
                    return false;
                }
                TextView textView = (TextView) item;
                if (!(textView.getText() instanceof SpannedString)) {
                    return false;
                }
                if (start >= textView.length() || end >= textView.length()) {
                    return false;
                }
                StyleSpan[] spans =
                        ((SpannedString) textView.getText()).getSpans(start, end, StyleSpan.class);
                for (StyleSpan span : spans) {
                    if (span.getStyle() == style) {
                        return true;
                    }
                }
                return false;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText(
                        "hasTypefaceSpan(" + style + ") in [" + start + ", " + end + "]");
            }
        };
    }

    static Matcher<View> isImportantForAccessibility(int mode) {
        return new TypeSafeMatcher<View>() {
            @Override
            protected boolean matchesSafely(View item) {
                return mode == item.getImportantForAccessibility();
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("important for Accessibility set to " + mode);
            }
        };
    }

    static Matcher<View> hasTintColor(final int colorResId) {
        return new BoundedMatcher<View, ImageView>(ImageView.class) {
            private Context mContext;

            @Override
            protected boolean matchesSafely(ImageView imageView) {
                if (VERSION.SDK_INT < 21) {
                    // Image tint didn't exist before then.
                    return true;
                }
                if (imageView.getImageTintList() == null) {
                    return false;
                }
                this.mContext = imageView.getContext();
                int imageTintColor = imageView.getImageTintList().getColorForState(
                        imageView.getDrawable().getState(), -1);
                int expectedColor = mContext.getColor(colorResId);
                return imageTintColor == expectedColor;
            }

            @Override
            public void describeTo(Description description) {
                String colorId = String.valueOf(colorResId);
                if (this.mContext != null) {
                    colorId = this.mContext.getResources().getResourceName(colorResId);
                }

                description.appendText("has tint with ID " + colorId);
            }
        };
    }

    static Matcher<View> isNextAfterSibling(final Matcher<View> siblingMatcher) {
        assert siblingMatcher != null;
        return new TypeSafeMatcher<View>() {
            @Override
            public void describeTo(Description description) {
                description.appendText("is next after sibling: ");
                siblingMatcher.describeTo(description);
            }

            @Override
            public boolean matchesSafely(View view) {
                ViewParent parent = view.getParent();
                if (!(parent instanceof ViewGroup)) {
                    return false;
                } else {
                    ViewGroup parentGroup = (ViewGroup) parent;

                    for (int i = 1; i < parentGroup.getChildCount(); ++i) {
                        if (view == parentGroup.getChildAt(i)) {
                            return siblingMatcher.matches(parentGroup.getChildAt(i - 1));
                        }
                    }

                    return false;
                }
            }
        };
    }

    static Matcher<View> withParentIndex(int parentIndex) {
        return new TypeSafeMatcher<View>() {
            @Override
            public void describeTo(Description description) {
                description.appendText("withParentIndex: " + parentIndex);
            }

            @Override
            public boolean matchesSafely(View view) {
                ViewParent parent = view.getParent();
                if (!(parent instanceof ViewGroup)) {
                    return false;
                } else {
                    ViewGroup parentGroup = (ViewGroup) parent;
                    return parentGroup.getChildAt(parentIndex) == view;
                }
            }
        };
    }

    static Matcher<View> withMinimumSize(int minWidthInPixels, int minHeightInPixels) {
        return new TypeSafeMatcher<View>() {
            @Override
            protected boolean matchesSafely(View view) {
                return view.getWidth() >= minWidthInPixels && view.getHeight() >= minHeightInPixels
                        && view.getMinimumWidth() == minWidthInPixels
                        && view.getMinimumHeight() == minHeightInPixels;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText(
                        "Width >= " + minWidthInPixels + " and height >= " + minHeightInPixels);
            }
        };
    }

    static Matcher<View> fullyCovers(Rect rect) {
        return new TypeSafeMatcher<View>() {
            @Override
            protected boolean matchesSafely(View view) {
                Rect viewRect = new Rect();
                if (!view.getGlobalVisibleRect(viewRect)) {
                    throw new AssertionError("Expected view to be visible.");
                }

                return rect.left >= viewRect.left && rect.right <= viewRect.right
                        && rect.top >= viewRect.top && rect.bottom <= viewRect.bottom;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("fully covering [" + rect.left + ", " + rect.top + ", "
                        + rect.right + ", " + rect.bottom + "]");
            }
        };
    }

    static Matcher<View> withTextGravity(int gravity) {
        return new TypeSafeMatcher<View>() {
            @Override
            protected boolean matchesSafely(View view) {
                if (!(view instanceof TextView)) {
                    return false;
                }
                return ((TextView) view).getGravity() == gravity;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("withTextGravity " + gravity);
            }
        };
    }

    /**
     * Runs the main loop for at least the specified amount of time. Useful in cases where you need
     * to ensure a negative, e.g., a certain view is never displayed. Intended usage:
     * onView(isRoot()).waitAtLeast(...);
     */
    static ViewAction waitAtLeast(long millis) {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return ViewMatchers.isRoot();
            }

            @Override
            public String getDescription() {
                return "Waits/idles for a specified amount of time";
            }

            @Override
            public void perform(UiController uiController, View view) {
                uiController.loopMainThreadUntilIdle();

                long endTime = System.currentTimeMillis() + millis;
                while (System.currentTimeMillis() < endTime) {
                    uiController.loopMainThreadForAtLeast(
                            Math.max(endTime - System.currentTimeMillis(), 50));
                }
            }
        };
    }

    static ViewAction openTextLink(String textLink) {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return Matchers.instanceOf(TextView.class);
            }

            @Override
            public String getDescription() {
                return "Opens a textlink of a TextView";
            }

            @Override
            public void perform(UiController uiController, View view) {
                TextView textView = (TextView) view;
                Spanned spannedString = (Spanned) textView.getText();
                ClickableSpan[] spans =
                        spannedString.getSpans(0, spannedString.length(), ClickableSpan.class);
                for (ClickableSpan span : spans) {
                    if (textLink.contentEquals(
                                spannedString.subSequence(spannedString.getSpanStart(span),
                                        spannedString.getSpanEnd(span)))) {
                        span.onClick(view);
                        return;
                    }
                }

                throw new NoMatchingViewException.Builder()
                        .includeViewHierarchy(true)
                        .withRootView(textView)
                        .build();
            }
        };
    }

    /** Returns all views with a matching tag. */
    public static List<View> findViewsWithTag(View view, Object tag) {
        List<View> viewsWithTag = new ArrayList<>();
        if (view instanceof ViewGroup) {
            for (int i = 0; i < ((ViewGroup) view).getChildCount(); i++) {
                viewsWithTag.addAll(findViewsWithTag(((ViewGroup) view).getChildAt(i), tag));
            }
        }
        if (view.getTag() != null && view.getTag().equals(tag)) {
            viewsWithTag.add(view);
        }
        return viewsWithTag;
    }

    static Matcher<View> withTextId(int id) {
        return ViewMatchers.withText(
                InstrumentationRegistry.getTargetContext().getResources().getString(id));
    }

    /** Checks whether the scrollbar fading is enabled. */
    static TypeSafeMatcher<View> isScrollbarFadingEnabled() {
        return new TypeSafeMatcher<View>() {
            @Override
            protected boolean matchesSafely(View item) {
                if (!(item instanceof ScrollView)) {
                    return false;
                }
                return ((ScrollView) item).isScrollbarFadingEnabled();
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("isScrollbarFadingEnabled");
            }
        };
    }

    /**
     * Waits until {@code matcher} matches {@code condition}. Will automatically fail after a
     * default timeout.
     */
    public static void waitUntilViewMatchesCondition(
            Matcher<View> matcher, Matcher<View> condition) {
        waitUntilViewMatchesCondition(matcher, condition, DEFAULT_MAX_TIME_TO_POLL);
    }

    /**
     * Same as {@link #waitUntilViewMatchesCondition(Matcher, Matcher)} but also uses {@code
     * rootMatcher} to select the correct window.
     */
    public static void waitUntilViewInRootMatchesCondition(
            Matcher<View> matcher, Matcher<Root> rootMatcher, Matcher<View> condition) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            try {
                onView(matcher).inRoot(rootMatcher).check(matches(condition));
            } catch (NoMatchingViewException | AssertionError e) {
                // Note: all other exceptions are let through, in particular
                // AmbiguousViewMatcherException.
                throw new CriteriaNotSatisfiedException(
                        "Timeout while waiting for " + matcher + " to satisfy " + condition);
            }
        }, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }

    /** @see {@link #waitUntilViewMatchesCondition(Matcher, Matcher)} */
    public static void waitUntilViewMatchesCondition(
            Matcher<View> matcher, Matcher<View> condition, long maxTimeoutMs) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            try {
                onView(matcher).check(matches(condition));
            } catch (NoMatchingViewException | AssertionError e) {
                // Note: all other exceptions are let through, in particular
                // AmbiguousViewMatcherException.
                throw new CriteriaNotSatisfiedException(
                        "Timeout while waiting for " + matcher + " to satisfy " + condition);
            }
        }, maxTimeoutMs, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Same as {@link #waitUntilViewMatchesCondition(Matcher, Matcher, long)}, but waits for a view
     * assertion instead.
     */
    public static void waitUntilViewAssertionTrue(
            Matcher<View> matcher, ViewAssertion viewAssertion, long maxTimeoutMs) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            try {
                onView(matcher).check(viewAssertion);
            } catch (NoMatchingViewException | AssertionError e) {
                // Note: all other exceptions are let through, in particular
                // AmbiguousViewMatcherException.
                throw new CriteriaNotSatisfiedException(
                        "Timeout while waiting for " + matcher + " to satisfy " + viewAssertion);
            }
        }, maxTimeoutMs, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Waits until keyboard is visible or not based on {@code isShowing}. Will automatically fail
     * after a default timeout.
     */
    public static void waitUntilKeyboardMatchesCondition(
            ChromeActivityTestRule testRule, boolean isShowing) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            boolean isKeyboardShowing =
                    testRule.getActivity()
                            .getWindowAndroid()
                            .getKeyboardDelegate()
                            .isKeyboardShowing(testRule.getActivity(),
                                    testRule.getActivity().getCompositorViewHolderForTesting());
            String errorMsg = "Timeout while waiting for the keyboard to be "
                    + (isShowing ? "visible" : "hidden");
            Criteria.checkThat(errorMsg, isKeyboardShowing, Matchers.is(isShowing));
        });
    }

    public static void waitUntil(Callable<Boolean> condition) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            try {
                Criteria.checkThat(condition.call(), Matchers.is(true));
            } catch (Exception e) {
                throw new CriteriaNotSatisfiedException(e);
            }
        }, DEFAULT_MAX_TIME_TO_POLL, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Get a {@link BottomSheetController} to run the tests with.
     */
    static BottomSheetController getBottomSheetController(ChromeActivity activity) {
        return activity.getRootUiCoordinatorForTesting().getBottomSheetController();
    }

    /**
     * Attaches the specified view to the Chrome coordinator. Must be called from the UI thread.
     */
    public static void attachToCoordinator(CustomTabActivity activity, View view) {
        ThreadUtils.assertOnUiThread();
        ViewGroup chromeCoordinatorView = activity.findViewById(R.id.coordinator);
        CoordinatorLayout.LayoutParams lp = new CoordinatorLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.gravity = Gravity.BOTTOM;
        chromeCoordinatorView.addView(view, lp);
    }

    public static void startAutofillAssistant(
            ChromeActivity activity, AutofillAssistantTestService testService) {
        startAutofillAssistant(activity, testService, /* initialUrl = */ null);
    }
    /**
     * Starts Autofill Assistant on the given {@code activity} and injects the given {@code
     * testService}. {@code initialUrl} will, if provided, override the default initial url for
     * the trigger context, which is the initial url of the activity.
     */
    public static void startAutofillAssistant(ChromeActivity activity,
            AutofillAssistantTestService testService, @Nullable String initialUrl) {
        testService.scheduleForInjection();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AutofillAssistantFacade.start(activity,
                                TriggerContext.newBuilder()
                                        .addParameter("ENABLED", true)
                                        .addParameter("START_IMMEDIATELY", true)
                                        .withInitialUrl(initialUrl != null
                                                        ? initialUrl
                                                        : activity.getInitialIntent()
                                                                  .getDataString())
                                        .build()));
    }

    /**
     * Starts Autofill Assistant on the given {@code activity}. Will add the provided {@code url}
     * and {@code scriptParameters} to the trigger context.
     */
    public static void startAutofillAssistantWithParams(
            ChromeActivity activity, String url, Map<String, Object> scriptParameters) {
        TriggerContext.Builder argsBuilder =
                TriggerContext.newBuilder().fromBundle(null).withInitialUrl(url);
        for (Map.Entry<String, Object> param : scriptParameters.entrySet()) {
            argsBuilder.addParameter(param.getKey(), param.getValue());
        }
        argsBuilder.addParameter("ENABLED", true);
        argsBuilder.addParameter("ORIGINAL_DEEPLINK", url);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> AutofillAssistantFacade.start(activity, argsBuilder.build()));
    }

    /** Performs a single tap on the center of the specified element. */
    public static void tapElement(ChromeActivityTestRule testRule, String... elementIds)
            throws Exception {
        Rect coords = getAbsoluteBoundingRect(testRule, elementIds);
        float x = coords.left + 0.5f * (coords.right - coords.left);
        float y = coords.top + 0.5f * (coords.bottom - coords.top);

        // Sanity check, can only click on coordinates on screen.
        DisplayMetrics displayMetrics = testRule.getActivity().getResources().getDisplayMetrics();
        BottomSheetController bottomSheetController =
                testRule.getActivity().getRootUiCoordinatorForTesting().getBottomSheetController();
        int totalBottomSheetHeight = bottomSheetController.getCurrentOffset();
        if (x < 0 || x > displayMetrics.widthPixels || y < 0
                || y > displayMetrics.heightPixels - totalBottomSheetHeight) {
            throw new IllegalArgumentException(Arrays.toString(elementIds)
                    + " not on screen: tried to tap x=" + x + ", y=" + y
                    + ", which is outside of display with w=" + displayMetrics.widthPixels
                    + ", h=" + displayMetrics.heightPixels
                    + ", or obstructed by the BottomSheet with height=" + totalBottomSheetHeight);
        }
        TestTouchUtils.singleClick(InstrumentationRegistry.getInstrumentation(), x, y);
    }

    /**
     * Similar to {@code tapElement}, but clicks with JS and does not check that the element is
     * currently visible or in the viewport.
     */
    public static void clickElementWithJs(WebContents webContents, String... elementIds)
            throws Exception {
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(webContents,
                "(function() {"
                        + " " + getElementSelectorString(elementIds) + ".click();"
                        + " return [true];"
                        + "})()");
        javascriptHelper.waitUntilHasValue();
        JSONArray result = new JSONArray(javascriptHelper.getJsonResultAndClear());
        assert result.getBoolean(0);
    }

    /** Scrolls to the specified element on the webpage, if necessary. */
    public static void scrollIntoViewIfNeeded(WebContents webContents, String... elementIds)
            throws Exception {
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(webContents,
                "(function() {"
                        + " " + getElementSelectorString(elementIds) + ".scrollIntoViewIfNeeded();"
                        + " return [true];"
                        + "})()");
        javascriptHelper.waitUntilHasValue();
        JSONArray result = new JSONArray(javascriptHelper.getJsonResultAndClear());
        assert result.getBoolean(0);
    }

    /** Computes the bounding rectangle of the specified DOM element in absolute screen space. */
    public static Rect getAbsoluteBoundingRect(
            ChromeActivityTestRule testRule, String... elementIds) throws Exception {
        // Get bounding rectangle in viewport space.
        Rect elementRect = getBoundingRectForElement(testRule.getWebContents(), elementIds);

        /*
         * Conversion from viewport space to screen space is done in two steps:
         * - First, convert viewport to compositor space (scrolling offset, multiply with factor).
         * - Then, convert compositor space to screen space (add content offset).
         */
        Coordinates coordinates = Coordinates.createFor(testRule.getWebContents());
        float left = coordinates.getScrollXPixInt() / coordinates.getPageScaleFactor()
                / coordinates.getDeviceScaleFactor();
        float top = coordinates.getScrollYPixInt() / coordinates.getPageScaleFactor()
                / coordinates.getDeviceScaleFactor();

        int[] compositorLocation = new int[2];
        testRule.getActivity().getCompositorViewHolderForTesting().getLocationOnScreen(
                compositorLocation);
        int offsetY = compositorLocation[1]
                + testRule.getActivity().getBrowserControlsManager().getContentOffset();

        return new Rect((int) (coordinates.fromLocalCssToPix(elementRect.left - left)),
                (int) (coordinates.fromLocalCssToPix(elementRect.top - top) + offsetY),
                (int) (coordinates.fromLocalCssToPix(elementRect.right - left)),
                (int) (coordinates.fromLocalCssToPix(elementRect.bottom - top) + offsetY));
    }

    /**
     * Retrieves the bounding rectangle for the specified element in the DOM tree in CSS pixel
     * coordinates.
     */
    public static Rect getBoundingRectForElement(WebContents webContents, String... elementIds)
            throws Exception {
        if (!checkElementExists(webContents, elementIds)) {
            throw new IllegalArgumentException(Arrays.toString(elementIds) + " does not exist");
        }
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        Rect rect = new Rect(0, 0, Integer.MAX_VALUE, Integer.MAX_VALUE);
        for (int i = 0; i < elementIds.length; ++i) {
            String offsetX = i == 0 ? "window.scrollX" : "0";
            String offsetY = i == 0 ? "window.scrollY" : "0";
            String elementSelector =
                    getElementSelectorString(Arrays.copyOfRange(elementIds, 0, i + 1));
            javascriptHelper.evaluateJavaScriptForTests(webContents,
                    "(function() {"
                            + " rect = " + elementSelector + ".getBoundingClientRect();"
                            + " return [" + offsetX + " + rect.left, " + offsetY + " + rect.top, "
                            + "         " + offsetX + " + rect.right, " + offsetY
                            + " + rect.bottom];"
                            + "})()");
            javascriptHelper.waitUntilHasValue();
            JSONArray rectJson = new JSONArray(javascriptHelper.getJsonResultAndClear());

            rect = new Rect(Math.min(rect.right, rect.left + rectJson.getInt(0)),
                    Math.min(rect.bottom, rect.top + rectJson.getInt(1)),
                    Math.min(rect.right, rect.left + rectJson.getInt(2)),
                    Math.min(rect.bottom, rect.top + rectJson.getInt(3)));
        }
        return rect;
    }

    public static boolean checkElementOnScreen(
            ChromeActivityTestRule testRule, String... elementIds) throws Exception {
        Rect coords = getAbsoluteBoundingRect(testRule, elementIds);
        DisplayMetrics displayMetrics = testRule.getActivity().getResources().getDisplayMetrics();

        return (coords.right < displayMetrics.widthPixels && 0 <= coords.left)
                && (coords.top < displayMetrics.heightPixels && 0 <= coords.bottom);
    }

    /** Checks whether the specified element exists in the DOM tree. */
    public static boolean checkElementExists(WebContents webContents, String... elementIds)
            throws Exception {
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(webContents,
                "(function() {"
                        + " return [" + getElementSelectorString(elementIds) + " != null]; "
                        + "})()");
        javascriptHelper.waitUntilHasValue();
        JSONArray result = new JSONArray(javascriptHelper.getJsonResultAndClear());
        return result.getBoolean(0);
    }

    /**
     * Wait for an element to be removed from the web page shown by the given {@link WebContents}.
     * @param webContents The web content to check.
     * @param id The ID of the element to look for.
     */
    public static void waitForElementRemoved(WebContents webContents, String id) {
        CriteriaHelper.pollInstrumentationThread(
                () -> !checkElementExists(webContents, id), "Element is still on the page!");
    }

    /**
     * Retrieves the value of a given property of an element.
     * @return A JSONArray containing the property value as the single element.
     */
    private static JSONArray getElementProperty(
            WebContents webContents, String propertyName, String... elementIds) throws Exception {
        if (!checkElementExists(webContents, elementIds)) {
            throw new IllegalArgumentException(Arrays.toString(elementIds) + " does not exist");
        }
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(webContents,
                "(function() {"
                        + " return [" + getElementSelectorString(elementIds) + "." + propertyName
                        + "];"
                        + "})()");
        javascriptHelper.waitUntilHasValue();
        JSONArray result = new JSONArray(javascriptHelper.getJsonResultAndClear());
        if (result.length() != 1) {
            throw new RuntimeException("Expected exactly one element in the result.");
        }
        return result;
    }

    /**
     * Retrieves whether the element is checked, using the .checked property.
     */
    public static boolean getElementChecked(WebContents webContents, String... elementIds)
            throws Exception {
        return getElementProperty(webContents, "checked", elementIds).getBoolean(0);
    }

    /**
     * Retrieves the value of the specified element.
     */
    public static String getElementValue(WebContents webContents, String... elementIds)
            throws Exception {
        return getElementProperty(webContents, "value", elementIds).getString(0);
    }

    /**
     * Converts a view into a bitmap.
     */
    public static Bitmap getBitmapFromView(View view) {
        Bitmap resultBitmap =
                Bitmap.createBitmap(view.getWidth(), view.getHeight(), Bitmap.Config.ARGB_8888);
        Drawable backgroundDrawable = view.getBackground();
        if (backgroundDrawable == null) {
            return resultBitmap;
        }
        Canvas canvas = new Canvas(resultBitmap);
        backgroundDrawable.draw(canvas);
        view.draw(canvas);
        return resultBitmap;
    }

    private static String getElementSelectorString(String[] elementIds) {
        StringBuilder builder = new StringBuilder();
        builder.append("document");

        for (int i = 0; i < elementIds.length; ++i) {
            builder.append(".getElementById('");
            builder.append(elementIds[i]);
            builder.append("')");
            if (i != elementIds.length - 1) {
                // Get the iFrame document. This only works for local iFrames, OutOfProcess iFrames
                // may respond with an error.
                builder.append(".contentWindow.document");
            }
        }

        return builder.toString();
    }

    /**
     * Creates a default trigger script UI, similar to the intended experience. It comprises three
     * chips: 'Preferences', 'Not now', 'Continue'. 'Preferences' opens the cancel popup containing
     * 'Not for this session' and 'Never show again'. Optionally, a blue message bubble and a
     * default progress bar are shown.
     */
    public static TriggerScriptUIProto.Builder createDefaultTriggerScriptUI(
            String statusMessage, String bubbleMessage, boolean withProgressBar) {
        TriggerScriptUIProto.Builder builder =
                TriggerScriptUIProto.newBuilder()
                        .setStatusMessage(statusMessage)
                        .setCalloutMessage(bubbleMessage)
                        .addLeftAlignedChips(
                                TriggerChip.newBuilder()
                                        .setChip(ChipProto.newBuilder()
                                                         .setType(ChipType.NORMAL_ACTION)
                                                         .setIcon(ChipIcon.ICON_OVERFLOW))
                                        .setAction(TriggerScriptAction.SHOW_CANCEL_POPUP))
                        .addRightAlignedChips(
                                TriggerChip.newBuilder()
                                        .setChip(ChipProto.newBuilder()
                                                         .setType(ChipType.NORMAL_ACTION)
                                                         .setText("Not now"))
                                        .setAction(TriggerScriptAction.NOT_NOW))
                        .addRightAlignedChips(
                                TriggerChip.newBuilder()
                                        .setChip(ChipProto.newBuilder()
                                                         .setType(ChipType.HIGHLIGHTED_ACTION)
                                                         .setText("Continue"))
                                        .setAction(TriggerScriptAction.ACCEPT))
                        .setCancelPopup(
                                TriggerScriptUIProto.Popup.newBuilder()
                                        .addChoices(
                                                TriggerScriptUIProto.Popup.Choice.newBuilder()
                                                        .setText("Not for this session")
                                                        .setAction(
                                                                TriggerScriptAction.CANCEL_SESSION))
                                        .addChoices(TriggerScriptUIProto.Popup.Choice.newBuilder()
                                                            .setText("Never show again")
                                                            .setAction(TriggerScriptAction
                                                                               .CANCEL_FOREVER)));
        if (withProgressBar) {
            builder.setProgressBar(
                    TriggerScriptUIProto.ProgressBar.newBuilder()
                            .addStepIcons(DrawableProto.newBuilder().setIcon(
                                    DrawableProto.Icon.PROGRESSBAR_DEFAULT_INITIAL_STEP))
                            .addStepIcons(DrawableProto.newBuilder().setIcon(
                                    DrawableProto.Icon.PROGRESSBAR_DEFAULT_DATA_COLLECTION))
                            .addStepIcons(DrawableProto.newBuilder().setIcon(
                                    DrawableProto.Icon.PROGRESSBAR_DEFAULT_PAYMENT))
                            .addStepIcons(DrawableProto.newBuilder().setIcon(
                                    DrawableProto.Icon.PROGRESSBAR_DEFAULT_FINAL_STEP))
                            .setActiveStep(1));
        }
        return builder;
    }
}
