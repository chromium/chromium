// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import android.content.res.Configuration;
import android.os.LocaleList;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.LocaleUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Locale;

/** Unit tests for {@link ComposeplateUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.ANDROID_COMPOSEPLATE})
public class ComposeplateUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ComposeplateUtils.Natives mMockComposeplateUtilsJni;
    @Mock private Profile mProfile;

    @Before
    public void setUp() {
        ComposeplateUtilsJni.setInstanceForTesting(mMockComposeplateUtilsJni);
        when(mMockComposeplateUtilsJni.isEnabledByPolicy(eq(mProfile))).thenReturn(true);
    }

    @Test
    public void testIsComposeplateEnabled() {
        // Verifies that the composeplate is disabled on tablets.
        assertFalse(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ true, mProfile));

        // Verifies that the composeplate is disabled in non-US country.
        Configuration config = new Configuration();
        String tag = "EN-CA";
        config.setLocales(LocaleList.forLanguageTags(tag));
        LocaleUtils.setDefaultLocalesFromConfiguration(config);
        Assert.assertEquals("CA", Locale.getDefault().getCountry());
        assertFalse(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false, mProfile));

        // Verifies that the composeplate is enabled in US.
        tag = "EN-US";
        config.setLocales(LocaleList.forLanguageTags(tag));
        LocaleUtils.setDefaultLocalesFromConfiguration(config);
        Assert.assertEquals("US", Locale.getDefault().getCountry());
        assertTrue(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false, mProfile));
    }

    @Test
    public void testIsComposeplateEnabled_SkipLocaleCheck() {
        ChromeFeatureList.sAndroidComposeplateSkipLocaleCheck.setForTesting(false);
        assertFalse(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ true, mProfile));

        Configuration config = new Configuration();
        String tag = "EN-CA";
        config.setLocales(LocaleList.forLanguageTags(tag));
        LocaleUtils.setDefaultLocalesFromConfiguration(config);
        Assert.assertEquals("CA", Locale.getDefault().getCountry());
        assertFalse(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false, mProfile));

        // Verifies that the composeplate is enabled in non-US country if the skip_locale_check is
        // true.
        ChromeFeatureList.sAndroidComposeplateSkipLocaleCheck.setForTesting(true);
        assertTrue(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false, mProfile));
    }

    @Test
    public void testIsComposeplateEnabled_DisabledByPolicy() {
        Configuration config = new Configuration();
        String tag = "EN-US";
        config.setLocales(LocaleList.forLanguageTags(tag));
        LocaleUtils.setDefaultLocalesFromConfiguration(config);
        Assert.assertEquals("US", Locale.getDefault().getCountry());
        assertTrue(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false, mProfile));

        when(mMockComposeplateUtilsJni.isEnabledByPolicy(eq(mProfile))).thenReturn(false);
        // Verifies that the composeplate is disabled by policy.
        assertFalse(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ true, mProfile));
    }
}
