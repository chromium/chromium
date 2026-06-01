// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.devtools;

import android.content.Intent;
import android.graphics.Color;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.flags.ActivityType;

/** BrowserServicesIntentDataProvider implementation for DevTools. */
@NullMarked
public class DevToolsIntentDataProvider extends BrowserServicesIntentDataProvider {
    private final Intent mIntent;
    private final ColorProvider mColorProvider;

    DevToolsIntentDataProvider(Intent intent) {
        mIntent = intent;
        mColorProvider =
                new ColorProvider() {
                    @Override
                    public int getToolbarColor() {
                        return getInitialBackgroundColor();
                    }

                    @Override
                    public boolean hasCustomToolbarColor() {
                        return false;
                    }

                    @Override
                    public @Nullable Integer getNavigationBarColor() {
                        return null;
                    }

                    @Override
                    public @Nullable Integer getNavigationBarDividerColor() {
                        return null;
                    }

                    @Override
                    public int getBottomBarColor() {
                        return getInitialBackgroundColor();
                    }

                    @Override
                    public int getInitialBackgroundColor() {
                        return Color.TRANSPARENT;
                    }
                };
    }

    @Override
    public @ActivityType int getActivityType() {
        return ActivityType.DEV_TOOLS;
    }

    @Override
    public Intent getIntent() {
        return mIntent;
    }

    @Override
    public @Nullable String getUrlToLoad() {
        return null;
    }

    @Override
    public ColorProvider getColorProvider() {
        return mColorProvider;
    }

    @Override
    public @TitleVisibility int getTitleVisibilityState() {
        return TitleVisibility.VISIBLE;
    }

    @Override
    public boolean shouldShowStarButton() {
        return false;
    }

    @Override
    public boolean shouldShowDownloadButton() {
        return false;
    }

    @Override
    public boolean shouldSuppressAppMenu() {
        return true;
    }

    @Override
    public boolean isCloseButtonEnabled() {
        return false;
    }
}
