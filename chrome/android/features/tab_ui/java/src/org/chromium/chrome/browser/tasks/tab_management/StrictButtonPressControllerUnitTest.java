// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.StrictButtonPressController.ButtonClickResult;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.MockitoHelper;

/** Unit tests for {@link StrictButtonPressController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class StrictButtonPressControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Callback<Integer> mOnButtonClick;
    @Mock private PropertyModel mPropertyModel;

    @Test
    public void testPositive() {
        StrictButtonPressController controller =
                new StrictButtonPressController(mModalDialogManager, mOnButtonClick);
        MockitoHelper.doCallback(1, (Integer cause) -> controller.onDismiss(mPropertyModel, cause))
                .when(mModalDialogManager)
                .dismissDialog(any(), anyInt());
        controller.onClick(mPropertyModel, ButtonType.POSITIVE);
        verify(mOnButtonClick).onResult(ButtonClickResult.POSITIVE);
    }

    @Test
    public void testNegative() {
        StrictButtonPressController controller =
                new StrictButtonPressController(mModalDialogManager, mOnButtonClick);
        MockitoHelper.doCallback(1, (Integer cause) -> controller.onDismiss(mPropertyModel, cause))
                .when(mModalDialogManager)
                .dismissDialog(any(), anyInt());
        controller.onClick(mPropertyModel, ButtonType.NEGATIVE);
        verify(mOnButtonClick).onResult(ButtonClickResult.NEGATIVE);
    }

    @Test
    public void testTouchOutside() {
        StrictButtonPressController controller =
                new StrictButtonPressController(mModalDialogManager, mOnButtonClick);
        controller.onDismiss(mPropertyModel, DialogDismissalCause.TOUCH_OUTSIDE);
        verify(mOnButtonClick).onResult(ButtonClickResult.NO_CLICK);
    }
}
