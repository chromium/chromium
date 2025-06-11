// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.res.Configuration;
import android.os.LocaleList;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.LocaleUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.Locale;

/** Unit tests for {@link ComposeplateUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ComposeplateUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_COMPOSEPLATE})
    public void testIsComposeplateEnabled() {
        // Verifies that the composeplate is disabled on tablets.
        assertFalse(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ true));

        // Verifies that the composeplate is disabled in non-US country.
        Configuration config = new Configuration();
        String tag = "EN-CA";
        config.setLocales(LocaleList.forLanguageTags(tag));
        LocaleUtils.setDefaultLocalesFromConfiguration(config);
        Assert.assertEquals("CA", Locale.getDefault().getCountry());
        assertFalse(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false));

        // Verifies that the composeplate is enabled in US.
        tag = "EN-US";
        config.setLocales(LocaleList.forLanguageTags(tag));
        LocaleUtils.setDefaultLocalesFromConfiguration(config);
        Assert.assertEquals("US", Locale.getDefault().getCountry());
        assertTrue(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_COMPOSEPLATE})
    public void testIsComposeplateEnabled_SkipLocaleCheck() {
        ChromeFeatureList.sAndroidComposeplateSkipLocaleCheck.setForTesting(false);
        assertFalse(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ true));

        Configuration config = new Configuration();
        String tag = "EN-CA";
        config.setLocales(LocaleList.forLanguageTags(tag));
        LocaleUtils.setDefaultLocalesFromConfiguration(config);
        Assert.assertEquals("CA", Locale.getDefault().getCountry());
        assertFalse(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false));

        // Verifies that the composeplate is enabled in non-US country if the skip_locale_check is
        // true.
        ChromeFeatureList.sAndroidComposeplateSkipLocaleCheck.setForTesting(true);
        assertTrue(ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false));
    }
}
