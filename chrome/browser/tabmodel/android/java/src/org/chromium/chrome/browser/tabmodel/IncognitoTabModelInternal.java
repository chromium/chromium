// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;

/** Glue type to combine {@link IncognitoTabModel} and {@link TabModelInternal} interfaces. */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
@NullMarked
public interface IncognitoTabModelInternal extends IncognitoTabModel, TabModelInternal {
    /**
     * Adds a listener that is notified when the current delegate model changes. The delegate model
     * is the model that the incognito tab model wraps and delegates operations to. This allows us
     * to tear down the delegate model with the lifecycle of the OTR profile while always having an
     * Incognito version of the tab model in existence.
     */
    /*package*/ void addDelegateModelObserver(Callback<TabModelInternal> delegateModel);
}
