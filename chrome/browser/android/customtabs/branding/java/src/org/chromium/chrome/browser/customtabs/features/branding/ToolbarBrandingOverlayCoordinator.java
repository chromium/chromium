// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator that creates/shows and hides/destroys the toolbar branding overlay over the title and
 * url.
 */
public class ToolbarBrandingOverlayCoordinator {
    private View mView;

    /**
     * Constructs and shows the toolbar branding overlay.
     *
     * @param viewStub The {@link ViewStub} to inflate the branding overlay.
     * @param model The {@link PropertyModel} with the properties of the branding overlay.
     */
    public ToolbarBrandingOverlayCoordinator(ViewStub viewStub, PropertyModel model) {
        assert viewStub.getLayoutResource() == R.layout.custom_tabs_toolbar_branding_layout;
        mView = viewStub.inflate();
        PropertyModelChangeProcessor.create(model, mView, ToolbarBrandingOverlayViewBinder::bind);
    }

    /** Hides the toolbar branding overlay and performs necessary clean-up. */
    public void hideAndDestroy() {
        assert mView != null : "Toolbar branding overlay is already destroyed.";

        ((ViewGroup) mView.getParent()).removeView(mView);
        mView = null;
    }
}
