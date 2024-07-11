// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.equalTo;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.download.DownloadTestRule.CustomMainActivityStart;
import org.chromium.chrome.browser.download.settings.DownloadDirectoryAdapter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.util.ArrayList;

/** Test to verify download end to end flow with download location dialog. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DownloadLocationChangeEnd2EndTest implements CustomMainActivityStart {
    @Rule public DownloadTestRule mDownloadTestRule = new DownloadTestRule(this);

    private EmbeddedTestServer mTestServer;
    private static final String TEST_DATA_DIRECTORY = "/chrome/test/data/android/download/";
    private static final String TEST_FILE = "test.gzip";
    private static final long STORAGE_SIZE = 1024000;

    @Before
    public void setUp() {
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        ApplicationProvider.getApplicationContext(), ServerCertificate.CERT_OK);

        // Show the location dialog for the first time.
        promptDownloadLocationDialog(DownloadPromptStatus.SHOW_INITIAL);
    }

    // CustomMainActivityStart implementation.
    @Override
    public void customMainActivityStart() throws InterruptedException {
        mDownloadTestRule.startMainActivityOnBlankPage();
    }

    /** Ensures the default download location dialog is shown to the user with SD card inserted. */
    @Test
    @MediumTest
    @Feature({"Downloads"})
    @DisabledTest(message = "crbug.com/1415500")
    public void testDefaultDialogPositiveButtonClickThrough() {
        startDownload(/* hasSDCard= */ true);

        // Ensure the dialog is being shown.
        CriteriaHelper.pollUiThread(
                () -> mDownloadTestRule.getActivity().getModalDialogManager().isShowing());

        int currentCallCount = mDownloadTestRule.getChromeDownloadCallCount();

        // Click the button to start download.
        Espresso.onView(withId(R.id.positive_button)).perform(click());

        // Ensure download is done.
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(currentCallCount));

        mDownloadTestRule.deleteFilesInDownloadDirectory(new String[] {TEST_FILE});
    }

    /** Matches the {@link DirectoryOption} used in the {@link DownloadDirectoryAdapter}. */
    private static class DirectoryOptionMatcher extends TypeSafeMatcher<DirectoryOption> {
        private Matcher<String> mNameMatcher;

        public DirectoryOptionMatcher(Matcher<String> nameMatcher) {
            mNameMatcher = nameMatcher;
        }

        @Override
        protected boolean matchesSafely(DirectoryOption directoryOption) {
            return mNameMatcher.matches(directoryOption.name);
        }

        @Override
        public void describeTo(Description description) {
            description.appendText("has DirectoryOption with name: ");
            description.appendDescriptionOf(mNameMatcher);
        }
    }

    /**
     * Ensures the default download location dialog has two download location options in the drop
     * down spinner.
     */
    @Test
    @MediumTest
    @Feature({"Downloads"})
    @DisabledTest(message = "https://crbug.com/1381286")
    public void testDefaultDialogShowSpinner() {
        startDownload(/* hasSDCard= */ true);

        // Ensure the dialog is being shown.
        CriteriaHelper.pollUiThread(
                () -> mDownloadTestRule.getActivity().getModalDialogManager().isShowing());

        // Open the spinner inside the dialog to show download location options.
        Espresso.onView(withId(R.id.file_location)).perform(click());

        // Wait for data to feed into the DownloadDirectoryAdapter.
        String defaultOptionName =
                ApplicationProvider.getApplicationContext().getString(R.string.menu_downloads);
        String sdCardOptionName =
                ApplicationProvider.getApplicationContext()
                        .getString(R.string.downloads_location_sd_card);
        onData(new DirectoryOptionMatcher(equalTo(defaultOptionName))).atPosition(0);
        onData(new DirectoryOptionMatcher(equalTo(sdCardOptionName))).atPosition(1);
    }

    /**
     * Ensures no default download location dialog is shown to the user without SD card inserted.
     */
    @Test
    @MediumTest
    @Feature({"Downloads"})
    @DisableFeatures(ChromeFeatureList.SMART_SUGGESTION_FOR_LARGE_DOWNLOADS)
    public void testNoDialogWithoutSDCard() {
        int currentCallCount = mDownloadTestRule.getChromeDownloadCallCount();
        startDownload(/* hasSDCard= */ false);

        // Ensure download is done, no download location dialog should show to interact with user.
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(currentCallCount));
        mDownloadTestRule.deleteFilesInDownloadDirectory(new String[] {TEST_FILE});
    }

    @Test
    @MediumTest
    @Feature({"Downloads"})
    @Policies.Add({@Policies.Item(key = "PromptForDownloadLocation", string = "true")})
    public void testShowDialogWithoutSDCardWithPolicy() {
        startDownload(/* hasSDCard= */ false);
        CriteriaHelper.pollUiThread(
                () -> mDownloadTestRule.getActivity().getModalDialogManager().isShowing());
    }

    @Test
    @MediumTest
    @Feature({"Downloads"})
    @Policies.Add({@Policies.Item(key = "PromptForDownloadLocation", string = "false")})
    public void testNoDialogWithSDCardWithPolicy() {
        int currentCallCount = mDownloadTestRule.getChromeDownloadCallCount();
        startDownload(/* hasSDCard= */ true);
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(currentCallCount));
        mDownloadTestRule.deleteFilesInDownloadDirectory(new String[] {TEST_FILE});
    }

    /**
     * Starts a download, the download location dialog will show afterward.
     *
     * @param hasSDCard Whether the SD card download option is valid.
     */
    private void startDownload(boolean hasSDCard) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            DownloadPromptStatus.SHOW_INITIAL,
                            DownloadDialogBridge.getPromptForDownloadAndroid(
                                    mDownloadTestRule
                                            .getActivity()
                                            .getProfileProviderSupplier()
                                            .get()
                                            .getOriginalProfile()));

                    simulateDownloadDirectories(hasSDCard);

                    // Trigger the download through navigation.
                    LoadUrlParams params =
                            new LoadUrlParams(mTestServer.getURL(TEST_DATA_DIRECTORY + TEST_FILE));
                    mDownloadTestRule.getActivity().getActivityTab().loadUrl(params);
                });
    }

    /**
     * Provides default download directory and SD card directory.
     *
     * @param hasSDCard Whether to simulate SD card inserted.
     */
    private void simulateDownloadDirectories(boolean hasSDCard) {
        ArrayList<DirectoryOption> dirs = new ArrayList<>();

        dirs.add(
                buildDirectoryOption(
                        DirectoryOption.DownloadLocationDirectoryType.DEFAULT,
                        PathUtils.getExternalStorageDirectory()));
        if (hasSDCard) {
            dirs.add(
                    buildDirectoryOption(
                            DirectoryOption.DownloadLocationDirectoryType.ADDITIONAL,
                            PathUtils.getDataDirectory()));
        }

        DownloadDirectoryProvider.getInstance()
                .setDirectoryProviderForTesting(new TestDownloadDirectoryProvider(dirs));
    }

    private DirectoryOption buildDirectoryOption(
            @DirectoryOption.DownloadLocationDirectoryType int type, String directoryPath) {
        return new DirectoryOption("Download", directoryPath, STORAGE_SIZE, STORAGE_SIZE, type);
    }

    private void promptDownloadLocationDialog(@DownloadPromptStatus int promptStatus) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DownloadDialogBridge.setPromptForDownloadAndroid(
                            mDownloadTestRule
                                    .getActivity()
                                    .getProfileProviderSupplier()
                                    .get()
                                    .getOriginalProfile(),
                            promptStatus);
                });
    }
}
