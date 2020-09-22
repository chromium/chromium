// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.widget.CheckBox;

import androidx.test.espresso.NoMatchingViewException;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.download.DownloadLaterPromptStatus;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Test to verify download later dialog.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DownloadLaterDialogTest {
    private static final long INVALID_START_TIME = -1;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private DownloadLaterDialogCoordinator mDialogCoordinator;
    private PropertyModel mModel;

    @Mock
    private DownloadLaterDialogController mController;

    @Mock
    DownloadDateTimePickerDialog mDateTimePicker;

    @Mock
    PrefService mPrefService;

    private ModalDialogManager getModalDialogManager() {
        return mActivityTestRule.getActivity().getModalDialogManager();
    }

    private DownloadLaterDialogView getDownloadLaterDialogView() {
        return (DownloadLaterDialogView) getModalDialogManager().getCurrentDialogForTest().get(
                ModalDialogProperties.CUSTOM_VIEW);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mPrefService.getInteger(Pref.DOWNLOAD_LATER_PROMPT_STATUS))
                .thenReturn(DownloadLaterPromptStatus.SHOW_INITIAL);
        doNothing().when(mPrefService).setInteger(anyString(), anyInt());

        mActivityTestRule.startMainActivityOnBlankPage();
        mDialogCoordinator = new DownloadLaterDialogCoordinator(mDateTimePicker);
        mModel = createModel(
                DownloadLaterDialogChoice.ON_WIFI, DownloadLaterPromptStatus.SHOW_INITIAL);

        Assert.assertNotNull(mController);
        mDialogCoordinator.initialize(mController);
    }

    private PropertyModel createModel(Integer choice, Integer promptStatus) {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(DownloadLaterDialogProperties.ALL_KEYS)
                        .with(DownloadLaterDialogProperties.CONTROLLER, mDialogCoordinator);
        if (choice != null) {
            builder.with(DownloadLaterDialogProperties.INITIAL_CHOICE, choice);
        }

        if (promptStatus != null) {
            builder.with(DownloadLaterDialogProperties.DONT_SHOW_AGAIN_SELECTION, promptStatus);
        }

        return builder.build();
    }

    private void showDialog() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDialogCoordinator.showDialog(
                    mActivityTestRule.getActivity(), getModalDialogManager(), mPrefService, mModel);
        });
    }

    private void clickPositiveButton() {
        onView(withId(org.chromium.chrome.R.id.positive_button)).perform(click());
    }

    private void clickNegativeButton() {
        onView(withId(org.chromium.chrome.R.id.negative_button)).perform(click());
    }

    private void assertPositiveButtonText(String expectedText) {
        onView(withId(org.chromium.chrome.R.id.positive_button))
                .check(matches(withText(expectedText)));
    }

    private void assertShowAgainCheckBox(boolean enabled, int visibility, boolean checked) {
        onView(withId(R.id.show_again_checkbox)).check((View view, NoMatchingViewException e) -> {
            Assert.assertEquals(enabled, view.isEnabled());
            Assert.assertEquals(visibility, view.getVisibility());
            if (visibility == View.VISIBLE) {
                Assert.assertEquals(checked, ((CheckBox) (view)).isChecked());
            }
        });
    }

    private void assertEditText(boolean hasEditText) {
        if (hasEditText) {
            onView(withId(R.id.edit_location)).check(matches(isDisplayed()));
        } else {
            onView(withId(R.id.edit_location)).check(matches(not(isDisplayed())));
        }
    }

    @Test
    @MediumTest
    public void testInitialSelectionDownloadNowWithOutCheckbox() {
        mModel = createModel(DownloadLaterDialogChoice.DOWNLOAD_NOW, null);
        showDialog();
        assertPositiveButtonText("Download");
        assertShowAgainCheckBox(true, View.GONE, true);
        assertEditText(false);
    }

    @Test
    @MediumTest
    public void testInitialSelectionOnWifiWithCheckbox() {
        mModel = createModel(
                DownloadLaterDialogChoice.ON_WIFI, DownloadLaterPromptStatus.SHOW_INITIAL);
        showDialog();
        assertPositiveButtonText("Download");
        assertShowAgainCheckBox(true, View.VISIBLE, false);
        assertEditText(false);
    }

    @Test
    @MediumTest
    public void testInitialSelectionOnWifiWithEditLocation() {
        mModel = createModel(
                DownloadLaterDialogChoice.ON_WIFI, DownloadLaterPromptStatus.SHOW_PREFERENCE);
        mModel.set(DownloadLaterDialogProperties.LOCATION_TEXT, "location");
        showDialog();
        assertPositiveButtonText("Download");
        assertShowAgainCheckBox(true, View.VISIBLE, false);
        assertEditText(true);
    }

    @Test
    @MediumTest
    public void testInitialSelectionDownloadLater() {
        mModel = createModel(
                DownloadLaterDialogChoice.DOWNLOAD_LATER, DownloadLaterPromptStatus.SHOW_INITIAL);
        showDialog();
        assertPositiveButtonText("Next");
        assertShowAgainCheckBox(false, View.VISIBLE, false);
        assertEditText(false);
    }

    @Test
    @MediumTest
    public void testClickNegativeButtonShouldCancel() {
        showDialog();
        clickNegativeButton();
        verify(mController).onDownloadLaterDialogCanceled();
    }

    @Test
    @MediumTest
    public void testSelectFromOnWifiToDownloadNow() {
        showDialog();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Verify the initial selection of the dialog. The controller should not get an event
            // for the initial setup.
            RadioButtonWithDescription onWifiButton =
                    (RadioButtonWithDescription) getDownloadLaterDialogView().findViewById(
                            org.chromium.chrome.browser.download.R.id.on_wifi);
            Assert.assertTrue(onWifiButton.isChecked());

            // Simulate a click on another radio button, the event should be propagated to
            // controller.
            RadioButtonWithDescription downloadNowButton =
                    getDownloadLaterDialogView().findViewById(R.id.download_now);
            Assert.assertNotNull(downloadNowButton);
            downloadNowButton.setChecked(true);
            getDownloadLaterDialogView().onCheckedChanged(null, -1);
        });

        clickPositiveButton();
        verify(mController)
                .onDownloadLaterDialogComplete(
                        eq(DownloadLaterDialogChoice.DOWNLOAD_NOW), eq(INVALID_START_TIME));
    }

    @Test
    @MediumTest
    public void testSelectFromOnWifiToDownloadLater() {
        showDialog();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            RadioButtonWithDescription downloadLaterButton =
                    getDownloadLaterDialogView().findViewById(R.id.choose_date_time);
            Assert.assertNotNull(downloadLaterButton);
            downloadLaterButton.setChecked(true);
            getDownloadLaterDialogView().onCheckedChanged(null, -1);
        });

        assertPositiveButtonText("Next");
        assertShowAgainCheckBox(false, View.VISIBLE, false);

        clickPositiveButton();
        verify(mController, times(0)).onDownloadLaterDialogComplete(anyInt(), anyLong());
        verify(mDateTimePicker).showDialog(any(), any(), any());
    }
}
