// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.support.v7.widget.RecyclerView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcpBase;

/**
 * Test utility class to allow using {@link
 * org.chromium.ui.modelutil.SimpleRecyclerViewMcpBase.ViewBinder} classes to be used in conjunction
 * with a {@link PropertyModelChangeProcessor} so that individual items in the RecyclerView can be
 * tested independently.
 * @param <VH> The ViewHolder class to be used.
 */
public class TestRecyclerViewSimpleViewBinder<VH extends RecyclerView.ViewHolder>
        implements PropertyModelChangeProcessor.ViewBinder<PropertyModel, VH, PropertyKey> {
    SimpleRecyclerViewMcpBase.ViewBinder<PropertyModel, VH, PropertyKey> mInternalViewBinder;

    /**
     * Main constructor
     * @param viewBinder The {@link org.chromium.ui.modelutil.SimpleRecyclerViewMcpBase.ViewBinder}
     *         to wrap around.
     */
    TestRecyclerViewSimpleViewBinder(
            SimpleRecyclerViewMcpBase.ViewBinder<PropertyModel, VH, PropertyKey> viewBinder) {
        mInternalViewBinder = viewBinder;
    }

    @Override
    public void bind(PropertyModel model, VH viewHolder, PropertyKey propertyKey) {
        mInternalViewBinder.onBindViewHolder(viewHolder, model, propertyKey);
    }
}