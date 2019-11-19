// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.sharing;

import android.net.Uri;
import android.text.TextUtils;
import android.util.Pair;

import org.chromium.base.Promise;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityUmaRecorder.ShareRequestMethod;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TwaVerifier;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.WebApkInfo;
import org.chromium.chrome.browser.webapps.WebApkPostShareTargetNavigator;

import java.util.ArrayList;
import java.util.Locale;

import javax.inject.Inject;

import androidx.browser.trusted.sharing.ShareData;
import androidx.browser.trusted.sharing.ShareTarget;

/**
 * Handles sharing intents coming to Trusted Web Activities.
 */
@ActivityScope
public class TwaSharingController {
    private final CustomTabActivityTabProvider mTabProvider;
    private final CustomTabActivityNavigationController mNavigationController;
    private final WebApkPostShareTargetNavigator mPostNavigator;
    private final TwaVerifier mVerifierDelegate;
    private final TrustedWebActivityUmaRecorder mUmaRecorder;

    @Inject
    public TwaSharingController(CustomTabActivityTabProvider tabProvider,
            CustomTabActivityNavigationController navigationController,
            WebApkPostShareTargetNavigator postNavigator,
            TwaVerifier verifierDelegate,
            TrustedWebActivityUmaRecorder umaRecorder) {
        mTabProvider = tabProvider;
        mNavigationController = navigationController;
        mPostNavigator = postNavigator;
        mVerifierDelegate = verifierDelegate;
        mUmaRecorder = umaRecorder;
    }

    /**
     * Checks whether the incoming intent (represented by a {@link CustomTabIntentDataProvider})
     * is a sharing intent and attempts to perform the sharing.
     *
     * Returns a {@link Promise<Boolean>} with a boolean telling whether sharing was successful.
     */
    public Promise<Boolean> deliverToShareTarget(CustomTabIntentDataProvider intentDataProvider) {
        ShareData shareData = intentDataProvider.getShareData();
        ShareTarget shareTarget = intentDataProvider.getShareTarget();
        if (shareTarget == null || shareData == null) {
            return Promise.fulfilled(false);
        }

        return mVerifierDelegate.verify(shareTarget.action).then(
                (Promise.Function<Boolean, Boolean>) (verified) -> {
            if (!verified) {
                return false;
            }
            WebApkInfo.ShareTarget target = toShareTargetInternal(shareTarget);
            if (target.isShareMethodPost()) {
                boolean success = sendPost(shareData, target);
                if (success) {
                    mUmaRecorder.recordShareTargetRequest(ShareRequestMethod.POST);
                }
                return success;
            }

            mNavigationController.navigate(computeStartUrlForGETShareTarget(shareData, target));
            mUmaRecorder.recordShareTargetRequest(ShareRequestMethod.GET);
            return true;
        });
    }

    /**
     * Converts to internal format.
     * TODO(pshmakov): pull WebApkInfo.ShareTarget out of WebApkInfo and rename to
     * ShareTargetInternal. Also, replace WebApkInfo.ShareData with ShareData from TWA API.
     */
    private WebApkInfo.ShareTarget toShareTargetInternal(ShareTarget shareTarget) {
        ShareTarget.Params params = shareTarget.params;
        String action = shareTarget.action;
        String paramTitle = params.title;
        String paramText = params.text;
        String method = shareTarget.method;
        boolean isPost = method != null && "POST".equals(method.toUpperCase(Locale.ENGLISH));
        String encodingType = shareTarget.encodingType;
        boolean isMultipart = encodingType != null &&
                "multipart/form-data".equals(encodingType.toLowerCase(Locale.ENGLISH));

        int numFiles = params.files == null ? 0 : params.files.size();
        String[] filesArray = new String[numFiles];
        String[][] acceptsArray = new String[numFiles][];
        for (int i = 0; i < numFiles; i++) {
            ShareTarget.FileFormField file = params.files.get(i);
            filesArray[i] = file.name;
            acceptsArray[i] =  file.acceptedTypes.toArray(new String[file.acceptedTypes.size()]);
        }
        return new WebApkInfo.ShareTarget(
                action, paramTitle, paramText, isPost, isMultipart, filesArray, acceptsArray);
    }

    private boolean sendPost(ShareData shareData, WebApkInfo.ShareTarget target) {
        WebApkInfo.ShareData webApkData = new WebApkInfo.ShareData();
        if (shareData.uris != null) {
            webApkData.files = new ArrayList<>(shareData.uris);
        }
        webApkData.subject = shareData.title;
        webApkData.text = shareData.text;

        Tab tab = mTabProvider.getTab();
        if (tab == null) {
            assert false : "Null tab when sharing";
            return false;
        }
        return mPostNavigator.navigateIfPostShareTarget(target.getAction(), target, webApkData,
                tab.getWebContents());
    }


    // Copy of HostBrowserLauncherParams#computeStartUrlForGETShareTarget().
    // Since the latter is in the WebAPK client code, we can't reuse it.
    private static String computeStartUrlForGETShareTarget(
            ShareData data, WebApkInfo.ShareTarget target) {

        // These can be null, they are checked downstream.
        ArrayList<Pair<String, String>> entryList = new ArrayList<>();
        entryList.add(new Pair<>(target.getParamTitle(), data.title));
        entryList.add(new Pair<>(target.getParamText(), data.text));

        return createGETWebShareTargetUriString(target.getAction(), entryList);
    }

    private static String createGETWebShareTargetUriString(
            String action, ArrayList<Pair<String, String>> entryList) {
        Uri.Builder queryBuilder = new Uri.Builder();
        for (Pair<String, String> nameValue : entryList) {
            if (!TextUtils.isEmpty(nameValue.first) && !TextUtils.isEmpty(nameValue.second)) {
                // Uri.Builder does URL escaping.
                queryBuilder.appendQueryParameter(nameValue.first, nameValue.second);
            }
        }
        Uri shareUri = Uri.parse(action);
        Uri.Builder builder = shareUri.buildUpon();
        // Uri.Builder uses %20 rather than + for spaces, the spec requires +.
        String queryString = queryBuilder.build().toString();
        if (TextUtils.isEmpty(queryString)) {
            return action;
        }
        builder.encodedQuery(queryString.replace("%20", "+").substring(1));
        return builder.build().toString();
    }
}
