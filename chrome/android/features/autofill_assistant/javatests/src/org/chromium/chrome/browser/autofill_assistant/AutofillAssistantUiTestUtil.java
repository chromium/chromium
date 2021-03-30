// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.content.Context;
import android.content.Intent;
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

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
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
                int expectedColor =
                        ApiCompatibilityUtils.getColor(mContext.getResources(), colorResId);
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
                                    testRule.getActivity().getCompositorViewHolder());
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
        ViewGroup chromeCoordinatorView =
                activity.findViewById(org.chromium.chrome.autofill_assistant.R.id.coordinator);
        CoordinatorLayout.LayoutParams lp = new CoordinatorLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.gravity = Gravity.BOTTOM;
        chromeCoordinatorView.addView(view, lp);
    }

    /**
     * Starts the CCT test rule on a blank page.
     */
    public static void startOnBlankPage(CustomTabActivityTestRule testRule) {
        testRule.startCustomTabActivityWithIntent(
                AutofillAssistantUiTestUtil.createMinimalCustomTabIntentForAutobot(
                        "about:blank", /* startImmediately = */ true));
    }

    /**
     * Starts Autofill Assistant on the given {@code activity} and injects the given {@code
     * testService}.
     */
    public static void startAutofillAssistant(
            ChromeActivity activity, AutofillAssistantTestService testService) {
        testService.scheduleForInjection();
        TestThreadUtils.runOnUiThreadBlocking(() -> AutofillAssistantFacade.start(activity));
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
        Rect viewport = getViewport(testRule.getWebContents());
        float cssToPysicalPixels =
                (((float) testRule.getActivity().getCompositorViewHolder().getWidth()
                        / (float) viewport.width()));

        int[] compositorLocation = new int[2];
        testRule.getActivity().getCompositorViewHolder().getLocationOnScreen(compositorLocation);
        int offsetY = compositorLocation[1]
                + testRule.getActivity().getBrowserControlsManager().getContentOffset();
        return new Rect((int) ((elementRect.left - viewport.left) * cssToPysicalPixels),
                (int) ((elementRect.top - viewport.top) * cssToPysicalPixels + offsetY),
                (int) ((elementRect.right - viewport.left) * cssToPysicalPixels),
                (int) ((elementRect.bottom - viewport.top) * cssToPysicalPixels + offsetY));
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

        return (coords.left < displayMetrics.widthPixels && 0 <= coords.right)
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

    /** Checks whether the specified element is displayed in the DOM tree. */
    public static boolean checkElementIsDisplayed(WebContents webContents, String... elementIds)
            throws Exception {
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(webContents,
                "(function() {"
                        + " return [" + getElementSelectorString(elementIds)
                        + ".style.display != \"none\"]; "
                        + "})()");
        javascriptHelper.waitUntilHasValue();
        JSONArray result = new JSONArray(javascriptHelper.getJsonResultAndClear());
        return result.getBoolean(0);
    }

    /**
     * Retrieves the visual viewport of the webpage in CSS pixel coordinates.
     */
    public static Rect getViewport(WebContents webContents) throws Exception {
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(webContents,
                "(function() {"
                        + " const v = window.visualViewport;"
                        + " return ["
                        + "   v.pageLeft, v.pageTop,"
                        + "   v.pageLeft + v.width, v.pageTop + v.height"
                        + " ];"
                        + "})()");
        javascriptHelper.waitUntilHasValue();
        JSONArray values = new JSONArray(javascriptHelper.getJsonResultAndClear());
        return new Rect(values.getInt(0), values.getInt(1), values.getInt(2), values.getInt(3));
    }

    /**
     * Retrieves the value of the specified element.
     */
    public static String getElementValue(WebContents webContents, String... elementIds)
            throws Exception {
        if (!checkElementExists(webContents, elementIds)) {
            throw new IllegalArgumentException(Arrays.toString(elementIds) + " does not exist");
        }
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(webContents,
                "(function() {"
                        + " return [" + getElementSelectorString(elementIds) + ".value]"
                        + "})()");
        javascriptHelper.waitUntilHasValue();
        JSONArray result = new JSONArray(javascriptHelper.getJsonResultAndClear());
        return result.getString(0);
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

    public static Intent createMinimalCustomTabIntentForAutobot(
            String url, boolean startImmediately) {
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), url);
        intent.putExtra(TriggerContext.PARAMETER_START_IMMEDIATELY, startImmediately);
        return intent;
    }
}
