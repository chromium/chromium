// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link IncognitoTabGroupModelFilterImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class IncognitoTabGroupModelFilterImplUnitTest {

    interface TabAndGroupModel extends TabModelInternal, TabGroupModelFilterInternal {}

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private IncognitoTabModelInternal mMockIncognitoTabModel;
    @Mock private TabAndGroupModel mMockDelegateModel;
    @Mock private EmptyTabModel mMockEmptyDelegateModel;
    @Mock private TabGroupModelFilterObserver mMockObserver1;
    @Mock private TabGroupModelFilterObserver mMockObserver2;

    @Captor private ArgumentCaptor<Callback<TabModelInternal>> mDelegateModelCallbackCaptor;

    private IncognitoTabGroupModelFilterImpl mIncognitoFilter;

    @Before
    public void setUp() {
        mIncognitoFilter = new IncognitoTabGroupModelFilterImpl(mMockIncognitoTabModel);
        verify(mMockIncognitoTabModel)
                .addDelegateModelObserver(mDelegateModelCallbackCaptor.capture());
    }

    @Test
    public void testSetDelegateModel_TransitionsAndObserverHandling() {
        assertNull(
                "mCurrentFilter should be null initially.",
                mIncognitoFilter.getCurrentFilterForTesting());

        mIncognitoFilter.addTabGroupObserver(mMockObserver1);
        verify(mMockDelegateModel, never()).addTabGroupObserver(mMockObserver1);

        Callback<TabModelInternal> callback = mDelegateModelCallbackCaptor.getValue();
        callback.onResult(mMockDelegateModel);

        assertEquals(
                "mCurrentFilter should be set to the new delegate filter.",
                mMockDelegateModel,
                mIncognitoFilter.getCurrentFilterForTesting());
        verify(mMockDelegateModel).addTabGroupObserver(mMockObserver1);

        mIncognitoFilter.addTabGroupObserver(mMockObserver2);
        verify(mMockDelegateModel).addTabGroupObserver(mMockObserver2);

        callback.onResult(mMockEmptyDelegateModel);

        assertNull(
                "mCurrentFilter should be null after EmptyTabModel is set.",
                mIncognitoFilter.getCurrentFilterForTesting());

        callback.onResult(mMockDelegateModel);
        assertEquals(
                "mCurrentFilter should be mMockDelegateModel again.",
                mMockDelegateModel,
                mIncognitoFilter.getCurrentFilterForTesting());
        verify(mMockDelegateModel, times(2)).addTabGroupObserver(mMockObserver1);
        verify(mMockDelegateModel, times(2)).addTabGroupObserver(mMockObserver2);

        mIncognitoFilter.removeTabGroupObserver(mMockObserver1);
        verify(mMockDelegateModel).removeTabGroupObserver(mMockObserver1);

        callback.onResult(mMockEmptyDelegateModel);
        assertNull(mIncognitoFilter.getCurrentFilterForTesting());
        mIncognitoFilter.removeTabGroupObserver(mMockObserver2);
        verify(mMockDelegateModel, never()).removeTabGroupObserver(mMockObserver2);
    }
}
