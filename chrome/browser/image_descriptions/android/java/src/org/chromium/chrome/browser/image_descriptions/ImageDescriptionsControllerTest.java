// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_descriptions;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/**
 *  Unit tests for {@link ImageDescriptionsController}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ImageDescriptionsControllerTest extends DummyUiActivityTestCase {
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

    @Mock
    private ModalDialogManager mModalDialogManager;

    private SharedPreferencesManager mManager;
    private ImageDescriptionsController mController;

    @Before
    public void setUp() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        Profile.setLastUsedProfileForTesting(mProfile);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);

        resetSharedPreferences();

        mController = ImageDescriptionsController.getInstance();
    }

    private void resetSharedPreferences() {
        mManager = SharedPreferencesManager.getInstance();
        mManager.removeKey(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT);
        mManager.removeKey(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN);
    }

    @Test
    @SmallTest
    public void testSharedPrefs_justOnceCounter() {
        mController.getImageDescriptionsJustOnce(false);
        Assert.assertEquals(
                1, mManager.readInt(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT));
        Assert.assertFalse("Don't ask again should only be true if our just once count is >= 3",
                mController.shouldShowDontAskAgainOption());

        mController.getImageDescriptionsJustOnce(false);
        Assert.assertEquals(
                2, mManager.readInt(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT));
        Assert.assertFalse("Don't ask again should only be true if our just once count is >= 3",
                mController.shouldShowDontAskAgainOption());

        mController.getImageDescriptionsJustOnce(false);
        Assert.assertEquals(
                3, mManager.readInt(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT));
        Assert.assertTrue("Don't ask again should be true since our just once count is >= 3",
                mController.shouldShowDontAskAgainOption());
    }

    @Test
    @SmallTest
    public void testSharedPrefs_dontAskAgain() {
        Assert.assertFalse("By default, dont ask again should be false",
                mManager.readBoolean(
                        ChromePreferenceKeys.IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN, false));

        mController.getImageDescriptionsJustOnce(true);

        Assert.assertTrue("After user sets dont ask again, value should stay true",
                mManager.readBoolean(
                        ChromePreferenceKeys.IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN, false));
    }

    @Test
    @SmallTest
    public void testUserPrefs_userEnablesFeature() {
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(false);
        Assert.assertFalse("Image descriptions should be disabled by default",
                mController.imageDescriptionsEnabled());

        mController.enableImageDescriptions(false);
        verify(mPrefService, times(1))
                .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID, true);
        verify(mPrefService, times(1))
                .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI, false);

        mController.enableImageDescriptions(true);
        verify(mPrefService, times(1))
                .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI, true);
    }

    @Test
    @SmallTest
    public void testUserPrefs_userDisablesFeature() {
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(true);
        Assert.assertTrue(
                "Image descriptions should be enabled", mController.imageDescriptionsEnabled());

        mController.disableImageDescriptions();
        verify(mPrefService, times(1))
                .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID, false);
    }

    @Test
    @SmallTest
    public void testUserPrefs_userGetsDescriptionsJustOnce() {
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(false);
        Assert.assertFalse("Image descriptions should be disabled by default",
                mController.imageDescriptionsEnabled());

        mController.getImageDescriptionsJustOnce(false);
        verify(mPrefService, never()).setBoolean(anyString(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testMenuItemSelected_featureEnabled() {
        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(true);
        Assert.assertTrue(
                "Image descriptions should be enabled", mController.imageDescriptionsEnabled());

        mController.onImageDescriptionsMenuItemSelected(getActivity(), mModalDialogManager);
        verify(mPrefService, times(1))
                .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID, false);
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
    }

    @Test
    @SmallTest
    public void testMenuItemSelected_dontAskAgainEnabled() {
        mController.setDelegateForTesting(mDelegate);

        when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                .thenReturn(false);
        mManager.writeBoolean(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN, true);

        mController.onImageDescriptionsMenuItemSelected(getActivity(), mModalDialogManager);
        verify(mDelegate, times(1)).getImageDescriptionsJustOnce(anyBoolean());
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
    }

    @Test
    @SmallTest
    public void testMenuItemSelected_featureDisabled() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            when(mPrefService.getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID))
                    .thenReturn(false);
            mManager.writeBoolean(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN, false);

            mController.onImageDescriptionsMenuItemSelected(getActivity(), mModalDialogManager);
            verify(mModalDialogManager, times(1)).showDialog(any(), anyInt());
        });
    }
}
