// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.chromium.chrome.browser.privacy_sandbox.TrackingProtectionNoticeController.NOTICE_CONTROLLER_EVENT_HISTOGRAM;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.privacy_sandbox.TrackingProtectionNoticeController.NoticeControllerEvent;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.url.GURL;

/**
 * Controller for the Tracking Protection onboarding logic. Handles notice eligibility and triggers
 * UI.
 */
public class TrackingProtectionOnboardingController {
    private final Context mContext;
    private final TrackingProtectionBridge mTrackingProtectionBridge;
    private final MessageDispatcher mMessageDispatcher;
    private final SettingsLauncher mSettingsLauncher;
    private final ActivityTabProvider mActivityTabProvider;

    private ActivityTabTabObserver mActivityTabTabObserver;
    private TrackingProtectionModeBOnboardingView mTrackingProtectionModeBOnboardingView;
    private TrackingProtectionOnboardingView mTrackingProtectionOnboardingView;

    /**
     * Creates a TrackingProtectionNoticeController.
     *
     * @param context The application context.
     * @param trackingProtectionBridge The bridge to interact with tracking protection state.
     * @param activityTabProvider Provides the currently active tab.
     * @param messageDispatcher The message dispatcher for enqueuing messages.
     * @param settingsLauncher The settings launcher for opening tracking protection settings.
     * @param onboardingType The {@link TrackingProtectionOnboardingType} determining the onboarding
     *     version for Tracking Protection logic.
     */
    private TrackingProtectionOnboardingController(
            Context context,
            TrackingProtectionBridge trackingProtectionBridge,
            ActivityTabProvider activityTabProvider,
            MessageDispatcher messageDispatcher,
            SettingsLauncher settingsLauncher,
            TrackingProtectionOnboardingType onboardingType) {
        mActivityTabProvider = activityTabProvider;
        mContext = context;
        mTrackingProtectionBridge = trackingProtectionBridge;
        mMessageDispatcher = messageDispatcher;
        mSettingsLauncher = settingsLauncher;

        createActivityTabTabObserver(tab -> maybeOnboard(tab, onboardingType));
    }

    /**
     * Creates a TrackingProtectionNoticeController instance.
     *
     * @param context The application context.
     * @param trackingProtectionBridge The TrackingProtectionBridge to communicate with the backend.
     * @param activityTabProvider Provides the currently active tab.
     * @param messageDispatcher The message dispatcher for enqueuing messages.
     * @param settingsLauncher The settings launcher for opening tracking protection settings.
     * @return A new instance of TrackingProtectionNoticeController.
     */
    public static TrackingProtectionOnboardingController create(
            Context context,
            TrackingProtectionBridge trackingProtectionBridge,
            ActivityTabProvider activityTabProvider,
            MessageDispatcher messageDispatcher,
            SettingsLauncher settingsLauncher,
            TrackingProtectionOnboardingType onboardingType) {
        return new TrackingProtectionOnboardingController(
                context,
                trackingProtectionBridge,
                activityTabProvider,
                messageDispatcher,
                settingsLauncher,
                onboardingType);
    }

    /**
     * Checks conditions and potentially shows the tracking protection notice.
     *
     * @param tab The tab on which the notice might be shown.
     */
    public void maybeOnboard(Tab tab, TrackingProtectionOnboardingType onboardingType) {
        if (onboardingType == TrackingProtectionOnboardingType.MODE_B) {
            maybeOnboardModeB(tab);
        } else if (onboardingType == TrackingProtectionOnboardingType.TP_FULL_LAUNCH) {
            maybeOnboardFullLaunch();
        }
    }

    // TODO(b/341968245): Add proper logic once b/341975190 is finished.
    private void maybeOnboardFullLaunch() {
        try {
            TrackingProtectionOnboardingView onboardingView = getTrackingProtectionOnboardingView();
            onboardingView.showNotice(
                    (b) -> {}, (i) -> {}, () -> PrimaryActionClickBehavior.DISMISS_IMMEDIATELY);
        } finally {
            destroy();
        }
    }

    private void maybeOnboardModeB(Tab tab) {
        try {
            if (tab == null || tab.isIncognito()) return;
            if (!shouldShowNotice(mTrackingProtectionBridge)) return;

            int securityLevel =
                    SecurityStateModel.getSecurityLevelForWebContents(tab.getWebContents());

            if (securityLevel != ConnectionSecurityLevel.SECURE) {
                logForSecurityLevel(securityLevel);
                return;
            }

            TrackingProtectionModeBOnboardingView trackingProtectionModeBOnboardingView =
                    getTrackingProtectionModeBOnboardingView();

            if (trackingProtectionModeBOnboardingView.wasNoticeRequested()) {
                logNoticeControllerEvent(NoticeControllerEvent.NOTICE_ALREADY_SHOWING);
                return;
            }

            if (getNoticeType() == NoticeType.SILENT_ONBOARDING) {
                mTrackingProtectionBridge.noticeShown(getNoticeType());
                return;
            }

            trackingProtectionModeBOnboardingView.showNotice(
                    getOnNoticeShownCallback(),
                    getOnNoticeDismissedCallback(),
                    getInNoticePrimaryActionSupplier());

        } finally {
            // Controller's main job is done; view handles the rest
            destroy();
        }
    }

