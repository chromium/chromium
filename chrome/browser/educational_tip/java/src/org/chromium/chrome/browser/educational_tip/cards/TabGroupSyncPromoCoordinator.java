// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import android.content.Context;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;

import org.chromium.base.CallbackController;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.hub.PaneId;

/** Coordinator for the Tab group sync promo card. */
public class TabGroupSyncPromoCoordinator implements EducationalTipCardProvider {

    private final Context mContext;
    private final ShowHubPaneCallback mShowHubPaneRunnable;
    private final Runnable mOnClickedRunnable;
    private CallbackController mCallbackController;

    /**
     * @param context The Context of the application.
     * @param onModuleClickedCallback The callback to be called when the module is clicked.
     * @param callbackController The instance of {@link CallbackController}.
     * @param showHubPaneCallback The callback to open a Hub Pane.
     */
    public TabGroupSyncPromoCoordinator(
            @NonNull Context context,
            @NonNull Runnable onModuleClickedCallback,
            @NonNull CallbackController callbackController,
            @NonNull ShowHubPaneCallback showHubPaneCallback) {
        mContext = context;
        mShowHubPaneRunnable = showHubPaneCallback;
        mCallbackController = callbackController;

        mOnClickedRunnable =
                mCallbackController.makeCancelable(
                        () -> {
                            mShowHubPaneRunnable.onClick(PaneId.TAB_GROUPS);
                            onModuleClickedCallback.run();
                        });
    }

    // EducationalTipCardProvider implementation.

    @Override
    public String getCardTitle() {
        return mContext.getString(R.string.educational_tip_tab_group_sync_title);
    }

    @Override
    public String getCardDescription() {
        return mContext.getString(R.string.educational_tip_tab_group_sync_description);
    }

    @Override
    public @DrawableRes int getCardImage() {
        return R.drawable.tab_group_sync_promo_logo;
    }

    @Override
    public void onCardClicked() {
        mOnClickedRunnable.run();
    }
}
