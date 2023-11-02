// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.components.page_info.proto.AboutThisSiteMetadataProto.SiteDescription;
import org.chromium.components.page_info.proto.AboutThisSiteMetadataProto.SiteInfo;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * View for the AboutThisSite subpage. Shows a description and a source link.
 */
public class AboutThisSiteView extends LinearLayout implements View.OnClickListener {
    public static final String LINK_START = "<link>";
    public static final String LINK_END = "</link>";
    private TextView mDescriptionView;
    private TextView mSourceView;
    private Runnable mOnSourceClicked;

    public AboutThisSiteView(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public void setSiteInfo(SiteInfo siteInfo, Runnable onSourceClicked) {
        mOnSourceClicked = onSourceClicked;

        SiteDescription description = siteInfo.getDescription();
        mDescriptionView.setText(description.getDescription());

        String link = LINK_START + description.getSource().getLabel() + LINK_END;
        String sourceString = getContext().getResources().getString(
                R.string.page_info_about_this_site_subpage_from_label, link);
        mSourceView.setText(SpanApplier.applySpans(sourceString,
                new SpanInfo(LINK_START, LINK_END,
                        new NoUnderlineClickableSpan(getContext(), this::onClick))));
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mDescriptionView = findViewById(R.id.description_text);
        mSourceView = findViewById(R.id.source_link);
        mSourceView.setMovementMethod(LinkMovementMethod.getInstance());
    }

    @Override
    public void onClick(View view) {
        if (view == mSourceView) {
            if (mOnSourceClicked != null) {
                mOnSourceClicked.run();
            }
        } else {
            assert false : "Click on view not handled: " + view;
        }
    }
}
