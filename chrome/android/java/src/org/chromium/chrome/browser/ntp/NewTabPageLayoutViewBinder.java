// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.chromium.chrome.browser.ntp.NewTabPageLayoutProperties.DELEGATE;
import static org.chromium.chrome.browser.ntp.NewTabPageLayoutProperties.ON_LAYOUT_CHANGE_LISTENER;
import static org.chromium.chrome.browser.ntp.NewTabPageLayoutProperties.TOP_INSET_PX;
import static org.chromium.chrome.browser.ntp.NewTabPageLayoutProperties.TRANSITION_Y;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** This class is responsible for pushing updates to the NewTabPageLayout. */
@NullMarked
public class NewTabPageLayoutViewBinder {
    /**
     * View binder that associates a view with a model.
     *
     * @param model The {@link PropertyModel} that contains the data.
     * @param newTabPageLayout The {@link NewTabPageLayout} that is changing.
     * @param key The property of the view that changed.
     */
    public static void bind(
            PropertyModel model, NewTabPageLayout newTabPageLayout, PropertyKey key) {
        if (DELEGATE == key) {
            newTabPageLayout.setDelegate(model.get(DELEGATE));
        } else if (ON_LAYOUT_CHANGE_LISTENER == key) {
            // Sets the layout change listener for NewTabPageLayout. Previously added listener will
            // be removed.
            View.OnLayoutChangeListener oldListener =
                    (View.OnLayoutChangeListener)
                            newTabPageLayout.getTag(R.id.ntp_view_layout_change_listener_tag);
            if (oldListener != null) {
                newTabPageLayout.removeOnLayoutChangeListener(oldListener);
            }
            View.OnLayoutChangeListener newListener = model.get(ON_LAYOUT_CHANGE_LISTENER);
            if (newListener != null) {
                newTabPageLayout.addOnLayoutChangeListener(newListener);
            }
            newTabPageLayout.setTag(R.id.ntp_view_layout_change_listener_tag, newListener);
        } else if (TOP_INSET_PX == key) {
            newTabPageLayout.setPaddingRelative(
                    newTabPageLayout.getPaddingStart(),
                    model.get(TOP_INSET_PX),
                    newTabPageLayout.getPaddingEnd(),
                    newTabPageLayout.getPaddingBottom());
        } else if (TRANSITION_Y == key) {
            newTabPageLayout.setTranslationYOfFakeboxAndAbove(model.get(TRANSITION_Y));
        }
    }
}
