// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.support.test.filters.SmallTest;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterizedCommandLineFlags;
import org.chromium.base.test.params.ParameterizedCommandLineFlags.Switches;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.common.ContentUrlConstants;

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
    public ChromeActivityTestRule<ChromeTabbedActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

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
    @RetryOnFailure
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
    @RetryOnFailure
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
    @RetryOnFailure
    public void testCopyHuge() {
        mActivityTestRule.startMainActivityWithURL(HUGE_URL);
        OmniboxTestUtils.toggleUrlBarFocus(getUrlBar(), true);
        Assert.assertEquals(HUGE_URL, copyUrlToClipboard(android.R.id.copy));
    }

    @Test
    @SmallTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testCutHuge() {
        mActivityTestRule.startMainActivityWithURL(HUGE_URL);
        OmniboxTestUtils.toggleUrlBarFocus(getUrlBar(), true);
        Assert.assertEquals(HUGE_URL, copyUrlToClipboard(android.R.id.cut));
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
    @RetryOnFailure
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

        TouchCommon.longPressView(getUrlBar());

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return callback.actionModeCreated && getUrlBar().getSelectionStart() == 0
                        && getUrlBar().getSelectionEnd() == longPressUrl.length();
            }
        });
    }
}
