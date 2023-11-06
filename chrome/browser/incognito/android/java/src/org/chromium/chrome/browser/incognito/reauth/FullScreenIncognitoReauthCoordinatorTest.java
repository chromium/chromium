// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.base.test.util.Batch.UNIT_TESTS;

import android.content.Context;
import android.view.View;

import androidx.activity.OnBackPressedCallback;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Robolectric tests for {@link FullScreenIncognitoReauthCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
@Batch(UNIT_TESTS)
public class FullScreenIncognitoReauthCoordinatorTest {
    @Mock private Context mContextMock;
    @Mock private IncognitoReauthManager mIncognitoReauthManagerMock;
    @Mock private IncognitoReauthManager.IncognitoReauthCallback mIncognitoReauthCallbackMock;
    @Mock private Runnable mSeeOtherTabsRunnableMock;
    @Mock private ModalDialogManager mModalDialogManagerMock;
    @Mock private IncognitoReauthMenuDelegate mIncognitoReauthMenuDelegateMock;
    @Mock private ListMenuButtonDelegate mIncognitoReauthListMenuButtonDelegateMock;

    @Mock private View mIncognitoReauthViewMock;
    @Mock private PropertyModel mPropertyModelMock;
    @Mock private PropertyModelChangeProcessor mPropertyModelChangeProcessorMock;

    @Mock private IncognitoReauthDialog mIncognitoReauthDialogMock;

    private OnBackPressedCallback mOnBackPressedCallbackMock =
            new OnBackPressedCallback(false) {
                @Override
                public void handleOnBackPressed() {}
            };

    private FullScreenIncognitoReauthCoordinator mFullScreenIncognitoReauthCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mFullScreenIncognitoReauthCoordinator =
                new FullScreenIncognitoReauthCoordinator(
                        mContextMock,
                        mIncognitoReauthManagerMock,
                        mIncognitoReauthCallbackMock,
                        mSeeOtherTabsRunnableMock,
                        mModalDialogManagerMock,
                        mIncognitoReauthMenuDelegateMock,
                        mOnBackPressedCallbackMock);
        // Allows to bypass assertion checks inside this class for testing.
        mFullScreenIncognitoReauthCoordinator.mIgnoreViewAndModelCreationForTesting = true;
        mFullScreenIncognitoReauthCoordinator.mIgnoreDialogCreationForTesting = true;

        mFullScreenIncognitoReauthCoordinator.setIncognitoReauthViewForTesting(
                mIncognitoReauthViewMock);
        mFullScreenIncognitoReauthCoordinator.setModelChangeProcessorForTesting(
                mPropertyModelChangeProcessorMock);
        mFullScreenIncognitoReauthCoordinator.setPropertyModelForTesting(mPropertyModelMock);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testNullMenuButtonDelegate_ForFullScreen_ThrowsAssertionError() {
        when(mIncognitoReauthMenuDelegateMock.getListMenuButtonDelegate()).thenReturn(null);
        mFullScreenIncognitoReauthCoordinator.show();
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testDialogAlreadyCreated_DuringShow_ThrowsAssertionError() {
        mFullScreenIncognitoReauthCoordinator.setIncognitoReauthDialogForTesting(
                mIncognitoReauthDialogMock);

        mFullScreenIncognitoReauthCoordinator.show();
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testHide_BeforeDialogCreation_ThrowsAssertionError() {
        mFullScreenIncognitoReauthCoordinator.hide(
                DialogDismissalCause.DIALOG_INTERACTION_DEFERRED);
    }

    @Test
    @SmallTest
    public void testShow_Invoke_InternalDialogMethod() {
        when(mIncognitoReauthMenuDelegateMock.getListMenuButtonDelegate())
                .thenReturn(mIncognitoReauthListMenuButtonDelegateMock);
        mFullScreenIncognitoReauthCoordinator.setIncognitoReauthDialogForTesting(
                mIncognitoReauthDialogMock);
        doNothing().when(mIncognitoReauthDialogMock).showIncognitoReauthDialog();

        mFullScreenIncognitoReauthCoordinator.show();

        verify(mIncognitoReauthMenuDelegateMock, times(1)).getListMenuButtonDelegate();
        verify(mIncognitoReauthDialogMock, times(1)).showIncognitoReauthDialog();
    }

    @Test
    @SmallTest
    public void testHide_Invoke_InternalDialogMethod() {
        mFullScreenIncognitoReauthCoordinator.setIncognitoReauthDialogForTesting(
                mIncognitoReauthDialogMock);
        doNothing()
                .when(mIncognitoReauthDialogMock)
                .dismissIncognitoReauthDialog(eq(DialogDismissalCause.DIALOG_INTERACTION_DEFERRED));
        doNothing().when(mPropertyModelChangeProcessorMock).destroy();

        mFullScreenIncognitoReauthCoordinator.hide(
                DialogDismissalCause.DIALOG_INTERACTION_DEFERRED);

        verify(mIncognitoReauthDialogMock, times(1))
                .dismissIncognitoReauthDialog(eq(DialogDismissalCause.DIALOG_INTERACTION_DEFERRED));
        verify(mPropertyModelChangeProcessorMock, times(1)).destroy();
    }
}
