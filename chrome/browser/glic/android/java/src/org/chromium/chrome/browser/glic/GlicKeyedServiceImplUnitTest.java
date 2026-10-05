// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlicInvocationSource;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.url.GURL;

import java.util.List;

/** Unit tests for {@link GlicKeyedServiceImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
public class GlicKeyedServiceImplUnitTest {
    private static final long NATIVE_PTR = 12345L;
    private static final long BROWSER_WINDOW_PTR = 67890L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private GlicKeyedServiceImpl.Natives mNativesMock;
    @Mock private Profile mProfileMock;
    @Mock private Tab mTabMock;
    @Mock private Tracker mTrackerMock;
    @Mock private RenderFrameHost mRenderFrameHostMock;

    private GlicKeyedServiceImpl mService;

    @Before
    public void setUp() {
        GlicKeyedServiceImplJni.setInstanceForTesting(mNativesMock);
        TrackerFactory.setTrackerForTests(mTrackerMock);
        mService = new GlicKeyedServiceImpl(NATIVE_PTR);
    }

    private static void setAdaptiveToolbarButton(@AdaptiveToolbarButtonVariant int variant) {
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS, variant);
    }

    @Test
    public void testGetRecentlyActiveInstances_UnpacksFlattenedPairs() {
        when(mNativesMock.getRecentlyActiveInstances(NATIVE_PTR, 5))
                .thenReturn(List.of("conv_1", "First Chat", "conv_2", "Second Chat", "odd"));

        List<ConversationInfo> instances = mService.getRecentlyActiveInstances(5);

        assertEquals(2, instances.size());
        assertEquals("conv_1", instances.get(0).instanceId);
        assertEquals("First Chat", instances.get(0).title);
        assertEquals("conv_2", instances.get(1).instanceId);
        assertEquals("Second Chat", instances.get(1).title);
    }

    @Test
    public void testIsGlicShortcutActive_AdaptiveToolbarSelection() {
        setAdaptiveToolbarButton(AdaptiveToolbarButtonVariant.GLIC);
        assertTrue(mService.isGlicShortcutActive(mProfileMock));

        setAdaptiveToolbarButton(AdaptiveToolbarButtonVariant.NEW_TAB);
        assertFalse(mService.isGlicShortcutActive(mProfileMock));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsGlicShortcutActive_FalseWhenBottomBarEnabled() {
        setAdaptiveToolbarButton(AdaptiveToolbarButtonVariant.GLIC);
        assertFalse(mService.isGlicShortcutActive(mProfileMock));
        assertTrue(mService.isBottomBarEnabled());
    }

    @Test
    public void testShareTabsAndShareContextImage_NullGuards() {
        List<Tab> tabs = List.of(mTabMock);
        mService.shareTabs(
                tabs,
                /* instanceId= */ null,
                /* newConversation= */ true,
                GlicInvocationSource.UNSUPPORTED);
        verify(mNativesMock)
                .shareTabs(
                        NATIVE_PTR,
                        tabs,
                        /* instanceId= */ "",
                        /* newConversation= */ true,
                        GlicInvocationSource.UNSUPPORTED);

        GURL url = new GURL("https://example.com/image.png");
        mService.shareContextImage(mTabMock, /* renderFrameHost= */ null, url);
        verify(mNativesMock, never()).shareContextImage(anyLong(), any(), any(), any());

        mService.shareContextImage(mTabMock, mRenderFrameHostMock, url);
        verify(mNativesMock).shareContextImage(NATIVE_PTR, mTabMock, mRenderFrameHostMock, url);
    }

    @Test
    public void testOnNativeDestroyed_StopsForwardingCalls() {
        mService.toggleUI(
                BROWSER_WINDOW_PTR,
                /* preventClose= */ false,
                mProfileMock,
                GlicInvocationSource.UNSUPPORTED);
        verify(mTrackerMock).notifyEvent(EventConstants.GLIC_ANDROID_USED);
        verify(mNativesMock)
                .toggleUI(
                        NATIVE_PTR,
                        BROWSER_WINDOW_PTR,
                        /* preventClose= */ false,
                        mProfileMock,
                        GlicInvocationSource.UNSUPPORTED);

        mService.onNativeDestroyed();

        assertFalse(
                mService.invokeWithAutoSubmit(
                        mTabMock, "prompt", GlicInvocationSource.UNSUPPORTED));
        assertFalse(mService.isPanelShowingForBrowser(BROWSER_WINDOW_PTR));
        assertTrue(mService.getRecentlyActiveInstances(5).isEmpty());
        verify(mNativesMock, never()).getRecentlyActiveInstances(anyLong(), anyInt());
    }
}
