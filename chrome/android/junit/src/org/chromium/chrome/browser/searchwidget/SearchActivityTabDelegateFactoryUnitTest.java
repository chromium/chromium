// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.searchwidget.SearchActivityTabDelegateFactory.WebContentsDelegate;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Trivial test suite ensuring that all of the TabDelegateFactory calls have no unexpected side
 * effects.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SearchActivityTabDelegateFactoryUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock Tab mTab;
    private @Spy SearchActivityTabDelegateFactory mFactory;
    private @Spy WebContentsDelegate mWebContentsDelegate;

    @Before
    public void setUp() {
        // We could rely on just the @Spy above, but we want to be sure about what Factory produces.
        // Removing @Spy above could result in Proguard stripping some of the symbols, making the
        // test produce confusing results.
        mWebContentsDelegate = spy((WebContentsDelegate) mFactory.createWebContentsDelegate(mTab));
        clearInvocations(mFactory);
    }

    @Test
    public void tabWebContentsDelegate_getDisplayMode() {
        assertEquals(DisplayMode.BROWSER, mWebContentsDelegate.getDisplayMode());
        verifyNoMoreInteractions(mTab);
    }

    @Test
    public void tabWebContentsDelegate_shouldResumeRequestsForCreatedWindow() {
        assertFalse(mWebContentsDelegate.shouldResumeRequestsForCreatedWindow());
        verifyNoMoreInteractions(mTab);
    }

    @Test
    public void tabWebContentsDelegate_addNewContents() {
        assertFalse(mWebContentsDelegate.addNewContents(null, null, 0, null, false));
        verifyNoMoreInteractions(mTab);
    }

    @Test
    public void tabWebContentsDelegate_setOverlayMode() {
        mWebContentsDelegate.setOverlayMode(false);
        // Ignore the call itself.
        verify(mWebContentsDelegate).setOverlayMode(false);
        verifyNoMoreInteractions(mFactory, mWebContentsDelegate, mTab);
    }

    @Test
    public void tabWebContentsDelegate_canShowAppBanners() {
        assertFalse(mWebContentsDelegate.canShowAppBanners());
        verifyNoMoreInteractions(mTab);
    }

    @Test
    public void createExternalNavigationHandler() {
        assertNull(mFactory.createExternalNavigationHandler(mTab));
        verifyNoMoreInteractions(mTab);
    }

    @Test
    public void createContextMenuPopulatorFactory() {
        assertNull(mFactory.createContextMenuPopulatorFactory(mTab));
        verifyNoMoreInteractions(mTab);
    }

    @Test
    public void createBrowserControlsVisibilityDelegate() {
        assertNull(mFactory.createBrowserControlsVisibilityDelegate(mTab));
        verifyNoMoreInteractions(mTab);
    }

    @Test
    public void createNativePage() {
        // Let the test crash if any parameters are actually used.
        assertNull(mFactory.createNativePage(null, null, mTab, null));
        verifyNoMoreInteractions(mTab);
    }
}
