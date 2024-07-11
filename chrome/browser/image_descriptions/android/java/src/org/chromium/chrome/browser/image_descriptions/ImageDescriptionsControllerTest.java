// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_descriptions;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.ConnectionType;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/** Unit tests for {@link ImageDescriptionsController} */
@RunWith(BaseJUnit4ClassRunner.class)
public class ImageDescriptionsControllerTest extends BlankUiTestActivityTestCase {
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private ImageDescriptionsController.Natives mControllerJniMock;

    @Mock private UserPrefs.Natives mUserPrefsJniMock;

    @Mock private Profile mProfile;
    @Mock private Profile.Natives mProfileJniMock;

    @Mock private PrefService mPrefService;

    @Mock private ModalDialogManager mModalDialogManager;

    @Mock private WebContents mWebContents;

    private SharedPreferencesManager mManager;
    private ImageDescriptionsController mController;
    private ImageDescriptionsControllerDelegate mDelegate;

    @Before
    public void setUp() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(ProfileJni.TEST_HOOKS, mProfileJniMock);
        when(mProfileJniMock.fromWebContents(mWebContents)).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);

        mJniMocker.mock(ImageDescriptionsControllerJni.TEST_HOOKS, mControllerJniMock);

        resetSharedPreferences();

        mController = ImageDescriptionsController.getInstance();
        mDelegate = ImageDescriptionsController.getInstance().getDelegate();
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> DeviceConditions.sForceConnectionTypeForTesting = false);
    }

    private void resetSharedPreferences() {
        mManager = ChromeSharedPreferences.getInstance();
        mManager.removeKey(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT);
        mManager.removeKey(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN);
    }

    private void simulateMenuItemClick() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.onImageDescriptionsMenuItemSelected(
                            getActivity(), mModalDialogManager, mWebContents);
                });
    }

    @Test
    @SmallTest
    public void testSharedPrefs_justOnceCounter() {
        mDelegate.getImageDescriptionsJustOnce(false, mWebContents);
        Assert.assertEquals(
                1, mManager.readInt(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT));
        Assert.assertFalse(
                "Don't ask again should only be true if our just once count is >= 3",
                mController.shouldShowDontAskAgainOption());

        mDelegate.getImageDescriptionsJustOnce(false, mWebContents);
        Assert.assertEquals(
                2, mManager.readInt(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT));
        Assert.assertFalse(
                "Don't ask again should only be true if our just once count is >= 3",
                mController.shouldShowDontAskAgainOption());

        mDelegate.getImageDescriptionsJustOnce(false, mWebContents);
        Assert.assertEquals(
                3, mManager.readInt(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT));
        Assert.assertTrue(
                "Don't ask again should be true since our just once count is >= 3",
                mController.shouldShowDontAskAgainOption());
    }

    @Test
    @SmallTest
    public void testSharedPrefs_dontAskAgain() {
        Assert.assertFalse(
                "By default, dont ask again should be false",
                mManager.readBoolean(
                        ChromePreferenceKeys.IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN, false));

        mDelegate.getImageDescriptionsJustOnce(true, mWebContents);

        Assert.assertTrue(
                "After user sets dont ask again, value should stay true",
                mManager.readBoolean(
                        ChromePreferenceKeys.IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN, false));
    }

    @Test
    @SmallTest
    public void testUserPrefs_userEnablesFeature() {
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(false);
        Assert.assertFalse(
                "Image descriptions should be disabled by default",
                mController.imageDescriptionsEnabled(mProfile));

        mDelegate.enableImageDescriptions(mProfile);
        mDelegate.setOnlyOnWifiRequirement(false, mProfile);
        verify(mPrefService, times(1))
                .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID, true);
        verify(mPrefService, times(1))
                .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI, false);

        mDelegate.setOnlyOnWifiRequirement(true, mProfile);
        verify(mPrefService, times(1))
                .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI, true);
    }

    @Test
    @SmallTest
    public void testUserPrefs_userDisablesFeature() {
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(true);
        Assert.assertTrue(
                "Image descriptions should be enabled",
                mController.imageDescriptionsEnabled(mProfile));

        mDelegate.disableImageDescriptions(mProfile);
        verify(mPrefService, times(1))
                .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID, false);
    }

    @Test
    @SmallTest
    public void testUserPrefs_userGetsDescriptionsJustOnce() {
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(false);
        Assert.assertFalse(
                "Image descriptions should be disabled by default",
                mController.imageDescriptionsEnabled(mProfile));

        mDelegate.getImageDescriptionsJustOnce(false, mWebContents);
        verify(mPrefService, never()).setBoolean(anyString(), anyBoolean());
        verify(mControllerJniMock, times(1)).getImageDescriptionsOnce(mWebContents);
    }

    @Test
    @SmallTest
    public void testMenuItemSelected_featureEnabled() {
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(true);
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI))
                .thenReturn(false);
        Assert.assertTrue(
                "Image descriptions should be enabled",
                mController.imageDescriptionsEnabled(mProfile));

        simulateMenuItemClick();

        verify(mPrefService, times(1))
                .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID, false);
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());

        onView(withText(R.string.image_descriptions_toast_off))
                .inRoot(withDecorView(not(is(getActivity().getWindow().getDecorView()))))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testMenuItemSelected_featureEnabled_onlyOnWifi_noWifi() throws Exception {
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(true);
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI))
                .thenReturn(true);
        Assert.assertTrue(
                "Image descriptions should be enabled",
                mController.imageDescriptionsEnabled(mProfile));
        Assert.assertTrue(
                "Image descriptions only on wifi option should be enabled",
                mController.onlyOnWifiEnabled(mProfile));

        // Setup no wifi condition.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DeviceConditions.sForceConnectionTypeForTesting = true;
                    DeviceConditions.mConnectionTypeForTesting = ConnectionType.CONNECTION_NONE;
                });

        simulateMenuItemClick();

        verify(mPrefService, never()).setBoolean(any(), anyBoolean());
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
        verify(mControllerJniMock, times(1)).getImageDescriptionsOnce(eq(mWebContents));

        onView(withText(R.string.image_descriptions_toast_just_once))
                .inRoot(withDecorView(not(is(getActivity().getWindow().getDecorView()))))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testMenuItemSelected_dontAskAgainEnabled() {
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(false);
        mManager.writeBoolean(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN, true);
        Assert.assertFalse(
                "Image descriptions should be disabled",
                mController.imageDescriptionsEnabled(mProfile));

        simulateMenuItemClick();

        verify(mPrefService, never()).setBoolean(any(), anyBoolean());
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
        verify(mControllerJniMock, times(1)).getImageDescriptionsOnce(eq(mWebContents));

        onView(withText(R.string.image_descriptions_toast_just_once))
                .inRoot(withDecorView(not(is(getActivity().getWindow().getDecorView()))))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testMenuItemSelected_featureDisabled() {
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(false);
        mManager.writeBoolean(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN, false);

        simulateMenuItemClick();

        verify(mModalDialogManager, times(1)).showDialog(any(), anyInt());
    }
}
