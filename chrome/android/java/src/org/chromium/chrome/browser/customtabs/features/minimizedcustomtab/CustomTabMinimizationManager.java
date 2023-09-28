// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.ALL_KEYS;
import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.TITLE;
import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.URL;
import static org.chromium.chrome.browser.tab.TabSelectionType.FROM_USER;
import static org.chromium.chrome.browser.tab.TabUtils.LoadIfNeededCaller.ON_ACTIVITY_SHOWN_THEN_SHOW;

import android.app.PictureInPictureParams;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.util.Rational;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.PictureInPictureModeChangedInfo;
import androidx.core.util.Consumer;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Class that manages minimizing a Custom Tab into picture-in-picture.
 */
@RequiresApi(VERSION_CODES.O)
public class CustomTabMinimizationManager implements Consumer<PictureInPictureModeChangedInfo> {
    @VisibleForTesting
    static final Rational ASPECT_RATIO = new Rational(16, 9);
    private final AppCompatActivity mActivity;
    private final ActivityTabProvider mTabProvider;
    private MinimizedCardCoordinator mMinimizedCardCoordinator;

    /**
     * @param activity The {@link AppCompatActivity} to minimize.
     * @param tabProvider The {@link ActivityTabProvider} that provides the Tab that will be
     *                    minimized.
     */
    public CustomTabMinimizationManager(
            AppCompatActivity activity, ActivityTabProvider tabProvider) {
        mActivity = activity;
        mActivity.addOnPictureInPictureModeChangedListener(this);
        mTabProvider = tabProvider;
    }

    /**
     * Minimize the Custom Tab into picture-in-picture.
     */
    public void minimize() {
        if (!mTabProvider.hasValue()) return;
        var builder = new PictureInPictureParams.Builder().setAspectRatio(ASPECT_RATIO);
        if (VERSION.SDK_INT >= VERSION_CODES.S) {
            builder.setSeamlessResizeEnabled(false);
        }
        mActivity.enterPictureInPictureMode(builder.build());
    }

    @Override
    public void accept(PictureInPictureModeChangedInfo pictureInPictureModeChangedInfo) {
        Tab tab = mTabProvider.get();
        assert tab != null;
        if (pictureInPictureModeChangedInfo.isInPictureInPictureMode()) {
            updateTabForMinimization(tab);
        } else {
            updateTabForMaximization(tab);
        }
    }

    private void updateTabForMinimization(Tab tab) {
        if (tab == null) return;
        PropertyModel model = new PropertyModel.Builder(ALL_KEYS)
                                      .with(TITLE, tab.getTitle())
                                      .with(URL, tab.getUrl().getHost())
                                      .build();
        mMinimizedCardCoordinator = new MinimizedCardCoordinator(mActivity, model);
        tab.stopLoading();
        tab.hide(TabHidingType.ACTIVITY_HIDDEN);
        var webContents = tab.getWebContents();
        if (webContents != null) {
            webContents.suspendAllMediaPlayers();
            webContents.setAudioMuted(true);
        }
    }

    private void updateTabForMaximization(Tab tab) {
        tab.show(FROM_USER, ON_ACTIVITY_SHOWN_THEN_SHOW);
        var webContents = tab.getWebContents();
        if (webContents != null) {
            webContents.setAudioMuted(false);
        }
        mMinimizedCardCoordinator.destroy();
        mMinimizedCardCoordinator = null;
    }
}
