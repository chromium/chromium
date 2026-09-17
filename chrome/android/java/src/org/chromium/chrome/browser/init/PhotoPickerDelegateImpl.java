// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinatorSupplier;
import org.chromium.components.browser_ui.photo_picker.PhotoPickerDialog;
import org.chromium.ui.base.PhotoPicker;
import org.chromium.ui.base.PhotoPickerDelegate;
import org.chromium.ui.base.PhotoPickerListener;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeStateProvider;

import java.util.List;

/** A delegate for the PhotoPicker that shows the Chrome-specific implementation. */
@NullMarked
public class PhotoPickerDelegateImpl implements PhotoPickerDelegate {
    @Override
    public PhotoPicker showPhotoPicker(
            WindowAndroid windowAndroid,
            PhotoPickerListener listener,
            boolean allowMultiple,
            List<String> mimeTypes) {
        Context context = windowAndroid.getContext().get();
        assumeNonNull(context);
        PhotoPickerDialog dialog =
                new PhotoPickerDialog(
                        windowAndroid,
                        context.getContentResolver(),
                        listener,
                        allowMultiple,
                        mimeTypes,
                        shouldDialogPadForContent(windowAndroid));
        assumeNonNull(dialog.getWindow()).getAttributes().windowAnimations =
                R.style.PickerDialogAnimation;
        dialog.show();
        return dialog;
    }

    @Override
    public boolean shouldBlockFilePicker(WindowAndroid windowAndroid) {
        var supplier = EphemeralTabCoordinatorSupplier.from(windowAndroid);
        if (supplier == null) return false;

        EphemeralTabCoordinator coordinator = supplier.get();
        return coordinator != null && coordinator.isOpened();
    }

    private static boolean shouldDialogPadForContent(WindowAndroid windowAndroid) {
        return EdgeToEdgeStateProvider.isEdgeToEdgeEnabledForWindow(windowAndroid);
    }
}
