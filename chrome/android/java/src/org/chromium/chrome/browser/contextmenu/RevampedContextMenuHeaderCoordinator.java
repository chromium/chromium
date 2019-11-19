// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.text.SpannableString;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.webkit.URLUtil;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.night_mode.GlobalNightModeStateProviderHolder;
import org.chromium.chrome.browser.omnibox.OmniboxUrlEmphasizer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.ui.modelutil.PropertyModel;

class RevampedContextMenuHeaderCoordinator {
    private PropertyModel mModel;
    private RevampedContextMenuHeaderMediator mMediator;

    private Context mContext;

    RevampedContextMenuHeaderCoordinator(Activity activity, ContextMenuParams params) {
        mContext = activity;
        mModel = buildModel(getTitle(params), getUrl(activity, params));
        mMediator = new RevampedContextMenuHeaderMediator(activity, mModel, params);
    }

    private PropertyModel buildModel(String title, CharSequence url) {
        return new PropertyModel.Builder(RevampedContextMenuHeaderProperties.ALL_KEYS)
                .with(RevampedContextMenuHeaderProperties.TITLE, title)
                .with(RevampedContextMenuHeaderProperties.TITLE_MAX_LINES,
                        TextUtils.isEmpty(url) ? 2 : 1)
                .with(RevampedContextMenuHeaderProperties.URL, url)
                .with(RevampedContextMenuHeaderProperties.URL_MAX_LINES,
                        TextUtils.isEmpty(title) ? 2 : 1)
                .with(RevampedContextMenuHeaderProperties.IMAGE, null)
                .with(RevampedContextMenuHeaderProperties.CIRCLE_BG_VISIBLE, false)
                .build();
    }

    private String getTitle(ContextMenuParams params) {
        if (!TextUtils.isEmpty(params.getTitleText())) {
            return params.getTitleText();
        }
        if (!TextUtils.isEmpty(params.getLinkText())) {
            return params.getLinkText();
        }
        if (params.isImage() || params.isVideo() || params.isFile()) {
            return URLUtil.guessFileName(params.getSrcUrl(), null, null);
        }
        return "";
    }

    private CharSequence getUrl(Activity activity, ContextMenuParams params) {
        CharSequence url = params.getUrl();
        if (!TextUtils.isEmpty(url)) {
            boolean useDarkColors =
                    !GlobalNightModeStateProviderHolder.getInstance().isInNightMode();
            if (activity instanceof ChromeBaseAppCompatActivity) {
                useDarkColors = !((ChromeBaseAppCompatActivity) activity)
                                         .getNightModeStateProvider()
                                         .isInNightMode();
            }

            SpannableString spannableUrl =
                    new SpannableString(ChromeContextMenuPopulator.createUrlText(params));
            OmniboxUrlEmphasizer.emphasizeUrl(spannableUrl, activity.getResources(),
                    Profile.getLastUsedProfile(), ConnectionSecurityLevel.NONE, false,
                    useDarkColors, false);
            url = spannableUrl;
        }
        return url;
    }

    Callback<Bitmap> getOnImageThumbnailRetrievedReference() {
        return mMediator::onImageThumbnailRetrieved;
    }

    PropertyModel getModel() {
        return mModel;
    }

    View getView() {
        return LayoutInflater.from(mContext).inflate(R.layout.revamped_context_menu_header, null);
    }
}
