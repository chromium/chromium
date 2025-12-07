// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.ObserverList;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.cc.input.BrowserControlsOffsetTagModifications;
import org.chromium.cc.input.BrowserControlsOffsetTags;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsOffsetTagsInfo;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link TabBrowserControlsConstraintsHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class TabBrowserControlsConstraintsHelperTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private final UserDataHost mUserDataHost = new UserDataHost();

    @Mock TabImpl mTab;
    @Mock WebContents mWebContents;
    @Mock TabDelegateFactory mDelegateFactory;
    @Mock TabBrowserControlsConstraintsHelper.Natives mJniMock;
    @Mock TabObserver mTabObserver;

    private TabBrowserControlsConstraintsHelper mHelper;
    private TabObserver mRegisteredTabObserver;
    private TestVisibilityDelegate mVisibilityDelegate;

    @Before
    public void setUp() {
        TabBrowserControlsConstraintsHelperJni.setInstanceForTesting(mJniMock);
        Mockito.when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        Mockito.when(mTab.getDelegateFactory()).thenReturn(mDelegateFactory);
        Mockito.when(mTab.getWebContents()).thenReturn(mWebContents);

        ObserverList<TabObserver> observers = new ObserverList<>();
        observers.addObserver(mTabObserver);
        Mockito.when(mTab.getTabObservers())
                .thenAnswer(invocation -> observers.rewindableIterator());

        mVisibilityDelegate = new TestVisibilityDelegate();
        Mockito.when(mDelegateFactory.createBrowserControlsVisibilityDelegate(Mockito.any()))
                .thenReturn(mVisibilityDelegate);
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
                        Mockito.anyInt(),
                        Mockito.anyInt(),
                        Mockito.anyBoolean(),
                        Mockito.any());
    }

    @Test
    public void testUpdateOffsetTag_visibilityConstraintsChanged() {
        initHelper();
        ArgumentCaptor<BrowserControlsOffsetTagsInfo> tagsInfoArg =
                ArgumentCaptor.forClass(BrowserControlsOffsetTagsInfo.class);
        ArgumentCaptor<BrowserControlsOffsetTagModifications> tagModificationsArg =
                ArgumentCaptor.forClass(BrowserControlsOffsetTagModifications.class);
        mRegisteredTabObserver.onInitialized(mTab, null);

        // During init, delegate gets set with BOTH, check that we create and propagate offset tags.
        Mockito.verify(mTabObserver)
                .onOffsetTagsInfoChanged(
                        Mockito.any(), Mockito.any(), tagsInfoArg.capture(), Mockito.eq(3));
        assertOffsetTagsNotNull(tagsInfoArg.getValue().getTags());
        verifyUpdateState(BrowserControlsState.BOTH, tagModificationsArg);
        assertOffsetTagsNotNull(tagModificationsArg.getValue().getTags());

        // When visibility is forced, we should have null tags.
        mVisibilityDelegate.set(BrowserControlsState.SHOWN);
        Mockito.verify(mTabObserver)
                .onOffsetTagsInfoChanged(
                        Mockito.any(), Mockito.any(), tagsInfoArg.capture(), Mockito.eq(1));
        assertOffsetTagsNull(tagsInfoArg.getValue().getTags());
        verifyUpdateState(BrowserControlsState.SHOWN, tagModificationsArg);
        assertOffsetTagsNull(tagModificationsArg.getValue().getTags());

        // Back to non forced state, check that we create and propagate tags again.
        mVisibilityDelegate.set(BrowserControlsState.BOTH);
        Mockito.verify(mTabObserver, Mockito.times(2))
                .onOffsetTagsInfoChanged(
                        Mockito.any(), Mockito.any(), tagsInfoArg.capture(), Mockito.eq(3));
        assertOffsetTagsNotNull(tagsInfoArg.getValue().getTags());
        verifyUpdateState(BrowserControlsState.BOTH, tagModificationsArg);
        assertOffsetTagsNotNull(tagModificationsArg.getValue().getTags());
    }

    @Test
    public void testUpdateOffsetTag_onTabShownAndHidden() {
        initHelper();
        ArgumentCaptor<BrowserControlsOffsetTagsInfo> tagsInfoArg =
                ArgumentCaptor.forClass(BrowserControlsOffsetTagsInfo.class);
        ArgumentCaptor<BrowserControlsOffsetTagModifications> tagModificationsArg =
                ArgumentCaptor.forClass(BrowserControlsOffsetTagModifications.class);
        mRegisteredTabObserver.onInitialized(mTab, null);
        Mockito.verify(mTabObserver)
                .onOffsetTagsInfoChanged(
                        Mockito.any(), Mockito.any(), tagsInfoArg.capture(), Mockito.eq(3));
        assertOffsetTagsNotNull(tagsInfoArg.getValue().getTags());
        verifyUpdateState(BrowserControlsState.BOTH, tagModificationsArg);
        assertOffsetTagsNotNull(tagModificationsArg.getValue().getTags());

        // Unregister tags when tab is hidden.
        mRegisteredTabObserver.onHidden(mTab, TabHidingType.CHANGED_TABS);
        Mockito.verify(mTabObserver, Mockito.times(2))
                .onOffsetTagsInfoChanged(
                        Mockito.any(), Mockito.any(), tagsInfoArg.capture(), Mockito.anyInt());
        assertOffsetTagsNull(tagsInfoArg.getValue().getTags());

        // Visibility is not forced, register tags again when tab is shown.
        mRegisteredTabObserver.onShown(mTab, TabHidingType.CHANGED_TABS);
        Mockito.verify(mTabObserver, Mockito.times(3))
                .onOffsetTagsInfoChanged(
                        Mockito.any(), Mockito.any(), tagsInfoArg.capture(), Mockito.anyInt());
        assertOffsetTagsNotNull(tagsInfoArg.getValue().getTags());
    }

    private void assertOffsetTagsNull(BrowserControlsOffsetTags tags) {
        Assert.assertNull(tags.getTopControlsOffsetTag());
        Assert.assertNull(tags.getContentOffsetTag());
        Assert.assertNull(tags.getBottomControlsOffsetTag());
    }

    private void assertOffsetTagsNotNull(BrowserControlsOffsetTags tags) {
        Assert.assertNotNull(tags.getTopControlsOffsetTag());
        Assert.assertNotNull(tags.getContentOffsetTag());
        Assert.assertNotNull(tags.getBottomControlsOffsetTag());
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
                        Mockito.same(mWebContents),
                        Mockito.eq(constraints),
                        Mockito.eq(current),
                        Mockito.eq(animate),
                        Mockito.any());
        Mockito.clearInvocations(mJniMock);
    }

    private void verifyUpdateState(
            @BrowserControlsState int constraints,
            ArgumentCaptor<BrowserControlsOffsetTagModifications> captor) {
        Mockito.verify(mJniMock)
                .updateState(
                        Mockito.anyLong(),
                        Mockito.same(mWebContents),
                        Mockito.eq(constraints),
                        Mockito.anyInt(),
                        Mockito.anyBoolean(),
                        captor.capture());
        Mockito.clearInvocations(mJniMock);
    }

    private static class TestVisibilityDelegate extends BrowserControlsVisibilityDelegate {
        public TestVisibilityDelegate() {
            super(BrowserControlsState.BOTH);
        }
    }
}
