// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabCreatorManager;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link TabModelSelectorImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
public class TabModelSelectorImplTest {
    // Test activity type that does not restore tab on cold restart.
    // Any type other than ActivityType.TABBED works.
    private static final @ActivityType int NO_RESTORE_TYPE = ActivityType.CUSTOM_TAB;

    @Mock TabModelFilterFactory mMockTabModelFilterFactory;
    @Mock TabContentManager mMockTabContentManager;
    @Mock TabDelegateFactory mTabDelegateFactory;
    @Mock NextTabPolicySupplier mNextTabPolicySupplier;

    @Mock
    IncognitoTabModelObserver.IncognitoReauthDialogDelegate mIncognitoReauthDialogDelegateMock;

    @Mock TabModelSelectorObserver mTabModelSelectorObserverMock;
    @Mock Profile mProfile;
    @Mock Profile mIncognitoProfile;

    private TabModelSelectorImpl mTabModelSelector;
    private MockTabCreatorManager mTabCreatorManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn(true).when(mIncognitoProfile).isOffTheRecord();
        doReturn(mock(TabModelFilter.class))
                .when(mMockTabModelFilterFactory)
                .createTabModelFilter(any());
        mTabCreatorManager = new MockTabCreatorManager();

        AsyncTabParamsManager realAsyncTabParamsManager =
                AsyncTabParamsManagerFactory.createAsyncTabParamsManager();
        mTabModelSelector =
                new TabModelSelectorImpl(
                        null,
                        mTabCreatorManager,
                        mMockTabModelFilterFactory,
                        mNextTabPolicySupplier,
                        realAsyncTabParamsManager,
                        /* supportUndo= */ false,
                        NO_RESTORE_TYPE,
                        /* startIncognito= */ false);
        mTabCreatorManager.initialize(mTabModelSelector);
        mTabModelSelector.onNativeLibraryReadyInternal(
                mMockTabContentManager,
                new MockTabModel(mProfile, null),
                new MockTabModel(mIncognitoProfile, null));
    }

    @Test
    public void testTabActivityAttachmentChanged_detaching() {
        MockTab tab = new MockTab(1, mProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        tab.updateAttachment(null, null);

        Assert.assertEquals(
                "detaching a tab should result in it being removed from the model",
                0,
                mTabModelSelector.getModel(false).getCount());
    }

    @Test
    public void testTabActivityAttachmentChanged_movingWindows() {
        MockTab tab = new MockTab(1, mProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        tab.updateAttachment(Mockito.mock(WindowAndroid.class), mTabDelegateFactory);

        Assert.assertEquals(
                "moving a tab between windows shouldn't remove it from the model",
                1,
                mTabModelSelector.getModel(false).getCount());
    }

    @Test
    public void testTabActivityAttachmentChanged_detachingWhileReparentingInProgress() {
        MockTab tab = new MockTab(1, mProfile);
        mTabModelSelector
                .getModel(false)
                .addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        mTabModelSelector.enterReparentingMode();
        tab.updateAttachment(null, null);

        Assert.assertEquals(
                "tab shouldn't be removed while reparenting is in progress",
                1,
                mTabModelSelector.getModel(false).getCount());
    }

    /**
     * A test method to verify that {@link
     * IncognitoReauthDialogDelegate#OnBeforeIncognitoTabModelSelected} gets called before any other
     * {@link TabModelSelectorObserver} listening to {@link
     * TabModelSelectorObserver#onTabModelSelected}.
     */
    @Test
    @SmallTest
    public void
            testIncognitoReauthDialogDelegate_OnBeforeIncognitoTabModelSelected_called_Before() {
        doNothing().when(mIncognitoReauthDialogDelegateMock).onBeforeIncognitoTabModelSelected();
        doNothing().when(mTabModelSelectorObserverMock).onTabModelSelected(any(), any());
        mTabModelSelector.setIncognitoReauthDialogDelegate(mIncognitoReauthDialogDelegateMock);
        mTabModelSelector.addObserver(mTabModelSelectorObserverMock);

        InOrder order = inOrder(mIncognitoReauthDialogDelegateMock, mTabModelSelectorObserverMock);
        mTabModelSelector.selectModel(/* incognito= */ true);

        order.verify(mIncognitoReauthDialogDelegateMock, times(1))
                .onBeforeIncognitoTabModelSelected();
        order.verify(mTabModelSelectorObserverMock, times(1)).onTabModelSelected(any(), any());
    }

    /**
     * A test method to verify that {@link
     * IncognitoReauthDialogDelegate#onAfterRegularTabModelChanged} gets called after any other
     * {@link TabModelSelectorObserver} listening to {@link TabModelSelectorObserver#onChange()}.
     */
    @Test
    @SmallTest
    public void testIncognitoReauthDialogDelegate_onAfterRegularTabModelChanged() {
        // Start-off with an Incognito tab model. This is needed to set up the environment.
        mTabModelSelector.selectModel(/* incognito= */ true);
        // The above calls posts a tasks which can get executed after we add
        // mTabModelSelectorObserverMock below and interfering with the verify onChange test below.
        // Therefore execute that task immediately now.
        ShadowLooper.shadowMainLooper().idle();
        // Add the observers now to prevent any firing from the previous selectModel which is
        // separate from the actual test.
        mTabModelSelector.setIncognitoReauthDialogDelegate(mIncognitoReauthDialogDelegateMock);
        mTabModelSelector.addObserver(mTabModelSelectorObserverMock);

        doNothing().when(mIncognitoReauthDialogDelegateMock).onAfterRegularTabModelChanged();
        doNothing().when(mTabModelSelectorObserverMock).onTabModelSelected(any(), any());
        doNothing().when(mTabModelSelectorObserverMock).onChange();

        InOrder order = inOrder(mTabModelSelectorObserverMock, mIncognitoReauthDialogDelegateMock);
        mTabModelSelector.selectModel(/* incognito= */ false);
        verify(mTabModelSelectorObserverMock, times(1)).onTabModelSelected(any(), any());

        // The onChange method below is posted as a task to the main looper, and therefore we need
        // to wait until it gets executed.
        ShadowLooper.shadowMainLooper().idle();
        order.verify(mTabModelSelectorObserverMock, times(1)).onChange();
        order.verify(mIncognitoReauthDialogDelegateMock, times(1)).onAfterRegularTabModelChanged();
    }
}
