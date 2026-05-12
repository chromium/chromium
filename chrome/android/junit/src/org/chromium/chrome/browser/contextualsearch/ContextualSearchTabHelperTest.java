// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Tests for {@link ContextualSearchTabHelper} dynamic TabObserver memory optimizations. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContextualSearchTabHelperTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ContextualSearchTabHelper.Natives mTabHelperJniMock;
    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private SelectionPopupControllerImpl mSelectionPopupController;

    private final UserDataHost mUserDataHost = new UserDataHost();
    private static final long NATIVE_PTR = 12345;
    private ContextualSearchTabHelper mHelper;

    @Before
    public void setUp() {
        ContextualSearchTabHelperJni.setInstanceForTesting(mTabHelperJniMock);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        when(mTabHelperJniMock.init(any())).thenReturn(NATIVE_PTR);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getOrSetUserData(eq(SelectionPopupControllerImpl.class), any()))
                .thenReturn(mSelectionPopupController);
    }

    @After
    public void tearDown() throws Exception {
        java.lang.reflect.Field field =
                ContextualSearchTabHelper.class.getDeclaredField("sNativeHelperMap");
        field.setAccessible(true);
        java.util.Map<?, ?> map = (java.util.Map<?, ?>) field.get(null);
        map.clear();
    }

    @Test
    public void testLazyInitializationWhenWebContentsPresent() {
        mHelper = ContextualSearchTabHelper.from(mTab);
        assertNotNull(mHelper);

        // Simulate content changed event triggered by Tab
        mHelper.onContentChanged(mTab);

        // JNI init should be called once WebContents is attached
        verify(mTabHelperJniMock, times(1)).init(mProfile);
    }

    @Test
    public void testEarlyReturnWhenNoWebContents() {
        // Simulate frozen tab (no WebContents)
        when(mTab.getWebContents()).thenReturn(null);

        mHelper = ContextualSearchTabHelper.from(mTab);

        // Helper should not be created and JNI should not be called
        assertNull(mHelper);
        verify(mTabHelperJniMock, never()).init(any());
    }

    @Test
    public void testCleanupOnDestroy() {
        // 1. Setup active foreground tab helper
        mHelper = ContextualSearchTabHelper.from(mTab);
        assertNotNull(mHelper);

        // Trigger JNI init
        mHelper.onContentChanged(mTab);
        verify(mTabHelperJniMock, times(1)).init(mProfile);

        // 2. Destroy the helper via UserDataHost integration
        mHelper.destroy();

        // 3. Verify JNI and observers are cleanly released
        verify(mTabHelperJniMock, times(1)).destroy(NATIVE_PTR);
        verify(mTab).removeObserver(mHelper);
    }

    @Test
    public void testDynamicRecreationOnTabRestore() {
        // 1. Start as frozen tab (no WebContents)
        when(mTab.getWebContents()).thenReturn(null);
        mHelper = ContextualSearchTabHelper.from(mTab);
        assertNull(mHelper);

        // 2. Simulate tab unfreeze (new WebContents attached)
        WebContents newWebContents = mock(WebContents.class);
        SelectionPopupControllerImpl newSelectionController =
                mock(SelectionPopupControllerImpl.class);
        when(newWebContents.getOrSetUserData(eq(SelectionPopupControllerImpl.class), any()))
                .thenReturn(newSelectionController);
        when(mTab.getWebContents()).thenReturn(newWebContents);

        // 3. Re-querying from(Tab) should successfully lazy-create and initialize the helper
        mHelper = ContextualSearchTabHelper.from(mTab);
        assertNotNull(mHelper);

        // Trigger JNI setup on the new WebContents
        mHelper.onContentChanged(mTab);
        verify(mTabHelperJniMock, times(1)).init(mProfile);
    }

    private void assertNotNull(Object object) {
        org.junit.Assert.assertNotNull(object);
    }

    private void assertNull(Object object) {
        org.junit.Assert.assertNull(object);
    }
}
