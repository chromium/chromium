// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.espresso.action.ViewActions;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.download.DuplicateDownloadDialog;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Test to verify download dialog scenarios.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.EnableFeatures({ChromeFeatureList.INCOGNITO_DOWNLOADS_WARNING})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DownloadDialogIncognitoTest {
    private static final long TOTAL_BYTES = 1024L;
    private static final String DOWNLOAD_PATH = "/android/Download";
    private static final String PAGE_URL = "www.pageurl.com/download";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ModalDialogManager mModalDialogManager;

    private Callback<Boolean> mResultCallback = Mockito.mock(Callback.class);

    @Before
    public void setUpTest() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        MockitoAnnotations.initMocks(this);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        AppModalPresenter mAppModalPresenter =
                new AppModalPresenter(mActivityTestRule.getActivity());
        mModalDialogManager = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return new ModalDialogManager(
                    mAppModalPresenter, ModalDialogManager.ModalDialogType.APP);
        });
    }

    @Test
    @MediumTest
    public void testDuplicateDownloadForIncognitoMode() throws Exception {
        // Showing a duplicate download dialog with an Incognito profile.
        OTRProfileID primaryProfileID = OTRProfileID.getPrimaryOTRProfileID();
        showDuplicateDialogUsing(primaryProfileID);

        // Verify the Incognito warning message is shown.
        onView(withId(R.id.message_paragraph_2)).check(matches(withEffectiveVisibility(VISIBLE)));

        // Dismiss the dialog and verify the callback is called with false.
        onView(withId(R.id.negative_button)).perform(ViewActions.click());
        verify(mResultCallback).onResult(false);
    }

    @Test
    @MediumTest
    public void testDuplicateDownloadForRegularProfile() throws Exception {
        // Showing a duplicate download dialog with a regular profile.
        OTRProfileID regularProfileID = null;
        showDuplicateDialogUsing(regularProfileID);

        // Verify the Incognito warning message is NOT shown.
        onView(withId(R.id.message_paragraph_2)).check(matches(withEffectiveVisibility(GONE)));
    }

    @Test
    @MediumTest
    public void testDuplicateDownloadForIncognitoCCT() throws Exception {
        // Showing a duplicate download dialog with a non-primary off-the-record profile.
        OTRProfileID nonPrimaryOTRId = OTRProfileID.createUnique("CCT:Incognito");
        showDuplicateDialogUsing(nonPrimaryOTRId);

        // Verify the Incognito warning message is shown.
        onView(withId(R.id.message_paragraph_2)).check(matches(withEffectiveVisibility(VISIBLE)));

        // Accept the dialog and verify the callback is called with true.
        onView(withId(R.id.positive_button)).perform(ViewActions.click());
        verify(mResultCallback).onResult(true);
    }

    private void showDuplicateDialogUsing(OTRProfileID otrProfileID) {
        Context mContext = mActivityTestRule.getActivity().getApplicationContext();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            new DuplicateDownloadDialog().show(mContext, mModalDialogManager, DOWNLOAD_PATH,
                    PAGE_URL, TOTAL_BYTES, true, otrProfileID, mResultCallback);
        });
    }
}
