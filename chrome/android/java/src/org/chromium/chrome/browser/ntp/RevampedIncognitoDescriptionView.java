// Copyright 2021 The Chromium Authors. All rights reserved.
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

/**
 * The view to describe revamped incognito mode.
 */
public class RevampedIncognitoDescriptionView
        extends LinearLayout implements IncognitoDescriptionView {
    private Resources mResources;

    private int mWidthDp;
    private int mHeightDp;
    private int mWidthPx;

    private TextView mTitle;
    private LinearLayout mContainer;
    private LinearLayout mContent;
    private LinearLayout mDescriptionTextContainer;
    private RelativeLayout mDoesLayout;
    private TextView mLearnMore;
    private RelativeLayout mDoesNotLayout;
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
    public void setNewTabHeader(String newTabPageTitle) {
        mTitle.setText(newTabPageTitle);
    }

    @Override
    public void setLearnMoreOnclickListener(OnClickListener onClickListener) {
        // Adjust LearnMore text and add the callback for LearnMore link.
        adjustLearnMore(onClickListener);
    }

    @Override
    public void setCookieControlsToggleOnCheckedChangeListener(
            CompoundButton.OnCheckedChangeListener listener) {
        mCookieControlsToggle.setOnCheckedChangeListener(listener);
    }

    @Override
    public void setCookieControlsToggle(boolean enabled) {
        mCookieControlsToggle.setChecked(enabled);
    }

    @Override
    public void setCookieControlsIconOnclickListener(OnClickListener listener) {
        mCookieControlsManagedIcon.setOnClickListener(listener);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mResources = getContext().getResources();

        mWidthDp = mResources.getConfiguration().screenWidthDp;
        mHeightDp = mResources.getConfiguration().screenHeightDp;
        mWidthPx = dpToPx(getContext(), mWidthDp);

        mContainer = findViewById(R.id.revamped_incognito_ntp_container);

        populateDescriptions(R.id.revamped_incognito_ntp_does_description_view,
                R.string.revamped_incognito_ntp_does_description);
        populateDescriptions(R.id.revamped_incognito_ntp_does_not_description_view,
                R.string.revamped_incognito_ntp_does_not_description);

        mTitle = findViewById(R.id.revamped_incognito_ntp_title);
        mContent = findViewById(R.id.revamped_incognito_ntp_content);
        mDescriptionTextContainer =
                findViewById(R.id.revamped_incognito_ntp_description_text_container);
        mDoesLayout = findViewById(R.id.revamped_incognito_ntp_does_layout);
        mDoesNotLayout = findViewById(R.id.revamped_incognito_ntp_does_not_layout);
        mLearnMore = findViewById(R.id.revamped_incognito_ntp_learn_more);
        mCookieControlsCard = findViewById(R.id.revamped_cookie_controls_card);
        mCookieControlsToggle = findViewById(R.id.revamped_cookie_controls_card_toggle);
        mCookieControlsManagedIcon = findViewById(R.id.revamped_cookie_controls_card_managed_icon);
        mCookieControlsTitle = findViewById(R.id.revamped_cookie_controls_card_title);
        mCookieControlsSubtitle = findViewById(R.id.revamped_cookie_controls_card_subtitle);

        adjustViews();
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // View#onConfigurationChanged() doesn't get called when resizing this view in
        // multi-window mode, so #onMeasure() is used instead.
        Configuration config = mResources.getConfiguration();
        if (mWidthDp != config.screenWidthDp || mHeightDp != config.screenHeightDp) {
            mWidthDp = config.screenWidthDp;
            mWidthPx = dpToPx(getContext(), mWidthDp);
            mHeightDp = config.screenHeightDp;
            adjustViews();
        }

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    private void adjustViews() {
        adjustLayout();
        adjustIcon();
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

        view.setText(SpanApplier.applySpans(text,
                new SpanApplier.SpanInfo("<li1>", "</li1>", new ChromeBulletSpan(getContext())),
                new SpanApplier.SpanInfo("<li2>", "</li2>", new ChromeBulletSpan(getContext())),
                new SpanApplier.SpanInfo("<li3>", "</li3>", new ChromeBulletSpan(getContext()))));
    }

    /**
     * Adjusts the paddings, margins, and the orientation of "does", "doesn't do" and LearnMore
     * containers.
     */
    private void adjustLayout() {
        // Old title adjustments.
        int totalSpaceBetweenViews = getContext().getResources().getDimensionPixelSize(
                R.dimen.incognito_ntp_total_space_between_views);

        ((LinearLayout.LayoutParams) mTitle.getLayoutParams())
                .setMargins(0, totalSpaceBetweenViews, 0, 0);
        mTitle.setLayoutParams(mTitle.getLayoutParams());

        int paddingHorizontalPx;
        int paddingVerticalPx;

        int contentMaxWidthPx = mResources.getDimensionPixelSize(R.dimen.content_max_width);
        int contentWidthPx;

        int doesTopMarginPx = mResources.getDimensionPixelSize(R.dimen.does_and_doesnt_top_spacing);
        int descriptionsWeight = mResources.getInteger(R.integer.descriptions_weight);

        if (isNarrowScreen()) {
            // Small padding.
            int thresholdPx =
                    mResources.getDimensionPixelSize(R.dimen.portrait_small_or_big_threshold);
            paddingHorizontalPx = mResources.getDimensionPixelSize(mWidthPx <= thresholdPx
                            ? R.dimen.portrait_horizontal_small_padding
                            : R.dimen.portrait_horizontal_big_padding);
            paddingVerticalPx = mResources.getDimensionPixelSize(R.dimen.portrait_vertical_padding);

            mContainer.setGravity(Gravity.START);

            mDescriptionTextContainer.setOrientation(LinearLayout.VERTICAL);
            // Since the width can not be directly measured at this stage, we must calculate it.
            contentWidthPx = Math.min(contentMaxWidthPx, mWidthPx - 2 * paddingHorizontalPx);

            // Set layout params for portrait orientation. Must be done programmatically to cover
            // the case when the user switches from landscape to portrait.
            LinearLayout.LayoutParams layoutParams = new LinearLayoutCompat.LayoutParams(
                    LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
            layoutParams.setMargins(0, doesTopMarginPx, 0, 0);

            mDoesLayout.setLayoutParams(layoutParams);
            mDoesNotLayout.setLayoutParams(layoutParams);
        } else {
            // Large padding.
            int thresholdPx =
                    mResources.getDimensionPixelSize(R.dimen.landscape_small_or_big_threshold);
            paddingHorizontalPx =
                    mResources.getDimensionPixelSize(R.dimen.landscape_horizontal_padding);
            paddingVerticalPx = mResources.getDimensionPixelSize(mHeightDp <= thresholdPx
                            ? R.dimen.landscape_vertical_small_padding
                            : R.dimen.landscape_vertical_big_padding);

            mContainer.setGravity(Gravity.CENTER_HORIZONTAL);

            mDescriptionTextContainer.setOrientation(LinearLayout.HORIZONTAL);
            // Since the width can not be directly measured at this stage, we must calculate it.
            contentWidthPx = Math.min(contentMaxWidthPx, mWidthPx - 2 * paddingHorizontalPx);

            // Set layout params for landscape orientation.
            int doesRightMarginPx =
                    mResources.getDimensionPixelSize(R.dimen.descriptions_horizontal_spacing);
            LinearLayout.LayoutParams layoutParamsDoes = new LinearLayoutCompat.LayoutParams(
                    0, LayoutParams.WRAP_CONTENT, descriptionsWeight);
            layoutParamsDoes.setMargins(0, doesTopMarginPx, doesRightMarginPx, 0);
            mDoesLayout.setLayoutParams(layoutParamsDoes);

            LinearLayout.LayoutParams layoutParamsDoesNot = new LinearLayoutCompat.LayoutParams(
                    0, LayoutParams.WRAP_CONTENT, descriptionsWeight);
            layoutParamsDoesNot.setMargins(0, doesTopMarginPx, 0, 0);
            mDoesNotLayout.setLayoutParams(layoutParamsDoesNot);
        }

        mContent.setLayoutParams(new LinearLayout.LayoutParams(
                contentWidthPx, LinearLayout.LayoutParams.WRAP_CONTENT));

        // The learn more text view has height of min_touch_target_size. This effectively
        // creates padding above and below it, depending on Android font size settings.
        // We want to have a R.dimen.learn_more_vertical_spacing tall gap between the learn more
        // text and the adjacent elements. So adjust the margin to be the difference between
        // targeted spacing and effective padding.
        int innerSpacing = (int) ((getContext().getResources().getDimensionPixelSize(
                                           R.dimen.min_touch_target_size)
                                          - mLearnMore.getTextSize())
                / 2);
        int learnMoreVerticalMargin =
                mResources.getDimensionPixelSize(R.dimen.learn_more_vertical_spacing)
                - innerSpacing;

        LinearLayout.LayoutParams params = (LayoutParams) mLearnMore.getLayoutParams();
        params.setMargins(0, learnMoreVerticalMargin, 0, learnMoreVerticalMargin);

        mContainer.setPadding(
                paddingHorizontalPx, paddingVerticalPx, paddingHorizontalPx, paddingVerticalPx);
    }

    /** Adjust the Incognito icon. */
    private void adjustIcon() {
        // The icon resource is 120dp x 120dp (i.e. 120px x 120px at MDPI). This method always
        // resizes the icon view to 120dp x 120dp or smaller, therefore image quality is not lost.
        int sizeDp;
        if (isNarrowScreen()) {
            sizeDp = (mWidthDp <= 240 || mHeightDp <= 480) ? 48 : 72;
        } else {
            sizeDp = mHeightDp <= 480 ? 72 : 120;
        }

        ImageView icon = (ImageView) findViewById(R.id.revamped_incognito_ntp_icon);
        icon.getLayoutParams().width = dpToPx(getContext(), sizeDp);
        icon.getLayoutParams().height = dpToPx(getContext(), sizeDp);
    }

    /** Populate LearnMore view. **/
    private void adjustLearnMore(OnClickListener onClickListener) {
        String text =
                getContext().getResources().getString(R.string.revamped_incognito_ntp_learn_more);

        // Make the text between the <a> tags to be clickable, blue, without underline.
        SpanApplier.SpanInfo spanInfo = new SpanApplier.SpanInfo("<a>", "</a>",
                new NoUnderlineClickableSpan(getResources(), R.color.default_text_color_link_light,
                        onClickListener::onClick));

        SpannableString formattedText = SpanApplier.applySpans(text, spanInfo);

        mLearnMore.setText(formattedText);
        mLearnMore.setMovementMethod(LinkMovementMethod.getInstance());
    }

    private boolean isNarrowScreen() {
        int wideLayoutThresholdPx = mResources.getDimensionPixelSize(R.dimen.wide_layout_threshold);
        return mWidthPx <= wideLayoutThresholdPx;
    }

    @Override
    public void showCookieControlsCard(boolean showCard) {
        mCookieControlsCard.setVisibility(showCard ? View.VISIBLE : View.GONE);
    }

    @Override
    public void setCookieControlsEnforcement(@CookieControlsEnforcement int enforcement) {
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
                addition = resources.getString(
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
}