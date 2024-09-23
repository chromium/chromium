// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_descriptions;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.widget.CheckBox;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.ConnectionType;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/** Unit tests for {@link ImageDescriptionsDialog} */
@RunWith(BaseJUnit4ClassRunner.class)
public class ImageDescriptionsDialogTest extends BlankUiTestActivityTestCase {
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private ImageDescriptionsControllerDelegate mDelegate;

    @Mock private UserPrefs.Natives mUserPrefsJniMock;

    @Mock private Profile mProfile;
    @Mock private Profile.Natives mProfileJniMock;

    @Mock private PrefService mPrefService;

    @Mock private WebContents mWebContents;

    private SharedPreferencesManager mManager;
    private ImageDescriptionsController mController;

    private ModalDialogManager mModalDialogManager;
    private ModalDialogManager.Presenter mAppModalPresenter;

    @Before
    public void setUp() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(ProfileJni.TEST_HOOKS, mProfileJniMock);
        when(mProfileJniMock.fromWebContents(mWebContents)).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAppModalPresenter = new AppModalPresenter(getActivity());
                    mModalDialogManager =
                            new ModalDialogManager(
                                    mAppModalPresenter, ModalDialogManager.ModalDialogType.APP);
                });

        mManager = ChromeSharedPreferences.getInstance();
        mController = ImageDescriptionsController.getInstance();
        mController.setDelegateForTesting(mDelegate);
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> DeviceConditions.sForceConnectionTypeForTesting = false);
    }

    // Helper methods for driving dialog control

    private void showDialog() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                            .thenReturn(false);
                    mManager.writeInt(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT, 0);
                    mManager.writeBoolean(
                            ChromePreferenceKeys.IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN, false);
                    mController.onImageDescriptionsMenuItemSelected(
                            getActivity(), mModalDialogManager, mWebContents);
                });
    }

    private void showDialogWithDontAskAgainVisible() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                            .thenReturn(false);
                    mManager.writeInt(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT, 5);
                    mController.onImageDescriptionsMenuItemSelected(
                            getActivity(), mModalDialogManager, mWebContents);
                });
    }

    private void clickPositiveButton() {
        onView(withId(R.id.positive_button)).perform(click());
    }

    private void clickNegativeButton() {
        onView(withId(R.id.negative_button)).inRoot(isDialog()).perform(click());
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
        onView(allOf(isDescendantOfA(withId(R.id.title_container)), withId(R.id.title)))
                .inRoot(isDialog())
                .check(matches(withText("Get image descriptions?")));
        onView(withId(R.id.image_descriptions_dialog_content))
                .check(
                        matches(
                                withText(
                                        "Images are sent to Google to improve descriptions for"
                                                + " you.")));
        onView(withId(R.id.positive_button)).check(matches(withText("Get descriptions")));
        onView(withId(R.id.negative_button)).check(matches(withText("No thanks")));
    }

    @Test
    @SmallTest
    public void testRadioButtonState_initialDefaultDialog() {
        showDialog();

        // "Just once" should be visible, enabled, and checked
        onView(withId(R.id.image_descriptions_dialog_radio_button_just_once))
                .inRoot(isDialog())
                .check(
                        (view, e) -> {
                            Assert.assertEquals(
                                    "Just once",
                                    ((RadioButtonWithDescription) view).getPrimaryText());
                            assertVisibleEnabledAndChecked_RadioButton(view, "Just once");
                        });

        // "Always" should be visible, enabled, and unchecked
        onView(withId(R.id.image_descriptions_dialog_radio_button_always))
                .check(
                        (view, e) -> {
                            Assert.assertEquals(
                                    "Always", ((RadioButtonWithDescription) view).getPrimaryText());
                            assertVisibleEnabledAndUnchecked_RadioButton(view, "Always");
                        });
    }

    @Test
    @SmallTest
    public void testCheckBoxState_userSelectsAlwaysOption() {
        showDialog();

        // "Always" should be visible, enabled, and unchecked
        onView(withId(R.id.image_descriptions_dialog_radio_button_always))
                .inRoot(isDialog())
                .check(
                        (view, e) -> {
                            Assert.assertEquals(
                                    "Always", ((RadioButtonWithDescription) view).getPrimaryText());
                            assertVisibleEnabledAndUnchecked_RadioButton(view, "Always");
                        });

        // "Only on Wi-Fi" option should be gone
        onView(withId(R.id.image_descriptions_dialog_check_box))
                .check(
                        (view, e) -> {
                            Assert.assertEquals(View.GONE, view.getVisibility());
                        });

        // Click the "Always" option, then "Only on Wi-Fi" option should appear and be checked
        onView(withId(R.id.image_descriptions_dialog_radio_button_always)).perform(click());
        onView(withId(R.id.image_descriptions_dialog_check_box))
                .check(
                        (view, e) -> {
                            Assert.assertEquals("Only on Wi-Fi", ((CheckBox) view).getText());
                            assertVisibleEnabledAndChecked_CheckBox(view, "Only on Wi-Fi");
                        });
    }

    @Test
    @SmallTest
    public void testCheckBoxState_userTogglesOnlyOnWifi() {
        showDialog();

        // Click the "Always" option, then "Only on Wi-Fi" option should appear and be checked
        onView(withId(R.id.image_descriptions_dialog_radio_button_always))
                .inRoot(isDialog())
                .perform(click());
        onView(withId(R.id.image_descriptions_dialog_check_box))
                .check(
                        (view, e) -> {
                            Assert.assertEquals("Only on Wi-Fi", ((CheckBox) view).getText());
                            assertVisibleEnabledAndChecked_CheckBox(view, "Only on Wi-Fi");
                        });

        // Uncheck the "Only on Wi-Fi" option, switch radio buttons, then switch back
        onView(withId(R.id.image_descriptions_dialog_check_box)).perform(click());
        onView(withId(R.id.image_descriptions_dialog_radio_button_just_once)).perform(click());
        onView(withId(R.id.image_descriptions_dialog_radio_button_always)).perform(click());

        // The "Only on Wi-Fi" option should reappear, and still be unchecked
        onView(withId(R.id.image_descriptions_dialog_check_box))
                .check(
                        (view, e) -> {
                            Assert.assertEquals("Only on Wi-Fi", ((CheckBox) view).getText());
                            assertVisibleEnabledAndUnchecked_CheckBox(view, "Only on Wi-Fi");
                        });
    }

    @Test
    @SmallTest
    public void testCheckBoxState_dontAskAgainOptionVisible() {
        showDialogWithDontAskAgainVisible();

        // "Just once" should be visible, enabled, and checked
        onView(withId(R.id.image_descriptions_dialog_radio_button_just_once))
                .inRoot(isDialog())
                .check(
                        (view, e) -> {
                            Assert.assertEquals(
                                    "Just once",
                                    ((RadioButtonWithDescription) view).getPrimaryText());
                            assertVisibleEnabledAndChecked_RadioButton(view, "Just once");
                        });

        // The "Dont ask again" option should be visible, enabled, and unchecked
        onView(withId(R.id.image_descriptions_dialog_check_box))
                .check(
                        (view, e) -> {
                            Assert.assertEquals(
                                    "Dont ask again",
                                    ((CheckBox) view)
                                            .getText()
                                            .toString()
                                            .replaceAll("[^a-zA-Z\\s]", ""));
                            assertVisibleEnabledAndUnchecked_CheckBox(view, "Dont ask again");
                        });
    }

    @Test
    @SmallTest
    public void testCheckBoxState_userTogglesDontAskAgain() {
        showDialogWithDontAskAgainVisible();

        // The "Dont ask again" option should be visible, enabled, and unchecked
        onView(withId(R.id.image_descriptions_dialog_check_box))
                .inRoot(isDialog())
                .check(
                        (view, e) -> {
                            Assert.assertEquals(
                                    "Dont ask again",
                                    ((CheckBox) view)
                                            .getText()
                                            .toString()
                                            .replaceAll("[^a-zA-Z\\s]", ""));
                            assertVisibleEnabledAndUnchecked_CheckBox(view, "Dont ask again");
                        });

        // Check the "Dont ask again" option, switch radio buttons, then switch back
        onView(withId(R.id.image_descriptions_dialog_check_box)).perform(click());
        onView(withId(R.id.image_descriptions_dialog_radio_button_always)).perform(click());
        onView(withId(R.id.image_descriptions_dialog_radio_button_just_once)).perform(click());

        // The "Dont ask again" option should reappear, and still be checked
        onView(withId(R.id.image_descriptions_dialog_check_box))
                .check(
                        (view, e) -> {
                            Assert.assertEquals(
                                    "Dont ask again",
                                    ((CheckBox) view)
                                            .getText()
                                            .toString()
                                            .replaceAll("[^a-zA-Z\\s]", ""));
                            assertVisibleEnabledAndChecked_CheckBox(view, "Dont ask again");
                        });
    }

    @Test
    @SmallTest
    public void testUserInteraction_userClicksNoThanks() {
        showDialog();

        // User clicks "No thanks", dialog should dismiss with no action taken.
        clickNegativeButton();

        verify(mDelegate, never()).enableImageDescriptions(any());
        verify(mDelegate, never()).disableImageDescriptions(any());
        verify(mDelegate, never()).setOnlyOnWifiRequirement(anyBoolean(), any());
        verify(mDelegate, never()).getImageDescriptionsJustOnce(anyBoolean(), any());
    }

    @Test
    @SmallTest
    public void testUserInteraction_userGetsDescriptions_justOnce() {
        showDialog();

        // "Just once" should be visible, enabled, and checked
        onView(withId(R.id.image_descriptions_dialog_radio_button_just_once))
                .inRoot(isDialog())
                .check(
                        (view, e) -> {
                            Assert.assertEquals(
                                    "Just once",
                                    ((RadioButtonWithDescription) view).getPrimaryText());
                            assertVisibleEnabledAndChecked_RadioButton(view, "Just once");
                        });

        // User clicks "Get descriptions"
        clickPositiveButton();

        verify(mDelegate, never()).enableImageDescriptions(any());
        verify(mDelegate, never()).disableImageDescriptions(any());
        verify(mDelegate, never()).setOnlyOnWifiRequirement(anyBoolean(), any());
        verify(mDelegate, times(1)).getImageDescriptionsJustOnce(false, mWebContents);

        onView(withText(R.string.image_descriptions_toast_just_once))
                .inRoot(withDecorView(not(is(getActivity().getWindow().getDecorView()))))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testUserInteraction_userGetsDescriptions_justOnceDontAskAgain() {
        showDialogWithDontAskAgainVisible();

        // The "Dont ask again" option should be visible, enabled, and unchecked
        onView(withId(R.id.image_descriptions_dialog_check_box))
                .inRoot(isDialog())
                .check(
                        (view, e) -> {
                            Assert.assertEquals(
                                    "Dont ask again",
                                    ((CheckBox) view)
                                            .getText()
                                            .toString()
                                            .replaceAll("[^a-zA-Z\\s]", ""));
                            assertVisibleEnabledAndUnchecked_CheckBox(view, "Dont ask again");
                        });

        // Check the "Dont ask again" option
        onView(withId(R.id.image_descriptions_dialog_check_box)).perform(click());

        // User clicks "Get descriptions"
        clickPositiveButton();

        verify(mDelegate, never()).enableImageDescriptions(any());
        verify(mDelegate, never()).disableImageDescriptions(any());
        verify(mDelegate, never()).setOnlyOnWifiRequirement(anyBoolean(), any());
        verify(mDelegate, times(1)).getImageDescriptionsJustOnce(true, mWebContents);

        onView(withText(R.string.image_descriptions_toast_just_once))
                .inRoot(withDecorView(not(is(getActivity().getWindow().getDecorView()))))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testUserInteraction_userGetsDescriptions_always() {
        showDialog();

        // User clicks on the "Always" option, then turns off the "Only on Wi-Fi" option
        onView(withId(R.id.image_descriptions_dialog_radio_button_always))
                .inRoot(isDialog())
                .perform(click());
        onView(withId(R.id.image_descriptions_dialog_check_box)).perform(click());

        // Confirm state
        onView(withId(R.id.image_descriptions_dialog_radio_button_always))
                .check(
                        (view, e) -> {
                            Assert.assertEquals(
                                    "Always", ((RadioButtonWithDescription) view).getPrimaryText());
                            assertVisibleEnabledAndChecked_RadioButton(view, "Always");
                        });
        onView(withId(R.id.image_descriptions_dialog_check_box))
                .check(
                        (view, e) -> {
                            Assert.assertEquals("Only on Wi-Fi", ((CheckBox) view).getText());
                            assertVisibleEnabledAndUnchecked_CheckBox(view, "Only on Wi-Fi");
                        });

        // User clicks "Get descriptions"
        clickPositiveButton();

        verify(mDelegate, times(1)).enableImageDescriptions(mProfile);
        verify(mDelegate, never()).disableImageDescriptions(any());
        verify(mDelegate, times(1)).setOnlyOnWifiRequirement(false, mProfile);
        verify(mDelegate, never()).getImageDescriptionsJustOnce(anyBoolean(), any());

        onView(withText(R.string.image_descriptions_toast_on))
                .inRoot(withDecorView(not(is(getActivity().getWindow().getDecorView()))))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testUserInteraction_userGetsDescriptions_alwaysOnlyOnWifi() {
        showDialog();

        // User clicks on the "Always" option, keeps the "Only on Wi-Fi" option checked
        onView(withId(R.id.image_descriptions_dialog_radio_button_always))
                .inRoot(isDialog())
                .perform(click());

        // Setup wifi condition.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DeviceConditions.sForceConnectionTypeForTesting = true;
                    DeviceConditions.mConnectionTypeForTesting = ConnectionType.CONNECTION_WIFI;
                });

        // User clicks "Get descriptions"
        clickPositiveButton();

        verify(mDelegate, times(1)).enableImageDescriptions(mProfile);
        verify(mDelegate, never()).disableImageDescriptions(any());
        verify(mDelegate, times(1)).setOnlyOnWifiRequirement(true, mProfile);
        verify(mDelegate, never()).getImageDescriptionsJustOnce(anyBoolean(), any());

        onView(withText(R.string.image_descriptions_toast_on))
                .inRoot(withDecorView(not(is(getActivity().getWindow().getDecorView()))))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testUserInteraction_userGetsDescriptions_alwaysOnlyOnWifi_noWifi()
            throws Exception {
        showDialog();

        // User clicks on the "Always" option, keeps the "Only on Wi-Fi" option checked
        onView(withId(R.id.image_descriptions_dialog_radio_button_always))
                .inRoot(isDialog())
                .perform(click());

        // Setup no wifi condition.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DeviceConditions.sForceConnectionTypeForTesting = true;
                    DeviceConditions.mConnectionTypeForTesting = ConnectionType.CONNECTION_NONE;
                });

        // User clicks "Get descriptions"
        clickPositiveButton();

        verify(mDelegate, times(1)).enableImageDescriptions(mProfile);
        verify(mDelegate, never()).disableImageDescriptions(any());
        verify(mDelegate, times(1)).setOnlyOnWifiRequirement(true, mProfile);
        verify(mDelegate, never()).getImageDescriptionsJustOnce(anyBoolean(), any());

        onView(withText(R.string.image_descriptions_toast_on_no_wifi))
                .inRoot(withDecorView(not(is(getActivity().getWindow().getDecorView()))))
                .check(matches(isDisplayed()));
    }
}
