// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.net.Uri;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwMediaIntegrityApiStatusConfig;
import org.chromium.android_webview.AwMediaIntegrityApiStatusConfig.ApiStatus;

import java.util.Map;

/** {@link org.chromium.android_webview.AwMediaIntegrityApiStatusConfig} tests. */
@RunWith(AwJUnit4ClassRunner.class)
public class AwMediaIntegrityApiStatusConfigTest {
    @Rule public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    @Test
    @SmallTest
    public void testGetApiStatus_returnsEmptyConfig_whenNotSet() throws Throwable {
        AwMediaIntegrityApiStatusConfig config = new AwMediaIntegrityApiStatusConfig();

        Assert.assertEquals(ApiStatus.ENABLED, config.getDefaultStatus());
        Assert.assertTrue(config.getOverrideRules().isEmpty());
    }

    @Test
    @SmallTest
    public void testGetApiStatus_returnsConfig_whenSetWithRules() throws Throwable {
        AwMediaIntegrityApiStatusConfig config = new AwMediaIntegrityApiStatusConfig();
        Map<String, @ApiStatus Integer> overrideRules =
            Map.of("http://*.example.com", ApiStatus.ENABLED_WITHOUT_APP_IDENTITY);
        @ApiStatus int defaultPermission = ApiStatus.DISABLED;
        config.setApiAvailabilityRules(defaultPermission, overrideRules);

        Assert.assertEquals(defaultPermission, config.getDefaultStatus());
        Assert.assertEquals(overrideRules, config.getOverrideRules());
    }

    @Test
    @SmallTest
    public void testGetStatusForUri_returnsStatus_whenMatches() throws Throwable {
        AwMediaIntegrityApiStatusConfig config = new AwMediaIntegrityApiStatusConfig();
        Map<String, @ApiStatus Integer> overrideRules =
            Map.of(
                "http://*.example.com", ApiStatus.ENABLED_WITHOUT_APP_IDENTITY,
                "http://somesite.com", ApiStatus.ENABLED);
        @ApiStatus int defaultPermission = ApiStatus.DISABLED;
        config.setApiAvailabilityRules(defaultPermission, overrideRules);

        Assert.assertEquals(defaultPermission, config.getDefaultStatus());
        Assert.assertEquals(overrideRules, config.getOverrideRules());
        Assert.assertEquals(
            ApiStatus.ENABLED_WITHOUT_APP_IDENTITY,
            config.getStatusForUri(Uri.parse("http://sub.example.com")));
    }

    @Test
    @SmallTest
    public void testGetStatusForUri_returnsDefaultStatus_whenNoMatches() throws Throwable {
        AwMediaIntegrityApiStatusConfig config = new AwMediaIntegrityApiStatusConfig();
        Map<String, @ApiStatus Integer> overrideRules =
            Map.of(
                "http://*.example.com", ApiStatus.ENABLED_WITHOUT_APP_IDENTITY,
                "http://somesite.com", ApiStatus.ENABLED);
        @ApiStatus int defaultPermission = ApiStatus.DISABLED;
        config.setApiAvailabilityRules(defaultPermission, overrideRules);

        Assert.assertEquals(defaultPermission, config.getDefaultStatus());
        Assert.assertEquals(overrideRules, config.getOverrideRules());
        Assert.assertEquals(
            defaultPermission, config.getStatusForUri(Uri.parse("http://randomsite.com")));
    }

    @Test
    @MediumTest
    public void testSetConfigTwice_returnsUpdatedStatus_whenUriMatches() throws Throwable {
        AwMediaIntegrityApiStatusConfig config = new AwMediaIntegrityApiStatusConfig();
        Map<String, @ApiStatus Integer> overrideRules =
            Map.of(
                "http://*.example.com", ApiStatus.ENABLED_WITHOUT_APP_IDENTITY,
                "http://somesite.com", ApiStatus.ENABLED);
        @ApiStatus int defaultPermission = ApiStatus.DISABLED;
        config.setApiAvailabilityRules(defaultPermission, overrideRules);

        Assert.assertEquals(defaultPermission, config.getDefaultStatus());
        Assert.assertEquals(overrideRules, config.getOverrideRules());
        Assert.assertEquals(
            ApiStatus.ENABLED_WITHOUT_APP_IDENTITY,
            config.getStatusForUri(Uri.parse("http://sub.example.com")));

        Map<String, @ApiStatus Integer> overrideRules2 =
            Map.of("http://*.somedomain.com", ApiStatus.ENABLED_WITHOUT_APP_IDENTITY);
        @ApiStatus int defaultPermission2 = ApiStatus.ENABLED;
        config.setApiAvailabilityRules(defaultPermission2, overrideRules2);

        Assert.assertEquals(defaultPermission2, config.getDefaultStatus());
        Assert.assertEquals(overrideRules2, config.getOverrideRules());
        Assert.assertEquals(
            defaultPermission2, config.getStatusForUri(Uri.parse("http://sub.example.com")));
        Assert.assertEquals(
            ApiStatus.ENABLED_WITHOUT_APP_IDENTITY,
            config.getStatusForUri(Uri.parse("http://sub.somedomain.com")));
    }

