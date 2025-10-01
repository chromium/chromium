// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import static org.chromium.chrome.browser.hub.HubActionButtonProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubActionButtonProperties.ACTION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;

import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Arrays;
import java.util.Collection;

/** Unit tests for Hub Action Button using Button and HubActionButtonHelper. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Features.DisableFeatures({
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_UPDATE
})
public class HubActionButtonViewUnitTest {
    // All the tests in this file will run twice, once for isXrDevice=true and once for
    // isXrDevice=false. Expect all the tests with the same results on XR devices too.
    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {{true}, {false}});
    }

    @Parameter(0)
    public boolean mIsXrDevice;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Mock Runnable mOnButton;

    private Activity mActivity;
    private FrameLayout mActionButtonContainer;
    private Button mActionButton;
    private PropertyModel mPropertyModel;
    private HubColorMixer mColorMixer;
    private ObservableSupplierImpl<Pane> mFocusedPaneSupplier;

    @Before
    public void setUp() throws Exception {
        DeviceInfo.setIsXrForTesting(mIsXrDevice);

        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        mActivity = activity;
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        LayoutInflater inflater = LayoutInflater.from(mActivity);
        int layoutId = mIsXrDevice ? R.layout.hub_xr_toolbar_layout : R.layout.hub_toolbar_layout;
        FrameLayout toolbarContainer = (FrameLayout) inflater.inflate(layoutId, null, false);
        mActionButton = toolbarContainer.findViewById(R.id.toolbar_action_button);

        mFocusedPaneSupplier = new ObservableSupplierImpl<>();
        mColorMixer =
                spy(
                        new HubColorMixerImpl(
                                mActivity,
                                new ObservableSupplierImpl<>(true),
                                mFocusedPaneSupplier));
        mPropertyModel =
                new PropertyModel.Builder(HubActionButtonProperties.ALL_ACTION_BUTTON_KEYS)
                        .with(ACTION_BUTTON_VISIBLE, true)
                        .with(COLOR_MIXER, mColorMixer)
                        .build();
        PropertyModelChangeProcessor.create(
                mPropertyModel, mActionButton, HubActionButtonViewBinder::bind);
    }

    private FullButtonData makeTestButtonData() {
        DisplayButtonData displayButtonData =
                new ResourceButtonData(
                        R.string.button_new_tab, R.string.button_new_tab, R.drawable.ic_add);
        return new DelegateButtonData(displayButtonData, mOnButton);
    }

    @Test
    public void testActionButtonVisibility() {
        // Initially no button data is set, so button should be invisible
        assertEquals(View.GONE, mActionButton.getVisibility());

        // Set button data, button should be visible
        FullButtonData fullButtonData = makeTestButtonData();
        mPropertyModel.set(ACTION_BUTTON_DATA, fullButtonData);
        assertEquals(View.VISIBLE, mActionButton.getVisibility());

        // Set ACTION_BUTTON_VISIBLE to false, button should be invisible
        mPropertyModel.set(ACTION_BUTTON_VISIBLE, false);
        assertEquals(View.GONE, mActionButton.getVisibility());

        // Set ACTION_BUTTON_VISIBLE back to true, button should be visible again
        mPropertyModel.set(ACTION_BUTTON_VISIBLE, true);
        assertEquals(View.VISIBLE, mActionButton.getVisibility());
    }

    @Test
    public void testActionButtonCallback() {
        // Initially no button data is set, clicking should have no effect
        mActionButton.callOnClick();
        verifyNoInteractions(mOnButton);

        // Set button data, clicking should trigger the callback
        FullButtonData fullButtonData = makeTestButtonData();
        mPropertyModel.set(ACTION_BUTTON_DATA, fullButtonData);
        mActionButton.callOnClick();
        verify(mOnButton, times(1)).run();

        // Click again should trigger the callback again
        mActionButton.callOnClick();
        verify(mOnButton, times(2)).run();
    }

    @Test
    public void testActionButtonTooltipText() {
        assertNull(mActionButton.getTooltipText());

        // Set button data, tooltip should be set.
        FullButtonData fullButtonData = makeTestButtonData();
        mPropertyModel.set(ACTION_BUTTON_DATA, fullButtonData);
        assertEquals("New tab", mActionButton.getTooltipText());

        // Set ACTION_BUTTON_VISIBLE to false, button should be invisible and tooltip should be
        // null.
        mPropertyModel.set(ACTION_BUTTON_VISIBLE, false);
        assertEquals(View.GONE, mActionButton.getVisibility());
        assertNull(mActionButton.getTooltipText());

        // Set ACTION_BUTTON_VISIBLE back to true, button should be visible and tooltip should be
        // set.
        mPropertyModel.set(ACTION_BUTTON_VISIBLE, true);
        assertEquals(View.VISIBLE, mActionButton.getVisibility());
        assertEquals("New tab", mActionButton.getTooltipText());
    }

    @Test
    public void testColorMixer() {
        // Verify that color mixer was set on the button
        mPropertyModel.set(COLOR_MIXER, mColorMixer);
        verify(mColorMixer).registerBlend(any());
    }
}