    TrackingProtectionModeBOnboardingView getTrackingProtectionModeBOnboardingView() {
        if (mTrackingProtectionModeBOnboardingView == null) {
            mTrackingProtectionModeBOnboardingView =
                    new TrackingProtectionModeBOnboardingView(
                            mContext,
                            mMessageDispatcher,
                            mSettingsLauncher,
                            mTrackingProtectionBridge);
        }
        return mTrackingProtectionModeBOnboardingView;
    }

    void setTrackingProtectionOnboardingView(
            TrackingProtectionOnboardingView trackingProtectionOnboardingView) {
        mTrackingProtectionOnboardingView = trackingProtectionOnboardingView;
    }

    TrackingProtectionOnboardingView getTrackingProtectionOnboardingView() {
        if (mTrackingProtectionOnboardingView == null) {
            mTrackingProtectionOnboardingView =
                    new TrackingProtectionOnboardingView(
                            mContext, mMessageDispatcher, mSettingsLauncher);
        }
        return mTrackingProtectionOnboardingView;
    }

    void setTrackingProtectionModeBOnboardingView(
            TrackingProtectionModeBOnboardingView trackingProtectionModeBOnboardingView) {
        mTrackingProtectionModeBOnboardingView = trackingProtectionModeBOnboardingView;
    }

    private void logForSecurityLevel(int securityLevel) {
        if (securityLevel != ConnectionSecurityLevel.SECURE) {
            logNoticeControllerEvent(NoticeControllerEvent.NON_SECURE_CONNECTION);
        }
        logNoticeControllerEvent(NoticeControllerEvent.NOTICE_REQUESTED_BUT_NOT_SHOWN);
    }

    private static void logNoticeControllerEvent(@NoticeControllerEvent int action) {
        RecordHistogram.recordEnumeratedHistogram(
                NOTICE_CONTROLLER_EVENT_HISTOGRAM, action, NoticeControllerEvent.COUNT);
    }

    /**
     * Checks whether the Tracking Protection Notice should be shown.
     *
     * @param profile The {@link Profile} associated with the tracking protection state.
     * @return boolean value indicating if the Notice should be shown.
     */
    public static boolean shouldShowNotice(Profile profile) {
        return shouldShowNotice(new TrackingProtectionBridge(profile));
    }

    private static boolean shouldShowNotice(TrackingProtectionBridge trackingProtectionBridge) {
        return trackingProtectionBridge.getRequiredNotice() != NoticeType.NONE;
    }

    private @NoticeType int getNoticeType() {
        return mTrackingProtectionBridge.getRequiredNotice();
    }

    private void createActivityTabTabObserver(Callback<Tab> showNoticeCallback) {
        mActivityTabTabObserver =
                new ActivityTabTabObserver(mActivityTabProvider) {
                    @Override
                    protected void onObservingDifferentTab(Tab tab) {
                        logNoticeControllerEvent(NoticeControllerEvent.ACTIVE_TAB_CHANGED);
                        showNoticeCallback.onResult(tab);
                    }

                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        logNoticeControllerEvent(NoticeControllerEvent.NAVIGATION_FINISHED);
                        showNoticeCallback.onResult(tab);
                    }
                };
        logNoticeControllerEvent(NoticeControllerEvent.CONTROLLER_CREATED);
    }

    private Callback<Boolean> getOnNoticeShownCallback() {
        return (shown) -> {
            if (shown) {
                mTrackingProtectionBridge.noticeShown(getNoticeType());
                logNoticeControllerEvent(NoticeControllerEvent.NOTICE_REQUESTED_AND_SHOWN);
            }
        };
    }

    private Callback<Integer> getOnNoticeDismissedCallback() {
        return (dismissReason) -> {
            switch (dismissReason) {
                case DismissReason.GESTURE:
                    mTrackingProtectionBridge.noticeActionTaken(
                            getNoticeType(), NoticeAction.CLOSED);
                    break;
                case DismissReason.PRIMARY_ACTION:
                case DismissReason.SECONDARY_ACTION:
                    // No need to report these actions: they are already recorded in their
                    // respective handlers.
                    break;
                case DismissReason.SCOPE_DESTROYED:
                    // When the scope is destroyed, we do nothing as we want the user to be
                    // shown the notice again.
                    break;
                default:
                    mTrackingProtectionBridge.noticeActionTaken(
                            getNoticeType(), NoticeAction.OTHER);
            }
        };
    }

    private Supplier<Integer> getInNoticePrimaryActionSupplier() {
        return () -> {
            mTrackingProtectionBridge.noticeActionTaken(getNoticeType(), NoticeAction.GOT_IT);
            return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
        };
    }

    /** Cleans up resources and observers. */
    public void destroy() {
        if (mActivityTabTabObserver != null) {
            mActivityTabTabObserver.destroy();
            logNoticeControllerEvent(NoticeControllerEvent.CONTROLLER_NO_LONGER_OBSERVING);
        }
    }
}
