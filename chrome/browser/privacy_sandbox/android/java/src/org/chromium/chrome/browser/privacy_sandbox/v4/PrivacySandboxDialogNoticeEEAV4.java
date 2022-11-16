// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox.v4;

import android.app.Dialog;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.text.style.StyleSpan;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxReferrer;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsFragmentV3;
import org.chromium.chrome.browser.privacy_sandbox.R;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.ui.drawable.StateListDrawableBuilder;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.CheckableImageView;
import org.chromium.ui.widget.ChromeBulletSpan;

/**
 * Dialog in the form of a notice shown for the Privacy Sandbox.
 */
public class PrivacySandboxDialogNoticeEEAV4 extends Dialog implements View.OnClickListener {
    private Context mContext;
    private SettingsLauncher mSettingsLauncher;
    private View mContentView;

    private final CheckableImageView mExpandArrowView;
    private LinearLayout mDropdownContainer;

    public PrivacySandboxDialogNoticeEEAV4(
            Context context, @NonNull SettingsLauncher settingsLauncher) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        mContext = context;
        getContext();
        mSettingsLauncher = settingsLauncher;
        mContentView =
                LayoutInflater.from(context).inflate(R.layout.privacy_sandbox_notice_eea_v4, null);
        setContentView(mContentView);

        ButtonCompat ackButton = mContentView.findViewById(R.id.ack_button);
        ackButton.setOnClickListener(this);
        ButtonCompat settingsButton = mContentView.findViewById(R.id.settings_button);
        settingsButton.setOnClickListener(this);

        // Controls for the expanding section.
        LinearLayout dropdownElement = mContentView.findViewById(R.id.dropdown_element);
        dropdownElement.setOnClickListener(this);
        mDropdownContainer = mContentView.findViewById(R.id.dropdown_container);
        mExpandArrowView = mContentView.findViewById(R.id.expand_arrow);
        mExpandArrowView.setImageDrawable(createExpandDrawable(context));
        mExpandArrowView.setChecked(isDropdownExpanded());

