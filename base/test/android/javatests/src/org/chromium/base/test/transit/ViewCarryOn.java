// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.view.View;

/**
 * A {@link CarryOn} that contains a single {@link ViewElement}.
 *
 * @param <ViewT> the type of the View.
 */
public class ViewCarryOn<ViewT extends View> extends CarryOn {
    public final ViewElement<ViewT> viewElement;

    public ViewCarryOn(ViewSpec<ViewT> viewSpec, ViewElement.Options options) {
        super();
        viewElement = declareView(viewSpec.getViewClass(), viewSpec.getViewMatcher(), options);
    }

    public ViewT getView() {
        return viewElement.value();
    }
}
