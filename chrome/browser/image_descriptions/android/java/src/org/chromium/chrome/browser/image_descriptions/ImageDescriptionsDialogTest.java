// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_descriptions;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.thatMatchesFirst;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.View;
import android.widget.CheckBox;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/**
 *  Unit tests for {@link ImageDescriptionsDialog}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ImageDescriptionsDialogTest extends DummyUiActivityTestCase {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private ImageDescriptionsDialog.Delegate mDelegate;

    @Mock
    private UserPrefs.Natives mUserPrefsJniMock;

    @Mock
    private Profile mProfile;

    @Mock
    private PrefService mPrefService;

    private SharedPreferencesManager mManager;
    private ImageDescriptionsController mController;

    private ModalDialogManager mModalDialogManager;
    private ModalDialogManager.Presenter mAppModalPresenter;

    @Before
    public void setUp() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        Profile.setLastUsedProfileForTesting(mProfile);
        Mockito.when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);

        mAppModalPresenter = new AppModalPresenter(getActivity());
        mModalDialogManager =
                new ModalDialogManager(mAppModalPresenter, ModalDialogManager.ModalDialogType.APP);

        mManager = SharedPreferencesManager.getInstance();
        mController = ImageDescriptionsController.getInstance();
        mController.setDelegateForTesting(mDelegate);
    }

    // Helper methods for driving dialog control

    private void showDialog() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mController.disableImageDescriptions();
            mManager.writeInt(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT, 0);
            mManager.writeBoolean(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN, false);
            mController.onImageDescriptionsMenuItemSelected(getActivity(), mModalDialogManager);
        });
    }

    private void showDialogWithDontAskAgainVisible() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mController.disableImageDescriptions();
            mManager.writeInt(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT, 5);
            mController.onImageDescriptionsMenuItemSelected(getActivity(), mModalDialogManager);
        });
    }

    private void clickPositiveButton() {
        onView(withId(org.chromium.chrome.R.id.positive_button)).perform(click());
    }

    private void clickNegativeButton() {
        onView(withId(org.chromium.chrome.R.id.negative_button)).perform(click());
    }

    // Helper methods for assertions

    private void assertVisibleEnabledAndChecked_RadioButton(View view, String prefix) {
        Assert.assertEquals(View.VISIBLE, view.getVisibility());
        Assert.assertTrue(prefix + " should be enabled", view.isEnabled());
        Assert.assertTrue(
                prefix + " should be checked", ((RadioButtonWithDescription) view).isChecked());
    }

    private void assertVisibleEnabledAndUnchecked_RadioButton(View view, String prefix) {
        Assert.assertEquals(View.VISIBLE, view.getVisibility());
        Assert.assertTrue(prefix + " should be enabled", view.isEnabled());
        Assert.assertFalse(
                prefix + " should be unchecked", ((RadioButtonWithDescription) view).isChecked());
    }

    private void assertVisibleEnabledAndChecked_CheckBox(View view, String prefix) {
        Assert.assertEquals(View.VISIBLE, view.getVisibility());
        Assert.assertTrue(prefix + " should be enabled", view.isEnabled());
        Assert.assertTrue(prefix + " should be checked", ((CheckBox) view).isChecked());
    }

    private void assertVisibleEnabledAndUnchecked_CheckBox(View view, String prefix) {
        Assert.assertEquals(View.VISIBLE, view.getVisibility());
        Assert.assertTrue(prefix + " should be enabled", view.isEnabled());
        Assert.assertFalse(prefix + " should be unchecked", ((CheckBox) view).isChecked());
    }

    @Test
    @SmallTest
    public void testHeaderAndButtonContent() {
        showDialog();
        onView(thatMatchesFirst(withId(org.chromium.chrome.R.id.title)))
                .check(matches(withText("Get image descriptions?")));
        onView(withId(R.id.image_descriptions_dialog_content))
                .check(matches(
                        withText("Images are sent to Google to improve descriptions for you.")));
        onView(withId(org.chromium.chrome.R.id.positive_button))
                .check(matches(withText("Get descriptions")));
        onView(withId(org.chromium.chrome.R.id.negative_button))
                .check(matches(withText("No thanks")));
    }

    @Test
    @SmallTest
    public void testRadioButtonState_initialDefaultDialog() {
        showDialog();

        // "Just once" should be visible, enabled, and checked
        onView(withId(R.id.image_descriptions_dialog_radio_button_just_once)).check((view, e) -> {
            Assert.assertEquals("Just once", ((RadioButtonWithDescription) view).getPrimaryText());
            assertVisibleEnabledAndChecked_RadioButton(view, "Just once");
        });

        // "Always" should be visible, enabled, and unchecked
        onView(withId(R.id.image_descriptions_dialog_radio_button_always)).check((view, e) -> {
            Assert.assertEquals("Always", ((RadioButtonWithDescription) view).getPrimaryText());
            assertVisibleEnabledAndUnchecked_RadioButton(view, "Always");
        });
    }

    @Test
    @SmallTest
    public void testCheckBoxState_userSelectsAlwaysOption() {
        showDialog();

        // "Always" should be visible, enabled, and unchecked
        onView(withId(R.id.image_descriptions_dialog_radio_button_always)).check((view, e) -> {
            Assert.assertEquals("Always", ((RadioButtonWithDescription) view).getPrimaryText());
            assertVisibleEnabledAndUnchecked_RadioButton(view, "Always");
        });

        // "Only on Wi-Fi" option should be gone
        onView(withId(R.id.image_descriptions_dialog_check_box)).check((view, e) -> {
            Assert.assertEquals(View.GONE, view.getVisibility());
        });

        // Click the "Always" option, then "Only on Wi-Fi" option should appear and be checked
        onView(withId(R.id.image_descriptions_dialog_radio_button_always)).perform(click());
        onView(withId(R.id.image_descriptions_dialog_check_box)).check((view, e) -> {
            Assert.assertEquals("Only on Wi-Fi", ((CheckBox) view).getText());
            assertVisibleEnabledAndChecked_CheckBox(view, "Only on Wi-Fi");
        });
    }

    @Test
    @SmallTest
    public void testCheckBoxState_userTogglesOnlyOnWifi() {
        showDialog();

        // Click the "Always" option, then "Only on Wi-Fi" option should appear and be checked
        onView(withId(R.id.image_descriptions_dialog_radio_button_always)).perform(click());
        onView(withId(R.id.image_descriptions_dialog_check_box)).check((view, e) -> {
            Assert.assertEquals("Only on Wi-Fi", ((CheckBox) view).getText());
            assertVisibleEnabledAndChecked_CheckBox(view, "Only on Wi-Fi");
        });

        // Uncheck the "Only on Wi-Fi" option, switch radio buttons, then switch back
        onView(withId(R.id.image_descriptions_dialog_check_box)).perform(click());
        onView(withId(R.id.image_descriptions_dialog_radio_button_just_once)).perform(click());
        onView(withId(R.id.image_descriptions_dialog_radio_button_always)).perform(click());

        // The "Only on Wi-Fi" option should reappear, and still be unchecked
        onView(withId(R.id.image_descriptions_dialog_check_box)).check((view, e) -> {
            Assert.assertEquals("Only on Wi-Fi", ((CheckBox) view).getText());
            assertVisibleEnabledAndUnchecked_CheckBox(view, "Only on Wi-Fi");
        });
    }

    @Test
    @SmallTest
    public void testCheckBoxState_dontAskAgainOptionVisible() {
        showDialogWithDontAskAgainVisible();

        // "Just once" should be visible, enabled, and checked
        onView(withId(R.id.image_descriptions_dialog_radio_button_just_once)).check((view, e) -> {
            Assert.assertEquals("Just once", ((RadioButtonWithDescription) view).getPrimaryText());
            assertVisibleEnabledAndChecked_RadioButton(view, "Just once");
        });

        // The "Dont ask again" option should be visible, enabled, and unchecked
        onView(withId(R.id.image_descriptions_dialog_check_box)).check((view, e) -> {
            Assert.assertEquals("Dont ask again",
                    ((CheckBox) view).getText().toString().replaceAll("[^a-zA-Z\\s]", ""));
            assertVisibleEnabledAndUnchecked_CheckBox(view, "Dont ask again");
        });
    }

    @Test
    @SmallTest
    public void testCheckBoxState_userTogglesDontAskAgain() {
        showDialogWithDontAskAgainVisible();

        // The "Dont ask again" option should be visible, enabled, and unchecked
        onView(withId(R.id.image_descriptions_dialog_check_box)).check((view, e) -> {
            Assert.assertEquals("Dont ask again",
                    ((CheckBox) view).getText().toString().replaceAll("[^a-zA-Z\\s]", ""));
            assertVisibleEnabledAndUnchecked_CheckBox(view, "Dont ask again");
        });

        // Check the "Dont ask again" option, switch radio buttons, then switch back
        onView(withId(R.id.image_descriptions_dialog_check_box)).perform(click());
        onView(withId(R.id.image_descriptions_dialog_radio_button_always)).perform(click());
        onView(withId(R.id.image_descriptions_dialog_radio_button_just_once)).perform(click());

        // The "Dont ask again" option should reappear, and still be checked
        onView(withId(R.id.image_descriptions_dialog_check_box)).check((view, e) -> {
            Assert.assertEquals("Dont ask again",
                    ((CheckBox) view).getText().toString().replaceAll("[^a-zA-Z\\s]", ""));
            assertVisibleEnabledAndChecked_CheckBox(view, "Dont ask again");
        });
    }

    @Test
    @SmallTest
    public void testUserInteraction_userClicksNoThanks() {
        showDialog();

        // User clicks "No thanks", dialog should dismiss with no action taken.
        clickNegativeButton();

        verify(mDelegate, never()).enableImageDescriptions(anyBoolean());
        verify(mDelegate, never()).getImageDescriptionsJustOnce(anyBoolean());
    }

    @Test
    @SmallTest
    public void testUserInteraction_userGetsDescriptions_justOnce() {
        showDialog();

        // "Just once" should be visible, enabled, and checked
        onView(withId(R.id.image_descriptions_dialog_radio_button_just_once)).check((view, e) -> {
            Assert.assertEquals("Just once", ((RadioButtonWithDescription) view).getPrimaryText());
            assertVisibleEnabledAndChecked_RadioButton(view, "Just once");
        });

        // User clicks "Get descriptions"
        clickPositiveButton();

        verify(mDelegate, never()).enableImageDescriptions(anyBoolean());
        verify(mDelegate, times(1)).getImageDescriptionsJustOnce(false);
    }

    @Test
    @SmallTest
    public void testUserInteraction_userGetsDescriptions_justOnceDontAskAgain() {
        showDialogWithDontAskAgainVisible();

        // The "Dont ask again" option should be visible, enabled, and unchecked
        onView(withId(R.id.image_descriptions_dialog_check_box)).check((view, e) -> {
            Assert.assertEquals("Dont ask again",
                    ((CheckBox) view).getText().toString().replaceAll("[^a-zA-Z\\s]", ""));
            assertVisibleEnabledAndUnchecked_CheckBox(view, "Dont ask again");
        });

        // Check the "Dont ask again" option
        onView(withId(R.id.image_descriptions_dialog_check_box)).perform(click());

        // User clicks "Get descriptions"
        clickPositiveButton();

        verify(mDelegate, never()).enableImageDescriptions(anyBoolean());
        verify(mDelegate, times(1)).getImageDescriptionsJustOnce(true);
    }

    @Test
    @SmallTest
    public void testUserInteraction_userGetsDescriptions_always() {
        showDialog();

        // User clicks on the "Always" option, then turns off the "Only on Wi-Fi" option
        onView(withId(R.id.image_descriptions_dialog_radio_button_always)).perform(click());
        onView(withId(R.id.image_descriptions_dialog_check_box)).perform(click());

        // Confirm state
        onView(withId(R.id.image_descriptions_dialog_radio_button_always)).check((view, e) -> {
            Assert.assertEquals("Always", ((RadioButtonWithDescription) view).getPrimaryText());
            assertVisibleEnabledAndChecked_RadioButton(view, "Always");
        });
        onView(withId(R.id.image_descriptions_dialog_check_box)).check((view, e) -> {
            Assert.assertEquals("Only on Wi-Fi", ((CheckBox) view).getText());
            assertVisibleEnabledAndUnchecked_CheckBox(view, "Only on Wi-Fi");
        });

        // User clicks "Get descriptions"
        clickPositiveButton();

        verify(mDelegate, times(1)).enableImageDescriptions(false);
        verify(mDelegate, never()).getImageDescriptionsJustOnce(anyBoolean());
    }

    @Test
    @SmallTest
    public void testUserInteraction_userGetsDescriptions_alwaysOnlyOnWifi() {
        showDialog();

        // User clicks on the "Always" option, keeps the "Only on Wi-Fi" option checked
        onView(withId(R.id.image_descriptions_dialog_radio_button_always)).perform(click());

        // User clicks "Get descriptions"
        clickPositiveButton();

        verify(mDelegate, times(1)).enableImageDescriptions(true);
        verify(mDelegate, never()).getImageDescriptionsJustOnce(anyBoolean());
    }
}
