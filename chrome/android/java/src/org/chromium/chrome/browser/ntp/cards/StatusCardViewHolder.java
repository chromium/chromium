// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.content.Context;
import android.support.annotation.LayoutRes;
import android.support.annotation.StringRes;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.metrics.ImpressionTracker.Listener;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;

/**
 * ViewHolder for Status and Promo cards.
 */
public class StatusCardViewHolder extends CardViewHolder {
    private final TextView mTitleView;
    private final TextView mBodyView;
    private final Button mActionView;
    public StatusCardViewHolder(SuggestionsRecyclerView parent,
            ContextMenuManager contextMenuManager, UiConfig config) {
        super(getLayout(), parent, config, contextMenuManager);
        mTitleView = itemView.findViewById(R.id.status_title);
        mBodyView = itemView.findViewById(R.id.status_body);
        mActionView = itemView.findViewById(R.id.status_action_button);
    }

    /**
     * Interface for data items that will be shown in this card.
     */
    public interface DataSource {
        /**
         * @return Resource ID for the header string.
         */
        @StringRes
        int getHeader();

        /**
         * @return Description string.
         */
        String getDescription();

        /**
         * @return Resource ID for the action label string, or 0 if the card does not have a label.
         */
        @StringRes
        int getActionLabel();

        /**
         * Called when the user clicks on the action button.
         *
         * @param context The context to execute the action in.
         */
        void performAction(Context context);
    }

    public void onBindViewHolder(final DataSource item, Listener listener) {
        super.onBindViewHolder();

        mTitleView.setText(item.getHeader());
        mBodyView.setText(item.getDescription());

        @StringRes
        int actionLabel = item.getActionLabel();
        if (actionLabel != 0) {
            mActionView.setText(actionLabel);
            mActionView.setOnClickListener(view -> {
                SuggestionsMetrics.recordCardActionTapped();
                item.performAction(view.getContext());
            });
            mActionView.setVisibility(View.VISIBLE);
        } else {
            mActionView.setVisibility(View.GONE);
        }

        setImpressionListener(listener);
    }

    @LayoutRes
    private static int getLayout() {
        return R.layout.content_suggestions_status_card_modern;
    }
}
