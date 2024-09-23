// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.history.AppFilterCoordinator.AppInfo;
import org.chromium.chrome.browser.history.AppFilterCoordinator.CloseCallback;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;

/** Mediator class for history app filter sheet. */
class AppFilterMediator {
    private final ModelList mModelList;
    private final CloseCallback mCloseCallback;

    private @Nullable PropertyModel mSelectedModel;

    AppFilterMediator(
            Context context,
            ModelList modelList,
            List<AppInfo> appInfoList,
            CloseCallback closeCallback) {
        mModelList = modelList;
        mCloseCallback = closeCallback;
        for (AppInfo info : appInfoList) {
            PropertyModel item = generateListItem(info);
            mModelList.add(new SimpleRecyclerViewAdapter.ListItem(0, item));
        }
    }

    private @Nullable PropertyModel generateListItem(AppInfo info) {
        PropertyModel model =
                new PropertyModel.Builder(AppFilterProperties.LIST_ITEM_KEYS)
                        .with(AppFilterProperties.ID, info.id)
                        .with(AppFilterProperties.ICON, info.icon)
                        .with(AppFilterProperties.LABEL, info.label)
                        .with(AppFilterProperties.SELECTED, false)
                        .build();
        model.set(AppFilterProperties.CLICK_LISTENER, v -> handleClick(model));
        return model;
    }

    void resetState(AppInfo currentApp) {
        if (mSelectedModel != null) {
            mSelectedModel.set(AppFilterProperties.SELECTED, false);
            mSelectedModel = null;
        }
        if (currentApp != null) {
            mSelectedModel = getModelForAppId(currentApp.id);
            mSelectedModel.set(AppFilterProperties.SELECTED, true);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void handleClick(PropertyModel model) {
        PropertyModel prevModel = mSelectedModel;

        String appId = model.get(AppFilterProperties.ID);
        boolean toFullHistory = prevModel != null && prevModel == model;

        if (prevModel != null) prevModel.set(AppFilterProperties.SELECTED, false);
        mSelectedModel = toFullHistory ? null : model;
        if (toFullHistory) {
            mCloseCallback.onAppUpdated(null);
        } else {
            mSelectedModel.set(AppFilterProperties.SELECTED, true);
            AppInfo appInfo = new AppInfo(appId, null, model.get(AppFilterProperties.LABEL));
            mCloseCallback.onAppUpdated(appInfo);
        }
    }

    private PropertyModel getModelForAppId(String appId) {
        for (SimpleRecyclerViewAdapter.ListItem item : mModelList) {
            if (appId.equals(item.model.get(AppFilterProperties.ID))) {
                return item.model;
            }
        }
        return null;
    }

    void clickItemForTesting(String appId) {
        handleClick(getModelForAppId(appId));
    }

    void setCurrentAppForTesting(String appId) {
        mSelectedModel = getModelForAppId(appId);
    }

    String getCurrentAppIdForTesting() {
        return mSelectedModel != null ? mSelectedModel.get(AppFilterProperties.ID) : null;
    }
}
