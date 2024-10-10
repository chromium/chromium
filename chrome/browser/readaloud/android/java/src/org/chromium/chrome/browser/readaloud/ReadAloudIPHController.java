// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;

/**
 * Controller to manage when and how we show ReadAloud in-product-help messages to users in the app
 * menu and the CCT app menu.
 */
public class ReadAloudIPHController {
    private final UserEducationHelper mUserEducationHelper;
    private final AppMenuHandler mAppMenuHandler;
    private final View mToolbarMenuButton;
    private final ObservableSupplier<ReadAloudController> mReadAloudControllerSupplier;
    private final Supplier<Tab> mCurrentTabSupplier;
    private boolean mShowAppMenuTextBubble;
    private final Runnable mReadabilityUpdateListener = this::maybeShowReadAloudAppMenuIPH;

    /**
     * Constructor.
     *
     * @param activity The current activity.
     * @param profile The current Profile.
     * @param toolbarMenuButton The toolbar menu button to which IPH will be anchored.
     * @param appMenuHandler The app menu handler
     * @param tabSupplier The tab supplier
     * @param readAloudControllerSupplier Supplies the readaloud controller
     * @param showAppMenuTextBubble If the app menu text bubble should be shown. Should not be shown
     *     for custom tabs
     */
    public ReadAloudIPHController(
            Activity activity,
            Profile profile,
            View toolbarMenuButton,
            AppMenuHandler appMenuHandler,
            ObservableSupplier<Tab> tabSupplier,
            ObservableSupplier<ReadAloudController> readAloudControllerSupplier,
            boolean showAppMenuTextBubble) {
        this(
                activity,
                toolbarMenuButton,
                appMenuHandler,
                new UserEducationHelper(activity, profile, new Handler(Looper.getMainLooper())),
                tabSupplier,
                readAloudControllerSupplier,
                showAppMenuTextBubble);
    }

    ReadAloudIPHController(
            Activity activity,
            View toolbarMenuButton,
            AppMenuHandler appMenuHandler,
            UserEducationHelper userEducationHelper,
            ObservableSupplier<Tab> tabSupplier,
            ObservableSupplier<ReadAloudController> readAloudControllerSupplier,
            boolean showAppMenuTextBubble) {
        mToolbarMenuButton = toolbarMenuButton;
        mAppMenuHandler = appMenuHandler;
        mUserEducationHelper = userEducationHelper;
        mCurrentTabSupplier = tabSupplier;
        mReadAloudControllerSupplier = readAloudControllerSupplier;
        mReadAloudControllerSupplier.addObserver(this::readAloudControllerReady);
        mShowAppMenuTextBubble = showAppMenuTextBubble;
    }

    /**
     * If the current tab is readable, requests to show a "Listen to this page" IPH for the app menu
     * and turns on the highlight for the ReadAloud item in the menu. Depends on the
     * IPHMenuButtonHighlightCCTEnabled flag and if showing the app menu text bubble to decide
     * whether to show the app menu button highlight.
     *
     * @param url URL the readability check returns
     */
    public void maybeShowReadAloudAppMenuIPH() {
        if (shouldShowIPH()) {
            boolean isHighlightEnabled =
                    mShowAppMenuTextBubble
                            ? true
                            : ReadAloudFeatures.isIPHMenuButtonHighlightCCTEnabled();
            mUserEducationHelper.requestShowIPH(
                    new IPHCommandBuilder(
                                    mToolbarMenuButton.getContext().getResources(),
                                    FeatureConstants.READ_ALOUD_APP_MENU_FEATURE,
                                    R.string.menu_listen_to_this_page_iph,
                                    R.string.menu_listen_to_this_page_iph)
                            .setAnchorView(mToolbarMenuButton)
                            .setShowTextBubble(mShowAppMenuTextBubble)
                            .setOnShowCallback(
                                    () ->
                                            turnOnHighlightForMenuItem(
                                                    R.id.readaloud_menu_id, isHighlightEnabled))
                            .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                            .build());
        }
    }

    private void turnOnHighlightForMenuItem(int highlightMenuItemId, boolean highlightMenuButton) {
        mAppMenuHandler.setMenuHighlight(highlightMenuItemId, highlightMenuButton);
    }

    private void turnOffHighlightForMenuItem() {
        mAppMenuHandler.clearMenuHighlight();
    }

    private boolean shouldShowIPH() {
        if (mCurrentTabSupplier.get() == null
                || !mCurrentTabSupplier.get().getUrl().isValid()
                || mReadAloudControllerSupplier.get() == null) {
            return false;
        }
        return mReadAloudControllerSupplier.get().isReadable(mCurrentTabSupplier.get());
    }

    void readAloudControllerReady(@Nullable ReadAloudController readAloudController) {
        if (readAloudController != null) {
            readAloudController.addReadabilityUpdateListener(mReadabilityUpdateListener);
        }
    }

    public void destroy() {
        if (mReadAloudControllerSupplier.get() != null) {
            mReadAloudControllerSupplier
                    .get()
                    .removeReadabilityUpdateListener(mReadabilityUpdateListener);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    void setShowAppMenuTextBubble(boolean showAppMenuTextBubble) {
        mShowAppMenuTextBubble = showAppMenuTextBubble;
    }
}
