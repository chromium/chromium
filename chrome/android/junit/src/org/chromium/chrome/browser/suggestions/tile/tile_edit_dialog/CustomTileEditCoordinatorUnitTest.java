// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditCoordinator.CustomLinksDuplicateChecker;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditCoordinator.TileValueChangeHandler;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link CustomTileEditCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
public class CustomTileEditCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private CustomTileEditView mView;
    @Mock private CustomTileEditMediator mMediator;
    @Mock private TileValueChangeHandler mTileValueChangeHandler;
    @Mock private CustomLinksDuplicateChecker mCustomLinksDuplicateChecker;
    @Mock private PropertyModel mDialogModel;

    private static final String TEST_TITLE = "Test Title";
    private static final GURL TEST_URL = JUnitTestGURLs.URL_1;
    private static final GURL TEST_DUPLICATE_URL = JUnitTestGURLs.URL_2;

    @Before
    public void setUp() {
        when(mView.getDialogModel()).thenReturn(mDialogModel);
    }

    @Test
    public void testConstructor_SetsDelegates() {
        CustomTileEditCoordinator coordinator = createAndSetupCoordinator();

        verify(mView).setMediatorDelegate(eq(mMediator));
        verify(mMediator).setDelegates(eq(mView), eq(coordinator));
    }

    @Test
    public void testShow_StoresHandlersAndCallsMediator() {
        CustomTileEditCoordinator coordinator = createAndSetupCoordinator();

        coordinator.show(mTileValueChangeHandler, mCustomLinksDuplicateChecker);
        verify(mMediator).show();

        when(mCustomLinksDuplicateChecker.isUrlDuplicate(any(GURL.class))).thenReturn(false);
        coordinator.isUrlDuplicate(TEST_URL);
        verify(mCustomLinksDuplicateChecker).isUrlDuplicate(eq(TEST_URL));

        when(mTileValueChangeHandler.onTileValueChange(any(String.class), any(GURL.class)))
                .thenReturn(true);
        coordinator.submitChange(TEST_TITLE, TEST_URL);
        verify(mTileValueChangeHandler).onTileValueChange(eq(TEST_TITLE), eq(TEST_URL));
    }

    @Test
    public void testShowEditDialog_CallsModalDialogManager() {
        CustomTileEditCoordinator coordinator = createAndSetupCoordinator();
        coordinator.show(mTileValueChangeHandler, mCustomLinksDuplicateChecker);
        verify(mMediator).show();

        // Expect the Mediator to call showEditDialog().
        coordinator.showEditDialog();
        verify(mModalDialogManager).showDialog(eq(mDialogModel), eq(ModalDialogType.APP));
    }

    @Test
    public void testCloseEditDialog_Submit_CallsModalDialogManager() {
        CustomTileEditCoordinator coordinator = createAndSetupCoordinator();
        coordinator.show(mTileValueChangeHandler, mCustomLinksDuplicateChecker);

        coordinator.closeEditDialog(/* isSubmit= */ true);
        verify(mModalDialogManager)
                .dismissDialog(eq(mDialogModel), eq(DialogDismissalCause.POSITIVE_BUTTON_CLICKED));
    }

    @Test
    public void testCloseEditDialog_Cancel_CallsModalDialogManager() {
        CustomTileEditCoordinator coordinator = createAndSetupCoordinator();
        coordinator.show(mTileValueChangeHandler, mCustomLinksDuplicateChecker);

        coordinator.closeEditDialog(/* isSubmit= */ false);
        verify(mModalDialogManager)
                .dismissDialog(eq(mDialogModel), eq(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED));
    }

    @Test
    public void testIsUrlDuplicate_CallsCheckerAndReturnsValue() {
        CustomTileEditCoordinator coordinator = createAndSetupCoordinator();
        coordinator.show(mTileValueChangeHandler, mCustomLinksDuplicateChecker);

        when(mCustomLinksDuplicateChecker.isUrlDuplicate(TEST_URL)).thenReturn(false);
        when(mCustomLinksDuplicateChecker.isUrlDuplicate(TEST_DUPLICATE_URL)).thenReturn(true);

        Assert.assertFalse("URL should not be duplicate", coordinator.isUrlDuplicate(TEST_URL));
        Assert.assertTrue(
                "URL should be duplicate", coordinator.isUrlDuplicate(TEST_DUPLICATE_URL));
        verify(mCustomLinksDuplicateChecker, times(1)).isUrlDuplicate(eq(TEST_URL));
        verify(mCustomLinksDuplicateChecker, times(1)).isUrlDuplicate(eq(TEST_DUPLICATE_URL));
    }

    @Test
    public void testSubmitChange_CallsHandlerAndReturnsValue() {
        CustomTileEditCoordinator coordinator = createAndSetupCoordinator();
        coordinator.show(mTileValueChangeHandler, mCustomLinksDuplicateChecker);

        when(mTileValueChangeHandler.onTileValueChange(TEST_TITLE, TEST_URL)).thenReturn(true);
        when(mTileValueChangeHandler.onTileValueChange("Fail", TEST_URL)).thenReturn(false);

        Assert.assertTrue(
                "Submission should succeed", coordinator.submitChange(TEST_TITLE, TEST_URL));
        Assert.assertFalse("Submission should fail", coordinator.submitChange("Fail", TEST_URL));
        verify(mTileValueChangeHandler, times(1)).onTileValueChange(eq(TEST_TITLE), eq(TEST_URL));
        verify(mTileValueChangeHandler, times(1)).onTileValueChange(eq("Fail"), eq(TEST_URL));
    }

    /**
     * Helper to create the Coordinator, assuming mocks have been set up. Instantiates the
     * coordinator and calls show() with the standard mock handlers.
     */
    private CustomTileEditCoordinator createAndSetupCoordinator() {
        return new CustomTileEditCoordinator(mModalDialogManager, mView, mMediator);
    }
}
