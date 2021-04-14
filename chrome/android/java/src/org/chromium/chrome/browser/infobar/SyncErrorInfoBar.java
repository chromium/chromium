// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import static org.chromium.base.ContextUtils.getApplicationContext;

import android.content.Context;
import android.text.TextUtils;
import android.widget.ImageView;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SyncAndServicesSettings;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.infobars.ConfirmInfoBar;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarLayout;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/**
 * An {@link InfoBar} that shows sync errors and prompts the user to open settings page.
 */
public class SyncErrorInfoBar
        extends ConfirmInfoBar implements ProfileSyncService.SyncStateChangedListener {
    // Preference key to save the latest time this infobar is viewed.
    @VisibleForTesting
    static final String PREF_SYNC_ERROR_INFOBAR_SHOWN_AT_TIME =
            "sync_error_infobar_shown_shown_at_time";
    @VisibleForTesting
    static final long MINIMAL_DURATION_BETWEEN_INFOBARS_MS =
            TimeUnit.MILLISECONDS.convert(24, TimeUnit.HOURS);

    @IntDef({SyncErrorInfoBarType.NOT_SHOWN, SyncErrorInfoBarType.AUTH_ERROR,
            SyncErrorInfoBarType.PASSPHRASE_REQUIRED, SyncErrorInfoBarType.SYNC_SETUP_INCOMPLETE,
            SyncErrorInfoBarType.CLIENT_OUT_OF_DATE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface SyncErrorInfoBarType {
        int NOT_SHOWN = -1;
        int AUTH_ERROR = 0;
        int PASSPHRASE_REQUIRED = 1;
        int SYNC_SETUP_INCOMPLETE = 2;
        int CLIENT_OUT_OF_DATE = 3;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({SyncErrorInfoBarAction.SHOWN, SyncErrorInfoBarAction.DISMISSED,
            SyncErrorInfoBarAction.OPEN_SETTINGS_CLICKED, SyncErrorInfoBarAction.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    private @interface SyncErrorInfoBarAction {
        int SHOWN = 0;
        int DISMISSED = 1;
        int OPEN_SETTINGS_CLICKED = 2;
        int NUM_ENTRIES = 3;
    }

    private final @SyncErrorInfoBarType int mType;
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
        String error_message = (error == SyncError.SYNC_SETUP_INCOMPLETE)
                ? context.getString(R.string.sync_settings_not_confirmed_title)
                : SyncSettingsUtils.getSyncErrorHint(context, error);
        return new SyncErrorInfoBar(getSyncErrorInfoBarType(),
                context.getString(R.string.sync_error_card_title), error_message,
                context.getString(R.string.open_settings_button));
    }

    @CalledByNative
    private void accept() {
        ProfileSyncService.get().removeSyncStateChangedListener(this);
        recordHistogram(mType, SyncErrorInfoBarAction.OPEN_SETTINGS_CLICKED);

        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            settingsLauncher.launchSettingsActivity(getApplicationContext(),
                    ManageSyncSettings.class, ManageSyncSettings.createArguments(false));
        } else {
            settingsLauncher.launchSettingsActivity(getApplicationContext(),
                    SyncAndServicesSettings.class, SyncAndServicesSettings.createArguments(false));
        }
    }

    @CalledByNative
    private void dismissed() {
        ProfileSyncService.get().removeSyncStateChangedListener(this);
        recordHistogram(mType, SyncErrorInfoBarAction.DISMISSED);
    }

    private SyncErrorInfoBar(@SyncErrorInfoBarType int type, String title, String detailsMessage,
            String primaryButtonText) {
        super(R.drawable.ic_sync_error_legacy_40dp, R.color.default_red, null, title, null,
                primaryButtonText, null);
        mType = type;
        mDetailsMessage = detailsMessage;
        ProfileSyncService.get().addSyncStateChangedListener(this);
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putLong(PREF_SYNC_ERROR_INFOBAR_SHOWN_AT_TIME, System.currentTimeMillis())
                .apply();
        recordHistogram(mType, SyncErrorInfoBarAction.SHOWN);
    }

    @Override
    public void syncStateChanged() {
        if (mType != getSyncErrorInfoBarType()) {
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

    /**
     * Calls native side code to create an infobar.
     */
    public static void maybeLaunchSyncErrorInfoBar(WebContents webContents) {
        if (webContents == null) {
            return;
        }
        @SyncErrorInfoBarType
        int type = getSyncErrorInfoBarType();
        if (hasMinimalIntervalPassed() && type != SyncErrorInfoBarType.NOT_SHOWN) {
            SyncErrorInfoBarJni.get().launch(webContents);
        }
    }

    private static boolean hasMinimalIntervalPassed() {
        long lastShownTime = ContextUtils.getAppSharedPreferences().getLong(
                PREF_SYNC_ERROR_INFOBAR_SHOWN_AT_TIME, 0);
        return System.currentTimeMillis() - lastShownTime > MINIMAL_DURATION_BETWEEN_INFOBARS_MS;
    }

    @SyncErrorInfoBarType
    private static int getSyncErrorInfoBarType() {
        @SyncError
        int error = SyncSettingsUtils.getSyncError();
        switch (error) {
            case SyncError.AUTH_ERROR:
                return SyncErrorInfoBarType.AUTH_ERROR;
            case SyncError.PASSPHRASE_REQUIRED:
                return SyncErrorInfoBarType.PASSPHRASE_REQUIRED;
            case SyncError.SYNC_SETUP_INCOMPLETE:
                return SyncErrorInfoBarType.SYNC_SETUP_INCOMPLETE;
            case SyncError.CLIENT_OUT_OF_DATE:
                return SyncErrorInfoBarType.CLIENT_OUT_OF_DATE;
            default:
                return SyncErrorInfoBarType.NOT_SHOWN;
        }
    }

    private static String getSyncErrorInfoBarHistogramName(@SyncErrorInfoBarType int type) {
        assert type != SyncErrorInfoBarType.NOT_SHOWN;
        String name = "Signin.SyncErrorInfoBar.";
        switch (type) {
            case SyncErrorInfoBarType.AUTH_ERROR:
                name += "AuthError";
                break;
            case SyncErrorInfoBarType.PASSPHRASE_REQUIRED:
                name += "PassphraseRequired";
                break;
            case SyncErrorInfoBarType.SYNC_SETUP_INCOMPLETE:
                name += "SyncSetupIncomplete";
                break;
            case SyncErrorInfoBarType.CLIENT_OUT_OF_DATE:
                name += "ClientOutOfDate";
                break;
            default:
                assert false;
        }
        return name;
    }

    private static void recordHistogram(
            @SyncErrorInfoBarType int type, @SyncErrorInfoBarAction int action) {
        String name = getSyncErrorInfoBarHistogramName(type);
        RecordHistogram.recordEnumeratedHistogram(name, action, SyncErrorInfoBarAction.NUM_ENTRIES);
    }

    @NativeMethods
    interface Natives {
        void launch(WebContents webContents);
    }
}
