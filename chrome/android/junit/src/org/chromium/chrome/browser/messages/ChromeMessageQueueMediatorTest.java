// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doCallRealMethod;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Handler;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;

import java.util.concurrent.TimeoutException;

/**
 * Unit tests for {@link ChromeMessageQueueMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class ChromeMessageQueueMediatorTest {
    private static final int EXPECTED_TOKEN = 42;

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private BrowserControlsManager mBrowserControlsManager;

    @Mock
    private MessageContainerCoordinator mMessageContainerCoordinator;

    @Mock
    private LayoutStateProvider mLayoutStateProvider;

    @Mock
    private ManagedMessageDispatcher mMessageDispatcher;

    @Mock
    private ModalDialogManager mModalDialogManager;

    @Mock
    private ActivityTabProvider mActivityTabProvider;

    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    @Mock
    private Handler mQueueHandler;

    private ChromeMessageQueueMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMessageDispatcher.suspend()).thenReturn(EXPECTED_TOKEN);
    }

    private void initMediator() {
        OneshotSupplierImpl<LayoutStateProvider> layoutStateProviderOneShotSupplier =
                new OneshotSupplierImpl<>();
        ObservableSupplierImpl<ModalDialogManager> modalDialogManagerSupplier =
                new ObservableSupplierImpl<>();
        mMediator = new ChromeMessageQueueMediator(mBrowserControlsManager,
                mMessageContainerCoordinator, mActivityTabProvider,
                layoutStateProviderOneShotSupplier, modalDialogManagerSupplier,
                mActivityLifecycleDispatcher, mMessageDispatcher);
        layoutStateProviderOneShotSupplier.set(mLayoutStateProvider);
        modalDialogManagerSupplier.set(mModalDialogManager);
        mMediator.setQueueHandlerForTesting(mQueueHandler);
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
     * Test the queue can be suspended and resumed correctly when app is paused and resumed.
     */
    @Test
    public void testActivityStateChange() {
        final ArgumentCaptor<PauseResumeWithNativeObserver> observer =
                ArgumentCaptor.forClass(PauseResumeWithNativeObserver.class);
        doNothing().when(mActivityLifecycleDispatcher).register(observer.capture());
        initMediator();
        observer.getValue().onPauseWithNative();
        verify(mMessageDispatcher).suspend();
        observer.getValue().onResumeWithNative();
        verify(mMessageDispatcher).resume(EXPECTED_TOKEN);
    }

    /**
     * Test the runnable by #onStartShow is reset correctly.
     */
    @Test
    @Features.EnableFeatures({ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES})
    public void testResetOnStartShowRunnable() {
        when(mBrowserControlsManager.getBrowserControlHiddenRatio()).thenReturn(0.5f);
        OneshotSupplierImpl<LayoutStateProvider> layoutStateProviderOneShotSupplier =
                new OneshotSupplierImpl<>();
        ObservableSupplierImpl<ModalDialogManager> modalDialogManagerSupplier =
                new ObservableSupplierImpl<>();
        final ArgumentCaptor<ChromeMessageQueueMediator.BrowserControlsObserver>
                observerArgumentCaptor = ArgumentCaptor.forClass(
                        ChromeMessageQueueMediator.BrowserControlsObserver.class);
        doNothing().when(mBrowserControlsManager).addObserver(observerArgumentCaptor.capture());
        when(mBrowserControlsManager.getBrowserVisibilityDelegate())
                .thenReturn(new BrowserStateBrowserControlsVisibilityDelegate(
                        new ObservableSupplier<Boolean>() {
                            @Override
                            public Boolean addObserver(Callback<Boolean> obs) {
                                return null;
                            }

                            @Override
                            public void removeObserver(Callback<Boolean> obs) {}

                            @Override
                            public Boolean get() {
                                return false;
                            }
                        }));
        mMediator = new ChromeMessageQueueMediator(mBrowserControlsManager,
                mMessageContainerCoordinator, mActivityTabProvider,
                layoutStateProviderOneShotSupplier, modalDialogManagerSupplier,
                mActivityLifecycleDispatcher, mMessageDispatcher);
        ChromeMessageQueueMediator.BrowserControlsObserver observer =
                observerArgumentCaptor.getValue();
        Assert.assertFalse(mMediator.isReadyForShowing());
        Runnable runnable = () -> {};
        mMediator.onRequestShowing(runnable);
        Assert.assertNotNull(observer.getRunnableForTesting());
        Assert.assertFalse(mMediator.isReadyForShowing());
        Assert.assertTrue(mMediator.isPendingShow());

        mMediator.onFinishHiding();
        Assert.assertNull("Callback should be reset to null after hiding is finished",
                observer.getRunnableForTesting());
        Assert.assertFalse(mMediator.isReadyForShowing());
    }

    /**
     * Test whether #IsReadyForShowing returns correct value.
     */
    @Test
    @Features.EnableFeatures({ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES})
    public void testIsReadyForShowing() {
        final ArgumentCaptor<ChromeMessageQueueMediator.BrowserControlsObserver>
                observerArgumentCaptor = ArgumentCaptor.forClass(
                        ChromeMessageQueueMediator.BrowserControlsObserver.class);
        doNothing().when(mBrowserControlsManager).addObserver(observerArgumentCaptor.capture());
        var visibilitySupplier = new ObservableSupplierImpl<Boolean>();
        visibilitySupplier.set(false);
        var visibilityDelegate =
                new BrowserStateBrowserControlsVisibilityDelegate(visibilitySupplier);
        when(mBrowserControlsManager.getBrowserVisibilityDelegate()).thenReturn(visibilityDelegate);
        initMediator();
        Assert.assertFalse(mMediator.isReadyForShowing());
        visibilitySupplier.set(true);
        when(mBrowserControlsManager.getBrowserControlHiddenRatio()).thenReturn(0f);

        mMediator.onRequestShowing(() -> {});
        Assert.assertTrue(mMediator.isReadyForShowing());

        mMediator.onFinishHiding();
        Assert.assertFalse(mMediator.isReadyForShowing());
    }

    /**
     * Test multiple show requests can be made when tab browser controls state changes while browser
     * controls is not fully visible.
     * 1. Initially, tab constraints state is hidden but browser controls is not fully visible yet.
     * 2. A message is allowed to be displayed.
     * 3. Tab constraints is assumed to change from the hidden state while the first message is on
     * the screen.
     * 4. If a second message is enqueued but browser controls is still not ready, it will trigger
     * #onRequestShowing again.
     */
    @Test
    public void testRequestMultipleTimesWhenTabConstraintsChanges() throws TimeoutException {
        final ArgumentCaptor<ChromeMessageQueueMediator.BrowserControlsObserver>
                observerArgumentCaptor = ArgumentCaptor.forClass(
                        ChromeMessageQueueMediator.BrowserControlsObserver.class);
        doNothing().when(mBrowserControlsManager).addObserver(observerArgumentCaptor.capture());
        var visibilitySupplier = new ObservableSupplierImpl<Boolean>();
        visibilitySupplier.set(false);

        // Simulate the browser controls to not be fully visible.
        when(mBrowserControlsManager.getBrowserControlHiddenRatio()).thenReturn(0.5f);
        var visibilityDelegate =
                new BrowserStateBrowserControlsVisibilityDelegate(visibilitySupplier);
        when(mBrowserControlsManager.getBrowserVisibilityDelegate()).thenReturn(visibilityDelegate);
        initMediator();
        var mediator = Mockito.spy(mMediator);

        // #areBrowserControlsReady would return true when the tab browser controls constraint state
        // is hidden.
        doReturn(true).when(mediator).areBrowserControlsReady();
        Assert.assertFalse(mediator.isReadyForShowing());
        CallbackHelper callbackHelper = new CallbackHelper();
        mediator.onRequestShowing(callbackHelper::notifyCalled);
        callbackHelper.waitForFirst();
        ChromeMessageQueueMediator.BrowserControlsObserver observer =
                observerArgumentCaptor.getValue();
        Assert.assertFalse(observer.isRequesting());

        // Real method invocation will return false when the tab browser controls constraint state
        // changes from the hidden state.
        doCallRealMethod().when(mediator).areBrowserControlsReady();
        Assert.assertFalse(
                BrowserControlsUtils.areBrowserControlsFullyVisible(mBrowserControlsManager));
        Assert.assertFalse(mediator.areBrowserControlsReady());
        Assert.assertFalse(mediator.isReadyForShowing());
        mediator.onRequestShowing(() -> {});
        Assert.assertTrue(observer.isRequesting());
    }

    /**
     * Test NPE is not thrown when supplier offers a null value.
     */
    @Test
    public void testThrowNothingWhenModalDialogManagerIsNull() {
        OneshotSupplierImpl<LayoutStateProvider> layoutStateProviderOneShotSupplier =
                new OneshotSupplierImpl<>();
        ObservableSupplierImpl<ModalDialogManager> modalDialogManagerSupplier =
                new ObservableSupplierImpl<>();
        mMediator = new ChromeMessageQueueMediator(mBrowserControlsManager,
                mMessageContainerCoordinator, mActivityTabProvider,
                layoutStateProviderOneShotSupplier, modalDialogManagerSupplier,
                mActivityLifecycleDispatcher, mMessageDispatcher);
        layoutStateProviderOneShotSupplier.set(mLayoutStateProvider);
        // To offer a null value, we have to offer a value other than null first.
        modalDialogManagerSupplier.set(mModalDialogManager);
        modalDialogManagerSupplier.set(null);
    }

    /**
     * Test NPE is not thrown after destroy.
     */
    @Test
    public void testThrowNothingAfterDestroy() {
        OneshotSupplierImpl<LayoutStateProvider> layoutStateProviderOneShotSupplier =
                new OneshotSupplierImpl<>();
        ObservableSupplierImpl<ModalDialogManager> modalDialogManagerSupplier =
                new ObservableSupplierImpl<>();
        mMediator = new ChromeMessageQueueMediator(mBrowserControlsManager,
                mMessageContainerCoordinator, mActivityTabProvider,
                layoutStateProviderOneShotSupplier, modalDialogManagerSupplier,
                mActivityLifecycleDispatcher, mMessageDispatcher);
        layoutStateProviderOneShotSupplier.set(mLayoutStateProvider);
        modalDialogManagerSupplier.set(mModalDialogManager);
        mMediator.onAnimationStart();
        mMediator.onAnimationEnd();
        verify(mMessageContainerCoordinator, times(1)).onAnimationEnd();
        mMediator.destroy();
        mMediator.onAnimationEnd();
        verify(mMessageContainerCoordinator, times(1)).onAnimationEnd();
    }
    /**
     * Test the queue can be suspended and resumed correctly on omnibox focus events.
     */
    @Test
    public void testUrlFocusChange() {
        initMediator();
        // Omnibox is focused.
        mMediator.onUrlFocusChange(true);
        verify(mMessageDispatcher).suspend();
        verify(mQueueHandler).removeCallbacksAndMessages(null);
        // Omnibox is out of focus.
        mMediator.onUrlFocusChange(false);
        ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);
        // Verify that the queue is resumed 1s after the omnibox loses focus.
        verify(mQueueHandler).postDelayed(captor.capture(), eq(1000L));
        captor.getValue().run();
        verify(mMessageDispatcher).resume(EXPECTED_TOKEN);
        Assert.assertEquals("mUrlFocusToken should be invalidated.", TokenHolder.INVALID_TOKEN,
                mMediator.getUrlFocusTokenForTesting());
    }
}
