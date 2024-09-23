// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

/** Unit tests for ExpandedSheetHelperImpl. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExpandedSheetHelperImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabObscuringHandler mTabObscuringHandler;
    private ExpandedSheetHelperImpl mExpandedSheetHelperImpl;

    @Before
    public void setUp() {
        mExpandedSheetHelperImpl =
                new ExpandedSheetHelperImpl(() -> mModalDialogManager, mTabObscuringHandler);
    }

    @Test
    public void testTabObscuredAndDialogSuspended() {
        mExpandedSheetHelperImpl.onSheetExpanded();
        verify(mTabObscuringHandler).obscure(eq(TabObscuringHandler.Target.ALL_TABS_AND_TOOLBAR));
        verify(mModalDialogManager).suspendType(eq(ModalDialogType.APP));
        verify(mModalDialogManager).suspendType(eq(ModalDialogType.TAB));

        mExpandedSheetHelperImpl.onSheetCollapsed();
        verify(mTabObscuringHandler).unobscure(any());
        verify(mModalDialogManager).resumeType(eq(ModalDialogType.APP), anyInt());
        verify(mModalDialogManager).resumeType(eq(ModalDialogType.TAB), anyInt());
    }
}
