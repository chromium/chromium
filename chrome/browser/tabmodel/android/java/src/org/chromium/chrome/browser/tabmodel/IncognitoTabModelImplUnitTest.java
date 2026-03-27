// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelImpl.IncognitoTabModelDelegate;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;

import java.util.function.Supplier;

/** Unit tests for {@link IncognitoTabModelImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class IncognitoTabModelImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private IncognitoTabModelDelegate mIncognitoTabModelDelegate;
    @Mock private Profile mProfile;
    @Mock private IncognitoTabModelObserver mIncognitoTabModelObserver;
    @Mock private TabGroupModelFilterObserver mMockTabGroupModelFilterObserver;
    @Mock private TabCreator mTabCreator;

    private IncognitoTabModelImpl mIncognitoTabModel;
    private MockTabModel mMockTabModel;
    private Supplier<TabModelInternal> mTabModelSupplier;

    @Before
    public void setUp() {
        mMockTabModel = spy(new MockTabModel(mProfile, null));
        mTabModelSupplier = () -> mMockTabModel;
        mIncognitoTabModelDelegate =
                new IncognitoTabModelDelegate() {
                    @Override
                    public TabModelInternal createTabModel() {
                        return mTabModelSupplier.get();
                    }

                    @Override
                    public TabCreator getIncognitoTabCreator() {
                        return mTabCreator;
                    }
                };
        when(mProfile.isOffTheRecord()).thenReturn(true);

        mIncognitoTabModel = new IncognitoTabModelImpl(mIncognitoTabModelDelegate);
        mIncognitoTabModel.addIncognitoObserver(mIncognitoTabModelObserver);
    }

    @Test
    public void testOnIncognitoModelCreatedCalledBeforeWasFirstTabCreated() {
        MockTab tab = new MockTab(1, mProfile);
        mIncognitoTabModel.addTab(
                tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        InOrder inOrder = inOrder(mIncognitoTabModelObserver, mMockTabModel);
        inOrder.verify(mIncognitoTabModelObserver).onIncognitoModelCreated();
        inOrder.verify(mMockTabModel)
                .addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        inOrder.verify(mIncognitoTabModelObserver).wasFirstTabCreated();
        assertEquals(1, mMockTabModel.getCount());
    }

    @Test
    public void testTabGroupObserverAttachedToDelegate() {
        mIncognitoTabModel.addTabGroupObserver(mMockTabGroupModelFilterObserver);

        MockTab tab = new MockTab(1, mProfile);
        mIncognitoTabModel.addTab(
                tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        verify(mMockTabModel).addTabGroupObserver(mMockTabGroupModelFilterObserver);
    }

    @Test
    public void testTabGroupObserverReattachedOnModelRecreation() {
        mIncognitoTabModel.addTabGroupObserver(mMockTabGroupModelFilterObserver);

        // 1. Create model
        MockTab tab1 = new MockTab(1, mProfile);
        mIncognitoTabModel.addTab(
                tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        verify(mMockTabModel).addTabGroupObserver(mMockTabGroupModelFilterObserver);

        // 2. Destroy model (by removing last tab)
        mIncognitoTabModel.removeTab(tab1);
        // destroyIncognitoIfNecessary() is called, but implementation detail.

        // 3. Create new model
        MockTabModel newMockTabModel = spy(new MockTabModel(mProfile, null));
        mTabModelSupplier = () -> newMockTabModel;

        MockTab tab2 = new MockTab(2, mProfile);
        mIncognitoTabModel.addTab(
                tab2, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        verify(newMockTabModel).addTabGroupObserver(mMockTabGroupModelFilterObserver);
    }
}
