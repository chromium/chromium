// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.search_engines.DefaultSearchEngineDialogHelper;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogPriority;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.SEARCH_ENGINE_CHOICE})
public class ChoiceDialogCoordinatorUnitTest {
    public @Rule TestRule mFeatureProcessor = new Features.JUnitProcessor();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    public @Mock ChromeActivity<?> mActivity;
    public @Mock ModalDialogManager mModalDialogManager;
    public @Mock DefaultSearchEngineDialogHelper.Delegate mDialogHelperDelegate;
    public @Mock Callback<Boolean> mOnSuccessCallback;

    @Before
    public void setUp() {
        doReturn(mModalDialogManager).when(mActivity).getModalDialogManager();
    }

    @Test
    public void testCreatePopupMenuShownListener() {
        ChoiceDialogCoordinator coordinator =
                new ChoiceDialogCoordinator(mActivity, mDialogHelperDelegate, mOnSuccessCallback);

        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());
        coordinator.show();

        verify(mModalDialogManager, times(1))
                .showDialog(any(), eq(ModalDialogType.APP), eq(ModalDialogPriority.VERY_HIGH));
    }
}
