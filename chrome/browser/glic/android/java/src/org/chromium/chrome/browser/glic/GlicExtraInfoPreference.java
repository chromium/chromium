// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * A custom preference for displaying the AI info box. Applies the correct span to the learn more
 * link in the layout.
 */
@NullMarked
public class GlicExtraInfoPreference extends ChromeBasePreference {
    private @Nullable Runnable mOnLearnMoreClicked;
    private int mTextResId = R.string.settings_ai_page_main_managed_sublabel_3;
    private boolean mApplySpan = true;

    public GlicExtraInfoPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /** Sets the text resource ID and whether to apply the link span. */
    public void setTextResId(int resId, boolean applySpan) {
        mTextResId = resId;
        mApplySpan = applySpan;
        notifyChanged();
    }

    /** Sets the callback that should fire when the learn more link is clicked. */
    public void setOnLearnMoreClicked(@Nullable Runnable onLearnMoreClicked) {
        mOnLearnMoreClicked = onLearnMoreClicked;
        notifyChanged();
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        TextView textView = (TextView) holder.findViewById(R.id.ai_info_managed_sublabel_3);
        if (textView == null) return;

        Context context = getContext();
        String fullText = context.getString(mTextResId);

        if (mApplySpan) {
            SpanApplier.SpanInfo spanInfo =
                    new SpanApplier.SpanInfo(
                            "<link>",
                            "</link>",
                            new ChromeClickableSpan(
                                    context,
                                    v -> {
                                        if (mOnLearnMoreClicked != null) {
                                            mOnLearnMoreClicked.run();
                                        }
                                    }));

            textView.setText(SpanApplier.applySpans(fullText, spanInfo));
            textView.setMovementMethod(LinkMovementMethod.getInstance());
        } else {
            textView.setText(fullText);
            textView.setMovementMethod(null);
        }
    }
}
