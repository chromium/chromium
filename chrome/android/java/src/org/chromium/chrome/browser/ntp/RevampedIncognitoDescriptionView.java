// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.chromium.ui.base.ViewUtils.dpToPx;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.widget.CompoundButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.annotation.StringRes;
import androidx.appcompat.widget.LinearLayoutCompat;
import androidx.appcompat.widget.SwitchCompat;

import org.chromium.chrome.R;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.ChromeBulletSpan;

/** The view to describe revamped incognito mode. */
public class RevampedIncognitoDescriptionView extends LinearLayout
        implements IncognitoDescriptionView {
    private Resources mResources;

    private int mWidthPx;
    private int mHeightPx;

    private TextView mTitle;
    private LinearLayout mContainer;
    private LinearLayout mContent;
    private LinearLayout mDescriptionTextContainer;
    private LinearLayout mDoesLayout;
    private LinearLayout mDoesNotLayout;
    private TextView mLearnMore;
    private RelativeLayout mCookieControlsCard;
    private SwitchCompat mCookieControlsToggle;
    private ImageView mCookieControlsManagedIcon;
    private TextView mCookieControlsTitle;
    private TextView mCookieControlsSubtitle;

    /** Default constructor needed to inflate via XML. */
    public RevampedIncognitoDescriptionView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void setLearnMoreOnclickListener(OnClickListener onClickListener) {
        // Adjust LearnMore text and add the callback for LearnMore link.
        adjustLearnMore(onClickListener);
    }

    @Override
    public void setCookieControlsToggleOnCheckedChangeListener(
            CompoundButton.OnCheckedChangeListener listener) {
        if (!findCookieControlElements()) return;
        mCookieControlsToggle.setOnCheckedChangeListener(listener);
    }

    @Override
    public void setCookieControlsToggle(boolean enabled) {
        if (!findCookieControlElements()) return;
        mCookieControlsToggle.setChecked(enabled);
    }

    @Override
    public void setCookieControlsIconOnclickListener(OnClickListener listener) {
        if (!findCookieControlElements()) return;
        mCookieControlsManagedIcon.setOnClickListener(listener);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mResources = getContext().getResources();

        mWidthPx = dpToPx(getContext(), mResources.getConfiguration().screenWidthDp);
        mHeightPx = dpToPx(getContext(), mResources.getConfiguration().screenHeightDp);

        mContainer = findViewById(R.id.revamped_incognito_ntp_container);

        populateDescriptions(
                R.id.revamped_incognito_ntp_does_description_view,
                R.string.revamped_incognito_ntp_does_description);
        populateDescriptions(
                R.id.revamped_incognito_ntp_does_not_description_view,
                R.string.revamped_incognito_ntp_does_not_description);

        mTitle = findViewById(R.id.revamped_incognito_ntp_title);
        mContent = findViewById(R.id.revamped_incognito_ntp_content);
        mDescriptionTextContainer =
                findViewById(R.id.revamped_incognito_ntp_description_text_container);
        mDoesLayout = findViewById(R.id.revamped_incognito_ntp_does_layout);
        mDoesNotLayout = findViewById(R.id.revamped_incognito_ntp_does_not_layout);
        mLearnMore = findViewById(R.id.revamped_incognito_ntp_learn_more);
        mCookieControlsCard = findViewById(R.id.revamped_cookie_controls_card);

        adjustLayout();
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // View#onConfigurationChanged() doesn't get called when resizing this view in
        // multi-window mode, so #onMeasure() is used instead.
        Configuration config = mResources.getConfiguration();
        int widthPx = dpToPx(getContext(), config.screenWidthDp);
        int heightPx = dpToPx(getContext(), config.screenHeightDp);
        if (mWidthPx != widthPx || mHeightPx != heightPx) {
            mWidthPx = widthPx;
            mHeightPx = heightPx;
            adjustLayout();
        }

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * @param element Resource ID of the element to be populated with the description.
     * @param content String ID to serve as the text of |element|. Must contain three <li></li>
     *         items, which will be converted to bulletpoints.
     * Populates |element| with |content|.
     */
    private void populateDescriptions(@IdRes int element, @StringRes int content) {
        TextView view = (TextView) findViewById(element);
        String text = getContext().getResources().getString(content);

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

        view.setText(
                SpanApplier.applySpans(
                        text,
                        new SpanApplier.SpanInfo(
                                "<li1>", "</li1>", new ChromeBulletSpan(getContext())),
                        new SpanApplier.SpanInfo(
                                "<li2>", "</li2>", new ChromeBulletSpan(getContext())),
                        new SpanApplier.SpanInfo(
                                "<li3>", "</li3>", new ChromeBulletSpan(getContext()))));
    }

    /**
     * Adjusts the paddings, margins, and the orientation of "does", "doesn't do" and LearnMore
     * containers.
     */
    private void adjustLayout() {
        int paddingHorizontalPx;
        int paddingVerticalPx;

        int contentMaxWidthPx =
                mResources.getDimensionPixelSize(R.dimen.incognito_ntp_content_max_width);
        int contentWidthPx;

        int doesTopMarginPx =
                mResources.getDimensionPixelSize(R.dimen.incognito_ntp_does_and_doesnt_top_spacing);
        int descriptionsWeight = mResources.getInteger(R.integer.descriptions_weight);

        if (isNarrowScreen()) {
            // Small padding.
            int thresholdPx =
                    mResources.getDimensionPixelSize(
                            R.dimen.incognito_ntp_portrait_small_or_big_threshold);
            paddingHorizontalPx =
                    mResources.getDimensionPixelSize(
                            mWidthPx <= thresholdPx
                                    ? R.dimen.incognito_ntp_portrait_horizontal_small_padding
                                    : R.dimen.incognito_ntp_portrait_horizontal_big_padding);
            paddingVerticalPx =
                    mResources.getDimensionPixelSize(
                            R.dimen.incognito_ntp_portrait_vertical_padding);

            mContainer.setGravity(Gravity.START);

            mDescriptionTextContainer.setOrientation(LinearLayout.VERTICAL);
            // Since the width can not be directly measured at this stage, we must calculate it.
            contentWidthPx = Math.min(contentMaxWidthPx, mWidthPx - 2 * paddingHorizontalPx);

            // Set layout params for portrait orientation. Must be done programmatically to cover
            // the case when the user switches from landscape to portrait.
            LinearLayout.LayoutParams layoutParams =
                    new LinearLayoutCompat.LayoutParams(
                            LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
            layoutParams.setMargins(0, doesTopMarginPx, 0, 0);

            mDoesLayout.setLayoutParams(layoutParams);
            mDoesNotLayout.setLayoutParams(layoutParams);
        } else {
            // Large padding.
            int thresholdPx =
                    mResources.getDimensionPixelSize(
                            R.dimen.incognito_ntp_landscape_small_or_big_threshold);
            paddingHorizontalPx =
                    mResources.getDimensionPixelSize(
                            R.dimen.incognito_ntp_landscape_horizontal_padding);
            paddingVerticalPx =
                    mResources.getDimensionPixelSize(
                            mHeightPx <= thresholdPx
                                    ? R.dimen.incognito_ntp_landscape_vertical_small_padding
                                    : R.dimen.incognito_ntp_landscape_vertical_big_padding);

            mContainer.setGravity(Gravity.CENTER_HORIZONTAL);

            mDescriptionTextContainer.setOrientation(LinearLayout.HORIZONTAL);
            // Since the width can not be directly measured at this stage, we must calculate it.
            contentWidthPx = Math.min(contentMaxWidthPx, mWidthPx - 2 * paddingHorizontalPx);

            // Set layout params for landscape orientation.
            int doesRightMarginPx =
                    mResources.getDimensionPixelSize(
                            R.dimen.incognito_ntp_descriptions_horizontal_spacing);
            LinearLayout.LayoutParams layoutParamsDoes =
                    new LinearLayoutCompat.LayoutParams(
                            0, LayoutParams.WRAP_CONTENT, descriptionsWeight);
            layoutParamsDoes.setMargins(0, doesTopMarginPx, doesRightMarginPx, 0);
            mDoesLayout.setLayoutParams(layoutParamsDoes);

            LinearLayout.LayoutParams layoutParamsDoesNot =
                    new LinearLayoutCompat.LayoutParams(
                            0, LayoutParams.WRAP_CONTENT, descriptionsWeight);
            layoutParamsDoesNot.setMargins(0, doesTopMarginPx, 0, 0);
            mDoesNotLayout.setLayoutParams(layoutParamsDoesNot);
        }

        mContent.setLayoutParams(
                new LinearLayout.LayoutParams(
                        contentWidthPx, LinearLayout.LayoutParams.WRAP_CONTENT));

        // The learn more text view has height of min_touch_target_size. This effectively
        // creates padding above and below it, depending on Android font size settings.
        // We want to have a R.dimen.learn_more_vertical_spacing tall gap between the learn more
        // text and the adjacent elements. So adjust the margin to be the difference between
        // targeted spacing and effective padding.
        int innerSpacing =
                (int)
                        ((getContext()
                                                .getResources()
                                                .getDimensionPixelSize(
                                                        R.dimen.min_touch_target_size)
                                        - mLearnMore.getTextSize())
                                / 2);
        int learnMoreVerticalMargin =
                mResources.getDimensionPixelSize(R.dimen.incognito_ntp_learn_more_vertical_spacing)
                        - innerSpacing;

        LinearLayout.LayoutParams params = (LayoutParams) mLearnMore.getLayoutParams();
        params.setMargins(0, learnMoreVerticalMargin, 0, learnMoreVerticalMargin);

        mContainer.setPadding(
                paddingHorizontalPx, paddingVerticalPx, paddingHorizontalPx, paddingVerticalPx);
    }

    /** Populate LearnMore view. **/
    private void adjustLearnMore(OnClickListener onClickListener) {
        String text =
                getContext().getResources().getString(R.string.revamped_incognito_ntp_learn_more);

        // Make the text between the <a> tags to be clickable, blue, without underline.
        SpanApplier.SpanInfo spanInfo =
                new SpanApplier.SpanInfo(
                        "<a>",
                        "</a>",
                        new NoUnderlineClickableSpan(
                                getContext(),
                                R.color.default_text_color_link_light,
                                onClickListener::onClick));

        SpannableString formattedText = SpanApplier.applySpans(text, spanInfo);

        mLearnMore.setText(formattedText);
        mLearnMore.setMovementMethod(LinkMovementMethod.getInstance());
    }

    private boolean isNarrowScreen() {
        int wideLayoutThresholdPx =
                mResources.getDimensionPixelSize(R.dimen.incognito_ntp_wide_layout_threshold);
        return mWidthPx <= wideLayoutThresholdPx;
    }

    @Override
    public void setCookieControlsEnforcement(@CookieControlsEnforcement int enforcement) {
        if (!findCookieControlElements()) return;
        boolean enforced = enforcement != CookieControlsEnforcement.NO_ENFORCEMENT;
        mCookieControlsToggle.setEnabled(!enforced);
        mCookieControlsManagedIcon.setVisibility(enforced ? View.VISIBLE : View.GONE);
        mCookieControlsTitle.setEnabled(!enforced);
        mCookieControlsSubtitle.setEnabled(!enforced);

        Resources resources = getContext().getResources();
        StringBuilder subtitleText = new StringBuilder();
        subtitleText.append(resources.getString(R.string.new_tab_otr_third_party_cookie_sublabel));
        if (!enforced) {
            mCookieControlsSubtitle.setText(subtitleText.toString());
            return;
        }

        int iconRes;
        String addition;
        switch (enforcement) {
            case CookieControlsEnforcement.ENFORCED_BY_POLICY:
                iconRes = R.drawable.ic_business_small;
                addition = resources.getString(R.string.managed_by_your_organization);
                break;
            case CookieControlsEnforcement.ENFORCED_BY_COOKIE_SETTING:
                iconRes = R.drawable.settings_cog;
                addition =
                        resources.getString(
                                R.string.new_tab_otr_cookie_controls_controlled_tooltip_text);
                break;
            default:
                return;
        }
        mCookieControlsManagedIcon.setImageResource(iconRes);
        subtitleText.append("\n");
        subtitleText.append(addition);
        mCookieControlsSubtitle.setText(subtitleText.toString());
    }

    /** Finds the 3PC controls and returns true if they exist. */
    private boolean findCookieControlElements() {
        mCookieControlsToggle = findViewById(R.id.revamped_cookie_controls_card_toggle);
        if (mCookieControlsToggle == null) return false;
        mCookieControlsManagedIcon = findViewById(R.id.revamped_cookie_controls_card_managed_icon);
        mCookieControlsTitle = findViewById(R.id.revamped_cookie_controls_card_title);
        mCookieControlsSubtitle = findViewById(R.id.revamped_cookie_controls_card_subtitle);
        return true;
    }
}
