// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A mechanism binding AnswerSuggestion properties to its view. */
public class AnswerSuggestionViewBinder extends BaseSuggestionViewBinder {
    /** @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object) */
    @Override
    public void bind(PropertyModel model, BaseSuggestionView view, PropertyKey propertyKey) {
        super.bind(model, view, propertyKey);

        if (AnswerSuggestionViewProperties.TEXT_LINE_1_TEXT == propertyKey) {
            TextView tv = view.findContentView(R.id.omnibox_answer_line_1);
            tv.setText(model.get(AnswerSuggestionViewProperties.TEXT_LINE_1_TEXT));
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_2_TEXT == propertyKey) {
            TextView tv = view.findContentView(R.id.omnibox_answer_line_2);
            tv.setText(model.get(AnswerSuggestionViewProperties.TEXT_LINE_2_TEXT));
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION
                == propertyKey) {
            TextView tv = view.findContentView(R.id.omnibox_answer_line_1);
            tv.setContentDescription(model.get(
                    AnswerSuggestionViewProperties.TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION));
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION
                == propertyKey) {
            TextView tv = view.findContentView(R.id.omnibox_answer_line_2);
            tv.setContentDescription(model.get(
                    AnswerSuggestionViewProperties.TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION));
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_1_MAX_LINES == propertyKey) {
            TextView tv = view.findContentView(R.id.omnibox_answer_line_1);
            tv.setMaxLines(model.get(AnswerSuggestionViewProperties.TEXT_LINE_1_MAX_LINES));
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_2_MAX_LINES == propertyKey) {
            TextView tv = view.findContentView(R.id.omnibox_answer_line_2);
            tv.setMaxLines(model.get(AnswerSuggestionViewProperties.TEXT_LINE_2_MAX_LINES));
        }
    }
}
