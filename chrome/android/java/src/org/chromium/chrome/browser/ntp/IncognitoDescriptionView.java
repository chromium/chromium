// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.chromium.ui.base.ViewUtils.dpToPx;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.text.SpannableString;
import android.text.TextPaint;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.text.style.ForegroundColorSpan;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.appcompat.widget.SwitchCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.ChromeBulletSpan;

/** The view to describle incognito mode. */
public class IncognitoDescriptionView extends LinearLayout {
    private int mWidthDp;
    private int mHeightDp;

    private LinearLayout mContainer;
    private TextView mHeader;
    private TextView mSubtitle;
    private LinearLayout mBulletpointsContainer;
    private TextView mLearnMore;
    private TextView[] mParagraphs;
    private ViewGroup mCookieControlsCard;
    private SwitchCompat mCookieControlsToggle;
    private ImageView mCookieControlsManagedIcon;
    private TextView mCookieControlsTitle;
    private TextView mCookieControlsSubtitle;

    private static final int BULLETPOINTS_HORIZONTAL_SPACING_DP = 40;
    private static final int BULLETPOINTS_MARGIN_BOTTOM_DP = 12;
    private static final int CONTENT_WIDTH_DP = 600;
    private static final int WIDE_LAYOUT_THRESHOLD_DP = 720;
    private static final int COOKIES_CONTROL_MARGIN_TOP_DP = 12;

    static final String TRACKING_PROTECTION_URL =
            "https://support.google.com/chrome/?p=pause_protections";

    /** Default constructor needed to inflate via XML. */
    public IncognitoDescriptionView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void setLearnMoreOnclickListener(OnClickListener listener) {
        mLearnMore.setOnClickListener(listener);
    }

    public void setCookieControlsToggleOnCheckedChangeListener(OnCheckedChangeListener listener) {
        if (!findCookieControlElements()) return;
        mCookieControlsToggle.setOnCheckedChangeListener(listener);
    }

    public void setCookieControlsToggle(boolean enabled) {
        if (!findCookieControlElements()) return;
        mCookieControlsToggle.setChecked(enabled);
    }

