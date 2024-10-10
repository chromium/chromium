// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.feedmanagement;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.Log;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.R;
import org.chromium.chrome.browser.feed.StreamKind;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The MVC pattern Mediator for the Feed Management activity. This activity provides a common place
 * to present management options such as managing the user's activity, their interests, and their
 * list of followed sites.
 * Design doc here: https://docs.google.com/document/d/1D-ZfhGv9GFLXHYKzAqsaw-LiVhsENRTJC5ZMaZ9z0sQ/
 * edit#heading=h.p79wagdgjgx6
 */
public class FeedManagementMediator {
    private static final String TAG = "FeedManagementMdtr";
    private ModelList mModelList;
    private final Context mContext;
    private final @StreamKind int mInitiatingStreamKind;

    FeedManagementMediator(
            Context context, ModelList modelList, @StreamKind int initiatingStreamKind) {
        mModelList = modelList;
        mContext = context;
        mInitiatingStreamKind = initiatingStreamKind;

        // Add the menu items into the menu.
        PropertyModel activityModel =
                generateListItem(
                        R.string.feed_manage_activity,
                        R.string.feed_manage_activity_description,
                        this::handleActivityClick);
        mModelList.add(
                new ModelListAdapter.ListItem(
                        FeedManagementItemProperties.DEFAULT_ITEM_TYPE, activityModel));
        PropertyModel followingModel =
                generateListItem(
                        R.string.feed_manage_following,
                        R.string.feed_manage_following_description,
                        this::handleFollowingClick);
        mModelList.add(
                new ModelListAdapter.ListItem(
                        FeedManagementItemProperties.DEFAULT_ITEM_TYPE, followingModel));
        PropertyModel hiddenModel =
                generateListItem(
                        R.string.feed_manage_hidden,
                        R.string.feed_manage_hidden_description,
                        this::handleHiddenClick);
        mModelList.add(
                new ModelListAdapter.ListItem(
                        FeedManagementItemProperties.DEFAULT_ITEM_TYPE, hiddenModel));
    }

    private PropertyModel generateListItem(
            int titleResource, int descriptionResource, OnClickListener listener) {
        String title = mContext.getResources().getString(titleResource);
        String description = mContext.getResources().getString(descriptionResource);
        return new PropertyModel.Builder(FeedManagementItemProperties.ALL_KEYS)
                .with(FeedManagementItemProperties.TITLE_KEY, title)
                .with(FeedManagementItemProperties.DESCRIPTION_KEY, description)
                .with(FeedManagementItemProperties.ON_CLICK_KEY, listener)
                .build();
    }

    // TODO(petewil): Borrowed these from code we can't link to.  How do I keep them in sync?
    static final String TRUSTED_APPLICATION_CODE_EXTRA = "trusted_application_code_extra";

    // Launch a new activity in the same task with the given uri as a CCT.
    private void launchUriActivity(String uri) {
        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setShowTitle(true);
        builder.setShareState(CustomTabsIntent.SHARE_STATE_ON);
        Intent intent = builder.build().intent;
        intent.setPackage(mContext.getPackageName());
        // Adding trusted extras lets us know that the intent came from Chrome.
        intent.putExtra(TRUSTED_APPLICATION_CODE_EXTRA, getAuthenticationToken());
        intent.setData(Uri.parse(uri));
        intent.setAction(Intent.ACTION_VIEW);
        intent.setClassName(mContext, "org.chromium.chrome.browser.customtabs.CustomTabActivity");
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
        mContext.startActivity(intent);

        // TODO(crbug.com/40758890): Record uma by calling ReportOtherUserAction
        // on the stream.
    }

    // Copied from IntentHandler, which is in chrome_java, so we can't call it directly.
    private PendingIntent getAuthenticationToken() {
        Intent fakeIntent = new Intent();
        ComponentName fakeComponentName = new ComponentName(mContext.getPackageName(), "FakeClass");
        fakeIntent.setComponent(fakeComponentName);
        int mutabililtyFlag = PendingIntent.FLAG_IMMUTABLE;
        return PendingIntent.getActivity(mContext, 0, fakeIntent, mutabililtyFlag);
    }

    @VisibleForTesting
    void handleActivityClick(View view) {
        Log.d(TAG, "Activity click caught." + mInitiatingStreamKind);
        FeedServiceBridge.reportOtherUserAction(
                mInitiatingStreamKind, FeedUserActionType.TAPPED_MANAGE_ACTIVITY);
        launchUriActivity("https://myactivity.google.com/myactivity?product=50");
    }

    @VisibleForTesting
    void handleFollowingClick(View view) {
        Log.d(TAG, "Following click caught.");
        FeedServiceBridge.reportOtherUserAction(
                mInitiatingStreamKind, FeedUserActionType.TAPPED_MANAGE_INTERESTS);
        launchUriActivity("https://www.google.com/preferences/interests/yourinterests?sh=n");
    }

    @VisibleForTesting
    void handleHiddenClick(View view) {
        Log.d(TAG, "Hidden click caught.");
        FeedServiceBridge.reportOtherUserAction(
                mInitiatingStreamKind, FeedUserActionType.TAPPED_MANAGE_INTERESTS);
        launchUriActivity("https://www.google.com/preferences/interests/hidden?sh=n");
    }
}
