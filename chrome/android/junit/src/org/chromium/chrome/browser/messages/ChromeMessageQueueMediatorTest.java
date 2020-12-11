// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.messages.ManagedMessageDispatcher;

/**
 * Unit tests for {@link ChromeMessageQueueMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeMessageQueueMediatorTest {
    private static final int EXPECTED_TOKEN = 42;

    @Mock
    private BrowserControlsManager mBrowserControlsManager;

    @Mock
    private MessageContainerCoordinator mMessageContainerCoordinator;

    @Mock
    private FullscreenManager mFullscreenManager;

    @Mock
    private LayoutStateProvider mLayoutStateProvider;

    @Mock
    private TabModelSelector mTabModelSelector;

    @Mock
    private ManagedMessageDispatcher mMessageDispatcher;

    @Mock
    private TabModelFilterProvider mTabModelFilterProvider;

    private ChromeMessageQueueMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        doNothing().when(mTabModelFilterProvider).addTabModelFilterObserver(any());
        when(mMessageDispatcher.suspend()).thenReturn(EXPECTED_TOKEN);
    }

    private void initMediator() {
        OneshotSupplierImpl<LayoutStateProvider> layoutStateProviderOneShotSupplier =
                new OneshotSupplierImpl<>();
        ObservableSupplierImpl<TabModelSelector> tabModelSelectorSupplier =
                new ObservableSupplierImpl<>();
        mMediator = new ChromeMessageQueueMediator(mBrowserControlsManager,
                mMessageContainerCoordinator, mFullscreenManager,
                layoutStateProviderOneShotSupplier, tabModelSelectorSupplier, mMessageDispatcher);
        layoutStateProviderOneShotSupplier.set(mLayoutStateProvider);
        tabModelSelectorSupplier.set(mTabModelSelector);
    }

    /**
     * Test the queue can be suspended and resumed correctly when toggling full screen mode.
     */
    @Test
    public void testFullScreenModeChange() {
        final ArgumentCaptor<FullscreenManager.Observer> observer =
                ArgumentCaptor.forClass(FullscreenManager.Observer.class);
        doNothing().when(mFullscreenManager).addObserver(observer.capture());
        initMediator();
        observer.getValue().onEnterFullscreen(null, null);
        observer.getValue().onExitFullscreen(null);
        verify(mMessageDispatcher).resume(EXPECTED_TOKEN);
    }

    /**
     * Test the queue can be suspended and resumed correctly when toggling layout state change.
     */
    @Test
    public void testLayoutStateChange() {
        final ArgumentCaptor<LayoutStateObserver> observer =
                ArgumentCaptor.forClass(LayoutStateObserver.class);
        doNothing().when(mLayoutStateProvider).addObserver(observer.capture());
        initMediator();
        observer.getValue().onStartedShowing(LayoutType.TAB_SWITCHER, false);
        observer.getValue().onFinishedShowing(LayoutType.BROWSING);
        verify(mMessageDispatcher).resume(EXPECTED_TOKEN);
    }

    /**
     * Test the queue can be cleared when tab changes.
     * TODO(crbug.com/1123947): Clean this after message scope is implemented.
     */
    @Test
    public void testDismissAllMessages() {
        final ArgumentCaptor<TabModelObserver> observer =
                ArgumentCaptor.forClass(TabModelObserver.class);
        doNothing().when(mTabModelFilterProvider).addTabModelFilterObserver(observer.capture());
        initMediator();
        observer.getValue().didSelectTab(null, TabSelectionType.FROM_NEW, 1);
        verify(mMessageDispatcher).dismissAllMessages();
    }
}
