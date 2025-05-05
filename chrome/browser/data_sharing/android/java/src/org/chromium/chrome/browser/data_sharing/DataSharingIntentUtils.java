// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.intents.BrowserIntentUtils;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Util class for creating intents. */
@NullMarked
public final class DataSharingIntentUtils {
    public static final String ACTION_EXTRA = "org.chromium.chrome.browser.data_sharing.action";
    public static final String INVITATION_URL_EXTRA =
            "org.chromium.chrome.browser.data_sharing.invitation_url";
    public static final String TAB_GROUP_SYNC_ID_EXTRA =
            "org.chromium.chrome.browser.data_sharing.tab_group_sync_id";

    @IntDef({Action.UNKNOWN, Action.INVITATION_FLOW, Action.MANAGE_TAB_GROUP})
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    /* package */ @interface Action {
        /** Maybe from a parcelling error, expected to be no-oped. */
        int UNKNOWN = 0;

        /** Starts the invitation flow after opening the tab switcher. */
        int INVITATION_FLOW = 1;

        /** Opens the tab group dialog inside the tab switcher for the given tab group. */
        int MANAGE_TAB_GROUP = 2;
    }

    private DataSharingIntentUtils() {}

    /**
     * Create an intent to launch the invitation flow.
     *
     * @param context The {@link Context} to use.
     * @param url The URL associated with the invitation.
     * @return The {@link Intent} to launch the invitation flow.
     */
    public static Intent createInvitationIntent(Context context, GURL url) {
        Intent launchIntent = createdIntentShared(context, Action.INVITATION_FLOW);
        launchIntent.putExtra(INVITATION_URL_EXTRA, url.getSpec());
        return launchIntent;
    }

    /**
     * Create an intent to open the UI for a given tab group.
     *
     * @param syncId Identifies the group.
     * @return A "manage" intent to show the tab group ui.
     */
    public static Intent createManageIntent(Context context, @Nullable String syncId) {
        Intent launchIntent = createdIntentShared(context, Action.MANAGE_TAB_GROUP);
        launchIntent.putExtra(TAB_GROUP_SYNC_ID_EXTRA, syncId);
        return launchIntent;
    }

    private static Intent createdIntentShared(Context context, @Action int action) {
        Intent launchIntent = new Intent(Intent.ACTION_VIEW);
        launchIntent.addCategory(Intent.CATEGORY_DEFAULT);
        launchIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);
        launchIntent.setClassName(context, BrowserIntentUtils.CHROME_LAUNCHER_ACTIVITY_CLASS_NAME);
        launchIntent.putExtra(ACTION_EXTRA, action);
        IntentUtils.addTrustedIntentExtras(launchIntent);
        return launchIntent;
    }
}
