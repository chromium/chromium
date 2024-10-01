// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link TabListEditorMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class TabListEditorMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private ObservableSupplier<TabModelFilter> mTabModelFilterSupplier;
    @Mock private DesktopWindowStateProvider mDesktopWindowStateProvider;

    private PropertyModel mModel;
    private TabListEditorMediator mMediator;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(TabListEditorProperties.ALL_KEYS).build();
        mMediator =
                new TabListEditorMediator(
                        mContext,
                        mTabModelFilterSupplier,
                        mModel,
                        /* selectionDelegate= */ null,
                        /* actionOnRelatedTabs= */ false,
                        /* snackbarManager= */ null,
                        /* bottomSheetController= */ null,
                        /* tabListEditorLayout= */ null,
                        TabActionState.SELECTABLE,
                        mDesktopWindowStateProvider);
    }

    @After
    public void tearDown() {
        mMediator.destroy();
    }

    @Test
    @SmallTest
    public void testTopMarginOnAppHeaderStateChange() {
        AppHeaderState state = mock(AppHeaderState.class);
        when(state.getAppHeaderHeight()).thenReturn(10);

        mMediator.onAppHeaderStateChanged(state);

        assertEquals(10, mModel.get(TabListEditorProperties.TOP_MARGIN));
    }
}
