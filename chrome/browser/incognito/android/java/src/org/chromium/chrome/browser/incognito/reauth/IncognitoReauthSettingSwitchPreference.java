// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.ui.base.ViewUtils;

/**
 * A custom switch preference for the Incognito reauth lock setting.
 *
 * <p>TODO(crbug.com/40197623): Espresso's AccessibilityChecks can fail if the clickable text is
 * below 48dp in height which is currently the case for this summary where we add a clickable
 * action. The surface of the clickable action needs to be revisited after discussing with the UX.
 */
public class IncognitoReauthSettingSwitchPreference extends ChromeSwitchPreference {
    /** A boolean to indicate whether the preference should be interactable or not.*/
    private boolean mPreferenceInteractable;

    /** The action to perform when the summary is clicked.*/
    private Runnable mLinkClickDelegate;

    public IncognitoReauthSettingSwitchPreference(Context context) {
        super(context);
    }

    public IncognitoReauthSettingSwitchPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Set the {@link Runnable} that needs to be invoked when the user clicks on the summary.
     *
     * Please note, this runnable would only be run when the |mPreferenceInteractable| is set to
     * false.
     *
     * @param linkClickDelegate A {@link Runnable} object which needs to be run when the user clicks
     *         on the summary.
     */
    public void setLinkClickDelegate(Runnable linkClickDelegate) {
        mLinkClickDelegate = linkClickDelegate;
    }

    /**
     * Set the interactability of the preference.
     *
     * If set to false, the click events on the summary text would run |mLinkClickDelegate| and the
     * preference would not be toggled.
     *
     * If set to true, the click events on the summary text would be ignored and the preference
     * would be toggled instead.
     *
     * @param preferenceInteractable A boolean indicating whether the preference should be made
     *         interactable or not.
     */
    public void setPreferenceInteractable(boolean preferenceInteractable) {
        mPreferenceInteractable = preferenceInteractable;
        notifyChanged();
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        assert mLinkClickDelegate != null : "Must initialize mLinkClickDelegate";
        ViewUtils.setEnabledRecursive(holder.itemView, mPreferenceInteractable);
        if (!mPreferenceInteractable) {
            TextView summary = (TextView) holder.findViewById(android.R.id.summary);
            summary.setEnabled(true);
            summary.setOnClickListener(
                    new View.OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            mLinkClickDelegate.run();
                        }
                    });
        }
    }

    @Override
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void onClick() {
        if (mPreferenceInteractable) {
            super.onClick();
        }
    }
}
