// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.interstitial;

import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.STATE;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.holder.GenericViewHolder;
import org.chromium.chrome.browser.download.home.list.holder.InProgressGenericViewHolder;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.browser_ui.widget.DualControlLayout;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.ui.modelutil.PropertyModel;

/** Class for a download interstitial which handles all interaction with the view. */
class DownloadInterstitialView {
    private final View mView;
    private final TextView mTitle;
    private final GenericViewHolder mGenericViewHolder;
    private final InProgressGenericViewHolder mInProgressGenericViewHolder;
    private final Button mPrimaryButton;
    private final Button mSecondaryButton;

    /**
     * @param context The context of the tab to contain the download interstitial.
     * @return A new DownloadInterstitialView instance.
     */
    public static DownloadInterstitialView create(Context context) {
        View view = LayoutInflater.from(context).inflate(R.layout.download_interstitial, null);
        return new DownloadInterstitialView(view, context);
    }

    private DownloadInterstitialView(View view, Context context) {
        mView = view;
        mTitle = mView.findViewById(R.id.heading);
        FrameLayout fileInfo = mView.findViewById(R.id.file_info);
        mGenericViewHolder = GenericViewHolder.create(fileInfo);
        mInProgressGenericViewHolder = InProgressGenericViewHolder.create(fileInfo);
        mGenericViewHolder.itemView.setVisibility(View.GONE);
        mInProgressGenericViewHolder.itemView.setVisibility(View.INVISIBLE);
        fileInfo.addView(mGenericViewHolder.itemView);
        fileInfo.addView(mInProgressGenericViewHolder.itemView);

        mPrimaryButton = DualControlLayout.createButtonForLayout(context, true, "", null);
        mPrimaryButton.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mPrimaryButton.setVisibility(View.INVISIBLE);

        mSecondaryButton = DualControlLayout.createButtonForLayout(
                context, false, mView.getResources().getString(R.string.cancel), null);
        mSecondaryButton.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mSecondaryButton.setVisibility(View.INVISIBLE);

        DualControlLayout buttonBar = mView.findViewById(R.id.button_bar);
        buttonBar.addView(mPrimaryButton);
        buttonBar.addView(mSecondaryButton);
    }

    /** @return The parent view containing the download interstitial.  */
    public View getView() {
        return mView;
    }

    /**
     * Updates the file info section when the attached download's progress or state is updated.
     * @param item The offline item attached to the UI.
     * @param model The property model of the DownloadInterstitial.
     */
    void updateFileInfo(OfflineItem item, PropertyModel model) {
        // TODO(alexmitra): Investigate removing code which sets the item's state directly.
        if (model.get(STATE) == DownloadInterstitialProperties.State.PENDING_REMOVAL) {
            item.state = OfflineItemState.CANCELLED;
        } else if (model.get(STATE) == DownloadInterstitialProperties.State.SUCCESSFUL) {
            item.state = OfflineItemState.COMPLETE;
        }

        if (item.state == OfflineItemState.COMPLETE) {
            mInProgressGenericViewHolder.itemView.setVisibility(View.GONE);
            mGenericViewHolder.itemView.setVisibility(View.VISIBLE);
            mGenericViewHolder.bind(model, new ListItem.OfflineItemListItem(item));
        } else {
            mGenericViewHolder.itemView.setVisibility(View.GONE);
            mInProgressGenericViewHolder.itemView.setVisibility(View.VISIBLE);
            mInProgressGenericViewHolder.bind(model, new ListItem.OfflineItemListItem(item));
        }
    }

    /**
     * Sets the text shown as the title.
     * @param text The new text for the title to display.
     */
    void setTitleText(String text) {
        mTitle.setText(text);
    }

    /**
     * Sets whether the primary button should be shown.
     * @param visible Whether the primary button should be visible.
     */
    void setPrimaryButtonVisibility(boolean visible) {
        mPrimaryButton.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
    }

    /**
     * Sets the text shown on the primary button.
     * @param text The new text for the primary button to display.
     */
    void setPrimaryButtonText(String text) {
        mPrimaryButton.setText(text);
    }

    /**
     * Sets the callback which is run when the primary button is clicked.
     * @param callback The callback to run.
     */
    void setPrimaryButtonCallback(Runnable callback) {
        mPrimaryButton.setOnClickListener(v -> callback.run());
    }

    /**
     * Sets whether the secondary button should be shown.
     * @param visible Whether the secondary button should be visible.
     */
    void setSecondaryButtonVisibility(boolean visible) {
        mSecondaryButton.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
    }

    /**
     * Sets the text shown on the secondary button.
     * @param text The new text for the secondary button to display.
     */
    void setSecondaryButtonText(String text) {
        mSecondaryButton.setText(text);
    }

    /**
     * Sets the callback which is run when the secondary button is clicked.
     * @param callback The callback to run.
     */
    void setSecondaryButtonCallback(Runnable callback) {
        mSecondaryButton.setOnClickListener(v -> callback.run());
    }

    /** Removes the message shown before a download initially begins. */
    void removePendingMessage() {
        mView.findViewById(R.id.loading_message).setVisibility(View.GONE);
    }
}