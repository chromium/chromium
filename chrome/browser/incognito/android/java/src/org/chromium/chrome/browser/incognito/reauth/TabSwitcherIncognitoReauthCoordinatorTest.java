// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.base.test.util.Batch.UNIT_TESTS;

import android.content.Context;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherCustomViewManager;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Robolectric tests for {@link TabSwitcherIncognitoReauthCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
@Batch(UNIT_TESTS)
public class TabSwitcherIncognitoReauthCoordinatorTest {
    @Mock
    private Context mContextMock;
    @Mock
    private IncognitoReauthManager mIncognitoReauthManagerMock;
    @Mock
    private IncognitoReauthManager.IncognitoReauthCallback mIncognitoReauthCallbackMock;
    @Mock
    private Runnable mSeeOtherTabsRunnableMock;
    @Mock
    private Runnable mBackPressRunnableMock;
    @Mock
    private TabSwitcherCustomViewManager mTabSwitcherCustomViewManagerMock;
    @Mock
    private IncognitoReauthTopToolbarDelegate mIncognitoReauthTopToolbarDelegateMock;
    @Mock
    private View mIncognitoReauthViewMock;
    @Mock
    private PropertyModelChangeProcessor mPropertyModelChangeProcessorMock;

    private TabSwitcherIncognitoReauthCoordinator mTabSwitcherIncognitoReauthCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTabSwitcherIncognitoReauthCoordinator = new TabSwitcherIncognitoReauthCoordinator(
                mContextMock, mIncognitoReauthManagerMock, mIncognitoReauthCallbackMock,
                mSeeOtherTabsRunnableMock, mBackPressRunnableMock,
                mTabSwitcherCustomViewManagerMock, mIncognitoReauthTopToolbarDelegateMock);
    }

    @After
    public void tearDown() {
        mTabSwitcherIncognitoReauthCoordinator.mIgnoreViewAndModelCreationForTesting = false;
        mTabSwitcherIncognitoReauthCoordinator.setIncognitoReauthViewForTesting(null);
        mTabSwitcherIncognitoReauthCoordinator.setModelChangeProcessorForTesting(null);
    }

    @Test
    @SmallTest
    public void testShowMethod_Invokes_RequestView_And_DisableNewTabButton() {
        mTabSwitcherIncognitoReauthCoordinator.mIgnoreViewAndModelCreationForTesting = true;
        mTabSwitcherIncognitoReauthCoordinator.setIncognitoReauthViewForTesting(
                mIncognitoReauthViewMock);

        when(mTabSwitcherCustomViewManagerMock.requestView(
                     mIncognitoReauthViewMock, mBackPressRunnableMock, /*clearTabList=*/true))
                .thenReturn(true);
        when(mIncognitoReauthTopToolbarDelegateMock.disableNewTabButton())
                .thenReturn(/*token= */ 1);

        mTabSwitcherIncognitoReauthCoordinator.show();

        verify(mTabSwitcherCustomViewManagerMock, times(1))
                .requestView(
                        mIncognitoReauthViewMock, mBackPressRunnableMock, /*clearTabList=*/true);
        verify(mIncognitoReauthTopToolbarDelegateMock, times(1)).disableNewTabButton();
    }

    @Test
    @SmallTest
    public void testHideMethod_Invokes_ReleaseView_And_EnablesNewTabButton() {
        mTabSwitcherIncognitoReauthCoordinator.setNewTabInteractabilityTokenForTesting(
                /*token= */ 1);
        mTabSwitcherIncognitoReauthCoordinator.setModelChangeProcessorForTesting(
                mPropertyModelChangeProcessorMock);

        when(mTabSwitcherCustomViewManagerMock.releaseView()).thenReturn(true);
        doNothing()
                .when(mIncognitoReauthTopToolbarDelegateMock)
                .enableNewTabButton(/*clientToken= */ eq(1));
        doNothing().when(mPropertyModelChangeProcessorMock).destroy();

        mTabSwitcherIncognitoReauthCoordinator.hide(
                DialogDismissalCause.DIALOG_INTERACTION_DEFERRED);

        verify(mTabSwitcherCustomViewManagerMock, times(1)).releaseView();
        verify(mIncognitoReauthTopToolbarDelegateMock, times(1))
                .enableNewTabButton(/*clientToken= */ 1);
        verify(mPropertyModelChangeProcessorMock, times(1)).destroy();
    }
}
