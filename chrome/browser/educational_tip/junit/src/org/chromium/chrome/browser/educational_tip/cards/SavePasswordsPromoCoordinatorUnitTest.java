// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.CallbackController;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.shadows.ShadowAppCompatResources;
import org.chromium.ui.widget.ButtonCompat;

/** Test relating to {@link SavePasswordsPromoCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class SavePasswordsPromoCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mOnModuleClickedCallback;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private EducationTipModuleActionDelegate mActionDelegate;

    private SavePasswordsPromoCoordinator mSavePasswordsPromoCoordinator;
    private CallbackController mCallbackController;

    @Before
    public void setUp() {
        mCallbackController = new CallbackController();
        RuntimeEnvironment.application.setTheme(
                org.chromium.chrome.browser.educational_tip.R.style.Theme_AppCompat);
        when(mActionDelegate.getContext()).thenReturn(RuntimeEnvironment.application);
        when(mActionDelegate.getBottomSheetController()).thenReturn(mBottomSheetController);

        mSavePasswordsPromoCoordinator =
                new SavePasswordsPromoCoordinator(
                        mOnModuleClickedCallback, mCallbackController, mActionDelegate);
    }

    @After
    public void tearDown() {
        mCallbackController.destroy();
    }

    @Test
    @SmallTest
    public void testSavePasswordsPromoCardBottomSheet() {
        mSavePasswordsPromoCoordinator.onCardClicked();

        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
        verify(mOnModuleClickedCallback).run();

        @Nullable SavePasswordsInstructionalBottomSheetContent content =
                mSavePasswordsPromoCoordinator.getBottomSheetContent();
        ButtonCompat button =
                content.getContentView()
                        .findViewById(org.chromium.chrome.browser.educational_tip.R.id.button);
        button.performClick();
        verify(mBottomSheetController).hideContent(any(), anyBoolean(), anyInt());
    }
}
