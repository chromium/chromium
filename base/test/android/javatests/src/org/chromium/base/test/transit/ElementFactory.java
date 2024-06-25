// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import org.chromium.base.Callback;

/** Fabricates new elements after a Condition is first fulfilled. */
public class ElementFactory {
    private Elements mOwner;
    private Callback<Elements.Builder> mDelayedDeclarations;
    private boolean mIsProcessed;

    ElementFactory(Elements owner, Callback<Elements.Builder> delayedDeclarations) {
        assert owner != null;
        assert delayedDeclarations != null;
        mOwner = owner;
        mDelayedDeclarations = delayedDeclarations;
    }

    /**
     * Runs the delayed element declaration callback. Should only be called once.
     *
     * @return Newly declared elements from this factory.
     */
    public Elements processDelayedDeclarations() {
        assert !mIsProcessed
                : "ElementFactory#processDelayedDeclarations should only be called once";
        mIsProcessed = true;
        Elements.Builder builder = mOwner.newBuilder();
        mDelayedDeclarations.onResult(builder);
        Elements newElements = builder.consolidate();
        return newElements;
    }
}
