// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.transition.ChangeBounds;
import android.transition.Fade;
import android.transition.Transition;
import android.transition.TransitionManager;
import android.transition.TransitionSet;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.ui.drawable.StateListDrawableBuilder;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.CheckableImageView;
import org.chromium.ui.widget.ChromeImageView;

public class SafetyHubExpandablePreference extends ChromeBasePreference {
    private String mPrimaryButtonText;
    private String mSecondaryButtonText;
    private View.OnClickListener mPrimaryButtonClickListener;
    private View.OnClickListener mSecondaryButtonClickListener;
    private boolean mExpanded = true;
    private boolean mControlledByPolicy;
    private Drawable mDrawable;
    private PreferenceViewHolder mHolder;

    public SafetyHubExpandablePreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.safety_hub_expandable_preference);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        ButtonCompat primaryButton = (ButtonCompat) holder.findViewById(R.id.primary_button);
        assert primaryButton != null;

        if (!TextUtils.isEmpty(mPrimaryButtonText)) {
            primaryButton.setText(mPrimaryButtonText);
            primaryButton.setVisibility(View.VISIBLE);
            primaryButton.setOnClickListener(mPrimaryButtonClickListener);
        } else {
            primaryButton.setVisibility(View.GONE);
        }

        ButtonCompat secondaryButton = (ButtonCompat) holder.findViewById(R.id.secondary_button);
        assert secondaryButton != null;
        if (!TextUtils.isEmpty(mSecondaryButtonText)) {
            secondaryButton.setText(mSecondaryButtonText);
            secondaryButton.setVisibility(View.VISIBLE);
            secondaryButton.setOnClickListener(mSecondaryButtonClickListener);
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
        expandButton.setOnClickListener((v) -> setExpanded(!isExpanded()));

        ViewGroup buttonsContainer = (ViewGroup) holder.findViewById(R.id.buttons_container);
        assert buttonsContainer != null;
        buttonsContainer.setVisibility(isExpanded() ? View.VISIBLE : View.GONE);

        TextView summary = (TextView) holder.findViewById(android.R.id.summary);
        assert summary != null;
        summary.setVisibility(getSummary() != null && isExpanded() ? View.VISIBLE : View.GONE);

        ChromeImageView managedIcon = (ChromeImageView) holder.findViewById(R.id.managed_icon);
        assert managedIcon != null;
        managedIcon.setVisibility(mControlledByPolicy ? View.VISIBLE : View.GONE);

        mHolder = holder;
    }

    void setExpanded(boolean expanded) {
        if (mExpanded == expanded) return;
        mExpanded = expanded;
        onExpandedChanged();
        notifyChanged();
    }

    private void onExpandedChanged() {
        if (mHolder == null) return;
        Transition transition = createExpandCollapseTransition();

        LinearLayout preferenceLayout = (LinearLayout) mHolder.itemView;

        // Begin a transition, all layout changes after this call will be animated, onBindViewHolder
        // will be called next with the layout changes. The animation starts at the next frame.
        TransitionManager.beginDelayedTransition(preferenceLayout, transition);
    }

    private Transition createExpandCollapseTransition() {
        TransitionSet transitionSet = new TransitionSet();
        transitionSet.setOrdering(TransitionSet.ORDERING_TOGETHER);

        // This transition animates the fade in/out of the summary & buttons.
        Fade fade = new Fade();

        // This transition animates the height change when the preference expands/collapse, the
        // transition happens implicitly when the summary & buttons appear/disappear.
        ChangeBounds changeBounds = new ChangeBounds();

        transitionSet.addTransition(changeBounds).addTransition(fade);
        return transitionSet;
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

    void setControlledByPolicy(boolean controlledByPolicy) {
        if (mControlledByPolicy != controlledByPolicy) {
            mControlledByPolicy = controlledByPolicy;
            notifyChanged();
        }
    }
}
