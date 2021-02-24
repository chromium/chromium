// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;

import static org.chromium.chrome.test.util.ViewUtils.onViewWaiting;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.params.ParameterizedCommandLineFlags;
import org.chromium.base.test.params.ParameterizedCommandLineFlags.Switches;
import org.chromium.base.test.util.CloseableOnMainThread;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.Callable;

/**
 * Integration tests for the UrlBar.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// clang-format off
@ParameterizedCommandLineFlags({
  @Switches(),
  @Switches("disable-features=" + ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE),
})
// clang-format on
public class UrlBarIntegrationTest {
    // 9000+ chars of goodness
    private static final String HUGE_URL =
            "data:text/plain,H" + new String(new char[9000]).replace('\0', 'u') + "ge!";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private UrlBar getUrlBar() {
        return (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
    }

    /**
     * Test to verify the omnibox can take focus during startup before native libraries have
     * loaded.
     */
    @Test
    @SmallTest
    @Feature({"Omnibox"})
    public void testFocusingOnStartup() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityTestRule.prepareUrlIntent(intent, "about:blank");
        mActivityTestRule.startActivityCompletely(intent);

        UrlBar urlBar = getUrlBar();
        Assert.assertNotNull(urlBar);
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);
    }

    // This test relies on logic in LocationBarLayout to clear the selection.  Ideally, it can move
    // to a more isolated test suite in the future.
    @Test
    @SmallTest
    @Feature({"Omnibox"})
    public void testAutocompleteUpdatedOnDefocus() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();

        final UrlBar urlBar = getUrlBar();
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);

        String textWithAutocomplete = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            urlBar.setText("test");
            urlBar.setAutocompleteText("test", "ing is fun");
            return urlBar.getTextWithAutocomplete();
        });

        Assert.assertEquals("testing is fun", textWithAutocomplete);
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, false);

        textWithAutocomplete = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> urlBar.getTextWithAutocomplete());
        Assert.assertEquals(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL, textWithAutocomplete);
    }

    @Test
    @SmallTest
    @Feature({"Omnibox"})
    public void testCopyHuge() throws Throwable {
        mActivityTestRule.startMainActivityWithURL(HUGE_URL);
        OmniboxTestUtils.toggleUrlBarFocus(getUrlBar(), true);
        // Allow all thread policies temporarily in main thread to avoid
        // DiskWrite and UnBufferedIo violations during copying under
        // emulator environment.
        try (CloseableOnMainThread ignored =
                        CloseableOnMainThread.StrictMode.allowAllThreadPolicies()) {
            Assert.assertEquals(HUGE_URL, copyUrlToClipboard(android.R.id.copy));
        }
    }

    @Test
    @SmallTest
    @Feature({"Omnibox"})
    public void testCutHuge() throws Throwable {
        mActivityTestRule.startMainActivityWithURL(HUGE_URL);
        OmniboxTestUtils.toggleUrlBarFocus(getUrlBar(), true);
        // Allow all thread policies temporarily in main thread to avoid
        // DiskWrite and UnBufferedIo violations during copying under
        // emulator environment.
        try (CloseableOnMainThread ignored =
                        CloseableOnMainThread.StrictMode.allowAllThreadPolicies()) {
            Assert.assertEquals(HUGE_URL, copyUrlToClipboard(android.R.id.cut));
        }
    }

    @Test
    @SmallTest
    @Feature({"Omnibox"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testDarkThemeColor() throws Throwable {
        mActivityTestRule.startMainActivityWithURL(UrlUtils.encodeHtmlDataUri(
                "<html><meta name=\"theme-color\" content=\"#000000\" /></html>"));

        CriteriaHelper.pollUiThread(() -> {
            final int expectedTextColor =
                    ApiCompatibilityUtils.getColor(mActivityTestRule.getActivity().getResources(),
                            R.color.default_text_color_light);
            Criteria.checkThat(getUrlBar().getCurrentTextColor(), Matchers.is(expectedTextColor));
        });
    }

    /**
     * Clears the clipboard, executes specified action on the omnibox and
     * returns clipboard's content. Action can be either android.R.id.copy
     * or android.R.id.cut.
     */
    private String copyUrlToClipboard(final int action) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<String>() {
            @Override
            public String call() {
                ClipboardManager clipboardManager =
                        (ClipboardManager) mActivityTestRule.getActivity().getSystemService(
                                Context.CLIPBOARD_SERVICE);

                clipboardManager.setPrimaryClip(ClipData.newPlainText(null, ""));

                Assert.assertTrue(getUrlBar().onTextContextMenuItem(action));
                ClipData clip = clipboardManager.getPrimaryClip();
                CharSequence text = (clip != null && clip.getItemCount() != 0)
                        ? clip.getItemAt(0).getText()
                        : null;
                return text != null ? text.toString() : null;
            }
        });
    }

    @Test
    @SmallTest
    @Feature({"Omnibox"})
    // TODO(crbug.com/1028469): Investigate and enable this test for the search engine logo feature.
    @DisableFeatures("OmniboxSearchEngineLogo")
    public void testLongPress() {
        // This is a more realistic test than HUGE_URL because ita's full of separator characters
        // which have historically been known to trigger odd behavior with long-pressing.
        final String longPressUrl = "data:text/plain,hi.hi.hi.hi.hi.hi.hi.hi.hi.hi/hi/hi/hi/hi/hi/";
        mActivityTestRule.startMainActivityWithURL(longPressUrl);

        class ActionModeCreatedCallback implements ActionMode.Callback {
            public boolean actionModeCreated;

            @Override
            public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
                return false;
            }

            @Override
            public boolean onCreateActionMode(ActionMode mode, Menu menu) {
                actionModeCreated = true;
                return true;
            }

            @Override
            public void onDestroyActionMode(ActionMode mode) {}

            @Override
            public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
                return false;
            }
        }
        ActionModeCreatedCallback callback = new ActionModeCreatedCallback();
        getUrlBar().setCustomSelectionActionModeCallback(callback);

        onViewWaiting(allOf(is(getUrlBar()), isDisplayed()));
        TouchCommon.longPressView(getUrlBar());

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(callback.actionModeCreated, Matchers.is(true));
            Criteria.checkThat(getUrlBar().getSelectionStart(), Matchers.is(0));
            Criteria.checkThat(getUrlBar().getSelectionEnd(), Matchers.is(longPressUrl.length()));
        });
    }
}
