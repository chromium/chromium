// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.generic_ui;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.chromium.chrome.browser.autofill_assistant.generic_ui.AssistantGenericUiViewBinder.ViewHolder;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Java-side coordinator for the GenericUI client action.
 */
public class AssistantGenericUiCoordinator {
    private final ViewGroup mView;
    private AssistantGenericUiViewBinder mViewBinder;

    public AssistantGenericUiCoordinator(Context context, AssistantGenericUiModel model) {
        mView = new LinearLayout(context);
        ViewHolder viewHolder = new ViewHolder(mView);
        mViewBinder = new AssistantGenericUiViewBinder();
        PropertyModelChangeProcessor.create(model, viewHolder, mViewBinder);
    }

    /**
     * Return the view containing the generic UI.
     */
    public View getView() {
        return mView;
    }
}