    public void setCookieControlsIconOnclickListener(OnClickListener listener) {
        if (!findCookieControlElements()) return;
        mCookieControlsManagedIcon.setOnClickListener(listener);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mWidthDp = getContext().getResources().getConfiguration().screenWidthDp;
        mHeightDp = getContext().getResources().getConfiguration().screenHeightDp;

        populateBulletpoints(R.id.new_tab_incognito_features, R.string.new_tab_otr_not_saved);
        populateBulletpoints(R.id.new_tab_incognito_warning, R.string.new_tab_otr_visible);

        mContainer = findViewById(R.id.new_tab_incognito_container);
        mHeader = findViewById(R.id.new_tab_incognito_title);
        mSubtitle = findViewById(R.id.new_tab_incognito_subtitle);
        mLearnMore = findViewById(R.id.learn_more);
        mParagraphs =
                new TextView[] {
                    mSubtitle,
                    findViewById(R.id.new_tab_incognito_features),
                    findViewById(R.id.new_tab_incognito_warning)
                };
        mBulletpointsContainer = findViewById(R.id.new_tab_incognito_bulletpoints_container);

        adjustView();
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

    public void formatTrackingProtectionText(Context context, View layout) {
        TextView view = layout.findViewById(R.id.tracking_protection_description_two);
        if (view == null) {
            adjustCookieControlsCard();
            return;
        }

        String text =
                context.getResources()
                        .getString(R.string.new_tab_otr_third_party_blocked_cookie_part_two);
        ClickableSpan span =
                new ClickableSpan() {
                    @Override
                    public void onClick(View view) {
                        new ChromeAsyncTabLauncher(/* incognito= */ true)
                                .launchUrl(TRACKING_PROTECTION_URL, TabLaunchType.FROM_CHROME_UI);
                    }

                    @Override
                    public void updateDrawState(TextPaint textPaint) {
                        super.updateDrawState(textPaint);
                        textPaint.setColor(
                                context.getColor(R.color.default_text_color_secondary_light_list));
                    }
                };
        view.setText(
                SpanApplier.applySpans(text, new SpanApplier.SpanInfo("<link>", "</link>", span)));
        view.setMovementMethod(LinkMovementMethod.getInstance());
        adjustCookieControlsCard();
    }

    private void adjustView() {
        adjustIcon();
        adjustLayout();
        adjustLearnMore();
        adjustCookieControlsCard();
    }

    /**
     * @param element Resource ID of the element to be populated with the bulletpoints.
     * @param content String ID to serve as the text of |element|. Must contain an <em></em> span,
     *     which will be emphasized, and three
     *     <li>items, which will be converted to bulletpoints. Populates |element| with |content|.
     */
    private void populateBulletpoints(@IdRes int element, @StringRes int content) {
        TextView view = findViewById(element);
        SpannableString spannedText = getSpannedBulletText(getContext(), content);
        view.setText(spannedText);
    }

    @NonNull
    static SpannableString getSpannedBulletText(Context context, @StringRes int content) {
        String text = context.getResources().getString(content);
        // Some translations don't have a line break between list entries.
        text = text.replaceAll("([^\n ]) *(<li>|</?ul>)", "$1\n$2");

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

        String error =
                "Format error. Locale: "
                        + context.getResources().getConfiguration().getLocales()
                        + " \nstring: "
                        + context.getResources().getString(content);
        assert text.contains("<li1>") : error;
        assert text.contains("<li2>") : error;
        assert text.contains("<li3>") : error;

        // Remove the <ul></ul> tags which serve no purpose here, including the whitespace around
        // them.
        text = text.replaceAll(" *</?ul>\\n?", "");

        SpannableString spannedText =
                SpanApplier.applySpans(
                        text,
                        new SpanInfo(
                                "<em>",
                                "</em>",
                                new ForegroundColorSpan(
                                        context.getColor(R.color.incognito_emphasis))),
                        new SpanInfo("<li1>", "</li1>", new ChromeBulletSpan(context)),
                        new SpanInfo("<li2>", "</li2>", new ChromeBulletSpan(context)),
                        new SpanInfo("<li3>", "</li3>", new ChromeBulletSpan(context)));
        return spannedText;
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
                    new LinearLayout.LayoutParams(
                            LinearLayout.LayoutParams.WRAP_CONTENT,
                            LinearLayout.LayoutParams.WRAP_CONTENT));
            mSubtitle.setMaxWidth(dpToPx(getContext(), CONTENT_WIDTH_DP));

            // The bulletpoints container takes the same width as subtitle. Since the width can
            // not be directly measured at this stage, we must calculate it manually.
            mBulletpointsContainer.getLayoutParams().width =
                    dpToPx(
                            getContext(),
                            Math.min(CONTENT_WIDTH_DP, mWidthDp - 2 * paddingHorizontalDp));
        } else {
            // Large padding.
            paddingHorizontalDp = 0; // Should not be necessary on a screen this large.
            paddingVerticalDp = mHeightDp <= 320 ? 16 : 72;

            // Align to the center.
            mContainer.setGravity(Gravity.CENTER_HORIZONTAL);

            // Decide the bulletpoints orientation.
            bulletpointsArrangedHorizontally = true;

            int contentWidthPx = dpToPx(getContext(), CONTENT_WIDTH_DP);
            mSubtitle.setLayoutParams(
                    new LinearLayout.LayoutParams(
                            contentWidthPx, LinearLayout.LayoutParams.WRAP_CONTENT));
            mBulletpointsContainer.getLayoutParams().width = contentWidthPx;
        }

        // Apply the bulletpoints orientation.
        if (bulletpointsArrangedHorizontally) {
            mBulletpointsContainer.setOrientation(LinearLayout.HORIZONTAL);
        } else {
            mBulletpointsContainer.setOrientation(LinearLayout.VERTICAL);
        }

        // Set up paddings and margins.
        mContainer.setPadding(
                dpToPx(getContext(), paddingHorizontalDp),
                dpToPx(getContext(), paddingVerticalDp),
                dpToPx(getContext(), paddingHorizontalDp),
                dpToPx(getContext(), paddingVerticalDp));

