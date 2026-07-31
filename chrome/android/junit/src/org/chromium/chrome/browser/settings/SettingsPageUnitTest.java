// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.ViewGroup;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandlerRegistry;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.TestActivity;

/**
 * Unit tests for {@link SettingsPage}. More extensive testing is performed with {@link NativePage}
 * and {@link NativePageFactory}.
 */
@RunWith(BaseRobolectricTestRunner.class)
// Needed for {@link BasicNativePage.setBackPressHandler()}. See {@link SettingsActivityUnitTest}.
@EnableFeatures(ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES)
public class SettingsPageUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Profile mProfile;
    @Mock private NativePageHost mNativePageHost;
    @Mock private SettingsPage.FragmentDelegate mFragmentDelegate;
    @Mock private BackPressHandler mBackPressHandler;
    @Mock private BackPressHandlerRegistry mBackPressHandlerRegistry;

    private Activity mActivity;
    private SettingsPage mSettingsPage;

    @Before
    public void setup() {
        mActivityScenarios.getScenario().onActivity(activity -> mActivity = activity);
        when(mNativePageHost.getContext()).thenReturn(mActivity);

        mSettingsPage =
                new SettingsPage(
                        mActivity,
                        mProfile,
                        mNativePageHost,
                        mFragmentDelegate,
                        mBackPressHandler,
                        mBackPressHandlerRegistry,
                        UrlConstants.SETTINGS_URL);
    }

    @Test
    public void testGetters() {
        assertEquals("Settings", mSettingsPage.getTitle());
        assertEquals("settings", mSettingsPage.getHost());
    }

    @Test
    public void testInitSettings() {
        // initSettings() should be called once, in the constructor.
        verify(mFragmentDelegate).initSettings(any(ViewGroup.class), eq(UrlConstants.SETTINGS_URL));
    }

    @Test
    public void testDestroySettings() {
        // destroySettings() should be called once, in destroy().
        mSettingsPage.destroy();
        verify(mFragmentDelegate).destroySettings();
    }

    @Test
    public void testSetBackPressHandler() {
        BackPressHandlerRegistry registry = mock(BackPressHandlerRegistry.class);
        BackPressHandler handler = mock(BackPressHandler.class);
        SettingsPage page =
                new SettingsPage(
                        mActivity,
                        mProfile,
                        mNativePageHost,
                        mFragmentDelegate,
                        handler,
                        registry,
                        UrlConstants.SETTINGS_URL);
        mActivity.setContentView(page.getView());
        verify(registry).addHandler(eq(handler), eq(BackPressHandler.Type.NATIVE_PAGE));
    }
}
