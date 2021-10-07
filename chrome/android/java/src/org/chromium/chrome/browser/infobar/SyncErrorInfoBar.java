// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import static org.chromium.base.ContextUtils.getApplicationContext;

import android.content.Context;
import android.text.TextUtils;
import android.widget.ImageView;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.chrome.browser.sync.ui.SyncErrorPromptUtils;
import org.chromium.chrome.browser.sync.ui.SyncErrorPromptUtils.SyncErrorPromptAction;
import org.chromium.chrome.browser.sync.ui.SyncErrorPromptUtils.SyncErrorPromptType;
import org.chromium.components.infobars.ConfirmInfoBar;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarLayout;
import org.chromium.content_public.browser.WebContents;

/**
 * An {@link InfoBar} that shows sync errors and prompts the user to open settings page.
 */
public class SyncErrorInfoBar
        extends ConfirmInfoBar implements SyncService.SyncStateChangedListener {
    private final @SyncErrorPromptType int mType;
    private final String mDetailsMessage;


    /**
     * This function is called after maybeLaunchSyncErrorInfoBar sends launch signal to the native
     * side code.
     */
    @CalledByNative
    private static InfoBar show() {
        Context context = getApplicationContext();
        @SyncError
        int error = SyncSettingsUtils.getSyncError();
        String errorMessage = SyncErrorPromptUtils.getErrorMessage(context, error);
        String title = SyncErrorPromptUtils.getTitle(context, error);
        String primaryButtonText = SyncErrorPromptUtils.getPrimaryButtonText(context, error);

        return new SyncErrorInfoBar(SyncErrorPromptUtils.getSyncErrorUiType(error), title,
                errorMessage, primaryButtonText);
    }

    @CalledByNative
    private void accept() {
        SyncService.get().removeSyncStateChangedListener(this);
        recordHistogram(SyncErrorPromptAction.BUTTON_CLICKED);
        SyncErrorPromptUtils.onUserAccepted(mType);
    }

    @CalledByNative
    private void dismissed() {
        SyncService.get().removeSyncStateChangedListener(this);
        recordHistogram(SyncErrorPromptAction.DISMISSED);
    }

    private SyncErrorInfoBar(@SyncErrorPromptType int type, String title, String detailsMessage,
            String primaryButtonText) {
        super(R.drawable.ic_sync_error_legacy_40dp, R.color.default_red, null, title, null,
                primaryButtonText, null);
        mType = type;
        mDetailsMessage = detailsMessage;
        SyncService.get().addSyncStateChangedListener(this);
        SyncErrorPromptUtils.updateLastShownTime();
        recordHistogram(SyncErrorPromptAction.SHOWN);
    }

    @Override
    public void syncStateChanged() {
        if (mType != SyncErrorPromptUtils.getSyncErrorUiType(SyncSettingsUtils.getSyncError())) {
            onCloseButtonClicked();
        }
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        ImageView icon = layout.getIcon();
        icon.getLayoutParams().width = icon.getLayoutParams().height =
                getApplicationContext().getResources().getDimensionPixelSize(
                        R.dimen.sync_error_infobar_icon_size);
        if (!TextUtils.isEmpty(mDetailsMessage)) {
            layout.getMessageLayout().addDescription(mDetailsMessage);
        }
    }

    @Override
    protected void onStartedHiding() {
        super.onStartedHiding();
        if (!isFrontInfoBar()) {
            // SyncErrorInfoBar was not visible to the user, so we need to reset this pref that is
            // used to block SyncErrorInfoBars from appearing within the minimal interval.
            SyncErrorPromptUtils.resetLastShownTime();
        }
    }

    /**
     * Calls native side code to create an infobar.
     */
    public static void maybeLaunchSyncErrorInfoBar(@Nullable WebContents webContents) {
        if (webContents == null) {
            return;
        }
        if (!SyncErrorPromptUtils.shouldShowPrompt(
                    SyncErrorPromptUtils.getSyncErrorUiType(SyncSettingsUtils.getSyncError()))) {
            return;
        }
        SyncErrorInfoBarJni.get().launch(webContents);
    }

    private void recordHistogram(@SyncErrorPromptAction int action) {
        assert mType != SyncErrorPromptType.NOT_SHOWN;
        String name = "Signin.SyncErrorInfoBar."
                + SyncErrorPromptUtils.getSyncErrorPromptUiHistogramSuffix(mType);
        RecordHistogram.recordEnumeratedHistogram(name, action, SyncErrorPromptAction.NUM_ENTRIES);
    }

    @NativeMethods
    interface Natives {
        void launch(WebContents webContents);
    }
}
