// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/** Unit tests for {@link TabWebContentsDelegateAndroidImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabWebContentsDelegateAndroidImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabImpl mTab;
    @Mock private TabWebContentsDelegateAndroid mWrappedDelegate;
    @Mock private TabObserver mObserver1;
    @Mock private TabObserver mObserver2;

    private TabWebContentsDelegateAndroidImpl mDelegate;

    @Before
    public void setUp() {
        when(mTab.getTabObservers()).thenReturn(List.of(mObserver1, mObserver2));
        mDelegate = new TabWebContentsDelegateAndroidImpl(mTab, mWrappedDelegate);
    }

    @Test
    public void testOnBeforeUnloadFired_Proceed() {
        mDelegate.onBeforeUnloadFired(/* proceed= */ true);

        verify(mObserver1).onBeforeUnloadFired(mTab, true);
        verify(mObserver2).onBeforeUnloadFired(mTab, true);
    }

    @Test
    public void testOnBeforeUnloadFired_Cancel() {
        mDelegate.onBeforeUnloadFired(/* proceed= */ false);

        verify(mObserver1).onBeforeUnloadFired(mTab, false);
        verify(mObserver2).onBeforeUnloadFired(mTab, false);
    }
}
