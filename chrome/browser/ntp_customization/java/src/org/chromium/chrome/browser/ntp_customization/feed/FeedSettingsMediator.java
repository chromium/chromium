// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.feed;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.FEED_SWITCH_ON_CHECKED_CHANGE_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.IS_FEED_SWITCH_CHECKED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_VIEW_DELEGATE;
import static org.chromium.chrome.browser.ntp_customization.feed.FeedSettingsCoordinator.FeedSettingsBottomSheetSection.ACTIVITY;
import static org.chromium.chrome.browser.ntp_customization.feed.FeedSettingsCoordinator.FeedSettingsBottomSheetSection.FOLLOWING;
import static org.chromium.chrome.browser.ntp_customization.feed.FeedSettingsCoordinator.FeedSettingsBottomSheetSection.HIDDEN;
import static org.chromium.chrome.browser.ntp_customization.feed.FeedSettingsCoordinator.FeedSettingsBottomSheetSection.INTERESTS;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.provider.Browser;
import android.support.annotation.VisibleForTesting;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.ListContainerViewDelegate;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.feed.FeedSettingsCoordinator.FeedSettingsBottomSheetSection;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Mediator for the feed settings bottom sheet in the NTP customization. */
public class FeedSettingsMediator {
    private static final String TRUSTED_APPLICATION_CODE_EXTRA = "trusted_application_code_extra";
    private static final String ACTIVITY_CLICK_URL =
            "https://myactivity.google.com/myactivity?product=50";
    private static final String FOLLOWING_CLICK_URL =
            "https://www.google.com/preferences/interests/yourinterests?sh=n";
    private static final String HIDDEN_CLICK_URL =
            "https://www.google.com/preferences/interests/hidden?sh=n";
    private static final String INTERESTS_CLICK_URL =
            "https://www.google.com/preferences/interests";
    private final PropertyModel mContainerPropertyModel;
    private final PropertyModel mBottomSheetPropertyModel;
    private final PropertyModel mFeedSettingsPropertyModel;
    private final Profile mProfile;
    private final PrefChangeRegistrar mPrefChangeRegistrar;
    private static PrefService sPrefServiceForTest;
    private static PrefChangeRegistrar sPrefChangeRegistarForTest;

    public FeedSettingsMediator(
            PropertyModel containerPropertyModel,
            PropertyModel bottomSheetPropertyModel,
            PropertyModel feedSettingsPropertyModel,
            BottomSheetDelegate delegate,
            Profile profile) {
        mContainerPropertyModel = containerPropertyModel;
        mBottomSheetPropertyModel = bottomSheetPropertyModel;
        mFeedSettingsPropertyModel = feedSettingsPropertyModel;
        mProfile = profile;

        mContainerPropertyModel.set(LIST_CONTAINER_VIEW_DELEGATE, createListDelegate());
        mBottomSheetPropertyModel.set(
                BACK_PRESS_HANDLER, v -> delegate.backPressOnCurrentBottomSheet());
        mFeedSettingsPropertyModel.set(
                FEED_SWITCH_ON_CHECKED_CHANGE_LISTENER,
                (compoundButton, isChecked) -> onFeedSwitchToggled(isChecked));
        mFeedSettingsPropertyModel.set(IS_FEED_SWITCH_CHECKED, isFeedTurnedOn());

        if (sPrefChangeRegistarForTest != null) {
            mPrefChangeRegistrar = sPrefChangeRegistarForTest;
        } else {
            mPrefChangeRegistrar = PrefServiceUtil.createFor(mProfile);
        }
        mPrefChangeRegistrar.addObserver(Pref.ARTICLES_LIST_VISIBLE, this::updateFeedSwitch);
    }

    void destroy() {
        mPrefChangeRegistrar.removeObserver(Pref.ARTICLES_LIST_VISIBLE);
        mBottomSheetPropertyModel.set(BACK_PRESS_HANDLER, null);
        mContainerPropertyModel.set(LIST_CONTAINER_VIEW_DELEGATE, null);
        mFeedSettingsPropertyModel.set(FEED_SWITCH_ON_CHECKED_CHANGE_LISTENER, null);
    }

    /**
     * Callback on feed switch toggled. This will update the visibility of the feed switch, the feed
     * and the expand icon on the feed section header view.
     */
    @VisibleForTesting
    void onFeedSwitchToggled(boolean isChecked) {
        getPrefService().setBoolean(Pref.ARTICLES_LIST_VISIBLE, isChecked);
    }

    /**
     * Updates whether the feed switch should be turned on.
     *
     * <p>Called when users enable or disable showing Feeds.
     */
    @VisibleForTesting
    void updateFeedSwitch() {
        mFeedSettingsPropertyModel.set(IS_FEED_SWITCH_CHECKED, isFeedTurnedOn());
    }

