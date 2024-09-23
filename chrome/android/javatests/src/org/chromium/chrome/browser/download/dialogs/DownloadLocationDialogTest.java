// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isNotChecked;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import androidx.annotation.StringRes;
import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.FeatureList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DirectoryOption;
import org.chromium.chrome.browser.download.DownloadDialogBridge;
import org.chromium.chrome.browser.download.DownloadDialogBridgeJni;
import org.chromium.chrome.browser.download.DownloadDirectoryProvider;
import org.chromium.chrome.browser.download.DownloadLocationDialogType;
import org.chromium.chrome.browser.download.DownloadPromptStatus;
import org.chromium.chrome.browser.download.TestDownloadDirectoryProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/** Test focus on verifying UI elements in the download location dialog. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DownloadLocationDialogTest extends BlankUiTestActivityTestCase {
    private static final long TOTAL_BYTES = 1024L;
    private static final String SUGGESTED_PATH = "download.png";
    private static final String PRIMARY_STORAGE_PATH = "/sdcard";
    private static final String SECONDARY_STORAGE_PATH = "/android/Download";

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private DownloadLocationDialogController mController;
    @Mock private Profile mProfileMock;
    @Mock private Profile mIncognitoProfileMock;
    @Mock PrefService mPrefService;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private DownloadDialogBridge.Natives mDownloadDialogBridgeJniMock;

    private AppModalPresenter mAppModalPresenter;
    private ModalDialogManager mModalDialogManager;
    private DownloadLocationDialogCoordinator mDialogCoordinator;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(any())).thenReturn(mPrefService);
        mJniMocker.mock(DownloadDialogBridgeJni.TEST_HOOKS, mDownloadDialogBridgeJniMock);
        when(mPrefService.getString(Pref.DOWNLOAD_DEFAULT_DIRECTORY))
                .thenReturn(PRIMARY_STORAGE_PATH);

        when(mProfileMock.getOriginalProfile()).thenReturn(mProfileMock);
        when(mIncognitoProfileMock.getOriginalProfile()).thenReturn(mProfileMock);
        when(mIncognitoProfileMock.isOffTheRecord()).thenReturn(true);

        mAppModalPresenter = new AppModalPresenter(getActivity());
        mModalDialogManager =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return new ModalDialogManager(
                                    mAppModalPresenter, ModalDialogManager.ModalDialogType.APP);
                        });
        Map<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.SMART_SUGGESTION_FOR_LARGE_DOWNLOADS, false);
        FeatureList.setTestFeatures(features);

        setDownloadPromptStatus(DownloadPromptStatus.SHOW_INITIAL);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Create fake directory options.
                    ArrayList<DirectoryOption> dirs = new ArrayList<>();
                    dirs.add(
                            buildDirectoryOption(
                                    DirectoryOption.DownloadLocationDirectoryType.DEFAULT,
                                    PRIMARY_STORAGE_PATH));
                    dirs.add(
                            buildDirectoryOption(
                                    DirectoryOption.DownloadLocationDirectoryType.ADDITIONAL,
                                    SECONDARY_STORAGE_PATH));
                    DownloadDirectoryProvider.getInstance()
                            .setDirectoryProviderForTesting(
                                    new TestDownloadDirectoryProvider(dirs));
                });

        mDialogCoordinator = new DownloadLocationDialogCoordinator();
        mDialogCoordinator.initialize(mController);
    }

    private DirectoryOption buildDirectoryOption(
            @DirectoryOption.DownloadLocationDirectoryType int type, String directoryPath) {
        return new DirectoryOption("Download", directoryPath, 1024000, 1024000, type);
    }

    private void setDownloadPromptStatus(@DownloadPromptStatus int promptStatus) {
        when(mPrefService.getInteger(Pref.PROMPT_FOR_DOWNLOAD_ANDROID)).thenReturn(promptStatus);
    }

    private void setPromptForPolicy(boolean promptForPolicy) {
        when(mPrefService.getBoolean(Pref.PROMPT_FOR_DOWNLOAD)).thenReturn(promptForPolicy);
    }

    private void showDialog(
            long totalBytes,
            @DownloadLocationDialogType int dialogType,
            String suggestedPath,
            Profile profile) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialogCoordinator.showDialog(
                            getActivity(),
                            mModalDialogManager,
                            totalBytes,
                            dialogType,
                            suggestedPath,
                            profile);
                });
    }

    private void assertTitle(@StringRes int titleId) {
        onView(withText(getActivity().getString(titleId)))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    private void assertSubtitle(String subtitle) {
        onView(withText(subtitle)).inRoot(isDialog()).check(matches(isDisplayed()));
    }

    /**
     * Verifies the state of the "don't show again" checkbox.
     *
     * @param checked The expected state of the checkbox. If null, the checkbox is expected to be
     *     hidden.
     */
    private void assertDontShowAgainCheckbox(Boolean checked) {
        if (checked == null) {
            onView(withId(R.id.show_again_checkbox)).check(matches(withEffectiveVisibility(GONE)));
        } else if (checked) {
            onView(withId(R.id.show_again_checkbox)).check(matches(isChecked()));
        } else {
            onView(withId(R.id.show_again_checkbox)).check(matches(isNotChecked()));
        }
    }

    private void assertIncognitoWarningShown(boolean shown) {
        onView(withId(R.id.incognito_warning))
                .check(matches(withEffectiveVisibility(shown ? VISIBLE : GONE)));
    }

    @Test
    @MediumTest
    public void testDefaultLocationDialog() throws Exception {
        showDialog(TOTAL_BYTES, DownloadLocationDialogType.DEFAULT, SUGGESTED_PATH, mProfileMock);
        assertTitle(R.string.download_location_dialog_title);
        assertSubtitle(DownloadUtils.getStringForBytes(getActivity(), TOTAL_BYTES));
        assertIncognitoWarningShown(false);
        assertDontShowAgainCheckbox(true);
    }

    @Test
    @MediumTest
    public void testDefaultLocationDialogIncognito() {
        showDialog(
                TOTAL_BYTES,
                DownloadLocationDialogType.DEFAULT,
                SUGGESTED_PATH,
                mIncognitoProfileMock);
        assertTitle(R.string.download_location_dialog_title);
        assertSubtitle(DownloadUtils.getStringForBytes(getActivity(), TOTAL_BYTES));
        assertIncognitoWarningShown(true);
        assertDontShowAgainCheckbox(null);
    }

    @Test
    @MediumTest
    public void testDefaultLocationDialogUnchecked() throws Exception {
        setDownloadPromptStatus(DownloadPromptStatus.SHOW_PREFERENCE);
        showDialog(TOTAL_BYTES, DownloadLocationDialogType.DEFAULT, SUGGESTED_PATH, mProfileMock);
        assertTitle(R.string.download_location_dialog_title);
        assertSubtitle(DownloadUtils.getStringForBytes(getActivity(), TOTAL_BYTES));
        assertIncognitoWarningShown(false);
        assertDontShowAgainCheckbox(false);
    }

    @Test
    @MediumTest
    public void testLocationFull() throws Exception {
        showDialog(
                TOTAL_BYTES,
                DownloadLocationDialogType.LOCATION_FULL,
                SUGGESTED_PATH,
                mProfileMock);
        assertTitle(R.string.download_location_not_enough_space);
        assertSubtitle(
                getActivity()
                        .getResources()
                        .getString(R.string.download_location_download_to_default_folder));
        assertIncognitoWarningShown(false);
        assertDontShowAgainCheckbox(null);
    }

    @Test
    @MediumTest
    public void testLocationNotFound() throws Exception {
        showDialog(
                TOTAL_BYTES,
                DownloadLocationDialogType.LOCATION_NOT_FOUND,
                SUGGESTED_PATH,
                mProfileMock);
        assertTitle(R.string.download_location_no_sd_card);
        assertSubtitle(
                getActivity()
                        .getResources()
                        .getString(R.string.download_location_download_to_default_folder));
        assertIncognitoWarningShown(false);
        assertDontShowAgainCheckbox(null);
    }

    @Test
    @MediumTest
    public void testNameTooLong() throws Exception {
        showDialog(
                TOTAL_BYTES,
                DownloadLocationDialogType.NAME_TOO_LONG,
                SUGGESTED_PATH,
                mProfileMock);
        assertTitle(R.string.download_location_rename_file);
        assertSubtitle(
                getActivity().getResources().getString(R.string.download_location_name_too_long));
        assertIncognitoWarningShown(false);
        assertDontShowAgainCheckbox(null);
    }

    @Test
    @MediumTest
    public void testNameConflict() throws Exception {
        showDialog(
                TOTAL_BYTES,
                DownloadLocationDialogType.NAME_CONFLICT,
                SUGGESTED_PATH,
                mProfileMock);
        assertTitle(R.string.download_location_download_again);
        assertSubtitle(
                getActivity().getResources().getString(R.string.download_location_name_exists));
        assertIncognitoWarningShown(false);
        assertDontShowAgainCheckbox(true);
    }

    @Test
    @MediumTest
    public void testNameConflictIncognito() throws Exception {
        showDialog(
                TOTAL_BYTES,
                DownloadLocationDialogType.NAME_CONFLICT,
                SUGGESTED_PATH,
                mIncognitoProfileMock);
        assertTitle(R.string.download_location_download_again);
        assertSubtitle(
                getActivity().getResources().getString(R.string.download_location_name_exists));
        assertIncognitoWarningShown(true);
        assertDontShowAgainCheckbox(null);
    }

    @Test
    @MediumTest
    public void testForceShowEnterprisePolicy() throws Exception {
        when(mPrefService.isManagedPreference(Pref.PROMPT_FOR_DOWNLOAD)).thenReturn(true);
        setPromptForPolicy(true);
        setDownloadPromptStatus(DownloadPromptStatus.SHOW_PREFERENCE);
        showDialog(TOTAL_BYTES, DownloadLocationDialogType.DEFAULT, SUGGESTED_PATH, mProfileMock);
        assertTitle(R.string.download_location_dialog_title_confirm_download);
        assertSubtitle(DownloadUtils.getStringForBytes(getActivity(), TOTAL_BYTES));
        assertIncognitoWarningShown(false);
        assertDontShowAgainCheckbox(null);
    }
}
