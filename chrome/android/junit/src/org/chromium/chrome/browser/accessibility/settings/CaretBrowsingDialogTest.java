// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.settings;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.widget.CheckBox;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link CaretBrowsingDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CaretBrowsingDialogTest {

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private AccessibilitySettingsBridge.Natives mAccessibilitySettingsBridge;
    @Mock private Profile mProfile;

    private Activity mActivity;
    private CaretBrowsingDialog mCaretBrowsingDialog;

    @Rule // initialize mocks
    public MockitoRule rule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        AccessibilitySettingsBridgeJni.setInstanceForTesting(mAccessibilitySettingsBridge);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            mCaretBrowsingDialog =
                                    new CaretBrowsingDialog(
                                            mActivity, mModalDialogManager, mProfile);
                        });
    }

    @Test
    public void testDialogContent() {
        PropertyModel model = mCaretBrowsingDialog.getModelForTesting();
        Assert.assertEquals(
                "Dialog title should be correct.",
                mActivity.getString(R.string.caret_browsing_dialog_title),
                model.get(ModalDialogProperties.TITLE));
        Assert.assertEquals(
                "Message should be correct.",
                mActivity.getString(R.string.caret_browsing_dialog_message),
                model.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1));
        Assert.assertEquals(
                "Positive button text should be correct.",
                mActivity.getString(R.string.turn_on),
                model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        Assert.assertEquals(
                "Negative button text should be correct.",
                mActivity.getString(android.R.string.cancel),
                model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }

    @Test
    public void testTurnOnAction() {
        mCaretBrowsingDialog.onClick(
                mCaretBrowsingDialog.getModelForTesting(),
                ModalDialogProperties.ButtonType.POSITIVE);
        verify(mAccessibilitySettingsBridge).setCaretBrowsingEnabled(mProfile, true);
        verify(mAccessibilitySettingsBridge).setShowCaretBrowsingDialogPreference(mProfile, true);
    }

    @Test
    public void testCancelAction() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Accessibility.Android.CaretBrowsing.SelectedAction",
                                AccessibilitySettingsBridge.AccessibilityCaretBrowsingAction
                                        .DISMISSED)
                        .build();

        mCaretBrowsingDialog.onClick(
                mCaretBrowsingDialog.getModelForTesting(),
                ModalDialogProperties.ButtonType.NEGATIVE);
        verify(mModalDialogManager)
                .dismissDialog(
                        mCaretBrowsingDialog.getModelForTesting(),
                        DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        verify(mAccessibilitySettingsBridge, never()).setCaretBrowsingEnabled(mProfile, true);

        watcher.assertExpected();
    }

    @Test
    public void testDontAskAgain() {
        CheckBox checkbox =
                (CheckBox)
                        mCaretBrowsingDialog
                                .getCustomViewForTesting()
                                .findViewById(R.id.dont_ask_again);
        checkbox.setChecked(true);
        mCaretBrowsingDialog.onClick(
                mCaretBrowsingDialog.getModelForTesting(),
                ModalDialogProperties.ButtonType.POSITIVE);
        verify(mAccessibilitySettingsBridge).setCaretBrowsingEnabled(mProfile, true);
        verify(mAccessibilitySettingsBridge).setShowCaretBrowsingDialogPreference(mProfile, false);
    }

    @Test
    public void testShowDialogFeatureEnabledTogglesOff() {
        // WHEN: Caret browsing keyboard shortcut is triggered
        // GIVEN: Caret browsing is enabled
        // THEN: don't show dialog. Just toggle it off
        when(mAccessibilitySettingsBridge.isCaretBrowsingEnabled(mProfile)).thenReturn(true);
        assertThat(CaretBrowsingDialog.shouldShowDialogForKeyboardShortcut(mProfile), is(false));
        verify(mAccessibilitySettingsBridge).setCaretBrowsingEnabled(mProfile, false);
    }

    @Test
    public void testShowDialogPreferenceDisabledTogglesOn() {
        // WHEN: Caret browsing keyboard shortcut is triggered
        // GIVEN: Caret browsing is disabled && show dialog is false
        // THEN: don't show dialog. Just toggle it on
        when(mAccessibilitySettingsBridge.isCaretBrowsingEnabled(mProfile)).thenReturn(false);
        when(mAccessibilitySettingsBridge.isShowCaretBrowsingDialogPreference(mProfile))
                .thenReturn(false);
        assertThat(CaretBrowsingDialog.shouldShowDialogForKeyboardShortcut(mProfile), is(false));
        verify(mAccessibilitySettingsBridge).setCaretBrowsingEnabled(mProfile, true);
    }

    @Test
    public void testShowDialogPreferenceEnabledShowsDialog() {
        // WHEN: Caret browsing keyboard shortcut is triggered
        // GIVEN: Caret browsing is disabled && show dialog is true
        // THEN: show dialog and toggle it on
        when(mAccessibilitySettingsBridge.isCaretBrowsingEnabled(mProfile)).thenReturn(false);
        when(mAccessibilitySettingsBridge.isShowCaretBrowsingDialogPreference(mProfile))
                .thenReturn(true);
        assertThat(CaretBrowsingDialog.shouldShowDialogForKeyboardShortcut(mProfile), is(true));
        verify(mAccessibilitySettingsBridge, never()).setCaretBrowsingEnabled(mProfile, true);
    }
}
