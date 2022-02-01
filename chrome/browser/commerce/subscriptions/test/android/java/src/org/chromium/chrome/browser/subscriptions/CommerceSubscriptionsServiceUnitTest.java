// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;

/**
 * Unit tests for {@link CommerceSubscriptionsService}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CommerceSubscriptionsServiceUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private SubscriptionsManagerImpl mSubscriptionsManager;
    @Mock
    private IdentityManager mIdentityManager;
    @Mock
    private PrimaryAccountChangeEvent mChangeEvent;
    @Captor
    private ArgumentCaptor<IdentityManager.Observer> mIdentityManagerObserverCaptor;

    private CommerceSubscriptionsService mService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mService = new CommerceSubscriptionsService(mSubscriptionsManager, mIdentityManager);
        verify(mIdentityManager, times(1)).addObserver(mIdentityManagerObserverCaptor.capture());
    }

    @After
    public void tearDown() {
        mService.destroy();
        verify(mIdentityManager, times(1))
                .removeObserver(eq(mIdentityManagerObserverCaptor.getValue()));
    }

    @Test
    @SmallTest
    public void testOnPrimaryAccountChanged() {
        mIdentityManagerObserverCaptor.getValue().onPrimaryAccountChanged(mChangeEvent);
        verify(mSubscriptionsManager, times(1)).onIdentityChanged();
    }
}
