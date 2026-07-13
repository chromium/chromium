// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks.ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetPeekProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for Contextual Tasks peek control view. */
@NullMarked
public class ContextualTasksControlMediator {
    private final PropertyModel mModel;

    /**
     * Constructs the mediator.
     *
     * @param model The {@link PropertyModel} to use.
     */
    public ContextualTasksControlMediator(PropertyModel model) {
        mModel = model;
    }

    /**
     * Sets the title of the contextual tasks control view.
     *
     * @param title The title of the contextual tasks control view.
     */
    public void setTitle(String title) {
        mModel.set(TabBottomSheetPeekProperties.TITLE_TEXT, title);
    }
}
