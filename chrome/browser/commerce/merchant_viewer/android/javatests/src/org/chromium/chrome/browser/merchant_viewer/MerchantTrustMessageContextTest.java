// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** Tests for {@link MerchantTrustMessageContext}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@SuppressWarnings("DoNotMock") // Mocking GURL
public class MerchantTrustMessageContextTest {

    @Mock private GURL mMockGurl;

    @Mock private WebContents mMockWebContents;

    @Mock private NavigationHandle mMockNavigationHandle;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn("fake_host").when(mMockGurl).getHost();
        doReturn("fake_spec").when(mMockGurl).getSpec();
        doReturn(mMockGurl).when(mMockNavigationHandle).getUrl();
    }

    @Test
    public void testIsValid() {
        doReturn(false).when(mMockWebContents).isDestroyed();
        MerchantTrustMessageContext context =
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents);
        assertTrue(context.isValid());
    }

    @Test
    public void testIsValidDestroyedWebContents() {
        doReturn(true).when(mMockWebContents).isDestroyed();
        MerchantTrustMessageContext context =
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents);
        assertFalse(context.isValid());
    }

    @Test
    public void testIsValidEmptyHostname() {
        doReturn(false).when(mMockWebContents).isDestroyed();
        doReturn(true).when(mMockGurl).isEmpty();
        MerchantTrustMessageContext context =
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents);
        assertFalse(context.isValid());
    }

    @Test
    public void testIsValidNullHostname() {
        doReturn(true).when(mMockWebContents).isDestroyed();
        MerchantTrustMessageContext context =
                new MerchantTrustMessageContext(null, mMockWebContents);
        assertFalse(context.isValid());
    }

    @Test
    public void testIsValidNullWebContents() {
        MerchantTrustMessageContext context =
                new MerchantTrustMessageContext(mMockNavigationHandle, null);
        assertFalse(context.isValid());
    }

    @Test
    public void testGetHostName() {
        assertEquals(null, new MerchantTrustMessageContext(null, mMockWebContents).getHostName());
        assertEquals(
                "fake_host",
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents)
                        .getHostName());
    }

    @Test
    public void testGetUrl() {
        assertEquals(null, new MerchantTrustMessageContext(null, mMockWebContents).getUrl());
        assertEquals(
                "fake_spec",
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents).getUrl());
    }

    @Test
    public void testGetWebContents() {
        assertEquals(
                mMockWebContents,
                new MerchantTrustMessageContext(mMockNavigationHandle, mMockWebContents)
                        .getWebContents());
    }
}
