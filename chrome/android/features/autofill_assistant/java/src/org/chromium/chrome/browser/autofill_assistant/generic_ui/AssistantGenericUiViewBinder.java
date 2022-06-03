// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.generic_ui;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * This class is responsible for pushing updates to the Autofill Assistant generic UI view. These
 * updates are pulled from the {@link AssistantGenericUiModel} when a notification of an update is
 * received.
 */
class AssistantGenericUiViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<AssistantGenericUiModel,
                AssistantGenericUiViewBinder.ViewHolder, PropertyKey> {
    static class ViewHolder {
        final ViewGroup mViewContainer;

        ViewHolder(ViewGroup container) {
            mViewContainer = container;
        }
    }

    @Override
    public void bind(AssistantGenericUiModel model, ViewHolder view, PropertyKey propertyKey) {
        if (AssistantGenericUiModel.VIEW == propertyKey) {
            view.mViewContainer.removeAllViews();
            if (model.get(AssistantGenericUiModel.VIEW) != null) {
                view.mViewContainer.addView(model.get(AssistantGenericUiModel.VIEW));
                view.mViewContainer.setVisibility(View.VISIBLE);
            } else {
                view.mViewContainer.setVisibility(View.GONE);
            }
        } else {
            assert false : "Unhandled property detected in AssistantGenericUiViewBinder!";
        }
    }
}
