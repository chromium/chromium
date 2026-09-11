// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager.IncognitoReauthCallback;
import org.chromium.ui.modelutil.PropertyModel;

/** Robolectric tests for {@link IncognitoReauthMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class IncognitoReauthMediatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private IncognitoReauthCallback mIncognitoReauthCallbackMock;
    @Mock private IncognitoReauthManager mIncognitoReauthManagerMock;
    @Mock private Runnable mShowTabSwitcherRunnableMock;

    private IncognitoReauthMediator mIncognitoReauthMediator;

    @Before
    public void setUp() {
        mIncognitoReauthMediator =
                new IncognitoReauthMediator(
                        mIncognitoReauthCallbackMock,
                        mIncognitoReauthManagerMock,
                        mShowTabSwitcherRunnableMock);
    }

    @After
    public void tearDown() {
        verifyNoMoreInteractions(
                mIncognitoReauthCallbackMock,
                mIncognitoReauthManagerMock,
                mShowTabSwitcherRunnableMock);
    }

    @Test
    @SmallTest
    public void testOnUnlockIncognitoButtonClicked_StartsReauthenticationFlow() {
        mIncognitoReauthMediator.onUnlockIncognitoButtonClicked();

        verify(mIncognitoReauthManagerMock).startReauthenticationFlow(mIncognitoReauthCallbackMock);
        verifyNoInteractions(mShowTabSwitcherRunnableMock);
    }

    @Test
    @SmallTest
    public void testOnUnlockIncognitoButtonClicked_MultipleClicks_FiresFlowEachTime() {
        mIncognitoReauthMediator.onUnlockIncognitoButtonClicked();
        mIncognitoReauthMediator.onUnlockIncognitoButtonClicked();

        verify(mIncognitoReauthManagerMock, times(2))
                .startReauthenticationFlow(mIncognitoReauthCallbackMock);
        verifyNoInteractions(mShowTabSwitcherRunnableMock);
    }

    @Test
    @SmallTest
    public void testOnSeeOtherTabsButtonClicked_RunsShowTabSwitcherRunnable() {
        mIncognitoReauthMediator.onSeeOtherTabsButtonClicked();

        verify(mShowTabSwitcherRunnableMock).run();
        verifyNoInteractions(mIncognitoReauthManagerMock);
    }

    @Test
    @SmallTest
    public void testOnSeeOtherTabsButtonClicked_MultipleClicks_RunsRunnableEachTime() {
        mIncognitoReauthMediator.onSeeOtherTabsButtonClicked();
        mIncognitoReauthMediator.onSeeOtherTabsButtonClicked();

        verify(mShowTabSwitcherRunnableMock, times(2)).run();
        verifyNoInteractions(mIncognitoReauthManagerMock);
    }

    @Test
    @SmallTest
    public void testReauthCallback_Success() {
        doAnswer(
                        invocation -> {
                            IncognitoReauthCallback callback = invocation.getArgument(0);
                            callback.onIncognitoReauthSuccess();
                            return null;
                        })
                .when(mIncognitoReauthManagerMock)
                .startReauthenticationFlow(mIncognitoReauthCallbackMock);

        mIncognitoReauthMediator.onUnlockIncognitoButtonClicked();

        verify(mIncognitoReauthManagerMock).startReauthenticationFlow(mIncognitoReauthCallbackMock);
        verify(mIncognitoReauthCallbackMock).onIncognitoReauthSuccess();
        verifyNoInteractions(mShowTabSwitcherRunnableMock);
    }

    @Test
    @SmallTest
    public void testReauthCallback_Failure() {
        doAnswer(
                        invocation -> {
                            IncognitoReauthCallback callback = invocation.getArgument(0);
                            callback.onIncognitoReauthFailure();
                            return null;
                        })
                .when(mIncognitoReauthManagerMock)
                .startReauthenticationFlow(mIncognitoReauthCallbackMock);

        mIncognitoReauthMediator.onUnlockIncognitoButtonClicked();

        verify(mIncognitoReauthManagerMock).startReauthenticationFlow(mIncognitoReauthCallbackMock);
        verify(mIncognitoReauthCallbackMock).onIncognitoReauthFailure();
        verifyNoInteractions(mShowTabSwitcherRunnableMock);
    }

    @Test
    @SmallTest
    public void testReauthCallback_NotPossible() {
        doAnswer(
                        invocation -> {
                            IncognitoReauthCallback callback = invocation.getArgument(0);
                            callback.onIncognitoReauthNotPossible();
                            return null;
                        })
                .when(mIncognitoReauthManagerMock)
                .startReauthenticationFlow(mIncognitoReauthCallbackMock);

        mIncognitoReauthMediator.onUnlockIncognitoButtonClicked();

        verify(mIncognitoReauthManagerMock).startReauthenticationFlow(mIncognitoReauthCallbackMock);
        verify(mIncognitoReauthCallbackMock).onIncognitoReauthNotPossible();
        verifyNoInteractions(mShowTabSwitcherRunnableMock);
    }

    @Test
    @SmallTest
    public void testPropertyModel_UnlockIncognitoClicked_InvokesMediator() {
        PropertyModel model =
                new PropertyModel.Builder(IncognitoReauthProperties.ALL_KEYS)
                        .with(
                                IncognitoReauthProperties.ON_UNLOCK_INCOGNITO_CLICKED,
                                mIncognitoReauthMediator::onUnlockIncognitoButtonClicked)
                        .with(
                                IncognitoReauthProperties.ON_SEE_OTHER_TABS_CLICKED,
                                mIncognitoReauthMediator::onSeeOtherTabsButtonClicked)
                        .build();

        model.get(IncognitoReauthProperties.ON_UNLOCK_INCOGNITO_CLICKED).run();

        verify(mIncognitoReauthManagerMock).startReauthenticationFlow(mIncognitoReauthCallbackMock);
        verifyNoInteractions(mShowTabSwitcherRunnableMock);
    }

    @Test
    @SmallTest
    public void testPropertyModel_SeeOtherTabsClicked_InvokesMediator() {
        PropertyModel model =
                new PropertyModel.Builder(IncognitoReauthProperties.ALL_KEYS)
                        .with(
                                IncognitoReauthProperties.ON_UNLOCK_INCOGNITO_CLICKED,
                                mIncognitoReauthMediator::onUnlockIncognitoButtonClicked)
                        .with(
                                IncognitoReauthProperties.ON_SEE_OTHER_TABS_CLICKED,
                                mIncognitoReauthMediator::onSeeOtherTabsButtonClicked)
                        .build();

        model.get(IncognitoReauthProperties.ON_SEE_OTHER_TABS_CLICKED).run();

        verify(mShowTabSwitcherRunnableMock).run();
        verifyNoInteractions(mIncognitoReauthManagerMock);
    }
}
