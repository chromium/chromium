// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.DESCRIPTION;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.ICON;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.ON_OPEN;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.TITLE;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.download.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds {@link DownloadTrayItemProperties} to a {@code download_tray_item_row} view. */
@NullMarked
class DownloadTrayItemViewBinder {
    private DownloadTrayItemViewBinder() {}

    static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == ICON) {
            ImageView icon = view.findViewById(R.id.download_tray_item_icon);
            icon.setImageResource(model.get(ICON));
        } else if (key == TITLE) {
            TextView title = view.findViewById(R.id.download_tray_item_title);
            title.setText(model.get(TITLE));
        } else if (key == DESCRIPTION) {
            TextView description = view.findViewById(R.id.download_tray_item_description);
            description.setText(model.get(DESCRIPTION));
        } else if (key == ON_OPEN) {
            view.setOnClickListener(model.get(ON_OPEN));
            // Clicking the row opens the download, so announce "double-tap to open" rather than
            // the generic "double-tap to activate".
            ViewCompat.replaceAccessibilityAction(
                    view,
                    AccessibilityActionCompat.ACTION_CLICK,
                    view.getContext().getString(R.string.open_downloaded_label),
                    null);
        }
    }
}