        setBulletsDescription();
    }

    @Override
    public void show() {
        // TODO(b/254408752): Report show action.
        super.show();
    }

    // OnClickListener:
    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.ack_button) {
            // TODO(b/254408752): Report notice acknowledge action.
            dismiss();
        } else if (id == R.id.settings_button) {
            // TODO(b/254408752): Report open settings action.
            dismiss();
            // TODO(b/254408752): Launch PrivacySandboxSettingsFragmentV4 when available.
            PrivacySandboxSettingsFragmentV3.launchPrivacySandboxSettings(
                    mContext, mSettingsLauncher, PrivacySandboxReferrer.PRIVACY_SANDBOX_NOTICE);
        } else if (id == R.id.dropdown_element) {
            var content = mContentView.findViewById(R.id.privacy_sandbox_notice_eea_content);
            ScrollView scrollView =
                    mContentView.findViewById(R.id.privacy_sandbox_notice_eea_scroll_view);

            if (isDropdownExpanded()) {
                // TODO(b/254408752): Report notice eea more info section closed action.
                mDropdownContainer.setVisibility(View.GONE);
                mDropdownContainer.removeAllViews();

                ((FrameLayout.LayoutParams) content.getLayoutParams()).gravity =
                        Gravity.CENTER_VERTICAL;
            } else {
                mDropdownContainer.setVisibility(View.VISIBLE);
                // TODO(b/254408752): Report notice eea more info section opened action.
                LayoutInflater.from(mContext).inflate(
                        R.layout.privacy_sandbox_notice_eea_dropdown_v4, mDropdownContainer);

                setDropdownDescription(mDropdownContainer,
                        R.id.privacy_sandbox_m1_notice_eea_learn_more_bullet_one,
                        R.string.privacy_sandbox_m1_notice_eea_learn_more_bullet_1);
                setDropdownDescription(mDropdownContainer,
                        R.id.privacy_sandbox_m1_notice_eea_learn_more_bullet_two,
                        R.string.privacy_sandbox_m1_notice_eea_learn_more_bullet_2);
                setDropdownDescription(mDropdownContainer,
                        R.id.privacy_sandbox_m1_notice_eea_learn_more_bullet_three,
                        R.string.privacy_sandbox_m1_notice_eea_learn_more_bullet_3);

                ((FrameLayout.LayoutParams) content.getLayoutParams()).gravity = Gravity.TOP;

                scrollView.post(() -> {
                    scrollView.setSmoothScrollingEnabled(true);
                    scrollView.fullScroll(ScrollView.FOCUS_DOWN);
                });
            }

            mExpandArrowView.setChecked(isDropdownExpanded());
            updateDropdownControlContentDescription(view);
            view.announceForAccessibility(getContext().getResources().getString(isDropdownExpanded()
                            ? R.string.accessibility_expanded_group
                            : R.string.accessibility_collapsed_group));
        }
    }

    private void setDropdownDescription(
            ViewGroup container, @IdRes int viewId, @StringRes int stringRes) {
        TextView view = container.findViewById(viewId);
        SpannableString spannableString =
                SpanApplier.applySpans(getContext().getResources().getString(stringRes),
                        new SpanApplier.SpanInfo(
                                "<b>", "</b>", new StyleSpan(android.graphics.Typeface.BOLD)));
        spannableString.setSpan(new ChromeBulletSpan(getContext()), 0, spannableString.length(), 0);
        view.setText(spannableString);
    }

    private void setBulletsDescription() {
        TextView bulletView1 =
                mContentView.findViewById(R.id.privacy_sandbox_m1_notice_eea_bullet_one);
        TextView bulletView2 =
                mContentView.findViewById(R.id.privacy_sandbox_m1_notice_eea_bullet_two);

        SpannableString bullet1 = new SpannableString(getContext().getResources().getString(
                R.string.privacy_sandbox_m1_notice_eea_bullet_1));
        SpannableString bullet2 = new SpannableString(getContext().getResources().getString(
                R.string.privacy_sandbox_m1_notice_eea_bullet_2));

        bullet1.setSpan(new ChromeBulletSpan(getContext()), 0, bullet1.length(), 0);
        bulletView1.setText(bullet1);
        bullet2.setSpan(new ChromeBulletSpan(getContext()), 0, bullet2.length(), 0);
        bulletView2.setText(bullet2);
    }

    private void updateDropdownControlContentDescription(View dropdownElement) {
        String dropdownButtonText = getContext().getResources().getString(
                R.string.privacy_sandbox_m1_notice_eea_learn_more_expand_label);

        String collapseOrExpandedText = getContext().getResources().getString(isDropdownExpanded()
                        ? R.string.accessibility_expanded_group
                        : R.string.accessibility_collapsed_group);

        String description =
                getContext().getResources().getString(R.string.concat_two_strings_with_periods,
                        dropdownButtonText, collapseOrExpandedText);
        dropdownElement.setContentDescription(description);
    }

    private static Drawable createExpandDrawable(Context context) {
        StateListDrawableBuilder builder = new StateListDrawableBuilder(context);
        StateListDrawableBuilder.State checked = builder.addState(
                R.drawable.ic_expand_less_black_24dp, android.R.attr.state_checked);
        StateListDrawableBuilder.State unchecked =
                builder.addState(R.drawable.ic_expand_more_black_24dp);
        builder.addTransition(
                checked, unchecked, R.drawable.transition_expand_less_expand_more_black_24dp);
        builder.addTransition(
                unchecked, checked, R.drawable.transition_expand_more_expand_less_black_24dp);

        Drawable tintableDrawable = DrawableCompat.wrap(builder.build());
        DrawableCompat.setTintList(tintableDrawable,
                AppCompatResources.getColorStateList(
                        context, R.color.default_icon_color_tint_list));
        return tintableDrawable;
    }

    private boolean isDropdownExpanded() {
        return mDropdownContainer != null && mDropdownContainer.getVisibility() == View.VISIBLE;
    }
}
