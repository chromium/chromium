// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.download.DuplicateDownloadDialog;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils;
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
    private static final String DOWNLOAD_DOMAIN = "pageurl.com";
    public static final int ICON_ID = R.drawable.btn_close;

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private ModalDialogManager mModalDialogManager;

    private final Callback<Boolean> mResultCallback = Mockito.mock(Callback.class);

    @Before
    public void setUpTest() throws Exception {
        mActivityTestRule.startOnBlankPage();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppModalPresenter mAppModalPresenter =
                            new AppModalPresenter(mActivityTestRule.getActivity());
                    mModalDialogManager =
                            new ModalDialogManager(
                                    mAppModalPresenter, ModalDialogManager.ModalDialogType.APP);
                });
        ModalDialogView.disableButtonTapProtectionForTesting();
    }

    @Test
    @LargeTest
    public void testDuplicateDownloadForIncognitoMode() throws Exception {
        // Showing a duplicate download dialog with an Incognito profile.
        OtrProfileId primaryProfileId = OtrProfileId.getPrimaryOtrProfileId();
        showDuplicateDialog(primaryProfileId);

        // Verify the Incognito warning message is shown (2 paragraphs).
        ModalDialogTestUtils.assertMessageParagraphCount(2);

        // Dismiss the dialog and verify the callback is called with false.
        onView(withId(R.id.negative_button)).inRoot(isDialog()).perform(click());
        verify(mResultCallback).onResult(false);
    }

    @Test
    @LargeTest
    public void testDuplicateDownloadForRegularProfile() throws Exception {
        // Showing a duplicate download dialog with a regular profile.
        OtrProfileId regularProfileId = null;
        showDuplicateDialog(regularProfileId);

        // Verify the Incognito warning message is NOT shown (1 paragraph).
        ModalDialogTestUtils.assertMessageParagraphCount(1);
    }

    @Test
    @LargeTest
    public void testDuplicateDownloadForIncognitoCct() throws Exception {
        // Showing a duplicate download dialog with a non-primary off-the-record profile.
        OtrProfileId nonPrimaryOtrId = OtrProfileId.createUnique("CCT:Incognito");
        showDuplicateDialog(nonPrimaryOtrId);

        // Verify the Incognito warning message is shown (2 paragraphs).
        ModalDialogTestUtils.assertMessageParagraphCount(2);

        // Accept the dialog and verify the callback is called with true.
        onView(withId(R.id.positive_button)).inRoot(isDialog()).perform(click());
        verify(mResultCallback).onResult(true);
    }

    @Test
    @LargeTest
    public void testInsecureDownloadDownloadDoNotShowIncognitoWarning() throws Exception {
        // Showing an insecure download dialog with a regular profile.
        showInsecureDownloadDialog();

        // Verify the Incognito warning message is NOT shown (1 paragraph).
        ModalDialogTestUtils.assertMessageParagraphCount(1);

        // Dismiss the dialog and verify the callback is called with false.
        onView(withId(R.id.negative_button)).inRoot(isDialog()).perform(click());
        verify(mResultCallback).onResult(false);
    }

    @Test
    @LargeTest
    public void testDangerousContentDownloadDoNotShowIncognitoWarning() throws Exception {
        // Showing a dangerous content download dialog with a regular profile.
        showDangerousContentDialog();

        // Verify the Incognito warning message is NOT shown (1 paragraph).
        ModalDialogTestUtils.assertMessageParagraphCount(1);

        // Dismiss the dialog and verify the callback is called with false.
        onView(withId(R.id.negative_button)).inRoot(isDialog()).perform(click());
        verify(mResultCallback).onResult(false);
    }

    private void showDuplicateDialog(OtrProfileId otrProfileId) {
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
                                    otrProfileId,
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
                                    DOWNLOAD_DOMAIN,
                                    ICON_ID,
                                    mResultCallback);
                });
    }
}
