// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Tests for {@link SettingsNavigationFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsNavigationFactoryTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private WindowAndroid mWindowAndroid;

    private Activity mActivity;

    @Before
    public void setUp() {
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
    }

    @Test
    public void testShowClearBrowsingData_validActivity() {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));

        SettingsNavigationFactory.showClearBrowsingData(mWindowAndroid);

        verify(mSettingsNavigation).startSettings(mActivity, SettingsFragment.CLEAR_BROWSING_DATA);
    }

    @Test
    public void testShowClearBrowsingData_nullWindowAndroid() {
        SettingsNavigationFactory.showClearBrowsingData(null);

        verify(mSettingsNavigation, never())
                .startSettings(mActivity, SettingsFragment.CLEAR_BROWSING_DATA);
    }

    @Test
    public void testShowClearBrowsingData_nullActivity() {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(null));

        SettingsNavigationFactory.showClearBrowsingData(mWindowAndroid);

        verify(mSettingsNavigation, never())
                .startSettings(mActivity, SettingsFragment.CLEAR_BROWSING_DATA);
    }

    @Test
    public void testShowClearBrowsingData_finishingActivity() {
        mActivity.finish();
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));

        SettingsNavigationFactory.showClearBrowsingData(mWindowAndroid);

        verify(mSettingsNavigation, never())
                .startSettings(mActivity, SettingsFragment.CLEAR_BROWSING_DATA);
    }
}
