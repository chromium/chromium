// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;

/** Unit tests for {@link AccessibilityTabHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AccessibilityTabHelperTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private WebContentsImpl mWebContents;
    @Mock private WebContentsAccessibilityImpl mWebContentsAccessibility;

    private UserDataHost mUserDataHost;
    private AccessibilityTabHelper mHelper;

    @Before
    public void setUp() {
        mUserDataHost = new UserDataHost();
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.isCustomTab()).thenReturn(false);

        // Stub WebContentsAccessibility lookup
        when(mWebContents.getOrSetUserData(eq(WebContentsAccessibilityImpl.class), any()))
                .thenReturn(mWebContentsAccessibility);
    }

    @Test
    public void testLazyInitialization() {
        // Clear UserDataHost initially
        mUserDataHost = new UserDataHost();
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);

        // Calling from(Tab) with active WebContents should create the helper
        mHelper = AccessibilityTabHelper.from(mTab);
        assertNotNull(mHelper);

        // Verify it registers accessibility focus preferences on page load
        verify(mWebContentsAccessibility).setShouldFocusOnPageLoad(true);
    }

    @Test
    public void testEarlyReturnOnNullWebContents() {
        // Simulating no WebContents yet (tab uninitialized or early tab setup)
        when(mTab.getWebContents()).thenReturn(null);

        // from(Tab) should return null and NOT create anything
        mHelper = AccessibilityTabHelper.from(mTab);
        assertNull(mHelper);
        assertNull(mUserDataHost.getUserData(AccessibilityTabHelper.USER_DATA_KEY));
    }

    @Test
    public void testCleanupOnFreeze() {
        // Setup active helper first
        mHelper = AccessibilityTabHelper.from(mTab);
        assertNotNull(mHelper);
        Mockito.reset(mWebContentsAccessibility);

        // Simulate tab freeze (WebContents detached)
        when(mTab.getWebContents()).thenReturn(null);
        mHelper.onContentChanged(mTab);

        // Accessibility configurations should no longer be pushed
        verify(mWebContentsAccessibility, never()).setShouldFocusOnPageLoad(any(Boolean.class));
    }

    @Test
    public void testRestoreOnUnfreeze() {
        // Setup active helper first
        mHelper = AccessibilityTabHelper.from(mTab);
        assertNotNull(mHelper);

        // Simulate tab freeze
        when(mTab.getWebContents()).thenReturn(null);
        mHelper.onContentChanged(mTab);

        // Simulate tab restore (new WebContents attached)
        WebContentsImpl newWebContents = mock(WebContentsImpl.class);
        WebContentsAccessibilityImpl newWebContentsAccessibility =
                mock(WebContentsAccessibilityImpl.class);
        when(newWebContents.getOrSetUserData(eq(WebContentsAccessibilityImpl.class), any()))
                .thenReturn(newWebContentsAccessibility);
        when(mTab.getWebContents()).thenReturn(newWebContents);

        mHelper.onContentChanged(mTab);

        // Verify it successfully registered on the new WebContents
        verify(newWebContentsAccessibility).setShouldFocusOnPageLoad(true);
    }

    @Test
    public void testActivityAttachmentTriggersUpdate() {
        mHelper = AccessibilityTabHelper.from(mTab);
        assertNotNull(mHelper);
        Mockito.reset(mWebContentsAccessibility);

        // Simulate activity attachment changed
        mHelper.onActivityAttachmentChanged(mTab, null);

        // Verify it refreshed the accessibility configuration
        verify(mWebContentsAccessibility).setShouldFocusOnPageLoad(true);
    }
}
