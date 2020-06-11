// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyObservable;

/**
 * A specialized ModelChangeProcessor for compositor. When a new frame is generated, it will bind
 * the whole Model to the View. If everything is idle, it requests another frame on Property
 * changes.
 */
public class CompositorModelChangeProcessor {
    /**
     * A {@link ObservableSupplier} for the newly generated frame. In addition, this has ability to
     * request another frame.
     */
    public static class FrameRequestSupplier extends ObservableSupplierImpl<Long> {
        private final LayoutManagerHost mHost;

        public FrameRequestSupplier(LayoutManagerHost host) {
            mHost = host;
        }

        /**
         * Request to generate a new frame.
         */
        private void request() {
            mHost.requestRender();
        }
    }

    private final SceneLayer mView;
    private final PropertyModel mModel;
    private final PropertyModelChangeProcessor.ViewBinder mViewBinder;
    private final FrameRequestSupplier mFrameSupplier;
    private final PropertyObservable.PropertyObserver<PropertyKey> mPropertyObserver;
    private final Callback<Long> mNewFrameCallback;

    /**
     * Construct a new CompositorModelChangeProcessor.
     * @param model The model containing the data to be bound to the view.
     * @param view The view which the model will be bound to.
     * @param viewBinder This is used to bind the model to the view.
     * @param frameSupplier A supplier for the new generated frame.
     */
    private CompositorModelChangeProcessor(PropertyModel model, SceneLayer view,
            PropertyModelChangeProcessor.ViewBinder viewBinder, FrameRequestSupplier frameSupplier,
            boolean performInitialBind) {
        mModel = model;
        mView = view;
        mViewBinder = viewBinder;
        mFrameSupplier = frameSupplier;
        mNewFrameCallback = this::onNewFrame;
        mFrameSupplier.addObserver(mNewFrameCallback);

        if (performInitialBind) {
            onPropertyChanged(model, null);
        }

        mPropertyObserver = this::onPropertyChanged;
        model.addObserver(mPropertyObserver);
    }

    /**
     * Creates a CompositorModelChangeProcessor observing the given {@code model} and
     * {@code frameSupplier}.
     * @param model The model containing the data to be bound to the view.
     * @param view The view which the model will be bound to.
     * @param viewBinder This is used to bind the model to the view.
     * @param frameSupplier A supplier for the new generated frame.
     * @param performInitialBind Whether the model should be immediately bound to the view.
     */
    public static CompositorModelChangeProcessor create(PropertyModel model, SceneLayer view,
            PropertyModelChangeProcessor.ViewBinder viewBinder, FrameRequestSupplier frameSupplier,
            boolean performInitialBind) {
        return new CompositorModelChangeProcessor(
                model, view, viewBinder, frameSupplier, performInitialBind);
    }

    /**
     * Creates a CompositorModelChangeProcessor observing the given {@code model} and
     * {@code frameSupplier}. The model will be bound to the view initially, and request a new
     * frame.
     * @param model The model containing the data to be bound to the view.
     * @param view The view which the model will be bound to.
     * @param viewBinder This is used to bind the model to the view.
     * @param frameSupplier A supplier for the new generated frame.
     */
    public static CompositorModelChangeProcessor create(PropertyModel model, SceneLayer view,
            PropertyModelChangeProcessor.ViewBinder viewBinder,
            FrameRequestSupplier frameSupplier) {
        return create(model, view, viewBinder, frameSupplier, true);
    }

    /**
     * Clean up members.
     */
    public void destroy() {
        mModel.removeObserver(mPropertyObserver);
        mFrameSupplier.removeObserver(mNewFrameCallback);
    }

    private void onNewFrame(Long time) {
        pushUpdate();
    }

    private void pushUpdate() {
        mViewBinder.bind(mModel, mView, null);
    }

    private void onPropertyChanged(PropertyObservable<PropertyKey> model, PropertyKey propertyKey) {
        assert model == mModel;

        mFrameSupplier.request();
    }
}