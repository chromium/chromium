// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.automotive.backbuttontoolbar;

import static androidx.appcompat.app.ActionBar.DISPLAY_HOME_AS_UP;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.view.MenuItem;
import android.view.View;

import androidx.activity.OnBackPressedCallback;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.AppCompatImageButton;
import androidx.appcompat.widget.Toolbar;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags.Add;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DeviceRestriction;

/**
 * Instrumentation tests for the persistent back button toolbar in automotive.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "The two tests in this suite each launch different Activity's.")
@Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BackButtonToolbarTest {
    @Rule
    public final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(MainSettings.class);

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new AutomotiveContextWrapperTestRule();

    private CallbackHelper mBackPressCallbackHelper;

    @Before
    public void setUp() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        mBackPressCallbackHelper = new CallbackHelper();
    }

    @Test
    @SmallTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    @Feature({"Automotive Toolbar"})
    public void testAutomotiveToolbar_ActionBar2() throws Exception {
        mChromeTabbedActivityTestRule.startMainActivityOnBlankPage();
        ChromeTabbedActivity chromeTabbedActivity = mChromeTabbedActivityTestRule.getActivity();

        // Check that the automotive toolbar is present with only a back button.
        assertTrue(chromeTabbedActivity.getSupportActionBar().isShowing());
        assertEquals("Automotive toolbar should only contain a back button",
                chromeTabbedActivity.getSupportActionBar().getDisplayOptions(), DISPLAY_HOME_AS_UP);

        // Simulate a back button press on the automotive toolbar.
        addBackPressedCallback(chromeTabbedActivity, mBackPressCallbackHelper);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            MenuItem backButton = mock(MenuItem.class);
            when(backButton.getItemId()).thenReturn(android.R.id.home);
            chromeTabbedActivity.onOptionsItemSelected(backButton);
        });

        // Verify that #onBackPressed was called.
        mBackPressCallbackHelper.waitForFirst();
    }

    @Test
    @SmallTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    @Feature({"Automotive Toolbar"})
    public void testAutomotiveToolbar_ToolbarView() throws Exception {
        // Launch Settings Activity, which uses a Toolbar View to implement the automotive toolbar.
        mSettingsActivityTestRule.startSettingsActivity();
        SettingsActivity settingsActivity = mSettingsActivityTestRule.getActivity();

        // Check that the automotive toolbar is present with only a back button.
        Toolbar toolbar = settingsActivity.findViewById(R.id.automotive_back_button_toolbar);
        assertNotNull(toolbar);
        assertEquals("Toolbar not visible", toolbar.getVisibility(), View.VISIBLE);
        assertEquals("Toolbar should only contain a back button", toolbar.getChildCount(), 1);
        assertThat(toolbar.getChildAt(0), instanceOf(AppCompatImageButton.class));

        // Click the back button in the automotive toolbar.
        addBackPressedCallback(settingsActivity, mBackPressCallbackHelper);
        TestThreadUtils.runOnUiThreadBlocking(() -> { toolbar.getChildAt(0).performClick(); });

        // Verify that #onBackPressed was called.
        mBackPressCallbackHelper.waitForFirst();
    }

    private void addBackPressedCallback(
            AppCompatActivity activity, CallbackHelper backPressCallback) {
        activity.getOnBackPressedDispatcher().addCallback(new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {
                backPressCallback.notifyCalled();
            }
        });
    }
}
