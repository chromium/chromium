// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiSourceAttributionProperties.HeaderProperties;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiSourceAttributionProperties.SourceCardProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds item PropertyModels to views in the Autofill AI source attribution bottom sheet. */
@NullMarked
/*package*/ class AutofillAiSourceAttributionViewBinder {
    static void bindHeader(PropertyModel model, View view, PropertyKey key) {
        if (key == HeaderProperties.SUBTITLE) {
            TextView subtitleView = view.findViewById(R.id.autofill_ai_attribution_sheet_subtitle);
            subtitleView.setText(model.get(HeaderProperties.SUBTITLE));
        } else {
            assert false : "Unhandled property: " + key;
        }
    }

    static void bindSourceCard(PropertyModel model, View view, PropertyKey key) {
        if (key == SourceCardProperties.ICON_RES_ID) {
            ImageView sourceIcon = view.findViewById(R.id.source_icon);
            sourceIcon.setImageResource(model.get(SourceCardProperties.ICON_RES_ID));
        } else if (key == SourceCardProperties.TITLE) {
            TextView sourceTitle = view.findViewById(R.id.source_title);
            sourceTitle.setText(model.get(SourceCardProperties.TITLE));
        } else if (key == SourceCardProperties.CONTENT_DESCRIPTION) {
            view.setContentDescription(model.get(SourceCardProperties.CONTENT_DESCRIPTION));
        } else if (key == SourceCardProperties.ON_CLICK_LISTENER) {
            Runnable onClickListener = model.get(SourceCardProperties.ON_CLICK_LISTENER);
            view.setOnClickListener(onClickListener != null ? _ -> onClickListener.run() : null);
        } else {
            assert false : "Unhandled property: " + key;
        }
    }

    private AutofillAiSourceAttributionViewBinder() {}
}
