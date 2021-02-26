// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyObservable;

/** A customized model change processor for the SectionHeaderList. */
public class SectionHeaderListMcp implements ListObservable.ListObserver<PropertyKey>,
                                             PropertyObservable.PropertyObserver<PropertyKey> {
    private final SectionHeaderView mView;
    private final SectionHeaderViewBinder mBinder;
    private final SectionHeaderList mModel;

    public SectionHeaderListMcp(
            SectionHeaderList model, SectionHeaderView view, SectionHeaderViewBinder viewBinder) {
        mView = view;
        mBinder = viewBinder;
        mModel = model;
        mModel.addObserver(this);
        mModel.getHeaders().addObserver(this);
    }

    public void destroy() {
        mModel.removeObserver(this);
        mModel.getHeaders().removeObserver(this);
    }

    @Override
    public void onPropertyChanged(PropertyObservable<PropertyKey> source, PropertyKey propertyKey) {
        mBinder.bind(mModel, mView, propertyKey);
    }

    @Override
    public void onItemRangeInserted(ListObservable source, int index, int count) {
        mBinder.onItemsInserted(mModel, mView, index, count);
    }

    @Override
    public void onItemRangeRemoved(ListObservable source, int index, int count) {
        mBinder.onItemsRemoved(mModel, mView, index, count);
    }

    @Override
    public void onItemRangeChanged(
            ListObservable<PropertyKey> source, int index, int count, PropertyKey payload) {
        mBinder.onItemsChanged(mModel, mView, index, count, payload);
    }
}
