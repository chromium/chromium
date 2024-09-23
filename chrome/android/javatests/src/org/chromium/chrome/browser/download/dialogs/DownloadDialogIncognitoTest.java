// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.content.Context;

import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.action.ViewActions;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.chrome.browser.download.DuplicateDownloadDialog;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.modaldialog.ModalDialogView;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Test to verify download dialog scenarios. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DownloadDialogIncognitoTest {
    private static final long TOTAL_BYTES = 1024L;
    private static final String DOWNLOAD_PATH = "/android/Download";
    private static final String PAGE_URL = "www.pageurl.com/download";
    private static final String FILE_NAME = "download.pdf";
    public static final int ICON_ID = R.drawable.btn_close;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ModalDialogManager mModalDialogManager;

    private Callback<Boolean> mResultCallback = Mockito.mock(Callback.class);

    @Before
    public void setUpTest() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppModalPresenter mAppModalPresenter =
                            new AppModalPresenter(mActivityTestRule.getActivity());
                    mModalDialogManager =
                            ThreadUtils.runOnUiThreadBlocking(
                                    () -> {
                                        return new ModalDialogManager(
                                                mAppModalPresenter,
                                                ModalDialogManager.ModalDialogType.APP);
                                    });
                });
        ModalDialogView.disableButtonTapProtectionForTesting();
    }

    @Test
    @LargeTest
    public void testDuplicateDownloadForIncognitoMode() throws Exception {
        // Showing a duplicate download dialog with an Incognito profile.
        OTRProfileID primaryProfileID = OTRProfileID.getPrimaryOTRProfileID();
        showDuplicateDialog(primaryProfileID);

        // Verify the Incognito warning message is shown.
        waitForWarningVisibilityToBe(VISIBLE);

        // Dismiss the dialog and verify the callback is called with false.
        onView(withId(R.id.negative_button)).inRoot(isDialog()).perform(ViewActions.click());
        verify(mResultCallback).onResult(false);
    }

    @Test
    @LargeTest
    public void testDuplicateDownloadForRegularProfile() throws Exception {
        // Showing a duplicate download dialog with a regular profile.
        OTRProfileID regularProfileID = null;
        showDuplicateDialog(regularProfileID);

        // Verify the Incognito warning message is NOT shown.
        waitForWarningVisibilityToBe(GONE);
    }

    @Test
    @LargeTest
    public void testDuplicateDownloadForIncognitoCCT() throws Exception {
        // Showing a duplicate download dialog with a non-primary off-the-record profile.
        OTRProfileID nonPrimaryOTRId = OTRProfileID.createUnique("CCT:Incognito");
        showDuplicateDialog(nonPrimaryOTRId);

        // Verify the Incognito warning message is shown.
        waitForWarningVisibilityToBe(VISIBLE);

        // Accept the dialog and verify the callback is called with true.
        onView(withId(R.id.positive_button)).inRoot(isDialog()).perform(ViewActions.click());
        verify(mResultCallback).onResult(true);
    }

    @Test
    @LargeTest
    public void testInsecureDownloadDownloadDoNotShowIncognitoWarning() throws Exception {
        // Showing an insecure download dialog with a regular profile.
        showInsecureDownloadDialog();

        // Verify the Incognito warning message is NOT shown.
        waitForWarningVisibilityToBe(GONE);

        // Dismiss the dialog and verify the callback is called with false.
        onView(withId(R.id.negative_button)).inRoot(isDialog()).perform(ViewActions.click());
        verify(mResultCallback).onResult(false);
    }

    @Test
    @LargeTest
    public void testDangerousContentDownloadDoNotShowIncognitoWarning() throws Exception {
        // Showing a dengarious content download dialog with a regular profile.
        showDangerousContentDialog();

        // Verify the Incognito warning message is NOT shown.
        waitForWarningVisibilityToBe(GONE);

        // Dismiss the dialog and verify the callback is called with false.
        onView(withId(R.id.negative_button)).inRoot(isDialog()).perform(ViewActions.click());
        verify(mResultCallback).onResult(false);
    }

    private void showDuplicateDialog(OTRProfileID otrProfileID) {
        Context mContext = mActivityTestRule.getActivity().getApplicationContext();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    new DuplicateDownloadDialog()
                            .show(
                                    mContext,
                                    mModalDialogManager,
                                    DOWNLOAD_PATH,
                                    PAGE_URL,
                                    TOTAL_BYTES,
                                    true,
                                    otrProfileID,
                                    mResultCallback);
                });
    }

    private void showInsecureDownloadDialog() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Context mContext = mActivityTestRule.getActivity().getApplicationContext();
                    new InsecureDownloadDialog()
                            .show(
                                    mContext,
                                    mModalDialogManager,
                                    FILE_NAME,
                                    TOTAL_BYTES,
                                    mResultCallback);
                });
    }

    private void showDangerousContentDialog() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Context mContext = mActivityTestRule.getActivity().getApplicationContext();
                    new DangerousDownloadDialog()
                            .show(
                                    mContext,
                                    mModalDialogManager,
                                    FILE_NAME,
                                    TOTAL_BYTES,
                                    ICON_ID,
                                    mResultCallback);
                });
    }

    private void waitForWarningVisibilityToBe(Visibility visibility) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        onView(withId(R.id.message_paragraph_2)).inRoot(isDialog())
                                .check(matches(withEffectiveVisibility(visibility)));
                    } catch (NoMatchingViewException | AssertionError e) {
                        throw new CriteriaNotSatisfiedException(
                                "Timeout while waiting for warning to have visibility: "
                                        + (visibility == VISIBLE ? "VISIBLE" : "GONE"));
                    }
                },
                DEFAULT_MAX_TIME_TO_POLL * 10,
                DEFAULT_POLLING_INTERVAL);
    }
}
