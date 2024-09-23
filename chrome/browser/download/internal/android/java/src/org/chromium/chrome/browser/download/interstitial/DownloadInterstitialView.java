// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.interstitial;

import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.STATE;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.State.CANCELLED;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListProperties;
import org.chromium.chrome.browser.download.home.list.holder.GenericViewHolder;
import org.chromium.chrome.browser.download.home.list.holder.InProgressGenericViewHolder;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.browser_ui.widget.DualControlLayout;
import org.chromium.components.browser_ui.widget.DualControlLayout.ButtonType;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.ui.modelutil.PropertyModel;

/** Class for a download interstitial which handles all interaction with the view. */
class DownloadInterstitialView {
    private final View mView;
    private final TextView mTitle;
    private final TextView mLoadingMessage;
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
        mLoadingMessage = mView.findViewById(R.id.loading_message);
        FrameLayout fileInfo = mView.findViewById(R.id.file_info);
        mGenericViewHolder = GenericViewHolder.create(fileInfo);
        mInProgressGenericViewHolder = InProgressGenericViewHolder.create(fileInfo);
        mGenericViewHolder.itemView.setVisibility(View.GONE);
        mInProgressGenericViewHolder.itemView.setVisibility(View.INVISIBLE);
        fileInfo.addView(mGenericViewHolder.itemView);
        fileInfo.addView(mInProgressGenericViewHolder.itemView);

        mPrimaryButton =
                DualControlLayout.createButtonForLayout(
                        context, ButtonType.PRIMARY_FILLED, "", null);
        mPrimaryButton.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mPrimaryButton.setVisibility(View.INVISIBLE);

        mSecondaryButton =
                DualControlLayout.createButtonForLayout(
                        context,
                        ButtonType.SECONDARY_TEXT,
                        mView.getResources().getString(R.string.cancel),
                        null);
        mSecondaryButton.setLayoutParams(
                new ViewGroup.LayoutParams(
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
        if (item == null) return;

        if (item.state == OfflineItemState.COMPLETE && model.get(STATE) == CANCELLED) {
            model.get(ListProperties.CALLBACK_REMOVE).onResult(item);
            return;
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

    void setPendingMessageIsVisible(boolean isVisible) {
        if (isVisible) {
            mLoadingMessage.setVisibility(View.VISIBLE);
            mInProgressGenericViewHolder.itemView.setVisibility(View.INVISIBLE);
            mGenericViewHolder.itemView.setVisibility(View.INVISIBLE);
            mPrimaryButton.setVisibility(View.GONE);
            mSecondaryButton.setVisibility(View.GONE);
        } else {
            mLoadingMessage.setVisibility(View.GONE);
        }
    }

    void switchToCancelledViewHolder(OfflineItem item, PropertyModel model) {
        mGenericViewHolder.itemView.setVisibility(View.GONE);
        item.state = OfflineItemState.CANCELLED;
        mInProgressGenericViewHolder.bind(model, new ListItem.OfflineItemListItem(item));
        mInProgressGenericViewHolder.itemView.setVisibility(View.VISIBLE);
    }
}
