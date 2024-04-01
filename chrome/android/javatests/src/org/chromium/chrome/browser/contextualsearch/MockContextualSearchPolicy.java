// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.chrome.browser.profiles.Profile;

/**
 * A mock ContextualSearchPolicy class that excludes any business logic. TODO(mdjones): Allow the
 * return values of these function to be set.
 */
public class MockContextualSearchPolicy extends ContextualSearchPolicy {
    public MockContextualSearchPolicy(
            Profile profile, ContextualSearchSelectionController selectionController) {
        super(profile, selectionController, null);
    }

    @Override
    public boolean shouldPrefetchSearchResult() {
        return false;
    }

    @Override
    public boolean doSendBasePageUrl() {
        return false;
    }

    @Override
    public boolean isPromoAvailable() {
        return false;
    }

    @Override
    public boolean isUserUndecided() {
        return false;
    }

    @Override
    public boolean shouldPreviousGestureResolve() {
        return true;
    }

    @Override
    public boolean canSendSurroundings() {
        return true;
    }
}
