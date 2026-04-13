// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ActionRegistry}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActionRegistryUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PropertyModel mModel1;
    @Mock private PropertyModel mModel2;
    @Mock private Callback<PropertyModel> mModelObserver;

    private ActionRegistry mActionRegistry;

    @Before
    public void setUp() {
        mActionRegistry = new ActionRegistry();
    }

    @Test
    @SmallTest
    public void testUpdateActionAndGet() {
        mActionRegistry.register(ActionId.HOME_BUTTON, mModel1);
        mActionRegistry.register(ActionId.TAB_SWITCHER, mModel2);

        assertEquals(mModel1, mActionRegistry.get(ActionId.HOME_BUTTON).get());
        assertEquals(mModel2, mActionRegistry.get(ActionId.TAB_SWITCHER).get());
    }

    @Test
    @SmallTest
    public void testUnregister() {
        mActionRegistry.register(ActionId.HOME_BUTTON, mModel1);
        mActionRegistry.unregister(ActionId.HOME_BUTTON);

        assertNull(mActionRegistry.get(ActionId.HOME_BUTTON).get());
    }

    @Test
    @SmallTest
    public void testGet_UnregisteredAction() {
        assertNull(mActionRegistry.get(ActionId.HOME_BUTTON).get());
    }

    @Test
    @SmallTest
    public void testObserverNotifiedOnUpdate() {
        mActionRegistry.get(ActionId.HOME_BUTTON).addSyncObserver(mModelObserver);

        mActionRegistry.register(ActionId.HOME_BUTTON, mModel1);
        verify(mModelObserver).onResult(mModel1);
    }
}
