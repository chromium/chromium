// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Encapsulates the logic of binding a {@link NullableObservableSupplier<PropertyModel>} to a {@link
 * View} using {@link ActionButtonBinder}.
 */
@NullMarked
public class ActionViewBinding {
    private final NullableObservableSupplier<PropertyModel> mSupplier;
    private final View mView;
    private final Callback<@Nullable PropertyModel> mCallback = this::onPropertyModelChanged;
    private final PropertyModelChangeProcessor.ViewBinder<PropertyModel, View, PropertyKey> mBinder;
    private @Nullable PropertyModelChangeProcessor<PropertyModel, View, PropertyKey> mMcp;

    /**
     * Creates a new {@link ActionViewBinding}.
     *
     * @param supplier The observable supplier for the action property model.
     * @param view The view to bind the action to. This should be the view that represents the
     *     action button.
     */
    public ActionViewBinding(NullableObservableSupplier<PropertyModel> supplier, View view) {
        this(supplier, view, ActionButtonBinder::bind);
    }

    /**
     * Creates a new {@link ActionViewBinding}.
     *
     * @param supplier The observable supplier for the action property model.
     * @param view The view to bind the action to.
     * @param binder The {@link ViewBinder} to use for binding the action model to the view.
     */
    public ActionViewBinding(
            NullableObservableSupplier<PropertyModel> supplier,
            View view,
            PropertyModelChangeProcessor.ViewBinder<PropertyModel, View, PropertyKey> binder) {
        mSupplier = supplier;
        mView = view;
        mBinder = binder;
        mSupplier.addSyncObserverAndCallIfNonNull(mCallback);
    }

    private void onPropertyModelChanged(@Nullable PropertyModel actionModel) {
        if (mMcp != null) {
            mMcp.destroy();
            mMcp = null;
        }
        if (actionModel != null) {
            mMcp = PropertyModelChangeProcessor.create(actionModel, mView, mBinder);
        }
    }

    public void destroy() {
        mSupplier.removeObserver(mCallback);
        if (mMcp != null) {
            mMcp.destroy();
        }
    }
}
