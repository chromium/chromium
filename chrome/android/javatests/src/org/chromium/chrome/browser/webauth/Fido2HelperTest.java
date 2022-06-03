// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import androidx.test.filters.SmallTest;

import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialCreationOptions;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialRequestOptions;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.webauthn.Fido2Helper;
import org.chromium.mojo_base.mojom.TimeDelta;

/**
 * Unit tests for Fido2Helper.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class Fido2HelperTest {
    private org.chromium.blink.mojom.PublicKeyCredentialCreationOptions mCreationOptions;
    private org.chromium.blink.mojom.PublicKeyCredentialRequestOptions mRequestOptions;

    @Before
    public void setUp() throws Exception {
        mCreationOptions = Fido2ApiTestHelper.createDefaultMakeCredentialOptions();
        mRequestOptions = Fido2ApiTestHelper.createDefaultGetAssertionOptions();
    }

    @Test
    @SmallTest
    public void testToMakeCredentialOptions_nullTimeout() throws Exception {
        mCreationOptions.timeout = null;
        PublicKeyCredentialCreationOptions options =
                Fido2Helper.toMakeCredentialOptions(mCreationOptions);
        Assert.assertEquals(Fido2Helper.MAX_TIMEOUT_SECONDS, options.getTimeoutSeconds(), 0);
    }

    @Test
    @SmallTest
    public void testToMakeCredentialOptions_tinyTimeout() throws Exception {
        mCreationOptions.timeout = new TimeDelta();
        mCreationOptions.timeout.microseconds = 1;
        PublicKeyCredentialCreationOptions options =
                Fido2Helper.toMakeCredentialOptions(mCreationOptions);
        Assert.assertEquals(Fido2Helper.MIN_TIMEOUT_SECONDS, options.getTimeoutSeconds(), 0);
    }

    @Test
    @SmallTest
    public void testToMakeCredentialOptions_hugeTimeout() throws Exception {
        mCreationOptions.timeout = new TimeDelta();
        mCreationOptions.timeout.microseconds = 1_000_000L * 60 * 60; // One hour.
        PublicKeyCredentialCreationOptions options =
                Fido2Helper.toMakeCredentialOptions(mCreationOptions);
        Assert.assertEquals(Fido2Helper.MAX_TIMEOUT_SECONDS, options.getTimeoutSeconds(), 0);
    }

    @Test
    @SmallTest
    public void testToMakeCredentialOptions_reasonableTimeout() throws Exception {
        mCreationOptions.timeout = new TimeDelta();
        mCreationOptions.timeout.microseconds = 1_000_000L * 60 * 2; // Two minutes.
        PublicKeyCredentialCreationOptions options =
                Fido2Helper.toMakeCredentialOptions(mCreationOptions);
        Assert.assertEquals(2 * 60, options.getTimeoutSeconds(), 0);
    }

    @Test
    @SmallTest
    public void testToGetAssertionOptions_nullTimeout() throws Exception {
        mRequestOptions.timeout = null;
        PublicKeyCredentialRequestOptions options =
                Fido2Helper.toGetAssertionOptions(mRequestOptions);
        Assert.assertEquals(Fido2Helper.MAX_TIMEOUT_SECONDS, options.getTimeoutSeconds(), 0);
    }

    @Test
    @SmallTest
    public void testToGetAssertionOptions_tinyTimeout() throws Exception {
        mRequestOptions.timeout = new TimeDelta();
        mRequestOptions.timeout.microseconds = 1;
        PublicKeyCredentialRequestOptions options =
                Fido2Helper.toGetAssertionOptions(mRequestOptions);
        Assert.assertEquals(Fido2Helper.MIN_TIMEOUT_SECONDS, options.getTimeoutSeconds(), 0);
    }

    @Test
    @SmallTest
    public void testToGetAssertionOptions_hugeTimeout() throws Exception {
        mRequestOptions.timeout = new TimeDelta();
        mRequestOptions.timeout.microseconds = 1_000_000L * 60 * 60; // One hour.
        PublicKeyCredentialRequestOptions options =
                Fido2Helper.toGetAssertionOptions(mRequestOptions);
        Assert.assertEquals(Fido2Helper.MAX_TIMEOUT_SECONDS, options.getTimeoutSeconds(), 0);
    }

    @Test
    @SmallTest
    public void testToGetAssertionOptions_reasonableTimeout() throws Exception {
        mRequestOptions.timeout = new TimeDelta();
        mRequestOptions.timeout.microseconds = 1_000_000L * 60 * 2; // Two minutes.
        PublicKeyCredentialRequestOptions options =
                Fido2Helper.toGetAssertionOptions(mRequestOptions);
        Assert.assertEquals(2 * 60, options.getTimeoutSeconds(), 0);
    }
}
