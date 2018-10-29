// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.chromium.chrome.browser.util.ViewUtils.dpToPx;

import android.content.Context;
import android.content.res.Configuration;
import android.support.annotation.IdRes;
import android.support.annotation.StringRes;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.text.style.ForegroundColorSpan;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.ChromeBulletSpan;

/**
 * The Material Design New Tab Page for use in the Incognito profile. This is an extension
 * of the IncognitoNewTabPageView class with improved text content and a more responsive design.
 */
public class IncognitoNewTabPageViewMD extends IncognitoNewTabPageView {
    private final Context mContext;

    private int mWidthDp;
    private int mHeightDp;

    private LinearLayout mContainer;
    private TextView mHeader;
    private TextView mSubtitle;
    private LinearLayout mBulletpointsContainer;
    private TextView mLearnMore;
    private TextView[] mParagraphs;

    private static final int BULLETPOINTS_HORIZONTAL_SPACING_DP = 40;
    private static final int CONTENT_WIDTH_DP = 600;
    private static final int WIDE_LAYOUT_THRESHOLD_DP = 720;

    /** Default constructor needed to inflate via XML. */
    public IncognitoNewTabPageViewMD(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mWidthDp = mContext.getResources().getConfiguration().screenWidthDp;
        mHeightDp = mContext.getResources().getConfiguration().screenHeightDp;

        populateBulletpoints(R.id.new_tab_incognito_features, R.string.new_tab_otr_not_saved);
        populateBulletpoints(R.id.new_tab_incognito_warning, R.string.new_tab_otr_visible);

        mContainer = (LinearLayout) findViewById(R.id.new_tab_incognito_container);
        mHeader = (TextView) findViewById(R.id.new_tab_incognito_title);
        mSubtitle = (TextView) findViewById(R.id.new_tab_incognito_subtitle);
        mLearnMore = (TextView) findViewById(R.id.learn_more);
        mParagraphs =
                new TextView[] {mSubtitle, (TextView) findViewById(R.id.new_tab_incognito_features),
                        (TextView) findViewById(R.id.new_tab_incognito_warning), mLearnMore};
        mBulletpointsContainer =
                (LinearLayout) findViewById(R.id.new_tab_incognito_bulletpoints_container);

        adjustView();
    }

