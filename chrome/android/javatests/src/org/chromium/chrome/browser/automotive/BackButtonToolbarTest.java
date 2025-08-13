// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.automotive;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.LayoutParams;

import androidx.activity.ComponentDialog;
import androidx.activity.OnBackPressedCallback;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.AppCompatImageButton;
import androidx.appcompat.widget.Toolbar;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags.Add;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.widget.ChromeDialog;
import org.chromium.components.browser_ui.widget.FullscreenAlertDialog;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DeviceRestriction;

/** Instrumentation tests for the persistent back button toolbar in automotive. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Each test case launches different Activities.")
@Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BackButtonToolbarTest {
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mBlankUiActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(MainSettings.class);

    @Rule
    public OverrideContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new OverrideContextWrapperTestRule();

    private static final int TEST_DIALOG_LAYOUT = R.layout.image_zoom_view;
    private CallbackHelper mBackPressCallbackHelper;

    @Before
    public void setUp() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        mBackPressCallbackHelper = new CallbackHelper();
    }

    @Test
    @SmallTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    @DisableFeatures(ChromeFeatureList.AUTOMOTIVE_BACK_BUTTON_BAR_STREAMLINE)
    @Feature({"Automotive Toolbar"})
    public void testAutomotiveToolbar_ToolbarView_Legacy() throws Exception {
        // Launch Settings Activity, which uses a Toolbar View to implement the automotive toolbar.
        mSettingsActivityTestRule.startSettingsActivity();
        SettingsActivity settingsActivity = mSettingsActivityTestRule.getActivity();

        // Check that the automotive toolbar is present with only a back button.
        Toolbar toolbar = settingsActivity.findViewById(R.id.back_button_toolbar);
        assertNotNull(toolbar);
        CriteriaHelper.pollUiThread(() -> toolbar.getVisibility() == View.VISIBLE);
        assertEquals("Toolbar should only contain a back button", 1, toolbar.getChildCount());
        assertThat(toolbar.getChildAt(0), instanceOf(AppCompatImageButton.class));

        // Click the back button in the automotive toolbar.
        addOnBackPressedCallback(settingsActivity, mBackPressCallbackHelper);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    toolbar.getChildAt(0).performClick();
                });

        // Verify that #onBackPressed was called.
        mBackPressCallbackHelper.waitForOnly();
    }

    @Test
    @SmallTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    @EnableFeatures(ChromeFeatureList.AUTOMOTIVE_BACK_BUTTON_BAR_STREAMLINE)
    @Feature({"Automotive Toolbar"})
    public void testAutomotiveToolbar_ToolbarNotShowing() throws Exception {
        DisplayUtil.setCarmaPhase1Version2ComplianceForTesting(true);

        // Launch Settings Activity, which uses a Toolbar View to implement the automotive toolbar.
        mSettingsActivityTestRule.startSettingsActivity();
        SettingsActivity settingsActivity = mSettingsActivityTestRule.getActivity();

        // Check that the automotive toolbar is present with only a back button.
        Toolbar toolbar = settingsActivity.findViewById(R.id.back_button_toolbar);
        assertNotNull(toolbar);
        CriteriaHelper.pollUiThread(() -> toolbar.getVisibility() == View.GONE);
        assertEquals("Toolbar should only contain a back button", 1, toolbar.getChildCount());
        assertThat(toolbar.getChildAt(0), instanceOf(AppCompatImageButton.class));
    }

    @Test
    @SmallTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    @Feature({"Automotive Toolbar"})
    public void testAutomotiveToolbar_FullscreenAlertDialog() throws Exception {
        mBlankUiActivityTestRule.launchActivity(null);

        // Display a FullscreenAlertDialog.
        FullscreenAlertDialog dialog =
                createAndShowFullscreenAlertDialog(mBlankUiActivityTestRule.getActivity());

        // Check that the automotive toolbar is present with only a back button.
        Toolbar toolbar = dialog.findViewById(R.id.back_button_toolbar);
        assertNotNull(toolbar);
        assertEquals("Toolbar not visible", View.VISIBLE, toolbar.getVisibility());
        assertEquals("Toolbar should only contain a back button", 1, toolbar.getChildCount());
        assertThat(toolbar.getChildAt(0), instanceOf(AppCompatImageButton.class));

        // Click the back button in the automotive toolbar.
        addOnBackPressedCallback(dialog, mBackPressCallbackHelper);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    toolbar.getChildAt(0).performClick();
                });

        // Verify that #onBackPressed was called.
        mBackPressCallbackHelper.waitForOnly();
    }

    @Test
    @SmallTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    @Feature({"Automotive Toolbar"})
    public void testAutomotiveToolbar_FullscreenAlertDialogBuilder() throws Exception {
        mBlankUiActivityTestRule.launchActivity(null);

        // Display a full screen AlertDialog created using FullscreenAlertDialog.Builder.
        AlertDialog dialog =
                createAndShowFullscreenAlertDialogFromBuilder(
                        mBlankUiActivityTestRule.getActivity());

        // Check that the automotive toolbar is present with only a back button.
        Toolbar toolbar = dialog.findViewById(R.id.back_button_toolbar);
        assertNotNull(toolbar);
        assertEquals("Toolbar not visible", View.VISIBLE, toolbar.getVisibility());
        assertEquals("Toolbar should only contain a back button", 1, toolbar.getChildCount());
        assertThat(toolbar.getChildAt(0), instanceOf(AppCompatImageButton.class));

        // Click the back button in the automotive toolbar.
        addOnBackPressedCallback(dialog, mBackPressCallbackHelper);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    toolbar.getChildAt(0).performClick();
                });

        // Verify that #onBackPressed was called.
        mBackPressCallbackHelper.waitForOnly();
    }

    @Test
    @SmallTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    @Feature({"Automotive Toolbar"})
    public void testAutomotiveToolbar_FullscreenChromeDialog_setContentView() throws Exception {
        mBlankUiActivityTestRule.launchActivity(null);

        // Display a full screen AlertDialog created using FullscreenAlertDialog.Builder.
        ChromeDialog dialog =
                createAndShowFullscreenChromeDialog(mBlankUiActivityTestRule.getActivity(), true);

        // Check that the automotive toolbar is present with only a back button.
        Toolbar toolbar = dialog.findViewById(R.id.back_button_toolbar);
        assertNotNull(toolbar);
        assertEquals("Toolbar not visible", View.VISIBLE, toolbar.getVisibility());
        assertEquals("Toolbar should only contain a back button", 1, toolbar.getChildCount());
        assertThat(toolbar.getChildAt(0), instanceOf(AppCompatImageButton.class));

        // Click the back button in the automotive toolbar.
        addOnBackPressedCallback(dialog, mBackPressCallbackHelper);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    toolbar.getChildAt(0).performClick();
                });

        // Verify that #onBackPressed was called.
        mBackPressCallbackHelper.waitForOnly();
    }

    @Test
    @SmallTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    @Feature({"Automotive Toolbar"})
    public void testAutomotiveToolbar_FullscreenChromeDialog_addContentView() throws Exception {
        mBlankUiActivityTestRule.launchActivity(null);

        // Display a full screen AlertDialog created using FullscreenAlertDialog.Builder.
        ChromeDialog dialog =
                createAndShowFullscreenChromeDialog(mBlankUiActivityTestRule.getActivity(), false);

        // Check that the automotive toolbar is present with only a back button.
        Toolbar toolbar = dialog.findViewById(R.id.back_button_toolbar);
        assertNotNull(toolbar);
        assertEquals("Toolbar not visible", View.VISIBLE, toolbar.getVisibility());
        assertEquals("Toolbar should only contain a back button", 1, toolbar.getChildCount());
        assertThat(toolbar.getChildAt(0), instanceOf(AppCompatImageButton.class));

        // Click the back button in the automotive toolbar.
        addOnBackPressedCallback(dialog, mBackPressCallbackHelper);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    toolbar.getChildAt(0).performClick();
                });

        // Verify that #onBackPressed was called.
        mBackPressCallbackHelper.waitForOnly();
    }

    private void addOnBackPressedCallback(
            AppCompatActivity activity, CallbackHelper backPressCallback) {
        activity.getOnBackPressedDispatcher()
                .addCallback(
                        new OnBackPressedCallback(true) {
                            @Override
                            public void handleOnBackPressed() {
                                backPressCallback.notifyCalled();
                            }
                        });
    }

    private void addOnBackPressedCallback(AlertDialog dialog, CallbackHelper backPressCallback) {
        dialog.getOnBackPressedDispatcher()
                .addCallback(
                        new OnBackPressedCallback(true) {
                            @Override
                            public void handleOnBackPressed() {
                                backPressCallback.notifyCalled();
                            }
                        });
    }

    private void addOnBackPressedCallback(
            ComponentDialog dialog, CallbackHelper backPressCallback) {
        dialog.getOnBackPressedDispatcher()
                .addCallback(
                        new OnBackPressedCallback(true) {
                            @Override
                            public void handleOnBackPressed() {
                                backPressCallback.notifyCalled();
                            }
                        });
    }

    private FullscreenAlertDialog createAndShowFullscreenAlertDialog(Context context)
            throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final FullscreenAlertDialog dialog =
                            new FullscreenAlertDialog(context, /* shouldPadForContent= */ false);
                    View testView = LayoutInflater.from(context).inflate(TEST_DIALOG_LAYOUT, null);
                    dialog.setView(testView);
                    dialog.show();
                    return dialog;
                });
    }

    private AlertDialog createAndShowFullscreenAlertDialogFromBuilder(Context context)
            throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final AlertDialog dialog =
                            new FullscreenAlertDialog.Builder(
                                            context, /* shouldPadForContent= */ false)
                                    .setView(TEST_DIALOG_LAYOUT)
                                    .create();
                    dialog.show();
                    return dialog;
                });
    }

    private ChromeDialog createAndShowFullscreenChromeDialog(
            Activity activity, boolean setContentView) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final ChromeDialog dialog =
                            new ChromeDialog(
                                    activity,
                                    R.style.ThemeOverlay_BrowserUI_Fullscreen,
                                    /* shouldPadForWindowInsets= */ true);
                    if (setContentView) {
                        dialog.setContentView(TEST_DIALOG_LAYOUT);
                    } else {
                        dialog.addContentView(
                                LayoutInflater.from(activity).inflate(TEST_DIALOG_LAYOUT, null),
                                new LayoutParams(MATCH_PARENT, MATCH_PARENT));
                    }
                    dialog.show();
                    return dialog;
                });
    }
}
