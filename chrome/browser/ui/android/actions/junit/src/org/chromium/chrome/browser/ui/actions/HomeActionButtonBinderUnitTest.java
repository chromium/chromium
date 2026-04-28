// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.ClickWithMetaStateCallback;

/** Unit tests for {@link HomeActionButtonBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HomeActionButtonBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ListMenuButton mView;
    @Mock private ListMenuDelegate mDelegate;
    @Mock private ClickWithMetaStateCallback mClickCallback;

    private PropertyModel mModel;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(HomeActionProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(mModel, mView, HomeActionButtonBinder::bind);
    }

    @Test
    @SmallTest
    public void testLongPressMenuDelegate() {
        mModel.set(HomeActionProperties.LONG_PRESS_MENU_DELEGATE, mDelegate);
        verify(mView).setDelegate(mDelegate, false);
    }

    @Test
    @SmallTest
    public void testClickWithMetaCallback() {
        mModel.set(HomeActionProperties.CLICK_WITH_META_CALLBACK, mClickCallback);
        verify(mView).setClickCallback(mClickCallback);
    }

    @Test
    @SmallTest
    public void testFallbackToActionButtonBinder() {
        mModel.set(ActionProperties.ICON_ID, 123);
        verify(mView).setImageResource(123);
    }
}
