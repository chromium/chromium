// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A mechanism binding EntitySuggestion properties to its view. */
public class EntitySuggestionViewBinder extends BaseSuggestionViewBinder {
    /** @see BaseSuggestionViewBinder#bind(PropertyModel, BaseSuggestionView, PropertyKey) */
    @Override
    public void bind(PropertyModel model, BaseSuggestionView view, PropertyKey propertyKey) {
        super.bind(model, view, propertyKey);

        if (EntitySuggestionViewProperties.SUBJECT_TEXT == propertyKey) {
            TextView tv = view.findContentView(R.id.entity_subject);
            tv.setText(model.get(EntitySuggestionViewProperties.SUBJECT_TEXT));
        } else if (EntitySuggestionViewProperties.DESCRIPTION_TEXT == propertyKey) {
            TextView tv = view.findContentView(R.id.entity_description);
            tv.setText(model.get(EntitySuggestionViewProperties.DESCRIPTION_TEXT));
        }
    }
}
