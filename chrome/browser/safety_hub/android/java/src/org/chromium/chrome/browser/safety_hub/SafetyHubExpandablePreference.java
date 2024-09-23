// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.view.View.AccessibilityDelegate;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.ui.drawable.StateListDrawableBuilder;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.CheckableImageView;

public class SafetyHubExpandablePreference extends ChromeBasePreference {
    private String mPrimaryButtonText;
    private String mSecondaryButtonText;
    private View.OnClickListener mPrimaryButtonClickListener;
    private View.OnClickListener mSecondaryButtonClickListener;
    private boolean mExpanded = true;
    private Drawable mDrawable;

    public SafetyHubExpandablePreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.safety_hub_expandable_preference);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        TextView title = (TextView) holder.findViewById(android.R.id.title);
        assert title != null;

        ButtonCompat primaryButton = (ButtonCompat) holder.findViewById(R.id.primary_button);
        assert primaryButton != null;

        if (!TextUtils.isEmpty(mPrimaryButtonText)) {
            primaryButton.setText(mPrimaryButtonText);
            primaryButton.setVisibility(View.VISIBLE);
            primaryButton.setOnClickListener(mPrimaryButtonClickListener);
            primaryButton.setAccessibilityDelegate(createButtonAccessibilityDelegate(title));
        } else {
            primaryButton.setVisibility(View.GONE);
        }

        ButtonCompat secondaryButton = (ButtonCompat) holder.findViewById(R.id.secondary_button);
        assert secondaryButton != null;
        if (!TextUtils.isEmpty(mSecondaryButtonText)) {
            secondaryButton.setText(mSecondaryButtonText);
            secondaryButton.setVisibility(View.VISIBLE);
            secondaryButton.setOnClickListener(mSecondaryButtonClickListener);
            secondaryButton.setAccessibilityDelegate(createButtonAccessibilityDelegate(title));
        } else {
            secondaryButton.setVisibility(View.GONE);
        }

        if (mDrawable == null) {
            mDrawable = createDrawable(getContext());
        }

        CheckableImageView expandButton =
                (CheckableImageView) holder.findViewById(R.id.checkable_image_view);
        assert expandButton != null;
        expandButton.setImageDrawable(mDrawable);
        expandButton.setChecked(mExpanded);

        ViewGroup buttonsContainer = (ViewGroup) holder.findViewById(R.id.buttons_container);
        assert buttonsContainer != null;
        buttonsContainer.setVisibility(isExpanded() ? View.VISIBLE : View.GONE);

        TextView summary = (TextView) holder.findViewById(android.R.id.summary);
        assert summary != null;
        summary.setVisibility(getSummary() != null && isExpanded() ? View.VISIBLE : View.GONE);

        updatePreferenceContentDescription(holder.itemView);
    }

    @Override
    protected void onClick() {
        setExpanded(!isExpanded());
    }

    void setExpanded(boolean expanded) {
        if (mExpanded == expanded) return;
        mExpanded = expanded;
        notifyChanged();
    }

    boolean isExpanded() {
        return mExpanded;
    }

    void setPrimaryButtonText(@Nullable String buttonText) {
        if (!TextUtils.equals(mPrimaryButtonText, buttonText)) {
            mPrimaryButtonText = buttonText;
            this.notifyChanged();
        }
    }

    void setSecondaryButtonText(@Nullable String buttonText) {
        if (!TextUtils.equals(mSecondaryButtonText, buttonText)) {
            mSecondaryButtonText = buttonText;
            this.notifyChanged();
        }
    }

    void setPrimaryButtonClickListener(@Nullable View.OnClickListener clickListener) {
        if (mPrimaryButtonClickListener != clickListener) {
            mPrimaryButtonClickListener = clickListener;
            this.notifyChanged();
        }
    }

    void setSecondaryButtonClickListener(@Nullable View.OnClickListener clickListener) {
        if (mSecondaryButtonClickListener != clickListener) {
            mSecondaryButtonClickListener = clickListener;
            this.notifyChanged();
        }
    }

    @Nullable
    String getPrimaryButtonText() {
        return mPrimaryButtonText;
    }

    @Nullable
    String getSecondaryButtonText() {
        return mSecondaryButtonText;
    }

    private void updatePreferenceContentDescription(View view) {
        // For accessibility, read out the whole title and whether the group is collapsed/expanded.
        String collapseOrExpandedText =
                getContext()
                        .getResources()
                        .getString(
                                mExpanded
                                        ? R.string.accessibility_expanded_group
                                        : R.string.accessibility_collapsed_group);

        String description =
                getContext()
                        .getResources()
                        .getString(
                                R.string.concat_two_strings_with_periods,
                                getTitle(),
                                collapseOrExpandedText);

        view.setContentDescription(description);
    }

    private AccessibilityDelegate createButtonAccessibilityDelegate(View labelView) {
        return new AccessibilityDelegate() {
            @Override
            public void onInitializeAccessibilityNodeInfo(View host, AccessibilityNodeInfo info) {
                super.onInitializeAccessibilityNodeInfo(host, info);
                info.setLabeledBy(labelView);
            }
        };
    }

    private static Drawable createDrawable(Context context) {
        // TODO(crbug.com/324562205): Refactor this to avoid duplication with
        // PrivacySandboxDialogUtils & ExpandablePreferenceGroup.
        StateListDrawableBuilder builder = new StateListDrawableBuilder(context);
        StateListDrawableBuilder.State checked =
                builder.addState(
                        R.drawable.ic_expand_less_black_24dp, android.R.attr.state_checked);
        StateListDrawableBuilder.State unchecked =
                builder.addState(R.drawable.ic_expand_more_black_24dp);
        builder.addTransition(
                checked, unchecked, R.drawable.transition_expand_less_expand_more_black_24dp);
        builder.addTransition(
                unchecked, checked, R.drawable.transition_expand_more_expand_less_black_24dp);

        Drawable tintableDrawable = DrawableCompat.wrap(builder.build());
        DrawableCompat.setTintList(
                tintableDrawable,
                AppCompatResources.getColorStateList(
                        context, R.color.default_icon_color_tint_list));
        return tintableDrawable;
    }
}
