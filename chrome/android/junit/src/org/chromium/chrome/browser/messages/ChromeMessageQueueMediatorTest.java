// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

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
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modelutil.PropertyModel;

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
    private ManagedMessageDispatcher mMessageDispatcher;

    @Mock
    private ModalDialogManager mModalDialogManager;

    @Mock
    private ActivityTabProvider mActivityTabProvider;

    private ChromeMessageQueueMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMessageDispatcher.suspend()).thenReturn(EXPECTED_TOKEN);
    }

    private void initMediator() {
        OneshotSupplierImpl<LayoutStateProvider> layoutStateProviderOneShotSupplier =
                new OneshotSupplierImpl<>();
        ObservableSupplierImpl<TabModelSelector> tabModelSelectorSupplier =
                new ObservableSupplierImpl<>();
        ObservableSupplierImpl<ModalDialogManager> modalDialogManagerSupplier =
                new ObservableSupplierImpl<>();
        mMediator = new ChromeMessageQueueMediator(mBrowserControlsManager,
                mMessageContainerCoordinator, mFullscreenManager, mActivityTabProvider,
                layoutStateProviderOneShotSupplier, modalDialogManagerSupplier, mMessageDispatcher);
        layoutStateProviderOneShotSupplier.set(mLayoutStateProvider);
        modalDialogManagerSupplier.set(mModalDialogManager);
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
        verify(mMessageDispatcher).suspend();
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
        verify(mMessageDispatcher).suspend();
        observer.getValue().onFinishedShowing(LayoutType.BROWSING);
        verify(mMessageDispatcher).resume(EXPECTED_TOKEN);
    }

    /**
     * Test the queue can be suspended and resumed correctly when showing/hiding modal dialogs.
     */
    @Test
    public void testModalDialogChange() {
        final ArgumentCaptor<ModalDialogManagerObserver> observer =
                ArgumentCaptor.forClass(ModalDialogManagerObserver.class);
        doNothing().when(mModalDialogManager).addObserver(observer.capture());
        initMediator();
        observer.getValue().onDialogAdded(new PropertyModel());
        verify(mMessageDispatcher).suspend();
        observer.getValue().onLastDialogDismissed();
        verify(mMessageDispatcher).resume(EXPECTED_TOKEN);
    }

    /**
     * Test NPE is not thrown when supplier offers a null value.
     */
    @Test
    public void testThrowNothingWhenModalDialogManagerIsNull() {
        OneshotSupplierImpl<LayoutStateProvider> layoutStateProviderOneShotSupplier =
                new OneshotSupplierImpl<>();
        ObservableSupplierImpl<TabModelSelector> tabModelSelectorSupplier =
                new ObservableSupplierImpl<>();
        ObservableSupplierImpl<ModalDialogManager> modalDialogManagerSupplier =
                new ObservableSupplierImpl<>();
        mMediator = new ChromeMessageQueueMediator(mBrowserControlsManager,
                mMessageContainerCoordinator, mFullscreenManager, mActivityTabProvider,
                layoutStateProviderOneShotSupplier, modalDialogManagerSupplier, mMessageDispatcher);
        layoutStateProviderOneShotSupplier.set(mLayoutStateProvider);
        // To offer a null value, we have to offer a value other than null first.
        modalDialogManagerSupplier.set(mModalDialogManager);
        modalDialogManagerSupplier.set(null);
    }
}
