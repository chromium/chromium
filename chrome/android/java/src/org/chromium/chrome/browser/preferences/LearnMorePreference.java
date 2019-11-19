// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.app.Activity;
import android.content.Context;
import android.content.res.TypedArray;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceViewHolder;
import android.util.AttributeSet;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * A preference that opens a HelpAndFeedback activity to learn more about the specified context.
 */
public class LearnMorePreference extends Preference {
    /**
     * Resource id for the help page to link to by context name. Corresponds to go/mobilehelprecs.
     */
    private final int mHelpContext;

    /**
     * Link text color.
     */
    private final int mColor;

    public LearnMorePreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray styledAttributes =
                context.obtainStyledAttributes(attrs, R.styleable.LearnMorePreference, 0, 0);
        mHelpContext =
                styledAttributes.getResourceId(R.styleable.LearnMorePreference_helpContext, 0);
        mColor = ApiCompatibilityUtils.getColor(
                context.getResources(), R.color.default_text_color_link);
        styledAttributes.recycle();

        setTitle(R.string.learn_more);
        setSelectable(false);
        setSingleLineTitle(false);
    }

    @Override
    protected void onClick() {
        Activity activity = ContextUtils.activityFromContext(getContext());
        HelpAndFeedback.getInstance().show(
                activity, activity.getString(mHelpContext), Profile.getLastUsedProfile(), null);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        TextView titleView = (TextView) holder.findViewById(android.R.id.title);
        titleView.setClickable(true);
        titleView.setTextColor(mColor);
        titleView.setOnClickListener(v -> LearnMorePreference.this.onClick());
    }
}