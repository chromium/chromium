// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
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

import org.chromium.base.StrictModeContext;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.download.DirectoryOption;
import org.chromium.chrome.browser.download.DownloadDialogBridge;
import org.chromium.chrome.browser.download.DownloadDialogBridgeJni;
import org.chromium.chrome.browser.download.DownloadDirectoryProvider;
import org.chromium.chrome.browser.download.DownloadLocationDialogType;
import org.chromium.chrome.browser.download.DownloadPromptStatus;
import org.chromium.chrome.browser.download.R;
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
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/**
 * Test focus on verifying UI elements in the download location dialog.
 */
// TODO(xingliu): Implement more test cases to test every MVC properties.
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DownloadLocationDialogTest extends DummyUiActivityTestCase {
    private static final long TOTAL_BYTES = 1024L;
    private static final String SUGGESTED_PATH = "download.png";
    private static final String PRIMARY_STORAGE_PATH = "/sdcard";
    private static final String SECONDARY_STORAGE_PATH = "/android/Download";

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private DownloadLocationDialogController mController;
    @Mock
    private Profile mProfileMock;
    @Mock
    PrefService mPrefService;
    @Mock
    private UserPrefs.Natives mUserPrefsJniMock;
    @Mock
    private DownloadDialogBridge.Natives mDownloadDialogBridgeJniMock;

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
        when(mDownloadDialogBridgeJniMock.getDownloadDefaultDirectory())
                .thenReturn(PRIMARY_STORAGE_PATH);
        Profile.setLastUsedProfileForTesting(mProfileMock);
        mAppModalPresenter = new AppModalPresenter(getActivity());
        mModalDialogManager =
                new ModalDialogManager(mAppModalPresenter, ModalDialogManager.ModalDialogType.APP);
        Map<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.SMART_SUGGESTION_FOR_LARGE_DOWNLOADS, false);
        ChromeFeatureList.setTestFeatures(features);

        setDownloadPromptStatus(DownloadPromptStatus.SHOW_INITIAL);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Create fake directory options.
            ArrayList<DirectoryOption> dirs = new ArrayList<>();
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                dirs.add(buildDirectoryOption(DirectoryOption.DownloadLocationDirectoryType.DEFAULT,
                        PRIMARY_STORAGE_PATH));
                dirs.add(buildDirectoryOption(
                        DirectoryOption.DownloadLocationDirectoryType.ADDITIONAL,
                        SECONDARY_STORAGE_PATH));
            }
            DownloadDirectoryProvider.getInstance().setDirectoryProviderForTesting(
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
            long totalBytes, @DownloadLocationDialogType int dialogType, String suggestedPath) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDialogCoordinator.showDialog(
                    getActivity(), mModalDialogManager, totalBytes, dialogType, suggestedPath);
        });
    }

    private void assertTitle(@StringRes int titleId) {
        onView(withText(getActivity().getString(titleId))).check(matches(isDisplayed()));
    }

    private void assertSubtitle(String subtitle) {
        onView(withText(subtitle)).check(matches(isDisplayed()));
    }

    /**
     * Verifies the state of the "don't show again" checkbox.
     * @param checked The expected state of the checkbox. If null, the checkbox is expected to be
     *         hidden.
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

    @Test
    @MediumTest
    public void testDefaultLocationDialog() throws Exception {
        showDialog(TOTAL_BYTES, DownloadLocationDialogType.DEFAULT, SUGGESTED_PATH);
        assertTitle(R.string.download_location_dialog_title);
        assertSubtitle(DownloadUtils.getStringForBytes(getActivity(), TOTAL_BYTES));
        assertDontShowAgainCheckbox(true);
    }

    @Test
    @MediumTest
    public void testDefaultLocationDialogUnchecked() throws Exception {
        setDownloadPromptStatus(DownloadPromptStatus.SHOW_PREFERENCE);
        showDialog(TOTAL_BYTES, DownloadLocationDialogType.DEFAULT, SUGGESTED_PATH);
        assertTitle(R.string.download_location_dialog_title);
        assertSubtitle(DownloadUtils.getStringForBytes(getActivity(), TOTAL_BYTES));
        assertDontShowAgainCheckbox(false);
    }

    @Test
    @MediumTest
    public void testForceShowEnterprisePolicy() {
        when(mDownloadDialogBridgeJniMock.isLocationDialogManaged()).thenReturn(true);
        setPromptForPolicy(true);
        setDownloadPromptStatus(DownloadPromptStatus.SHOW_PREFERENCE);
        showDialog(TOTAL_BYTES, DownloadLocationDialogType.DEFAULT, SUGGESTED_PATH);
        assertTitle(R.string.download_location_dialog_title_confirm_download);
        assertSubtitle(DownloadUtils.getStringForBytes(getActivity(), TOTAL_BYTES));
        assertDontShowAgainCheckbox(null);
    }
}
