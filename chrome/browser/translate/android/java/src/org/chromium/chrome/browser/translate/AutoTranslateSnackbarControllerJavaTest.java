// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.app.Activity;
import android.widget.Button;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

import java.lang.ref.WeakReference;
import java.util.concurrent.ExecutionException;

/** Instrumentation tests for {@link AutoTranslateSnackbarController} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class AutoTranslateSnackbarControllerJavaTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public JniMocker mJniMocker = new JniMocker();

    private static final long NATIVE_SNACKBAR_VIEW = 1001L;

    @Mock AutoTranslateSnackbarController.Natives mMockJni;

    private AutoTranslateSnackbarController mAutoTranslateSnackbarController;
    private SnackbarManager mSnackbarManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivityTestRule.startMainActivityOnBlankPage();
        mSnackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        WeakReference<Activity> weakReference =
                new WeakReference<Activity>(mActivityTestRule.getActivity());

        mAutoTranslateSnackbarController =
                new AutoTranslateSnackbarController(
                        weakReference, mSnackbarManager, NATIVE_SNACKBAR_VIEW);

        mJniMocker.mock(AutoTranslateSnackbarControllerJni.TEST_HOOKS, mMockJni);
    }

    @Test
    @SmallTest
    public void testShowSnackbar() throws Exception {
        showSnackbar("en");

        Snackbar currentSnackbar = getCurrentSnackbar();

        Assert.assertEquals("Page translated to English", getSnackbarMessageText());
        Assert.assertEquals("Undo", getSnackbarActionText());
        Assert.assertTrue(
                "Incorrect SnackbarController type",
                currentSnackbar.getController() instanceof AutoTranslateSnackbarController);
        Assert.assertTrue(
                "Incorrect ActionData type",
                currentSnackbar.getActionDataForTesting()
                        instanceof AutoTranslateSnackbarController.TargetLanguageData);
        AutoTranslateSnackbarController.TargetLanguageData data =
                (AutoTranslateSnackbarController.TargetLanguageData)
                        currentSnackbar.getActionDataForTesting();
        Assert.assertEquals("en", data.getTargetLanguage());
    }

    /**
     * The target language is stored in Translate format, which uses the old deprecated Java codes
     * for several languages (Hebrew, Indonesian), and uses "tl" while Chromium uses "fil" for
     * Tagalog/Filipino. This tests that when using Translate format codes the Chrome version is
     * displayed in the Snackbar.
     */
    @Test
    @SmallTest
    public void testShowSnackbarChromeLanguage() throws Exception {
        // Use the Translate tag tl which Chrome should display as "Filipino"
        showSnackbar("tl");

        Snackbar currentSnackbar = getCurrentSnackbar();

        // Should be Filipino and not Tagalog.
        Assert.assertEquals("Page translated to Filipino", getSnackbarMessageText());
        AutoTranslateSnackbarController.TargetLanguageData data =
                (AutoTranslateSnackbarController.TargetLanguageData)
                        currentSnackbar.getActionDataForTesting();
        Assert.assertEquals("tl", data.getTargetLanguage());
    }

    @Test
    @SmallTest
    public void testDismiss() throws Exception {
        showSnackbar("en");
        dismissSnackbar();
        Assert.assertNull(getCurrentSnackbar());
    }

    @Test
    @SmallTest
    public void testOnSnackbarActionClicked() throws Exception {
        showSnackbar("tl");
        clickSnackbarAction();

        Mockito.verify(mMockJni).onUndoActionPressed(NATIVE_SNACKBAR_VIEW, "tl");
    }

    @Test
    @SmallTest
    public void testOnSnackbarAutoDismissed() throws Exception {
        showSnackbar("en");

        timeoutSnackbar();

        Mockito.verify(mMockJni).onDismissNoAction(NATIVE_SNACKBAR_VIEW);
    }

    private void showSnackbar(String targetLanguage) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mAutoTranslateSnackbarController.show(targetLanguage));
    }

    private void dismissSnackbar() {
        ThreadUtils.runOnUiThreadBlocking(() -> mAutoTranslateSnackbarController.dismiss());
    }

    private void clickSnackbarAction() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mSnackbarManager.onClick(
                                mActivityTestRule
                                        .getActivity()
                                        .findViewById(R.id.snackbar_button)));
    }

    private void timeoutSnackbar() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mSnackbarManager.dismissSnackbars(
                                mSnackbarManager.getCurrentSnackbarForTesting().getController()));
    }

    private String getSnackbarMessageText() {
        return ((TextView) mActivityTestRule.getActivity().findViewById(R.id.snackbar_message))
                .getText()
                .toString();
    }

    private String getSnackbarActionText() {
        return ((Button) mActivityTestRule.getActivity().findViewById(R.id.snackbar_button))
                .getText()
                .toString();
    }

    private Snackbar getCurrentSnackbar() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mSnackbarManager.getCurrentSnackbarForTesting());
    }
}
