// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.view.View;
import android.view.View.AccessibilityDelegate;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.CheckableImageView;

@NullMarked
public class SafetyHubExpandablePreference extends ChromeBasePreference {
    private @Nullable String mPrimaryButtonText;
    private @Nullable String mSecondaryButtonText;
    private View.@Nullable OnClickListener mPrimaryButtonClickListener;
    private View.@Nullable OnClickListener mSecondaryButtonClickListener;
    private boolean mExpanded = true;
    private @Nullable Drawable mDrawable;
    private boolean mHasProgressBar;

    public SafetyHubExpandablePreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.safety_hub_expandable_preference);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        ProgressBar progressBar = (ProgressBar) holder.findViewById(R.id.progress_bar);
        assert progressBar != null;
        progressBar.setVisibility(mHasProgressBar ? View.VISIBLE : View.GONE);

        if (mHasProgressBar) {
            holder.findViewById(R.id.icon_frame).setVisibility(View.VISIBLE);
        }

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
            mDrawable = SettingsUtils.createExpandArrow(getContext());
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
        CharSequence summaryStr = getSummary();
        summary.setVisibility(summaryStr != null && isExpanded() ? View.VISIBLE : View.GONE);

        if (summaryStr instanceof SpannableString) {
            summary.setMovementMethod(LinkMovementMethod.getInstance());
        }

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

    void setPrimaryButtonClickListener(View.@Nullable OnClickListener clickListener) {
        if (mPrimaryButtonClickListener != clickListener) {
            mPrimaryButtonClickListener = clickListener;
            this.notifyChanged();
        }
    }

    void setSecondaryButtonClickListener(View.@Nullable OnClickListener clickListener) {
        if (mSecondaryButtonClickListener != clickListener) {
            mSecondaryButtonClickListener = clickListener;
            this.notifyChanged();
        }
    }

    @Nullable String getPrimaryButtonText() {
        return mPrimaryButtonText;
    }

    @Nullable String getSecondaryButtonText() {
        return mSecondaryButtonText;
    }

    View.@Nullable OnClickListener getPrimaryButtonClickListener() {
        return mPrimaryButtonClickListener;
    }

    public void setHasProgressBar(boolean hasProgressBar) {
        if (mHasProgressBar == hasProgressBar) return;
        mHasProgressBar = hasProgressBar;
        setIconSpaceReserved(hasProgressBar);
    }

    private void updatePreferenceContentDescription(View view) {
        // For accessibility, read out the whole title and whether the group is collapsed/expanded.
        String collapseOrExpandedText =
                getContext()
                        .getString(
                                mExpanded
                                        ? R.string.accessibility_expanded_group
                                        : R.string.accessibility_collapsed_group);

        String description =
                getContext()
                        .getString(
                                R.string.concat_two_strings_with_comma,
                                getTitle(),
                                collapseOrExpandedText);

        view.setContentDescription(description);
        if (view.isAccessibilityFocused()) {
            view.sendAccessibilityEvent(AccessibilityEvent.CONTENT_CHANGE_TYPE_CONTENT_DESCRIPTION);
        }
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
}
