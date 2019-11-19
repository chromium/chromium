// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.drawable.Drawable;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * This is a {@link PropertyModel} for the TabSuggestionMessageCard.
 */
public class TabSuggestionMessageCardViewModel
        extends PropertyModel implements MessageCardView.IconProvider {
    // TODO(meiliang): should take in a TabSuggestion object from the suggestion service, and set
    // the property model based on the TabSuggestion object. ACTION_TEXT and DESCRIPTION_TEXT should
    // set based on the TabSuggestion object.
    public TabSuggestionMessageCardViewModel() {
        super(MessageCardViewProperties.ALL_KEYS);
        set(MessageCardViewProperties.ICON_PROVIDER, this);
    }

    @Override
    public Drawable getIconDrawable() {
        // TODO(meiliang): returns a drawable with first tab suggested tab's favicon.
        return null;
    }
}
