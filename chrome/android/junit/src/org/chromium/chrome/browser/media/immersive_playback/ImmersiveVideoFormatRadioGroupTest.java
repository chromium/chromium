// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.os.Looper;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.media.immersive_playback.ImmersiveVideoFormatRadioGroup.FormatOption;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;

import java.util.concurrent.TimeUnit;

/** Tests for {@link ImmersiveVideoFormatRadioGroup}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImmersiveVideoFormatRadioGroupTest {
    private Activity mActivity;
    private ImmersiveVideoFormatRadioGroup mRadioGroup;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mRadioGroup = new ImmersiveVideoFormatRadioGroup(mActivity);
        mActivity.setContentView(mRadioGroup);

        mRadioGroup.findViewById(R.id.standard_option).setFocusableInTouchMode(true);
        mRadioGroup.findViewById(R.id.stereoscopic_option).setFocusableInTouchMode(true);
        mRadioGroup.findViewById(R.id.hemisphere_option).setFocusableInTouchMode(true);
        mRadioGroup.findViewById(R.id.sphere_option).setFocusableInTouchMode(true);
        mRadioGroup.findViewById(R.id.recommended_option).setFocusableInTouchMode(true);
    }

    @Test
    public void testInitialization() {
        // Recommended button should be hidden initially
        RadioButtonWithDescription recommendedBtn =
                mRadioGroup.findViewById(R.id.recommended_option);
        assertNotNull(recommendedBtn);
        assertEquals(View.GONE, recommendedBtn.getVisibility());

        // Standard options should be present and have correct tags
        RadioButtonWithDescription standardBtn = mRadioGroup.findViewById(R.id.standard_option);
        assertNotNull(standardBtn);
        FormatOption standardOption = (FormatOption) standardBtn.getTag();
        assertNotNull(standardOption);
        assertEquals(ImmersiveStereoMode.MONO, standardOption.stereoMode);
        assertEquals(ImmersiveProjectionType.QUAD, standardOption.projectionType);

        // Nothing should be selected by default
        assertNull(mRadioGroup.getSelectedOption());
    }

    @Test
    public void testSetRecommendedOption() {
        mRadioGroup.setRecommendedOption(
                ImmersiveStereoMode.SIDE_BY_SIDE, ImmersiveProjectionType.SPHERE);

        RadioButtonWithDescription recommendedBtn =
                mRadioGroup.findViewById(R.id.recommended_option);
        assertEquals(View.VISIBLE, recommendedBtn.getVisibility());

        FormatOption option = (FormatOption) recommendedBtn.getTag();
        assertNotNull(option);
        assertEquals(ImmersiveStereoMode.SIDE_BY_SIDE, option.stereoMode);
        assertEquals(ImmersiveProjectionType.SPHERE, option.projectionType);

        // Verify description text was set
        CharSequence desc = recommendedBtn.getDescriptionText();
        assertNotNull(desc);
        assertTrue(desc.length() > 0);
    }

    @Test
    public void testCheckOption() {
        mRadioGroup.checkOption(ImmersiveStereoMode.MONO, ImmersiveProjectionType.SPHERE);

        RadioButtonWithDescription sphereBtn = mRadioGroup.findViewById(R.id.sphere_option);
        assertTrue(sphereBtn.isChecked());

        FormatOption selected = mRadioGroup.getSelectedOption();
        assertNotNull(selected);
        assertEquals(ImmersiveStereoMode.MONO, selected.stereoMode);
        assertEquals(ImmersiveProjectionType.SPHERE, selected.projectionType);
    }

    @Test
    public void testSelectionCallback() {
        final FormatOption[] selected = new FormatOption[1];
        mRadioGroup.setSelectionCallback(option -> selected[0] = option);

        // Simulate user clicking standard option
        RadioButtonWithDescription standardBtn = mRadioGroup.findViewById(R.id.standard_option);
        standardBtn.performClick();

        assertNotNull(selected[0]);
        assertEquals(ImmersiveStereoMode.MONO, selected[0].stereoMode);
        assertEquals(ImmersiveProjectionType.QUAD, selected[0].projectionType);
    }

    @Test
    public void testRecommendedOptionMatchesExistingOption() {
        // Set recommended option to match "standard" (MONO, QUAD)
        mRadioGroup.setRecommendedOption(ImmersiveStereoMode.MONO, ImmersiveProjectionType.QUAD);

        RadioButtonWithDescription recommendedBtn =
                mRadioGroup.findViewById(R.id.recommended_option);
        RadioButtonWithDescription standardBtn = mRadioGroup.findViewById(R.id.standard_option);

        // Check recommended option should be visible
        assertEquals(View.VISIBLE, recommendedBtn.getVisibility());

        // Check programmatically selecting (MONO, QUAD)
        // Since recommendedBtn is first in mRadioButtons, it should be checked
        mRadioGroup.checkOption(ImmersiveStereoMode.MONO, ImmersiveProjectionType.QUAD);
        assertTrue(recommendedBtn.isChecked());
        assertFalse(standardBtn.isChecked());
        assertEquals(recommendedBtn.getTag(), mRadioGroup.getSelectedOption());

        // Re-selecting: click the standard button manually
        standardBtn.setChecked(true);
        assertFalse(recommendedBtn.isChecked());
        assertTrue(standardBtn.isChecked());
        assertEquals(standardBtn.getTag(), mRadioGroup.getSelectedOption());

        // Re-selecting: select the recommended button manually
        recommendedBtn.setChecked(true);
        assertTrue(recommendedBtn.isChecked());
        assertFalse(standardBtn.isChecked());
        assertEquals(recommendedBtn.getTag(), mRadioGroup.getSelectedOption());
    }

    @Test
    public void testRequestFocusForAccessibility_withCheckedButton() {
        RadioButtonWithDescription sphereBtn = mRadioGroup.findViewById(R.id.sphere_option);
        sphereBtn.setChecked(true);

        mRadioGroup.requestFocusForAccessibility();

        shadowOf(Looper.getMainLooper()).idleFor(150, TimeUnit.MILLISECONDS);

        assertTrue(sphereBtn.isFocused());
    }

    @Test
    public void testRequestFocusForAccessibility_fallbackToFirstVisible() {
        RadioButtonWithDescription standardBtn = mRadioGroup.findViewById(R.id.standard_option);

        mRadioGroup.requestFocusForAccessibility();

        shadowOf(Looper.getMainLooper()).idleFor(150, TimeUnit.MILLISECONDS);

        assertTrue(standardBtn.isFocused());
    }

    @Test
    public void testOnAttachedToWindow_RequestsFocus() {
        ImmersiveVideoFormatRadioGroup unattachedGroup =
                new ImmersiveVideoFormatRadioGroup(mActivity);
        RadioButtonWithDescription sphereBtn = unattachedGroup.findViewById(R.id.sphere_option);
        sphereBtn.setFocusableInTouchMode(true);
        sphereBtn.setChecked(true);

        mActivity.setContentView(unattachedGroup);
        unattachedGroup.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY));
        unattachedGroup.layout(0, 0, 100, 100);

        shadowOf(Looper.getMainLooper()).idleFor(150, TimeUnit.MILLISECONDS);

        assertTrue(sphereBtn.isFocused());
    }
}
