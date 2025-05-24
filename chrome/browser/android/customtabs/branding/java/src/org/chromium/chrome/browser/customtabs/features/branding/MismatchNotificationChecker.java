// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.customtabs.features.branding.proto.AccountMismatchData.CloseType;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.HashUtil;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GaiaId;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * Class that drives the account mismatch notification flow. Works in conjunction with {@link
 * BrandingController} for global branding rate-limiting policy.
 */
@NullMarked
public class MismatchNotificationChecker {
    private final Profile mProfile;
    private final Delegate mDelegate;
    private final IdentityManager mIdentityManager;
    private final CallbackController mCallbackController = new CallbackController();

    /** Used to suppress IPH UIs while the mismatch notification UI is on the screen. */
    private Tracker.@Nullable DisplayLockHandle mFeatureEngagementLock;

    /**
     * Whether the other prompt UIs should be suppressed. This var is set to {@code true} while the
     * mismatch notification UI is on the screen.
     */
    private boolean mShouldSuppressPromptUis;

    /** Interface bridging the checker with the account mismatch rate-limiting logic. */
    public interface Delegate {
        /**
         * Show the account mismatch UI if conditions are right.
         *
         * @param accountId Account ID to be used to access notification data.
         * @param lastShownTime The last time the notification was shown to user.
         * @param mimData Account mismatch notification data.
         * @param onClose Callback to be invoked when the notification is closed.
         * @return Whether the UI will be shown or not.
         */
        boolean maybeShow(
                String accountId,
                long lastShownTime,
                @Nullable MismatchNotificationData mimData,
                Callback<Integer> onClose);
    }

    /**
     * Constructor.
     *
     * @param profile The current profile object.
     * @param identityManager The manager providing the account info.
     * @param delegate Delegate providing the actual decision/UI logic.
     */
    public MismatchNotificationChecker(
            Profile profile, IdentityManager identityManager, Delegate delegate) {
        mProfile = profile;
        mIdentityManager = identityManager;
        mDelegate = delegate;
    }

    /** Show account mismatch notification UI if all the conditions are met. */
    public boolean maybeShow(
            String appId,
            long lastShowTime,
            @Nullable MismatchNotificationData data,
            Callback<MismatchNotificationData> closeCallback) {
        String accountId = getAccountId();
        MismatchNotificationData mimData = data; // effective final
        boolean show =
                mDelegate.maybeShow(
                        accountId,
                        lastShowTime,
                        data,
                        mCallbackController.makeCancelable(
                                (closeType) -> {
                                    mShouldSuppressPromptUis = false;
                                    if (mFeatureEngagementLock != null) {
                                        mFeatureEngagementLock.release();
                                    }
                                    // The UI was not visible. Do not do the update.
                                    if (closeType == CloseType.UNKNOWN.getNumber()) return;

                                    MismatchNotificationData res =
                                            mimData != null
                                                    ? mimData
                                                    : new MismatchNotificationData();
                                    var appData = res.getAppData(accountId, appId);
                                    appData.showCount++;
                                    appData.closeType = closeType;
                                    if (closeType == CloseType.DISMISSED.getNumber()
                                            || closeType == CloseType.ACCEPTED.getNumber()) {
                                        appData.userActCount++;
                                    }
                                    res.setAppData(accountId, appId, appData);
                                    closeCallback.onResult(res);
                                }));
        if (show) {
            Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
            mFeatureEngagementLock = tracker.acquireDisplayLock();
            mShouldSuppressPromptUis = true;
        }
        return show;
    }

    /** Returns a cropped hash of the currently sign-in account ID. */
    @VisibleForTesting
    String getAccountId() {
        CoreAccountInfo account = mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        GaiaId gaiaId = CoreAccountInfo.getGaiaIdFrom(account);
        if (gaiaId == null) return "";
        var hash = HashUtil.getMd5Hash(new HashUtil.Params(gaiaId.toString()));
        if (hash == null) return "";
        return hash.substring(0, 16);
    }

    /** Whether prompt Ui components should be temporaily suppressed. */
    public boolean shouldSuppressPromptUis() {
        return mShouldSuppressPromptUis;
    }

    void cancel() {
        mCallbackController.destroy();
        mShouldSuppressPromptUis = false;
        if (mFeatureEngagementLock != null) mFeatureEngagementLock.release();
    }
}
