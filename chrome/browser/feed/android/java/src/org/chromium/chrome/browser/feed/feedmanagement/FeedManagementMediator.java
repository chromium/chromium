// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.feedmanagement;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.provider.Browser;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.compat.ApiHelperForM;
import org.chromium.chrome.browser.feed.webfeed.R;
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

class FeedManagementMediator {
    private static final String TAG = "FeedManagementMdtr";
    private ModelList mModelList;
    private final Context mContext;

    FeedManagementMediator(Context context, ModelList modelList) {
        mModelList = modelList;
        mContext = context;
        PropertyModel activityModel = generateListItem(R.string.feed_manage_activity,
                R.string.feed_manage_activity_description, this::handleActivityClick);
        PropertyModel interestsModel = generateListItem(R.string.feed_manage_interests,
                R.string.feed_manage_interests_description, this::handleInterestsClick);
        PropertyModel hiddenModel = generateListItem(R.string.feed_manage_hidden,
                R.string.feed_manage_hidden_description, this::handleHiddenClick);
        PropertyModel followingModel = generateListItem(R.string.feed_manage_following,
                R.string.feed_manage_following_description, this::handleFollowingClick);
        // Add the menu items into the menu.
        mModelList.add(new ModelListAdapter.ListItem(
                FeedManagementItemProperties.DEFAULT_ITEM_TYPE, activityModel));
        mModelList.add(new ModelListAdapter.ListItem(
                FeedManagementItemProperties.DEFAULT_ITEM_TYPE, interestsModel));
        mModelList.add(new ModelListAdapter.ListItem(
                FeedManagementItemProperties.DEFAULT_ITEM_TYPE, hiddenModel));
        mModelList.add(new ModelListAdapter.ListItem(
                FeedManagementItemProperties.DEFAULT_ITEM_TYPE, followingModel));
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
    static final String EXTRA_UI_TYPE = "org.chromium.chrome.browser.customtabs.EXTRA_UI_TYPE";
    static final String TRUSTED_APPLICATION_CODE_EXTRA = "trusted_application_code_extra";

    // Launch a new activity in the same task with the given uri as a CCT.
    private void launchUriActivity(String uri) {
        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
        builder.setShowTitle(true);
        builder.setShareState(CustomTabsIntent.SHARE_STATE_ON);
        Intent intent = builder.build().intent;
        intent.setData(Uri.parse(uri));
        intent.setAction(Intent.ACTION_VIEW);
        intent.setClassName(mContext, "org.chromium.chrome.browser.customtabs.CustomTabActivity");

        // Do the things that createCustomTabActivityIntent does:
        intent.setPackage(mContext.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK); // Needed for pre-N versions of android.
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());

        // Adding trusted extras lets us know that the intent came from Chrome.
        intent.setPackage(ContextUtils.getApplicationContext().getPackageName());
        intent.putExtra(TRUSTED_APPLICATION_CODE_EXTRA, getAuthenticationToken());
        mContext.startActivity(intent);
        // TODO(https://crbug.com/1195209): Record uma by calling ReportOtherUserAction
        // on the stream.
    }

    // Copied from IntentHandler, which is in chrome_java, so we can't call it directly.
    private static PendingIntent getAuthenticationToken() {
        Intent fakeIntent = new Intent();
        Context appContext = ContextUtils.getApplicationContext();
        ComponentName fakeComponentName =
                new ComponentName(appContext.getPackageName(), "FakeClass");
        fakeIntent.setComponent(fakeComponentName);
        int mutabililtyFlag = 0;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            mutabililtyFlag = ApiHelperForM.getPendingIntentImmutableFlag();
        }
        return PendingIntent.getActivity(appContext, 0, fakeIntent, mutabililtyFlag);
    }

    private void handleActivityClick(View view) {
        Log.d(TAG, "Activity click caught.");
        launchUriActivity("https://myactivity.google.com/myactivity?product=50");
    }

    private void handleInterestsClick(View view) {
        Log.d(TAG, "Interests click caught.");
        launchUriActivity("https://www.google.com/preferences/interests/yourinterests");
    }

    private void handleHiddenClick(View view) {
        Log.d(TAG, "Hidden click caught.");
        launchUriActivity("https://www.google.com/preferences/interests/hidden");
    }

    private void handleFollowingClick(View view) {
        Log.d(TAG, "Following click caught.");
        // TODO(petewil): Launch a new activity for the following management page.
    }
}
