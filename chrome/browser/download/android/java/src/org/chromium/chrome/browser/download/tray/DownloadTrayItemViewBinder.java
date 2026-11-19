// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.DESCRIPTION;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.ICON;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.IS_IN_PROGRESS;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.IS_PAUSED;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.ON_OPEN;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.PROGRESS;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.PROGRESS_INDETERMINATE;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.TITLE;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.View;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
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
            View.@Nullable OnClickListener onOpen = model.get(ON_OPEN);
            view.setOnClickListener(onOpen);
            // A row with nothing to open, e.g. a download in progress, takes no keyboard focus;
            // screen readers still read it as one item.
            view.setClickable(onOpen != null);
            view.setFocusable(onOpen != null);
            // Clicking the row opens the download, so announce "double-tap to open" rather than
            // the generic "double-tap to activate". A row with nothing to open has no click action.
            ViewCompat.replaceAccessibilityAction(
                    view,
                    AccessibilityActionCompat.ACTION_CLICK,
                    onOpen == null
                            ? null
                            : view.getContext().getString(R.string.open_downloaded_label),
                    null);
        } else if (key == IS_IN_PROGRESS) {
            ProgressBar progress = view.findViewById(R.id.download_tray_item_progress);
            progress.setVisibility(model.get(IS_IN_PROGRESS) ? View.VISIBLE : View.GONE);
        } else if (key == PROGRESS) {
            ProgressBar progress = view.findViewById(R.id.download_tray_item_progress);
            int value = model.get(PROGRESS);
            progress.setIndeterminate(value == PROGRESS_INDETERMINATE);
            if (value != PROGRESS_INDETERMINATE) {
                progress.setProgress(value);
            }
        } else if (key == IS_PAUSED) {
            // As on desktop, a paused download's progress bar loses its accent colour.
            Context context = view.getContext();
            ColorStateList tint =
                    ColorStateList.valueOf(
                            model.get(IS_PAUSED)
                                    ? SemanticColorUtils.getDefaultIconColorSecondary(context)
                                    : SemanticColorUtils.getProgressBarForeground(context));
            ProgressBar progress = view.findViewById(R.id.download_tray_item_progress);
            progress.setProgressTintList(tint);
            progress.setIndeterminateTintList(tint);
        }
    }
}
