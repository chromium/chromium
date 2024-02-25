// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import android.content.Context;
import android.text.TextUtils.TruncateAt;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;

/**
 * A {@link Preference} for each Safety check element. In addition to the
 * functionality, provided by the {@link IconPreference}, has a status indicator
 * in the widget area that displays a progress bar or a status icon.
 */
public class SafetyCheckElementPreference extends ChromeBasePreference {
    private View mProgressBar;
    private ImageView mStatusView;

    /**
     * Represents an action to take once the view elements are available.
     * This is needed because |SafetyCheckMediator::setInitialState()| is invoked before all the
     * nested views are available, so setting icons should be delayed.
     */
    private Callback<Void> mDelayedAction;

    /** Creates a new object and sets the widget layout. */
    public SafetyCheckElementPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setWidgetLayoutResource(R.layout.safety_check_status);
        // No delayed action to take.
        mDelayedAction = null;
    }

    /** Gets triggered when the view elements are created. */
    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        mProgressBar = holder.findViewById(R.id.progress);
        mStatusView = (ImageView) holder.findViewById(R.id.status_view);

        // Title should be max 3 lines length and ellipsized if it doesn't fit into 3 lines.
        TextView titleTextView = (TextView) holder.findViewById(android.R.id.title);
        titleTextView.setMaxLines(3);
        titleTextView.setEllipsize(TruncateAt.END);

        // If there is a delayed action - take it.
        if (mDelayedAction != null) {
            mDelayedAction.onResult(null);
        }
        // Reset the delayed action.
        mDelayedAction = null;
    }

    /** Displays the progress bar. */
    void showProgressBar() {
        // Delay if this gets invoked before onBindViewHolder.
        if (mStatusView == null || mProgressBar == null) {
            mDelayedAction = (ignored) -> showProgressBar();
            return;
        }
        mStatusView.setVisibility(View.GONE);
        mProgressBar.setVisibility(View.VISIBLE);
    }

    /**
     * Displays the status icon.
     * @param icon An icon to display.
     */
    void showStatusIcon(@DrawableRes int icon) {
        // Delay if this gets invoked before onBindViewHolder.
        if (mStatusView == null || mProgressBar == null) {
            mDelayedAction = (ignored) -> showStatusIcon(icon);
            return;
        }
        mStatusView.setImageResource(icon);
        mProgressBar.setVisibility(View.GONE);
        mStatusView.setVisibility(View.VISIBLE);
    }

    /** Hides anything in the status area. */
    void clearStatusIndicator() {
        // Delay if this gets invoked before onBindViewHolder.
        if (mStatusView == null || mProgressBar == null) {
            mDelayedAction = (ignored) -> clearStatusIndicator();
            return;
        }
        mStatusView.setVisibility(View.GONE);
        mProgressBar.setVisibility(View.GONE);
    }
}
