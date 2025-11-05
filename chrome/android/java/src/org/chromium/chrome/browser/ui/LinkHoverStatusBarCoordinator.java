// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewStub;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

/** Coordinator for the link hover status bar. */
@NullMarked
public class LinkHoverStatusBarCoordinator extends EmptyTabObserver {
    private final CurrentTabObserver mCurrentTabObserver;
    private final TextView mLinkHoverStatusBar;
    private final Context mContext;
    private final Drawable mBackgroundDrawable;

    /**
     * @param context The current context.
     * @param tabProvider The provider of the current tab.
     * @param statusBarStub The {@link ViewStub} for the status bar.
     */
    public LinkHoverStatusBarCoordinator(
            Context context,
            ObservableSupplier<@Nullable Tab> tabProvider,
            ViewStub statusBarStub) {
        mContext = context;
        mCurrentTabObserver = new CurrentTabObserver(tabProvider, this);

        mLinkHoverStatusBar = (TextView) statusBarStub.inflate();
        mBackgroundDrawable =
                AppCompatResources.getDrawable(mContext, R.drawable.link_status_bar_background)
                        .mutate();
    }

    @Override
    public void onUpdateTargetUrl(Tab tab, GURL url) {
        if (url != null && !url.isEmpty()) {
            mLinkHoverStatusBar.setText(url.getSpec());
            boolean isIncognito = tab.isIncognitoBranded();
            boolean isNightMode = ColorUtils.inNightMode(mContext);
            if (isIncognito || isNightMode) {
                mBackgroundDrawable.setTint(SemanticColorUtils.getDefaultBgColor(mContext));
                mLinkHoverStatusBar.setTextAppearance(R.style.TextAppearance_TextMedium_Primary);
            } else {
                mBackgroundDrawable.setTint(
                        SemanticColorUtils.getDefaultControlColorActive(mContext));
                mLinkHoverStatusBar.setTextAppearance(
                        R.style.TextAppearance_TextMedium_Primary_OnAccent1);
            }
            mLinkHoverStatusBar.setBackground(mBackgroundDrawable);

            // TODO(crbug.com/454446656): Move the status bar to avoid the cursor.
            // TODO(crbug.com/456043302): Expand the status bar after a delay.
            // TODO(crbug.com/453901686): Implement fade-in/fade-out animation.
            mLinkHoverStatusBar.setVisibility(View.VISIBLE);
        } else {
            mLinkHoverStatusBar.setVisibility(View.GONE);
        }
    }

    /** Destroy the coordinator. */
    public void destroy() {
        mCurrentTabObserver.destroy();
    }
}
