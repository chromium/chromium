// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.connectors.device_trust.attestation;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link AttestationTokenGenerator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AttestationTokenGeneratorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AttestationTokenGeneratorDelegate mMockDelegate;

    @Test
    @SmallTest
    public void testDefaultDelegate_generateToken() {
        var defaultDelegate = new AttestationTokenGeneratorDefaultDelegate();
        byte[] contentBinding = new byte[] {1, 2, 3, 4};
        AttestationTokenResult result = defaultDelegate.generateToken(contentBinding);

        assertNotNull(result);
        assertNull(result.getToken());
        assertEquals("Not implemented", result.getErrorMessage());

        // Also verify through the generator when explicitly configured.
        AttestationTokenGenerator.setDelegateForTesting(defaultDelegate);
        AttestationTokenResult generatorResult =
                AttestationTokenGenerator.generateToken(contentBinding);
        assertNotNull(generatorResult);
        assertNull(generatorResult.getToken());
        assertEquals("Not implemented", generatorResult.getErrorMessage());
    }

    @Test
    @SmallTest
    public void testDefaultDelegate_preWarmCache() {
        // preWarmCache on default delegate should be a no-op and not throw.
        var defaultDelegate = new AttestationTokenGeneratorDefaultDelegate();
        defaultDelegate.preWarmCache();

        AttestationTokenGenerator.setDelegateForTesting(defaultDelegate);
        AttestationTokenGenerator.preWarmCache();
    }

    @Test
    @SmallTest
    public void testCustomDelegate_generateToken() {
        byte[] contentBinding = new byte[] {5, 6, 7, 8};
        byte[] expectedToken = new byte[] {9, 10, 11, 12};
        AttestationTokenResult expectedResult = new AttestationTokenResult(expectedToken, null);

        when(mMockDelegate.generateToken(contentBinding)).thenReturn(expectedResult);

        AttestationTokenGenerator.setDelegateForTesting(mMockDelegate);

        AttestationTokenResult result = AttestationTokenGenerator.generateToken(contentBinding);
        assertNotNull(result);
        assertArrayEquals(expectedToken, result.getToken());
        assertNull(result.getErrorMessage());

        verify(mMockDelegate).generateToken(contentBinding);
    }

    @Test
    @SmallTest
    public void testCustomDelegate_preWarmCache() {
        AttestationTokenGenerator.setDelegateForTesting(mMockDelegate);

        AttestationTokenGenerator.preWarmCache();

        verify(mMockDelegate).preWarmCache();
    }

    @Test
    @SmallTest
    public void testAttestationTokenResult_success() {
        byte[] token = new byte[] {1, 2, 3};
        AttestationTokenResult result = new AttestationTokenResult(token, null);

        assertArrayEquals(token, result.getToken());
        assertNull(result.getErrorMessage());
    }

    @Test
    @SmallTest
    public void testAttestationTokenResult_failure() {
        AttestationTokenResult result = new AttestationTokenResult(null, "Error generating token");

        assertNull(result.getToken());
        assertEquals("Error generating token", result.getErrorMessage());
    }
}
