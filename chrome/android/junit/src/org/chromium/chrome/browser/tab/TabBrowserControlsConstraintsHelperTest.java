// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.content.res.Resources;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.ObserverList;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link TabBrowserControlsConstraintsHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class TabBrowserControlsConstraintsHelperTest {
    private final UserDataHost mUserDataHost = new UserDataHost();

    @Rule public JniMocker mocker = new JniMocker();

    @Mock Context mContext;
    @Mock Resources mResources;
    @Mock TabImpl mTab;
    @Mock WebContents mWebContents;
    @Mock TabDelegateFactory mDelegateFactory;
    @Mock TabBrowserControlsConstraintsHelper.Natives mJniMock;

    private TabBrowserControlsConstraintsHelper mHelper;
    private TabObserver mRegisteredTabObserver;
    private TestVisibilityDelegate mVisibilityDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(TabBrowserControlsConstraintsHelperJni.TEST_HOOKS, mJniMock);
        Mockito.when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        Mockito.when(mTab.getDelegateFactory()).thenReturn(mDelegateFactory);
        Mockito.when(mTab.getWebContents()).thenReturn(mWebContents);

        ObserverList<TabObserver> observers = new ObserverList<>();
        Mockito.when(mTab.getTabObservers())
                .thenAnswer(invocation -> observers.rewindableIterator());

        mVisibilityDelegate = new TestVisibilityDelegate();
        Mockito.when(mDelegateFactory.createBrowserControlsVisibilityDelegate(Mockito.any()))
                .thenReturn(mVisibilityDelegate);

        // TODO(b/370495692) Remove when we don't need to restrict stable
        // experiment to phones.
        Mockito.when(mTab.getContext()).thenReturn(mContext);
        Mockito.when(mContext.getResources()).thenReturn(mResources);
        Mockito.when(mResources.getInteger(org.chromium.ui.R.integer.min_screen_width_bucket))
                .thenReturn(DeviceFormFactor.SCREEN_BUCKET_TABLET - 1);
    }

    private void initHelper() {
        ArgumentCaptor<TabObserver> observerArg = ArgumentCaptor.forClass(TabObserver.class);
        TabBrowserControlsConstraintsHelper.createForTab(mTab);
        mHelper = TabBrowserControlsConstraintsHelper.get(mTab);
        Mockito.verify(mTab).addObserver(observerArg.capture());
        mRegisteredTabObserver = observerArg.getValue();
    }

    @Test
    public void testUpdateVisibilityDelegate_onInitialized() {
        initHelper();
        Mockito.verify(mDelegateFactory, Mockito.never())
                .createBrowserControlsVisibilityDelegate(mTab);
        mRegisteredTabObserver.onInitialized(mTab, null);
        Mockito.verify(mDelegateFactory, Mockito.times(1))
                .createBrowserControlsVisibilityDelegate(mTab);
        verifyUpdateState(BrowserControlsState.BOTH);

        mVisibilityDelegate.set(BrowserControlsState.SHOWN);
        verifyUpdateState(BrowserControlsState.SHOWN);
    }

    @Test
    public void testUpdateVisibilityDelegate_TabAlreadyInitializedAndAttached() {
        Mockito.when(mTab.isInitialized()).thenReturn(true);
        WindowAndroid window = Mockito.mock(WindowAndroid.class);
        Mockito.when(mWebContents.getTopLevelNativeWindow()).thenReturn(window);
        ChromeActivity activity = Mockito.mock(ChromeActivity.class);
        WeakReference<Context> activityRef = new WeakReference<>(activity);
        Mockito.when(window.getContext()).thenReturn(activityRef);

        initHelper();
        Mockito.verify(mDelegateFactory, Mockito.times(1))
                .createBrowserControlsVisibilityDelegate(mTab);
        verifyUpdateState(BrowserControlsState.BOTH);

        mVisibilityDelegate.set(BrowserControlsState.SHOWN);
        verifyUpdateState(BrowserControlsState.SHOWN);
    }

    @Test
    public void testUpdateVisibilityDelegate_onAttachmentChanged() {
        initHelper();
        Mockito.verify(mDelegateFactory, Mockito.never())
                .createBrowserControlsVisibilityDelegate(mTab);
        mRegisteredTabObserver.onActivityAttachmentChanged(mTab, null);
        Mockito.verify(mDelegateFactory, Mockito.never())
                .createBrowserControlsVisibilityDelegate(mTab);

        WindowAndroid window = Mockito.mock(WindowAndroid.class);
        mRegisteredTabObserver.onActivityAttachmentChanged(mTab, window);
        Mockito.verify(mDelegateFactory, Mockito.times(1))
                .createBrowserControlsVisibilityDelegate(mTab);
        verifyUpdateState(BrowserControlsState.BOTH);

        mVisibilityDelegate.set(BrowserControlsState.SHOWN);
        verifyUpdateState(BrowserControlsState.SHOWN);
    }

    @Test
    public void testUpdateVisibilityDelegate_ChangeDelegates() {
        initHelper();
        Mockito.verify(mDelegateFactory, Mockito.never())
                .createBrowserControlsVisibilityDelegate(mTab);
        mRegisteredTabObserver.onInitialized(mTab, null);
        Mockito.verify(mDelegateFactory).createBrowserControlsVisibilityDelegate(mTab);
        Mockito.verifyNoMoreInteractions(mDelegateFactory);
        verifyUpdateState(BrowserControlsState.BOTH);

        mVisibilityDelegate.set(BrowserControlsState.HIDDEN);
        verifyUpdateState(BrowserControlsState.HIDDEN, false);

        TabDelegateFactory newDelegateFactory = Mockito.mock(TabDelegateFactory.class);
        TestVisibilityDelegate newVisibilityDelegate = new TestVisibilityDelegate();
        Mockito.when(mTab.getDelegateFactory()).thenReturn(newDelegateFactory);
        Mockito.when(newDelegateFactory.createBrowserControlsVisibilityDelegate(Mockito.any()))
                .thenReturn(newVisibilityDelegate);

        WindowAndroid window = Mockito.mock(WindowAndroid.class);
        mRegisteredTabObserver.onActivityAttachmentChanged(mTab, window);
        Mockito.verify(newDelegateFactory).createBrowserControlsVisibilityDelegate(mTab);

        verifyUpdateState(BrowserControlsState.BOTH);

        mVisibilityDelegate.set(BrowserControlsState.SHOWN);
        Mockito.verify(mJniMock, Mockito.never())
                .updateState(
                        Mockito.anyLong(),
                        Mockito.any(),
                        Mockito.any(),
                        Mockito.anyInt(),
                        Mockito.anyInt(),
                        Mockito.anyBoolean(),
                        Mockito.any());
    }

    @Test
    public void testWebContentsSwap() {
        initHelper();
        Mockito.verify(mDelegateFactory, Mockito.never())
                .createBrowserControlsVisibilityDelegate(mTab);
        mRegisteredTabObserver.onInitialized(mTab, null);
        Mockito.verify(mDelegateFactory).createBrowserControlsVisibilityDelegate(mTab);
        Mockito.verifyNoMoreInteractions(mDelegateFactory);
        verifyUpdateState(BrowserControlsState.BOTH);

        mRegisteredTabObserver.onWebContentsSwapped(mTab, true, true);
        verifyUpdateState(BrowserControlsState.BOTH);
    }

    @Test
    public void testWebContentsSwap_whenShown() {
        initHelper();
        Mockito.verify(mDelegateFactory, Mockito.never())
                .createBrowserControlsVisibilityDelegate(mTab);
        mRegisteredTabObserver.onInitialized(mTab, null);
        Mockito.verify(mDelegateFactory).createBrowserControlsVisibilityDelegate(mTab);
        Mockito.verifyNoMoreInteractions(mDelegateFactory);
        verifyUpdateState(BrowserControlsState.BOTH);

        mVisibilityDelegate.set(BrowserControlsState.SHOWN);
        verifyUpdateState(BrowserControlsState.SHOWN);

        mRegisteredTabObserver.onWebContentsSwapped(mTab, true, true);
        verifyUpdateState(BrowserControlsState.SHOWN, BrowserControlsState.SHOWN, false);
    }

    private void verifyUpdateState(@BrowserControlsState int constraints) {
        verifyUpdateState(constraints, BrowserControlsState.BOTH, true);
    }

    private void verifyUpdateState(@BrowserControlsState int constraints, boolean animate) {
        verifyUpdateState(constraints, BrowserControlsState.BOTH, animate);
    }

    private void verifyUpdateState(
            @BrowserControlsState int constraints,
            @BrowserControlsState int current,
            boolean animate) {
        Mockito.verify(mJniMock)
                .updateState(
                        Mockito.anyLong(),
                        Mockito.same(mHelper),
                        Mockito.same(mWebContents),
                        Mockito.eq(constraints),
                        Mockito.eq(current),
                        Mockito.eq(animate),
                        Mockito.any());
        Mockito.clearInvocations(mJniMock);
    }

    private static class TestVisibilityDelegate extends BrowserControlsVisibilityDelegate {
        public TestVisibilityDelegate() {
            super(BrowserControlsState.BOTH);
        }
    }
}
