// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.installedapp;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.installedapp.mojom.InstalledAppProvider;

/** Unit tests for {@link InstalledAppProviderFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InstalledAppProviderFactoryTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private RenderFrameHost mRenderFrameHost;
    @Mock private WebContents mWebContents;
    @Mock private Profile mProfile;
    private static final int RFH_INVALID_CALL_FROM_NOT_MAIN_FRAME = 227;

    @Before
    public void setUp() {
        WebContentsStatics.setWebContentsForTesting(mWebContents);
        Profile.setProfileFromWebContentsForTesting(mProfile);
    }

    @Test
    public void testCreateImpl_primaryMainFrame() {
        doReturn(true).when(mRenderFrameHost).isInPrimaryMainFrame();
        InstalledAppProviderFactory factory = new InstalledAppProviderFactory(mRenderFrameHost);
        InstalledAppProvider provider = factory.createImpl();
        assertNotNull(provider);
    }

    @Test
    public void testCreateImpl_notPrimaryMainFrame() {
        doReturn(false).when(mRenderFrameHost).isInPrimaryMainFrame();
        InstalledAppProviderFactory factory = new InstalledAppProviderFactory(mRenderFrameHost);
        InstalledAppProvider provider = factory.createImpl();
        assertNull(provider);
        verify(mRenderFrameHost).terminateRendererDueToBadMessage(RFH_INVALID_CALL_FROM_NOT_MAIN_FRAME);
    }
}
