// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.build.annotations.NullMarked;

/**
 * A {@link CarryOn} that contains a single {@link ViewElement}.
 *
 * @param <ViewT> the type of the View.
 */
@NullMarked
public class ViewCarryOn<ViewT extends View> extends CarryOn {
    public final ViewElement<ViewT> viewElement;

    /**
     * Constructor. Optionally, use the convenience #create() methods.
     *
     * @param viewSpec the {@link ViewSpec} with class and matcher that specify the View.
     * @param options the {@link ViewElement.Options} for the ViewElement.
     */
    public ViewCarryOn(ViewSpec<ViewT> viewSpec, ViewElement.Options options) {
        super();
        viewElement = declareView(viewSpec.getViewClass(), viewSpec.getViewMatcher(), options);
    }

    /**
     * Convenience method to create a {@link ViewCarryOn} from Class + Matcher instead of ViewSpec.
     */
    public static <ViewT extends View> ViewCarryOn<ViewT> create(
            Class<ViewT> viewClass, Matcher<View> viewMatcher, ViewElement.Options options) {
        return new ViewCarryOn<>(viewSpec(viewClass, viewMatcher), options);
    }

    /**
     * Returns the View associated with this {@link ViewCarryOn}.
     *
     * <p>Ensures the View has been found previously (by transitioning into this {@link
     * ViewCarryOn}).
     */
    public ViewT getView() {
        return viewElement.value();
    }
}