    private void adjustView() {
        adjustIcon();
        adjustLayout();
        adjustLearnMore();
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // View#onConfigurationChanged() doesn't get called when resizing this view in
        // multi-window mode, so #onMeasure() is used instead.
        Configuration config = getContext().getResources().getConfiguration();
        if (mWidthDp != config.screenWidthDp || mHeightDp != config.screenHeightDp) {
            mWidthDp = config.screenWidthDp;
            mHeightDp = config.screenHeightDp;
            adjustView();
        }

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * @param element Resource ID of the element to be populated with the bulletpoints.
     * @param content String ID to serve as the text of |element|. Must contain an <em></em> span,
     *         which will be emphasized, and three <li> items, which will be converted to
     *         bulletpoints.
     * Populates |element| with |content|.
     */
    private void populateBulletpoints(@IdRes int element, @StringRes int content) {
        TextView view = (TextView) findViewById(element);
        String text = mContext.getResources().getString(content);

        // TODO(msramek): Unfortunately, our strings are missing the closing "</li>" tag, which
        // is not a problem when they're used in the Desktop WebUI (omitting the tag is valid in
        // HTML5), but it is a problem for SpanApplier. Update the strings and remove this regex.
        // Note that modifying the strings is a non-trivial operation as they went through a special
        // translation process.
        text = text.replaceAll("<li>([^<]+)\n", "<li>$1</li>\n");

        // Format the bulletpoints:
        //   - Disambiguate the <li></li> spans for SpanApplier.
        //   - Remove leading whitespace (caused by formatting in the .grdp file)
        //   - Remove the trailing newline after the last bulletpoint.
        text = text.replaceFirst(" *<li>([^<]*)</li>", "<li1>$1</li1>");
        text = text.replaceFirst(" *<li>([^<]*)</li>", "<li2>$1</li2>");
        text = text.replaceFirst(" *<li>([^<]*)</li>\n", "<li3>$1</li3>");

        // Remove the <ul></ul> tags which serve no purpose here, including the whitespace around
        // them.
        text = text.replaceAll(" *</?ul>\\n?", "");

        view.setText(SpanApplier.applySpans(text,
                new SpanApplier.SpanInfo("<em>", "</em>",
                        new ForegroundColorSpan(ApiCompatibilityUtils.getColor(
                                mContext.getResources(), R.color.incognito_emphasis))),
                new SpanApplier.SpanInfo("<li1>", "</li1>", new ChromeBulletSpan(mContext)),
                new SpanApplier.SpanInfo("<li2>", "</li2>", new ChromeBulletSpan(mContext)),
                new SpanApplier.SpanInfo("<li3>", "</li3>", new ChromeBulletSpan(mContext))));
    }

    /** Adjusts the paddings, margins, and the orientation of bulletpoints. */
    private void adjustLayout() {
        int paddingHorizontalDp;
        int paddingVerticalDp;

        boolean bulletpointsArrangedHorizontally;

        if (mWidthDp <= WIDE_LAYOUT_THRESHOLD_DP) {
            // Small padding.
            paddingHorizontalDp = mWidthDp <= 240 ? 24 : 32;
            paddingVerticalDp = 32;

            // Align left.
            mContainer.setGravity(Gravity.START);

            // Decide the bulletpoints orientation.
            bulletpointsArrangedHorizontally = false;

            // The subtitle is sized automatically, but not wider than CONTENT_WIDTH_DP.
            mSubtitle.setLayoutParams(
                    new LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT,
                            LinearLayout.LayoutParams.WRAP_CONTENT));
            mSubtitle.setMaxWidth(dpToPx(mContext, CONTENT_WIDTH_DP));

            // The bulletpoints container takes the same width as subtitle. Since the width can
            // not be directly measured at this stage, we must calculate it manually.
            mBulletpointsContainer.setLayoutParams(new LinearLayout.LayoutParams(
                    dpToPx(mContext,
                            Math.min(CONTENT_WIDTH_DP, mWidthDp - 2 * paddingHorizontalDp)),
                    LinearLayout.LayoutParams.WRAP_CONTENT));
        } else {
            // Large padding.
            paddingHorizontalDp = 0; // Should not be necessary on a screen this large.
            paddingVerticalDp = mHeightDp <= 320 ? 16 : 72;

            // Align to the center.
            mContainer.setGravity(Gravity.CENTER_HORIZONTAL);

            // Decide the bulletpoints orientation.
            bulletpointsArrangedHorizontally = true;

            int contentWidthPx = dpToPx(mContext, CONTENT_WIDTH_DP);
            mSubtitle.setLayoutParams(new LinearLayout.LayoutParams(
                    contentWidthPx, LinearLayout.LayoutParams.WRAP_CONTENT));
            mBulletpointsContainer.setLayoutParams(new LinearLayout.LayoutParams(
                    contentWidthPx, LinearLayout.LayoutParams.WRAP_CONTENT));
        }

        // Apply the bulletpoints orientation.
        if (bulletpointsArrangedHorizontally) {
            mBulletpointsContainer.setOrientation(LinearLayout.HORIZONTAL);
        } else {
            mBulletpointsContainer.setOrientation(LinearLayout.VERTICAL);
        }

        // Set up paddings and margins.
        int paddingTop;
        int paddingBottom;
        paddingTop = paddingBottom = dpToPx(mContext, paddingVerticalDp);
        mContainer.setPadding(dpToPx(mContext, paddingHorizontalDp), paddingTop,
                dpToPx(mContext, paddingHorizontalDp), paddingBottom);

        int spacingPx =
                (int) Math.ceil(mParagraphs[0].getTextSize() * (mHeightDp <= 600 ? 1 : 1.5));

        for (TextView paragraph : mParagraphs) {
            // If bulletpoints are arranged horizontally, there should be space between them.
            int rightMarginPx = (bulletpointsArrangedHorizontally
                                        && paragraph == mBulletpointsContainer.getChildAt(0))
                    ? dpToPx(mContext, BULLETPOINTS_HORIZONTAL_SPACING_DP)
                    : 0;

            ((LinearLayout.LayoutParams) paragraph.getLayoutParams())
                    .setMargins(0, spacingPx, rightMarginPx, 0);
            paragraph.setLayoutParams(paragraph.getLayoutParams()); // Apply the new layout.
        }

        ((LinearLayout.LayoutParams) mHeader.getLayoutParams()).setMargins(0, spacingPx, 0, 0);
        mHeader.setLayoutParams(mHeader.getLayoutParams()); // Apply the new layout.
    }

    /** Adjust the Incognito icon. */
    private void adjustIcon() {
        // The icon resource is 120dp x 120dp (i.e. 120px x 120px at MDPI). This method always
        // resizes the icon view to 120dp x 120dp or smaller, therefore image quality is not lost.

        int sizeDp;
        if (mWidthDp <= WIDE_LAYOUT_THRESHOLD_DP) {
            sizeDp = (mWidthDp <= 240 || mHeightDp <= 480) ? 48 : 72;
        } else {
            sizeDp = mHeightDp <= 480 ? 72 : 120;
        }

        ImageView icon = (ImageView) findViewById(R.id.new_tab_incognito_icon);
        icon.getLayoutParams().width = dpToPx(mContext, sizeDp);
        icon.getLayoutParams().height = dpToPx(mContext, sizeDp);
    }

    /** Adjust the "Learn More" link. */
    private void adjustLearnMore() {
        final String subtitleText =
                mContext.getResources().getString(R.string.new_tab_otr_subtitle);
        boolean learnMoreInSubtitle = mWidthDp > WIDE_LAYOUT_THRESHOLD_DP;

        mLearnMore.setVisibility(learnMoreInSubtitle ? View.GONE : View.VISIBLE);

        if (!learnMoreInSubtitle) {
            // Revert to the original text.
            mSubtitle.setText(subtitleText);
            mSubtitle.setMovementMethod(null);
            return;
        }

        // Concatenate the original text with a clickable "Learn more" link.
        StringBuilder concatenatedText = new StringBuilder();
        concatenatedText.append(subtitleText);
        concatenatedText.append(" ");
        concatenatedText.append(mContext.getResources().getString(R.string.learn_more));
        SpannableString textWithLearnMoreLink = new SpannableString(concatenatedText.toString());

        NoUnderlineClickableSpan span = new NoUnderlineClickableSpan(
                R.color.modern_blue_300, (view) -> getManager().loadIncognitoLearnMore());
        textWithLearnMoreLink.setSpan(
                span, subtitleText.length() + 1, textWithLearnMoreLink.length(), 0 /* flags */);
        mSubtitle.setText(textWithLearnMoreLink);
        mSubtitle.setMovementMethod(LinkMovementMethod.getInstance());
    }
}