    @Test
    @SmallTest
    public void testSetApiConfig_throwsError_whenGivenInvalidRule() throws Throwable {
        AwMediaIntegrityApiStatusConfig config = new AwMediaIntegrityApiStatusConfig();
        Map<String, @ApiStatus Integer> overrideRules =
            Map.of(
                "xyz://*.abc.com", ApiStatus.ENABLED_WITHOUT_APP_IDENTITY);
        @ApiStatus int defaultPermission = ApiStatus.DISABLED;
        Assert.assertThrows(IllegalArgumentException.class,
            () -> config.setApiAvailabilityRules(defaultPermission, overrideRules));
        Assert.assertTrue(config.getOverrideRules().isEmpty());

    }

    @Test
    @SmallTest
    public void testSetApiConfig_doesNotOverwritePreviousRules_whenGivenNewInvalidRules()
        throws Throwable {
        AwMediaIntegrityApiStatusConfig config = new AwMediaIntegrityApiStatusConfig();
        Map<String, @ApiStatus Integer> overrideRules =
            Map.of(
                "http://*.example.com", ApiStatus.ENABLED_WITHOUT_APP_IDENTITY);
        @ApiStatus int defaultPermission = ApiStatus.DISABLED;
        config.setApiAvailabilityRules(defaultPermission, overrideRules);
        Assert.assertEquals(defaultPermission, config.getDefaultStatus());
        Assert.assertEquals(overrideRules, config.getOverrideRules());

        Map<String, @ApiStatus Integer> overrideRules2 =
            Map.of(
                // one good rule
                "http://*.sub.example.com", ApiStatus.ENABLED,
                // one bad rule
                "xyz://*.abc.com", ApiStatus.ENABLED_WITHOUT_APP_IDENTITY);
        Assert.assertThrows(IllegalArgumentException.class,
            () -> config.setApiAvailabilityRules(defaultPermission, overrideRules2));
        Assert.assertEquals(defaultPermission, config.getDefaultStatus());
        // Previously set rules have not been overwritten
        Assert.assertEquals(overrideRules, config.getOverrideRules());
        // Does not even set the good rule from overrideRules2
        Assert.assertFalse(config.getOverrideRules().containsKey("http://*.sub.example.com"));
    }

    @Test
    @SmallTest
    public void testGetStatusForUri_returnsLessPermissiveStatus_whenMultipleMatches() throws Throwable {
        AwMediaIntegrityApiStatusConfig config = new AwMediaIntegrityApiStatusConfig();
        Map<String, @ApiStatus Integer> overrideRules =
            Map.of(
                "http://*.example.com", ApiStatus.ENABLED_WITHOUT_APP_IDENTITY,
                "http://*.sub.example.com", ApiStatus.ENABLED,
                "http://*.other.example.com", ApiStatus.DISABLED,
                "http://*.xyz.sub.example.com", ApiStatus.DISABLED);
        @ApiStatus int defaultPermission = ApiStatus.ENABLED;
        config.setApiAvailabilityRules(defaultPermission, overrideRules);

        Assert.assertEquals(defaultPermission, config.getDefaultStatus());
        Assert.assertEquals(overrideRules, config.getOverrideRules());
        // Choose between ENABLED and ENABLED_WITHOUT_APP_IDENTITY
        Assert.assertEquals(
            ApiStatus.ENABLED_WITHOUT_APP_IDENTITY,
            config.getStatusForUri(Uri.parse("http://random.sub.example.com")));
        // Choose between ENABLED_WITHOUT_APP_IDENTITY and DISABLED
        Assert.assertEquals(
            ApiStatus.DISABLED,
            config.getStatusForUri(Uri.parse("http://random.other.example.com")));
        // Choose between ENABLED, ENABLED_WITHOUT_APP_IDENTITY and DISABLED
        Assert.assertEquals(
            ApiStatus.DISABLED,
            config.getStatusForUri(Uri.parse("http://random.xyz.sub.example.com")));
    }
}
