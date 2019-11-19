// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.infobar.SimpleConfirmInfoBarBuilder;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateInteractionSource;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateStatus;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.widget.Toast;

/** Helper class that creates infobars based on {@link UpdateState} changes. */
public class UpdateInfoBarController implements Destroyable {
    private final Callback<UpdateStatus> mObserver = status -> {
        handleStatusChange(status);
    };

    private ChromeActivity mActivity;

    /**
     * @param activity A {@link ChromeActivity} instance the infobars will be shown in.
     * @return A new instance of {@link UpdateInfoBarController}.
     */
    public static UpdateInfoBarController createInstance(ChromeActivity activity) {
        return new UpdateInfoBarController(activity);
    }

    // Destroyable implementation.
    @Override
    public void destroy() {
        UpdateStatusProvider.getInstance().removeObserver(mObserver);
        mActivity.getLifecycleDispatcher().unregister(this);
        mActivity = null;
    }

    private UpdateInfoBarController(ChromeActivity activity) {
        mActivity = activity;
        UpdateStatusProvider.getInstance().addObserver(mObserver);
        mActivity.getLifecycleDispatcher().register(this);
    }

    private void handleStatusChange(UpdateStatus status) {
        switch (status.updateState) {
            case UpdateState.INLINE_UPDATE_READY:
                showRestartInfobar();
                break;
            case UpdateState.INLINE_UPDATE_FAILED:
                showFailedInfobar();
                break;
            case UpdateState.INLINE_UPDATE_DOWNLOADING:
                showDownloadingToast();
                break;
        }
    }

    private void restartChrome() {
        UpdateStatusProvider.getInstance().finishInlineUpdate(UpdateInteractionSource.FROM_INFOBAR);
    }

    private void retryUpdate() {
        if (mActivity == null) return;
        UpdateStatusProvider.getInstance().retryInlineUpdate(
                UpdateInteractionSource.FROM_INFOBAR, mActivity);
    }

    private void showRestartInfobar() {
        if (mActivity == null) return;

        Tab tab = mActivity.getActivityTabProvider().get();
        if (tab == null) return;

        SimpleConfirmInfoBarBuilder.create(tab,
                new SimpleConfirmInfoBarBuilder.Listener() {
                    @Override
                    public void onInfoBarDismissed() {}

                    @Override
                    public boolean onInfoBarButtonClicked(boolean isPrimary) {
                        return false;
                    }

                    @Override
                    public boolean onInfoBarLinkClicked() {
                        restartChrome();
                        return false;
                    }
                },
                InfoBarIdentifier.INLINE_UPDATE_READY_INFOBAR_ANDROID,
                R.drawable.infobar_chrome /* drawableId */,
                mActivity.getString(R.string.inline_update_infobar_ready_message) /* message */,
                null /* primaryText */, null /* secondaryText */,
                mActivity.getString(R.string.inline_update_infobar_ready_link_text) /* linkText */,
                false /* autoExpire */);
    }

    private void showFailedInfobar() {
        if (mActivity == null) return;

        Tab tab = mActivity.getActivityTabProvider().get();
        if (tab == null) return;

        SimpleConfirmInfoBarBuilder.create(tab,
                new SimpleConfirmInfoBarBuilder.Listener() {
                    @Override
                    public void onInfoBarDismissed() {}

                    @Override
                    public boolean onInfoBarButtonClicked(boolean isPrimary) {
                        if (isPrimary) retryUpdate();
                        return false;
                    }

                    @Override
                    public boolean onInfoBarLinkClicked() {
                        return false;
                    }
                },
                InfoBarIdentifier.INLINE_UPDATE_FAILED_INFOBAR_ANDROID,
                R.drawable.infobar_chrome /* drawableId */,
                mActivity.getString(R.string.inline_update_infobar_failed_message) /* message */,
                mActivity.getString(R.string.try_again) /* primaryText */,
                mActivity.getString(R.string.cancel) /* secondaryText */, null /* linkText */,
                false /* autoExpire */);
    }

    private void showDownloadingToast() {
        if (mActivity == null) return;

        Toast.makeText(mActivity,
                     mActivity.getString(R.string.inline_update_toast_downloading_message),
                     Toast.LENGTH_LONG)
                .show();
    }
}
