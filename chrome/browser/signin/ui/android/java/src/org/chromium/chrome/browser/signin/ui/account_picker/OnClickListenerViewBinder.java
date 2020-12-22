// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.account_picker;

import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * This class binds an {@link OnClickListener} to a {@link View}.
 * TODO(crbug/1155123): Change this class to package internal after modularization
 */
public class OnClickListenerViewBinder implements ViewBinder<PropertyModel, View, PropertyKey> {
    private final ReadableObjectPropertyKey<OnClickListener> mOnClickListenerKey;

    /**
     * TODO(crbug/1155123): Change this method to package internal after modularization
     */
    public OnClickListenerViewBinder(
            ReadableObjectPropertyKey<OnClickListener> onClickListenerKey) {
        mOnClickListenerKey = onClickListenerKey;
    }

    /**
     * View binder that sets the {@link OnClickListener} of the view with the corresponding
     * property in model.
     */
    @Override
    public void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        assert propertyKey == mOnClickListenerKey : "Unknown  propertyKey: " + propertyKey;
        view.setOnClickListener(model.get(mOnClickListenerKey));
    }
}
