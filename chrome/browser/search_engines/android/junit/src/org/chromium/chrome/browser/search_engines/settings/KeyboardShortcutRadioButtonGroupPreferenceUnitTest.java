// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import androidx.preference.PreferenceViewHolder;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Unit tests for {@link KeyboardShortcutRadioButtonGroupPreference}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class KeyboardShortcutRadioButtonGroupPreferenceUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Mock private UserPrefs.Natives mUserPrefsJni;

    private ActivityController<BlankUiTestActivity> mActivityController;
    private Activity mActivity;
    private KeyboardShortcutRadioButtonGroupPreference mPreference;
    private PreferenceViewHolder mHolder;

    private static final String KEYWORD_SPACE_TRIGGERING_ENABLED =
            "omnibox.keyword_space_triggering_enabled";

    @Before
    public void setUp() {
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);
        doReturn(mPrefService).when(mUserPrefsJni).get(any(Profile.class));

        mActivityController = Robolectric.buildActivity(BlankUiTestActivity.class).setup();
        mActivity = mActivityController.get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mPreference = new KeyboardShortcutRadioButtonGroupPreference(mActivity, null);
        mPreference.setProfile(mProfile);

        LayoutInflater inflater = LayoutInflater.from(mActivity);
        View view = inflater.inflate(mPreference.getLayoutResource(), null);

        mHolder = PreferenceViewHolder.createInstanceForTests(view);
    }

    @After
    public void tearDown() {
        mActivityController.destroy();
    }

    @Test
    public void testInitialization_Enabled() {
        when(mPrefService.getBoolean(KEYWORD_SPACE_TRIGGERING_ENABLED)).thenReturn(true);

        mPreference.onBindViewHolder(mHolder);

        RadioButtonWithDescription spaceOrTabButton =
                (RadioButtonWithDescription)
                        mHolder.findViewById(R.id.keyboard_shortcut_space_or_tab);
        RadioButtonWithDescription tabButton =
                (RadioButtonWithDescription) mHolder.findViewById(R.id.keyboard_shortcut_tab);

        Assert.assertTrue(spaceOrTabButton.isChecked());
        Assert.assertFalse(tabButton.isChecked());
    }

    @Test
    public void testInitialization_Disabled() {
        when(mPrefService.getBoolean(KEYWORD_SPACE_TRIGGERING_ENABLED)).thenReturn(false);

        mPreference.onBindViewHolder(mHolder);

        RadioButtonWithDescription spaceOrTabButton =
                (RadioButtonWithDescription)
                        mHolder.findViewById(R.id.keyboard_shortcut_space_or_tab);
        RadioButtonWithDescription tabButton =
                (RadioButtonWithDescription) mHolder.findViewById(R.id.keyboard_shortcut_tab);

        Assert.assertFalse(spaceOrTabButton.isChecked());
        Assert.assertTrue(tabButton.isChecked());
    }

    @Test
    public void testSelectionChange() {
        when(mPrefService.getBoolean(KEYWORD_SPACE_TRIGGERING_ENABLED)).thenReturn(true);
        mPreference.onBindViewHolder(mHolder);

        RadioButtonWithDescription spaceOrTabButton =
                (RadioButtonWithDescription)
                        mHolder.findViewById(R.id.keyboard_shortcut_space_or_tab);
        Assert.assertTrue(spaceOrTabButton.isChecked());

        RadioButtonWithDescription tabButton =
                (RadioButtonWithDescription) mHolder.findViewById(R.id.keyboard_shortcut_tab);
        tabButton.performClick();

        verify(mPrefService).setBoolean(KEYWORD_SPACE_TRIGGERING_ENABLED, false);
    }
}
