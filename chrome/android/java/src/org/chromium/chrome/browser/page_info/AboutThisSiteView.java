// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatTextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.components.page_info.proto.AboutThisSiteMetadataProto.SiteDescription;
import org.chromium.components.page_info.proto.AboutThisSiteMetadataProto.SiteInfo;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * View for the AboutThisSite subpage. Shows a description and a source link.
 */
public class AboutThisSiteView extends LinearLayout {
    public static final String LINK_START = "<link>";
    public static final String LINK_END = "</link>";
    private final TextView mDescriptionView;
    private final TextView mSourceView;

    public AboutThisSiteView(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        setOrientation(LinearLayout.VERTICAL);
        int paddingSides = context.getResources().getDimensionPixelSize(
                org.chromium.components.page_info.R.dimen.page_info_popup_padding_sides);
        int paddingVertical = context.getResources().getDimensionPixelSize(
                org.chromium.components.page_info.R.dimen.page_info_popup_padding_vertical);
        setPadding(paddingSides, paddingVertical, paddingSides, paddingVertical);

        mDescriptionView = new AppCompatTextView(context);
        mDescriptionView.setPadding(0, 0, 0, paddingSides);
        ApiCompatibilityUtils.setTextAppearance(
                mDescriptionView, R.style.TextAppearance_TextMedium_Secondary);
        addView(mDescriptionView);

        mSourceView = new AppCompatTextView(context);
        mSourceView.setMovementMethod(LinkMovementMethod.getInstance());
        ApiCompatibilityUtils.setTextAppearance(
                mSourceView, R.style.TextAppearance_TextMedium_Secondary);
        addView(mSourceView);
    }

    public void setSiteInfo(SiteInfo siteInfo, Runnable onSourceClicked) {
        SiteDescription description = siteInfo.getDescription();
        mDescriptionView.setText(description.getDescription());

        String link = LINK_START + description.getSource().getLabel() + LINK_END;
        final ClickableSpan linkSpan =
                new NoUnderlineClickableSpan(getContext(), (view) -> { onSourceClicked.run(); });
        String sourceString = getContext().getResources().getString(
                R.string.page_info_about_this_site_subpage_from_label, link);
        mSourceView.setText(
                SpanApplier.applySpans(sourceString, new SpanInfo(LINK_START, LINK_END, linkSpan)));
    }
}
