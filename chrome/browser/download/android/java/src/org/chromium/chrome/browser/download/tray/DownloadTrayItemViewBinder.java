// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.DESCRIPTION;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.ICON;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.IS_IN_PROGRESS;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.IS_PAUSED;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.ITEM_ID;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.ON_OPEN;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.PRIMARY_ACTION;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.PROGRESS;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.PROGRESS_INDETERMINATE;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.SECONDARY_ACTION;
import static org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.TITLE;

import android.content.Context;
import android.content.res.ColorStateList;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.ActionButton;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.offline_items_collection.ContentId;
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
            // The actions' labels include the title.
            updateActionLabel(
                    model, view, R.id.download_tray_item_primary_action, model.get(PRIMARY_ACTION));
            updateActionLabel(
                    model,
                    view,
                    R.id.download_tray_item_secondary_action,
                    model.get(SECONDARY_ACTION));
        } else if (key == DESCRIPTION) {
            TextView description = view.findViewById(R.id.download_tray_item_description);
            description.setText(model.get(DESCRIPTION));
        } else if (key == ON_OPEN) {
            View.@Nullable OnClickListener onOpen = model.get(ON_OPEN);
            view.setOnClickListener(onOpen);
            view.setClickable(onOpen != null);
            updateHasActionsAndFocusability(model, view);
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
        } else if (key == PRIMARY_ACTION) {
            bindActionButton(
                    model, view, R.id.download_tray_item_primary_action, model.get(PRIMARY_ACTION));
            updateHasActionsAndFocusability(model, view);
        } else if (key == SECONDARY_ACTION) {
            bindActionButton(
                    model,
                    view,
                    R.id.download_tray_item_secondary_action,
                    model.get(SECONDARY_ACTION));
            updateHasActionsAndFocusability(model, view);
        }
    }

    /**
     * Binds {@code action} to its button and, as the button only shows while the row is hovered or
     * focused, to an accessibility action on the row for screen readers.
     */
    private static void bindActionButton(
            PropertyModel model, View row, @IdRes int buttonId, @Nullable ActionButton action) {
        ImageView button = row.findViewById(buttonId);
        if (action == null) {
            // Keyboard focus on the button moves to the row rather than jump to the tray's top.
            if (button.isFocused()) {
                row.requestFocus();
            }
            button.setVisibility(View.GONE);
            ViewCompat.replaceAccessibilityAction(
                    row,
                    new AccessibilityActionCompat(buttonId, /* label= */ null),
                    /* label= */ null,
                    /* command= */ null);
        } else {
            ContentId itemId = model.get(ITEM_ID);
            button.setVisibility(View.VISIBLE);
            button.setImageResource(action.mIconRes);
            button.setOnClickListener(_ -> action.mOnClick.onResult(itemId));
            updateActionLabel(model, row, buttonId, action);
        }
    }

    /**
     * Labels {@code action}'s button and its accessibility action on the row. As on desktop, the
     * label names the download, to tell rows' actions apart.
     */
    private static void updateActionLabel(
            PropertyModel model, View row, @IdRes int buttonId, @Nullable ActionButton action) {
        if (action == null) {
            return;
        }
        ContentId itemId = model.get(ITEM_ID);
        CharSequence title = model.get(TITLE);
        Context context = row.getContext();
        String label =
                TextUtils.isEmpty(title)
                        ? context.getString(action.mDescriptionRes)
                        : context.getString(action.mTitledDescriptionRes, title);
        row.findViewById(buttonId).setContentDescription(label);
        ViewCompat.replaceAccessibilityAction(
                row,
                new AccessibilityActionCompat(buttonId, /* label= */ null),
                label,
                (_, _) -> {
                    action.mOnClick.onResult(itemId);
                    return true;
                });
    }

    /** Tells the row whether it has action buttons, and sets whether it takes keyboard focus. */
    private static void updateHasActionsAndFocusability(PropertyModel model, View row) {
        boolean hasActions =
                model.get(PRIMARY_ACTION) != null || model.get(SECONDARY_ACTION) != null;
        ((DownloadTrayItemView) row).setHasActions(hasActions);
        // A row with nothing to open and no actions takes no keyboard focus; screen readers still
        // read it as one item.
        row.setFocusable(model.get(ON_OPEN) != null || hasActions);
    }
}