        // Total space between adjacent paragraphs (Including margins, paddings, etc.)
        int totalSpaceBetweenViews =
                getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.incognito_ntp_total_space_between_views);

        for (TextView paragraph : mParagraphs) {
            // If bulletpoints are arranged horizontally, there should be space between them.
            int rightMarginPx =
                    (bulletpointsArrangedHorizontally
                                    && paragraph == mBulletpointsContainer.getChildAt(0))
                            ? dpToPx(getContext(), BULLETPOINTS_HORIZONTAL_SPACING_DP)
                            : 0;

            ((LinearLayout.LayoutParams) paragraph.getLayoutParams())
                    .setMargins(0, totalSpaceBetweenViews, rightMarginPx, 0);
            paragraph.setLayoutParams(paragraph.getLayoutParams()); // Apply the new layout.
        }

        // The learn more text view has height of min_touch_target_size. Typically the actual text
        // is not that tall, and already has some space. We want to have a
        // totalSpaceBetweenViews tall gap between the learn more text and the adjacent
        // elements. So add the difference as an additional margin.
        int innerSpacing =
                (int)
                        ((getContext()
                                                .getResources()
                                                .getDimensionPixelSize(
                                                        R.dimen.min_touch_target_size)
                                        - mLearnMore.getTextSize())
                                / 2);
        int learnMoreSpacingTop =
                totalSpaceBetweenViews
                        - innerSpacing
                        - dpToPx(getContext(), BULLETPOINTS_MARGIN_BOTTOM_DP);
        int learnMoreSpacingBottom =
                dpToPx(getContext(), COOKIES_CONTROL_MARGIN_TOP_DP) - innerSpacing;

        LinearLayout.LayoutParams params = (LayoutParams) mLearnMore.getLayoutParams();
        params.setMargins(0, learnMoreSpacingTop, 0, learnMoreSpacingBottom);
        ViewUtils.requestLayout(mLearnMore, "IncognitoDescriptionView.adjustLayout");

        ((LinearLayout.LayoutParams) mHeader.getLayoutParams())
                .setMargins(0, totalSpaceBetweenViews, 0, 0);
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

        ImageView icon = findViewById(R.id.new_tab_incognito_icon);
        icon.getLayoutParams().width = dpToPx(getContext(), sizeDp);
        icon.getLayoutParams().height = dpToPx(getContext(), sizeDp);
    }

    /** Adjust the "Learn More" link. */
    private void adjustLearnMore() {
        final String subtitleText =
                getContext()
                        .getResources()
                        .getString(R.string.new_tab_otr_subtitle_with_reading_list);
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
        concatenatedText.append(getContext().getResources().getString(R.string.learn_more));
        SpannableString textWithLearnMoreLink = new SpannableString(concatenatedText.toString());

        NoUnderlineClickableSpan span =
                new NoUnderlineClickableSpan(
                        getContext(),
                        R.color.baseline_primary_80,
                        (view) -> mLearnMore.callOnClick());
        textWithLearnMoreLink.setSpan(
                span, subtitleText.length() + 1, textWithLearnMoreLink.length(), /* flags= */ 0);
        mSubtitle.setText(textWithLearnMoreLink);
        mSubtitle.setMovementMethod(LinkMovementMethod.getInstance());
    }

    /** Adjust the Cookie Controls Card. */
    private void adjustCookieControlsCard() {
        mCookieControlsCard = findViewById(R.id.cookie_controls_card);
        if (mCookieControlsCard == null) {
            mCookieControlsCard = findViewById(R.id.tracking_protection_card);
        }
        // Still null - not inflated yet.
        if (mCookieControlsCard == null) return;
        if (mWidthDp <= WIDE_LAYOUT_THRESHOLD_DP) {
            // Portrait
            mCookieControlsCard.getLayoutParams().width = LinearLayout.LayoutParams.MATCH_PARENT;
        } else {
            // Landscape
            mCookieControlsCard.getLayoutParams().width = dpToPx(getContext(), CONTENT_WIDTH_DP);
        }
    }

    public void setCookieControlsEnforcement(@CookieControlsEnforcement int enforcement) {
        // No cookie controls toggle on the page.
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
        mCookieControlsToggle = findViewById(R.id.cookie_controls_card_toggle);
        if (mCookieControlsToggle == null) return false;
        mCookieControlsManagedIcon = findViewById(R.id.cookie_controls_card_managed_icon);
        mCookieControlsTitle = findViewById(R.id.cookie_controls_card_title);
        mCookieControlsSubtitle = findViewById(R.id.cookie_controls_card_subtitle);
        return true;
    }
}
