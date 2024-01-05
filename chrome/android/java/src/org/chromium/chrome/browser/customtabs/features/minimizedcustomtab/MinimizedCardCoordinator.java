// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator that manages the showing and hiding of the minimized card that covers the whole
 * Activity.
 */
public class MinimizedCardCoordinator {
    private final ViewGroup mRoot;
    private final int mPreviousImportantForAccessibility;
    private final View mView;

    /**
     * @param context The {@link Context}.
     * @param root The content root view that the card should be attached to.
     * @param model The {@link PropertyModel} for the card.
     */
    public MinimizedCardCoordinator(Context context, ViewGroup root, PropertyModel model) {
        mRoot = root;
        mView =
                LayoutInflater.from(context)
                        .inflate(R.layout.custom_tabs_minimized_card, mRoot, false);
        mRoot.addView(mView);
        PropertyModelChangeProcessor.create(model, mView, MinimizedCardViewBinder::bind, true);
        View coordinator = mRoot.findViewById(R.id.coordinator);
        mPreviousImportantForAccessibility = coordinator.getImportantForAccessibility();
        coordinator.setImportantForAccessibility(
                View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
    }

    /** Removes the minimized card. */
    public void dismiss() {
        View coordinator = mRoot.findViewById(R.id.coordinator);
        coordinator.setImportantForAccessibility(mPreviousImportantForAccessibility);
        mRoot.removeView(mView);
    }
}
