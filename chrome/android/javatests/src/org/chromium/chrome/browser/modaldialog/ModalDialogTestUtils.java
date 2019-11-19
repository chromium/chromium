// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import android.content.res.Resources;

import androidx.annotation.Nullable;

import org.junit.Assert;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Utility methods and classes for testing modal dialogs.
 */
public class ModalDialogTestUtils {
    /**
     * Test observer that notifies dialog dismissal.
     */
    public interface TestDialogDismissedObserver {
        /**
         * Called when dialog is dismissed.
         * @param dismissalCause The dismissal cause.
         */
        void onDialogDismissed(@DialogDismissalCause int dismissalCause);
    }

    /**
     * @return A {@link PropertyModel} of a modal dialog that is used for testing.
     */
    public static PropertyModel createDialog(ChromeActivity activity, String title,
            @Nullable TestDialogDismissedObserver observer) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            ModalDialogProperties.Controller controller = new ModalDialogProperties.Controller() {
                @Override
                public void onDismiss(
                        PropertyModel model, @DialogDismissalCause int dismissalCause) {
                    if (observer != null) observer.onDialogDismissed(dismissalCause);
                }

                @Override
                public void onClick(PropertyModel model, int buttonType) {
                    switch (buttonType) {
                        case ModalDialogProperties.ButtonType.POSITIVE:
                        case ModalDialogProperties.ButtonType.NEGATIVE:
                            activity.getModalDialogManager().dismissDialog(
                                    model, DialogDismissalCause.UNKNOWN);
                            break;
                        default:
                            Assert.fail("Unknown button type: " + buttonType);
                    }
                }
            };
            Resources resources = activity.getResources();
            return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                    .with(ModalDialogProperties.CONTROLLER, controller)
                    .with(ModalDialogProperties.TITLE, title)
                    .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources, R.string.ok)
                    .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources, R.string.cancel)
                    .build();
        });
    }

    /**
     * Shows a dialog on the specified {@link ModalDialogManager} on the UI thread.
     */
    public static void showDialog(
            ModalDialogManager manager, PropertyModel model, @ModalDialogType int dialogType) {
        TestThreadUtils.runOnUiThreadBlocking(() -> manager.showDialog(model, dialogType));
    }

    /**
     * Checks whether the number of pending dialogs of a specified type is as expected.
     */
    public static void checkPendingSize(
            ModalDialogManager manager, @ModalDialogType int dialogType, int expected) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List list = manager.getPendingDialogsForTest(dialogType);
            Assert.assertEquals(expected, list != null ? list.size() : 0);
        });
    }

    /**
     * Checks whether the current presenter of the {@link ModalDialogManager} is as expected. If
     * {@code dialogType} is null, then the expected current presenter should be null.
     */
    public static void checkCurrentPresenter(
            ModalDialogManager manager, @Nullable Integer dialogType) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            if (dialogType == null) {
                Assert.assertFalse(manager.isShowing());
                Assert.assertNull(manager.getCurrentPresenterForTest());
            } else {
                Assert.assertTrue(manager.isShowing());
                Assert.assertEquals(manager.getPresenterForTest(dialogType),
                        manager.getCurrentPresenterForTest());
            }
        });
    }

    /**
     * Checks whether the dialog dismissal cause is as expected. If {@code expectedDismissalCause}
     * is null, then the check is skipped.
     */
    public static void checkDialogDismissalCause(
            @Nullable Integer expectedDismissalCause, @DialogDismissalCause int dismissalCause) {
        if (expectedDismissalCause == null) return;
        Assert.assertEquals(expectedDismissalCause.intValue(), dismissalCause);
    }

    /**
     * @param modelBuilder The builder for the modal dialog view model.
     * @param view The {@link ModalDialogView} that should be bound.
     * @return The {@link PropertyModel} that binds the {@code view}.
     */
    public static PropertyModel createModel(
            PropertyModel.Builder modelBuilder, ModalDialogView view) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            PropertyModel model = modelBuilder.build();
            PropertyModelChangeProcessor.create(model, view, new ModalDialogViewBinder());
            return model;
        });
    }
}
