// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.hub.DisplayButtonData;
import org.chromium.chrome.browser.hub.FullButtonData;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;

/**
 * Unit tests for {@link IncognitoTabSwitcherPane}. Refer to {@link TabSwitcherPaneUnitTest} for
 * tests for shared functionality with {@link TabSwitcherPaneBase}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class IncognitoTabSwitcherPaneUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabSwitcherPaneCoordinatorFactory mTabSwitcherPaneCoordinatorFactory;
    @Mock private TabSwitcherPaneCoordinator mTabSwitcherPaneCoordinator;
    @Mock private View.OnClickListener mNewTabButtonClickListener;
    @Mock private IncognitoTabModel mIncognitoTabModel;

    @Captor private ArgumentCaptor<IncognitoTabModelObserver> mIncognitoTabModelObserverCaptor;

    private Context mContext;
    private IncognitoTabSwitcherPane mIncognitoTabSwitcherPane;
    private int mTimesCreated;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        doAnswer(
                        invocation -> {
                            mTimesCreated++;
                            return mTabSwitcherPaneCoordinator;
                        })
                .when(mTabSwitcherPaneCoordinatorFactory)
                .create(any());

        mIncognitoTabSwitcherPane =
                new IncognitoTabSwitcherPane(
                        mContext,
                        mTabSwitcherPaneCoordinatorFactory,
                        () -> mIncognitoTabModel,
                        mNewTabButtonClickListener);
    }

    @After
    public void tearDown() {
        mIncognitoTabSwitcherPane.destroy();
        verify(mTabSwitcherPaneCoordinator, times(mTimesCreated)).destroy();
        verify(mIncognitoTabModel).removeIncognitoObserver(any());
    }

    @Test
    @SmallTest
    public void testInitWithNativeHasIncognitoTabs() {
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        mIncognitoTabSwitcherPane.initWithNative();
        verify(mIncognitoTabModel).addIncognitoObserver(mIncognitoTabModelObserverCaptor.capture());
        DisplayButtonData buttonData =
                mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier().get();
        assertNotNull(buttonData);

        checkIncognitoTabModelObserverAndButtonData();
    }

    @Test
    @SmallTest
    public void testInitWithNativeHasNoIncognitoTabs() {
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        mIncognitoTabSwitcherPane.initWithNative();
        verify(mIncognitoTabModel).addIncognitoObserver(mIncognitoTabModelObserverCaptor.capture());
        assertNull(mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier().get());

        checkIncognitoTabModelObserverAndButtonData();
    }

    @Test
    @SmallTest
    public void testPaneId() {
        assertEquals(PaneId.INCOGNITO_TAB_SWITCHER, mIncognitoTabSwitcherPane.getPaneId());
    }

    @Test
    @SmallTest
    public void testLoadHint() {
        // TODO(crbug/1505772): this is a noop right now.
        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.COLD);
        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.WARM);
        mIncognitoTabSwitcherPane.notifyLoadHint(LoadHint.HOT);
    }

    @Test
    @SmallTest
    public void testNewTabButton() {
        FullButtonData buttonData = mIncognitoTabSwitcherPane.getActionButtonDataSupplier().get();

        assertEquals(mContext.getString(R.string.button_new_tab), buttonData.resolveText(mContext));
        assertEquals(
                mContext.getString(R.string.button_new_incognito_tab),
                buttonData.resolveContentDescription(mContext));
        assertTrue(
                AppCompatResources.getDrawable(mContext, R.drawable.new_tab_icon)
                        .getConstantState()
                        .equals(buttonData.resolveIcon(mContext).getConstantState()));

        buttonData.getOnPressRunnable().run();
        verify(mNewTabButtonClickListener).onClick(isNull());
    }

    private void checkIncognitoTabModelObserverAndButtonData() {
        IncognitoTabModelObserver observer = mIncognitoTabModelObserverCaptor.getValue();

        observer.didBecomeEmpty();
        assertNull(mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier().get());

        // TODO(crbug/1505772): These resources need to be updated.
        observer.wasFirstTabCreated();
        DisplayButtonData buttonData =
                mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier().get();
        assertEquals(
                mContext.getString(R.string.accessibility_tab_switcher),
                buttonData.resolveText(mContext));
        assertEquals(
                mContext.getString(R.string.accessibility_tab_switcher),
                buttonData.resolveContentDescription(mContext));
        assertNotNull(buttonData.resolveIcon(mContext));

        observer.didBecomeEmpty();
        assertNull(mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier().get());
    }
}
