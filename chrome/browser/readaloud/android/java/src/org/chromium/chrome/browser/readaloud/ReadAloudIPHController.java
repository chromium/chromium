// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;

/**
 * Controller to manage when and how we show ReadAloud in-product-help messages to users in the app
 * menu.
 */
public class ReadAloudIPHController {
    private final UserEducationHelper mUserEducationHelper;
    private final AppMenuHandler mAppMenuHandler;
    private final View mToolbarMenuButton;
    private final ObservableSupplier<ReadAloudController> mReadAloudControllerSupplier;
    @Nullable ObservableSupplier<String> mReadAloudReadabilitySupplier;
    private final Supplier<Tab> mCurrentTabSupplier;

    /**
     * Constructor.
     *
     * @param activity The current activity.
     * @param toolbarMenuButton The toolbar menu button to which IPH will be anchored.
     * @param appMenuHandler The app menu handler
     * @param tabSupplier The tab supplier
     * @param readAloudControllerSupplier Supplies the readaloud controller
     */
    public ReadAloudIPHController(
            Activity activity,
            View toolbarMenuButton,
            AppMenuHandler appMenuHandler,
            ObservableSupplier<Tab> tabSupplier,
            ObservableSupplier<ReadAloudController> readAloudControllerSupplier) {
        this(
                activity,
                toolbarMenuButton,
                appMenuHandler,
                new UserEducationHelper(activity, new Handler(Looper.getMainLooper())),
                tabSupplier,
                readAloudControllerSupplier);
    }

    ReadAloudIPHController(
            Activity activity,
            View toolbarMenuButton,
            AppMenuHandler appMenuHandler,
            UserEducationHelper userEducationHelper,
            ObservableSupplier<Tab> tabSupplier,
            ObservableSupplier<ReadAloudController> readAloudControllerSupplier) {
        mToolbarMenuButton = toolbarMenuButton;
        mAppMenuHandler = appMenuHandler;
        mUserEducationHelper = userEducationHelper;
        mCurrentTabSupplier = tabSupplier;
        mReadAloudControllerSupplier = readAloudControllerSupplier;
        mReadAloudControllerSupplier.addObserver(this::readAloudControllerReady);
    }

    /**
     * If the current tab is readable, requests to show a "Listen to this page" IPH for the app menu
     * and turns on the highlight for the ReadAloud item in the menu.
     *
     * @param url URL the readability check returns
     */
    public void maybeShowReadAloudAppMenuIPH(String url) {
        if (shouldShowIPH(url)) {
            mUserEducationHelper.requestShowIPH(
                    new IPHCommandBuilder(
                                    mToolbarMenuButton.getContext().getResources(),
                                    FeatureConstants.READ_ALOUD_APP_MENU_FEATURE,
                                    R.string.menu_listen_to_this_page,
                                    R.string.menu_listen_to_this_page)
                            .setAnchorView(mToolbarMenuButton)
                            .setOnShowCallback(
                                    () -> turnOnHighlightForMenuItem(R.id.readaloud_menu_id))
                            .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                            .build());
        }
    }

    private void turnOnHighlightForMenuItem(int highlightMenuItemId) {
        mAppMenuHandler.setMenuHighlight(highlightMenuItemId);
    }

    private void turnOffHighlightForMenuItem() {
        mAppMenuHandler.clearMenuHighlight();
    }

    protected boolean shouldShowIPH(String url) {
        if (mCurrentTabSupplier.get() == null
                || !mCurrentTabSupplier.get().getUrl().isValid()
                || mReadAloudControllerSupplier.get() == null) {
            return false;
        }
        if (mCurrentTabSupplier.get().getUrl().getSpec().equals(url)) {
            return mReadAloudControllerSupplier.get().isReadable(mCurrentTabSupplier.get());
        }
        return false;
    }

    void readAloudControllerReady(@Nullable ReadAloudController readAloudController) {
        if (readAloudController != null) {
            mReadAloudReadabilitySupplier = readAloudController.getReadabilitySupplier();
            mReadAloudReadabilitySupplier.addObserver(this::maybeShowReadAloudAppMenuIPH);
        }
    }

    public void destroy() {
        if (mReadAloudReadabilitySupplier != null) {
            mReadAloudReadabilitySupplier.removeObserver(this::maybeShowReadAloudAppMenuIPH);
        }
    }
}