    /**
     * Returns {@link ListContainerViewDelegate} that defines the content of each list item in the
     * feed bottom sheet.
     */
    @VisibleForTesting
    ListContainerViewDelegate createListDelegate() {
        return new ListContainerViewDelegate() {
            @Override
            public List<Integer> getListItems() {
                List<Integer> content = new ArrayList<>();
                if (FeedServiceBridge.isSignedIn()) {
                    if (WebFeedBridge.isWebFeedEnabled()) {
                        content.add(ACTIVITY);
                        content.add(FOLLOWING);
                        content.add(HIDDEN);
                    } else {
                        content.add(ACTIVITY);
                        content.add(INTERESTS);
                    }
                }
                return content;
            }

            @Override
            public String getListItemTitle(int type, Context context) {
                return getTitleForSectionType(type, context.getResources());
            }

            @Override
            public String getListItemSubtitle(int type, Context context) {
                return getSubtitleForSectionType(type, context.getResources());
            }

            @Override
            public @Nullable OnClickListener getListener(int type) {
                return getListenerForSectionType(type);
            }

            @Override
            public Integer getTrailingIcon(int type) {
                return null;
            }
        };
    }

    /**
     * @param sectionType Type of the feed bottom sheet section.
     * @param resources The {@link Resources} instance to load Android resources from.
     * @return The string of section title for the feed bottom sheet section type.
     */
    private static String getTitleForSectionType(
            @FeedSettingsBottomSheetSection int sectionType, Resources resources) {
        switch (sectionType) {
            case ACTIVITY:
                return resources.getString(R.string.feed_manage_activity);
            case FOLLOWING:
                return resources.getString(R.string.feed_manage_following);
            case HIDDEN:
                return resources.getString(R.string.feed_manage_hidden);
            case INTERESTS:
                return resources.getString(R.string.feed_manage_interests);
            default:
                assert false : "Section type not supported!";
                return null;
        }
    }

    /**
     * @param sectionType Type of the feed bottom sheet section.
     * @param resources The {@link Resources} instance to load Android resources from.
     * @return The string of section subtitle for the feed bottom sheet section type.
     */
    private static String getSubtitleForSectionType(
            @FeedSettingsBottomSheetSection int sectionType, Resources resources) {
        switch (sectionType) {
            case ACTIVITY:
                return resources.getString(R.string.feed_manage_activity_description);
            case FOLLOWING:
                return resources.getString(R.string.feed_manage_following_description);
            case HIDDEN:
                return resources.getString(R.string.feed_manage_hidden_description);
            case INTERESTS:
                return resources.getString(R.string.feed_manage_following_description);
            default:
                assert false : "Section type not supported!";
                return null;
        }
    }

    /**
     * @param sectionType Type of the feed bottom sheet section.
     * @return The on click listener for the feed bottom sheet section type.
     */
    private static OnClickListener getListenerForSectionType(
            @FeedSettingsBottomSheetSection int sectionType) {
        switch (sectionType) {
            case ACTIVITY:
                return FeedSettingsMediator::handleActivityClick;
            case FOLLOWING:
                return FeedSettingsMediator::handleFollowingClick;
            case HIDDEN:
                return FeedSettingsMediator::handleHiddenClick;
            case INTERESTS:
                return FeedSettingsMediator::handleInterestsClick;
            default:
                assert false : "Section type not supported!";
                return null;
        }
    }

    private static void handleActivityClick(View view) {
        launchUriActivity(view.getContext(), ACTIVITY_CLICK_URL);
    }

    private static void handleFollowingClick(View view) {
        launchUriActivity(view.getContext(), FOLLOWING_CLICK_URL);
    }

    private static void handleHiddenClick(View view) {
        launchUriActivity(view.getContext(), HIDDEN_CLICK_URL);
    }

    private static void handleInterestsClick(View view) {
        launchUriActivity(view.getContext(), INTERESTS_CLICK_URL);
    }

    // Launch a new activity in the same task with the given uri as a CCT.
    private static void launchUriActivity(Context context, String uri) {
        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setShowTitle(true);
        builder.setShareState(CustomTabsIntent.SHARE_STATE_ON);
        Intent intent = builder.build().intent;
        intent.setPackage(context.getPackageName());
        // Adding trusted extras lets us know that the intent came from Chrome.
        intent.putExtra(TRUSTED_APPLICATION_CODE_EXTRA, getAuthenticationToken(context));
        intent.setData(Uri.parse(uri));
        intent.setAction(Intent.ACTION_VIEW);
        intent.setClassName(context, "org.chromium.chrome.browser.customtabs.CustomTabActivity");
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        context.startActivity(intent);
    }

    // Copied from IntentHandler, which is in chrome_java, so we can't call it directly.
    private static PendingIntent getAuthenticationToken(Context context) {
        Intent fakeIntent = new Intent();
        ComponentName fakeComponentName = new ComponentName(context.getPackageName(), "FakeClass");
        fakeIntent.setComponent(fakeComponentName);
        int mutabililtyFlag = PendingIntent.FLAG_IMMUTABLE;
        return PendingIntent.getActivity(context, 0, fakeIntent, mutabililtyFlag);
    }

    /** Returns whether the feed articles are turned on and visible to the user. */
    private boolean isFeedTurnedOn() {
        return getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE);
    }

    private PrefService getPrefService() {
        if (sPrefServiceForTest != null) return sPrefServiceForTest;
        return UserPrefs.get(mProfile);
    }

    static void setPrefForTesting(
            PrefChangeRegistrar prefChangeRegistrar, PrefService prefService) {
        sPrefChangeRegistarForTest = prefChangeRegistrar;
        sPrefServiceForTest = prefService;
        ResettersForTesting.register(() -> sPrefServiceForTest = null);
    }
}
