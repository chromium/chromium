// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackMode;
import org.chromium.components.feature_engagement.FeatureConstants;

/**
 * Controller to manage when and how we show ReadAloud in-product-help messages to users in the app
 * menu and the CCT app menu.
 */
@NullMarked
public class ReadAloudIphController {
    private final UserEducationHelper mUserEducationHelper;
    private final AppMenuHandler mAppMenuHandler;
    private final View mToolbarMenuButton;
    private final ObservableSupplier<ReadAloudController> mReadAloudControllerSupplier;
    private final ObservableSupplier<Tab> mCurrentTabSupplier;
    private boolean mShowAppMenuTextBubble;
    private final Runnable mReadabilityUpdateListener = this::maybeShowReadAloudAppMenuIph;

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
    public ReadAloudIphController(
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

    ReadAloudIphController(
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
    public void maybeShowReadAloudAppMenuIph() {
      AppMenuIphAvailability appMenuIphAvailability = shouldShowIph();
      if (appMenuIphAvailability == AppMenuIphAvailability.NO_IPH) {
        return;
      }
      boolean isHighlightEnabled =
              mShowAppMenuTextBubble
                      ? true
                      : ReadAloudFeatures.isIPHMenuButtonHighlightCctEnabled();
      mUserEducationHelper.requestShowIph(
              new IphCommandBuilder(
                              mToolbarMenuButton.getContext().getResources(),
                              FeatureConstants.READ_ALOUD_APP_MENU_FEATURE,
                              getIphStringResId(appMenuIphAvailability),
                              getIphStringResId(appMenuIphAvailability))
                      .setAnchorView(mToolbarMenuButton)
                      .setShowTextBubble(mShowAppMenuTextBubble)
                      .setOnShowCallback(
                              () ->
                                      turnOnHighlightForMenuItem(
                                              R.id.readaloud_menu_id, isHighlightEnabled))
                      .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                      .build());
    }

    private void turnOnHighlightForMenuItem(int highlightMenuItemId, boolean highlightMenuButton) {
        mAppMenuHandler.setMenuHighlight(highlightMenuItemId, highlightMenuButton);
    }

    private void turnOffHighlightForMenuItem() {
        mAppMenuHandler.clearMenuHighlight();
    }

    private AppMenuIphAvailability shouldShowIph() {
        if (mCurrentTabSupplier.get() == null
                || !mCurrentTabSupplier.get().getUrl().isValid()
                || mReadAloudControllerSupplier.get() == null) {
            return AppMenuIphAvailability.NO_IPH;
        }
        PlaybackMode modeToPlay = mReadAloudControllerSupplier.get().getModeToPlay(mCurrentTabSupplier.get());
        switch (modeToPlay) {
          case CLASSIC:
            return AppMenuIphAvailability.STANDARD_IPH;
          case OVERVIEW:
            return AppMenuIphAvailability.AI_IPH;
          default:
            return AppMenuIphAvailability.NO_IPH;
        }
    }

    private int getIphStringResId(AppMenuIphAvailability appMenuIphAvailability) {
        switch (appMenuIphAvailability) {
          case STANDARD_IPH:
            return R.string.menu_listen_to_this_page_iph;
          case AI_IPH:
            return R.string.menu_listen_to_this_page_with_ai_iph;
          default:
            return 0;
        }
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

    private enum AppMenuIphAvailability {
      NO_IPH,
      STANDARD_IPH,
      AI_IPH;
    }
}
