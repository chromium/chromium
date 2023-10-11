// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.content.res.Resources;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Controller for the Notice message for Tracking Protection. */
public class TrackingProtectionNoticeController {
    private Context mContext;
    private ActivityTabProvider mActivityTabProvider;
    private ActivityTabTabObserver mActivityTabTabObserver;
    private MessageDispatcher mMessageDispatcher;

    // Setting an indefinite message auto dismiss duration is not possible,
    // hence we provide a value high enough to maintain the message visible.
    private static final int AUTODISMISS_DURATION_ONE_DAY = 24 * 60 * 60 * 1000;

    /**
     * Checks whether the Tracking Protection Notice should be shown.
     *
     * @return boolean value indicating if the Notice should be shown.
     */
    public static boolean shouldShowNotice() {
        return TrackingProtectionBridge.shouldShowOnboardingNotice();
    }

    /**
     * Creates and initializes the Notice controller. Registers an {@link ActivityTabTabObserver}
     * that will attempt to show the 3PCD Notice on an eligible tab in one of the following cases:
     * 1. An already loaded tab page is SECURE. 2. The newly loaded page on the current tab is
     * SECURE.
     *
     * @param activityTabProvider The provider of the current activity tab.
     * @param messageDispatcher The {@link MessageDispatcher} to enqueue the message.
     */
    public static TrackingProtectionNoticeController create(
            Context context,
            ActivityTabProvider activityTabProvider,
            MessageDispatcher messageDispatcher) {
        return new TrackingProtectionNoticeController(
                context, activityTabProvider, messageDispatcher);
    }

    private TrackingProtectionNoticeController(
            Context context,
            ActivityTabProvider activityTabProvider,
            MessageDispatcher messageDispatcher) {
        mContext = context;
        mActivityTabProvider = activityTabProvider;
        mMessageDispatcher = messageDispatcher;

        createActivityTabTabObserver(tab -> showNotice());
    }

    private void showNotice() {
        if (mMessageDispatcher == null) return;

        // TODO(b/295927778): Use final strings.
        Resources resources = mContext.getResources();
        PropertyModel message =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TRACKING_PROTECTION_NOTICE)
                        .with(MessageBannerProperties.TITLE, "Loremi sumd lors Tametco")
                        .with(
                                MessageBannerProperties.DESCRIPTION,
                                "Lor'mi sum ol rsi ametc ns"
                                        + " cteturadip Scingeli Seddoeiusm, tempo incidi untut abor"
                                        + " etdol remag-aaliq autenim dm nimve iam ui nos rudexe.")
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Lor mi")
                        .with(MessageBannerProperties.SECONDARY_BUTTON_MENU_TEXT, "Loremips")
                        .with(MessageBannerProperties.ICON_RESOURCE_ID, R.drawable.ic_eye_crossed)
                        .with(
                                MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID,
                                R.drawable.ic_settings_gear_24dp)
                        .with(
                                MessageBannerProperties.DISMISSAL_DURATION,
                                AUTODISMISS_DURATION_ONE_DAY)
                        .with(
                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> {
                                    // TODO(b/295927778): Report action to the bridge
                                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                                })
                        .with(
                                MessageBannerProperties.ON_SECONDARY_ACTION,
                                () -> {
                                    // TODO(b/295927778): Launch settings and Report
                                    // action to the bridge
                                })
                        .with(
                                MessageBannerProperties.ON_DISMISSED,
                                (dismissReason) -> {
                                    // TODO(b/295927778): Report actions to the bridge
                                })
                        .build();
        mMessageDispatcher.enqueueWindowScopedMessage(message, /* highPriority= */ true);

        destroy();
    }

    private void createActivityTabTabObserver(Callback showNoticeCallback) {
        mActivityTabTabObserver =
                new ActivityTabTabObserver(mActivityTabProvider) {
                    @Override
                    protected void onObservingDifferentTab(Tab tab, boolean hint) {
                        maybeShowNotice(tab);
                    }

                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        maybeShowNotice(tab);
                    }

                    private void maybeShowNotice(Tab tab) {
                        if (tab == null) return;

                        int securityLevel =
                                SecurityStateModel.getSecurityLevelForWebContents(
                                        tab.getWebContents());

                        if (shouldShowNotice()
                                && (ChromeFeatureList.isEnabled(
                                                ChromeFeatureList
                                                        .TRACKING_PROTECTION_ONBOARDING_SKIP_SECURE_PAGE_CHECK)
                                        || securityLevel == ConnectionSecurityLevel.SECURE)) {
                            showNoticeCallback.onResult(tab);
                        }
                    }
                };
    }

    public void destroy() {
        if (mActivityTabTabObserver != null) {
            mActivityTabTabObserver.destroy();
        }
    }
}
