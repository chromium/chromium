// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.graphics.drawable.Drawable;
import android.support.graphics.drawable.Animatable2Compat;
import android.support.graphics.drawable.AnimatedVectorDrawableCompat;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v4.view.ViewCompat;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadInfoBarController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * An {@link InfoBar} to provide information about currently running downloads.
 */
public class DownloadProgressInfoBar extends InfoBar {
    /**
     * Represents the client of this InfoBar. Provides hooks to take actions on various UI events
     * associated with the InfoBar.
     */
    public interface Client {
        /**
         * Called when a link is clicked by the user.
         * @param itemId The ContentId of the item currently being shown in the InfoBar.
         */
        void onLinkClicked(@Nullable ContentId itemId);

        /**
         * Called when the InfoBar is closed either implicitly or explicitly by the user.
         * @param explicitly Whether the InfoBar was closed explicitly by the user from close
         * button.
         */
        void onInfoBarClosed(boolean explicitly);
    }

    private final Client mClient;
    private AnimatedVectorDrawableCompat mAnimatedDrawable;
    private DownloadInfoBarController.DownloadProgressInfoBarData mInfo;

    // Indicates whether there is a pending layout update waiting for the currently running
    // animation to finish.
    private boolean mUpdatePending;

    private DownloadProgressInfoBar(
            Client client, DownloadInfoBarController.DownloadProgressInfoBarData info) {
        super(info.icon, 0, null, null);
        mInfo = info;
        mClient = client;
    }

    @Override
    protected boolean usesCompactLayout() {
        return false;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        setLayoutProperties(layout, mInfo);
    }

    @Override
    public int getPriority() {
        return InfoBarPriority.BACKGROUND;
    }

    @Override
    public void onCloseButtonClicked() {
        mClient.onInfoBarClosed(true);
        super.onCloseButtonClicked();
    }

    @Override
    public void onLinkClicked() {
        mClient.onLinkClicked(mInfo.id);
    }

    /**
     * @return The tab associated with this infobar.
     */
    public Tab getTab() {
        return getNativeInfoBarPtr() == 0
                ? null
                : DownloadProgressInfoBarJni.get().getTab(
                        getNativeInfoBarPtr(), DownloadProgressInfoBar.this);
    }

    /**
     * Updates the infobar layout with the given data. If the infobar icon is already showing an
     * animation, it will wait for the currently animation to finish before showing the new data.
     */
    private void setLayoutProperties(
            InfoBarLayout layout, DownloadInfoBarController.DownloadProgressInfoBarData info) {
        mInfo = info;

        if (mAnimatedDrawable != null && mAnimatedDrawable.isRunning()) {
            // Wait for the current animation to finish.
            mUpdatePending = true;
        } else {
            updateLayout(layout);
        }
    }

    /**
     * Populates the infobar layout with the given information and handles the icon animation. Note
     * that the icon can be specified as a static image or an animated image. The animation can be
     * specified to be once-only or repeating.
     */
    private void updateLayout(InfoBarLayout layout) {
        layout.setMessage(mInfo.message);
        layout.appendMessageLinkText(mInfo.link);

        TextView messageView =
                (TextView) layout.getMessageLayout().findViewById(R.id.infobar_message);
        messageView.setContentDescription(mInfo.accessibilityMessage);
        ViewCompat.setAccessibilityLiveRegion(messageView, View.ACCESSIBILITY_LIVE_REGION_POLITE);

        if (!mInfo.hasAnimation) {
            if (mInfo.hasVectorDrawable) {
                layout.getIcon().setImageDrawable(VectorDrawableCompat.create(
                        layout.getResources(), mInfo.icon, layout.getContext().getTheme()));
            } else {
                layout.getIcon().setImageResource(mInfo.icon);
            }
            return;
        }

        mAnimatedDrawable = AnimatedVectorDrawableCompat.create(layout.getContext(), mInfo.icon);
        mAnimatedDrawable.registerAnimationCallback(new Animatable2Compat.AnimationCallback() {
            @Override
            public void onAnimationEnd(Drawable drawable) {
                if (mUpdatePending) {
                    mUpdatePending = false;
                    updateLayout(layout);
                    return;
                }

                if (mInfo.dontRepeat) return;
                restartIconAnimation();
            }
        });

        layout.getIcon().setImageDrawable(mAnimatedDrawable);
        mAnimatedDrawable.start();
    }

    private void restartIconAnimation() {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            if (mAnimatedDrawable == null) return;
            mAnimatedDrawable.start();
        });
    }

    /**
     * Updates an existing {@link DownloadProgressInfoBar} with the new information.
     * @param info The information to be updated on the UI.
     */
    public void updateInfoBar(DownloadInfoBarController.DownloadProgressInfoBarData info) {
        if (getView() == null) return;

        mInfo = info;
        setLayoutProperties((InfoBarLayout) getView(), info);
    }

    @Override
    public CharSequence getAccessibilityText() {
        return null;
    }

    /**
     * Creates and shows the {@link DownloadProgressInfoBar}.
     * @param tab The tab that the {@link DownloadProgressInfoBar} should be shown in.
     */
    public static void createInfoBar(
            Client client, Tab tab, DownloadInfoBarController.DownloadProgressInfoBarData info) {
        DownloadProgressInfoBarJni.get().create(client, tab, info);
    }

    /**
     * Closes the {@link DownloadProgressInfoBar}.
     */
    public void closeInfoBar() {
        mClient.onInfoBarClosed(false);
        if (mAnimatedDrawable != null) mAnimatedDrawable.clearAnimationCallbacks();
        super.onCloseButtonClicked();
    }

    @CalledByNative
    private static DownloadProgressInfoBar create(
            Client client, DownloadInfoBarController.DownloadProgressInfoBarData info) {
        return new DownloadProgressInfoBar(client, info);
    }

    @NativeMethods
    interface Natives {
        void create(
                Client client, Tab tab, DownloadInfoBarController.DownloadProgressInfoBarData info);
        Tab getTab(long nativeDownloadProgressInfoBar, DownloadProgressInfoBar caller);
    }
}
