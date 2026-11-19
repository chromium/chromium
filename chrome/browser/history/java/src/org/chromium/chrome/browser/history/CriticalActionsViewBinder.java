// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.history.CriticalActionsProperties.Expansion;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for the critical actions section of a history row and its action items. */
@NullMarked
class CriticalActionsViewBinder {
    /** Views and expansion animator of the host history row that {@link #bindRow} updates. */
    static class ViewHolder {
        final ImageView mExpandIcon;
        final CriticalActionsExpansionAnimator mExpansionAnimator;

        ViewHolder(ImageView expandIcon, CriticalActionsExpansionAnimator expansionAnimator) {
            mExpandIcon = expandIcon;
            mExpansionAnimator = expansionAnimator;
        }
    }

    /** Binds the row-level model to the host row's views. */
    static void bindRow(PropertyModel model, ViewHolder holder, PropertyKey key) {
        if (key == CriticalActionsProperties.EXPAND_ICON_VISIBLE) {
            holder.mExpandIcon.setVisibility(
                    model.get(CriticalActionsProperties.EXPAND_ICON_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (key == CriticalActionsProperties.EXPAND_ICON_CONTENT_DESCRIPTION) {
            holder.mExpandIcon.setContentDescription(
                    model.get(CriticalActionsProperties.EXPAND_ICON_CONTENT_DESCRIPTION));
        } else if (key == CriticalActionsProperties.EXPANSION) {
            Expansion expansion = model.get(CriticalActionsProperties.EXPANSION);
            holder.mExpansionAnimator.setExpanded(expansion.expanded, expansion.animate);
        } else if (key == CriticalActionsProperties.EXPAND_ICON_CLICK_HANDLER) {
            Runnable handler = model.get(CriticalActionsProperties.EXPAND_ICON_CLICK_HANDLER);
            holder.mExpandIcon.setOnClickListener(handler == null ? null : v -> handler.run());
            holder.mExpandIcon.setClickable(handler != null);
        } else if (key == CriticalActionsProperties.EXPAND_ICON_CLICK_ACCESSIBILITY_LABEL) {
            // The chevron is a plain button, so replacing its accessibility delegate is safe.
            ViewCompat.replaceAccessibilityAction(
                    holder.mExpandIcon,
                    AccessibilityActionCompat.ACTION_CLICK,
                    model.get(CriticalActionsProperties.EXPAND_ICON_CLICK_ACCESSIBILITY_LABEL),
                    null);
        }
    }

    /** Binds an action's model to its row in the panel's list. */
    static void bindActionItem(PropertyModel model, View view, PropertyKey key) {
        if (key == CriticalActionsProperties.LABEL) {
            ((TextView) view).setText(model.get(CriticalActionsProperties.LABEL));
        }
    }
}
