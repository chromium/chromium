// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TrustedCdn;
import org.chromium.chrome.browser.tab.TrustedCdnJni;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.security_state.SecurityStateModelJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * Tests for showing the publisher URL for a trusted CDN.
 *
 * This class tests the interactions between:
 * - CustomTabTrustedCdnPublisherUrlVisibility
 * - TrustedCdn
 * - TrustedCdn.PublisherUrlVisibility
 *
 * TrustedCdnPublisherUrlTest (the instrumentation test) is still used to test native functionality.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class TrustedCdnTest {
    private static final GURL PUBLISHER_URL = new GURL("https://www.publisher.com/");

    @Rule public JniMocker mocker = new JniMocker();

    @Mock TrustedCdn.Natives mTrustedCdnNatives;
    @Mock SecurityStateModel.Natives mSecurityStateModelNatives;
    @Mock Tab mTab;
    @Mock WindowAndroid mWindowAndroid;
    @Mock WebContents mWebContents;
    @Mock ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final UserDataHost mTabUserDataHost = new UserDataHost();
    private final UnownedUserDataHost mWindowUserDataHost = new UnownedUserDataHost();

    boolean mShouldPackageShowPublisherUrl = true;
    @ConnectionSecurityLevel int mConnectionSecurityLevel = ConnectionSecurityLevel.SECURE;
    CustomTabTrustedCdnPublisherUrlVisibility mCustomTabTrustedCdnPublisherUrlVisibility;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mocker.mock(TrustedCdnJni.TEST_HOOKS, mTrustedCdnNatives);
        mocker.mock(SecurityStateModelJni.TEST_HOOKS, mSecurityStateModelNatives);
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(eq(mWebContents)))
                .thenAnswer((mock) -> mConnectionSecurityLevel);

        when(mTab.getUserDataHost()).thenReturn(mTabUserDataHost);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mWindowUserDataHost);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);

        mCustomTabTrustedCdnPublisherUrlVisibility =
                new CustomTabTrustedCdnPublisherUrlVisibility(
                        mWindowAndroid, mLifecycleDispatcher, () -> mShouldPackageShowPublisherUrl);
        doReturn(PUBLISHER_URL)
                .when(mTrustedCdnNatives)
                .getPublisherUrl(ArgumentMatchers.anyLong());
        TrustedCdn.initForTesting(mTab);
    }

    @Test
    public void publisherUrlIsUsed() {
        assertEquals(PUBLISHER_URL, TrustedCdn.getPublisherUrl(mTab));
    }

    @Test
    public void publisherUrlIsNotUsed_untrustedClient() {
        mShouldPackageShowPublisherUrl = false;
        assertNull(TrustedCdn.getPublisherUrl(mTab));
    }

    @Test
    public void publisherUrlIsNotUsed_dangerousSecurityLevel() {
        mConnectionSecurityLevel = ConnectionSecurityLevel.DANGEROUS;
        assertNull(TrustedCdn.getPublisherUrl(mTab));
    }
}
