// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.desktop_popup_header;

import android.content.Context;

import androidx.core.graphics.Insets;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
class DesktopPopupHeaderMediator implements DesktopWindowStateManager.AppHeaderObserver {
    private final PropertyModel mModel;
    private final DesktopWindowStateManager mDesktopWindowStateManager;
    private final TabSupplierObserver mTabSupplierObserver;
    private final Context mContext;
    private final boolean mIsIncognito;

    private String mDisplayedTabTitle = "";
    private @Nullable AppHeaderState mCurrentHeaderState;

    public DesktopPopupHeaderMediator(
            PropertyModel model,
            DesktopWindowStateManager desktopWindowStateManager,
            ObservableSupplier<@Nullable Tab> tabSupplier,
            Context context,
            boolean isIncognito) {
        mModel = model;
        mDesktopWindowStateManager = desktopWindowStateManager;
        mContext = context;
        mIsIncognito = isIncognito;

        mDesktopWindowStateManager.addObserver(this);
        onAppHeaderStateChanged(mDesktopWindowStateManager.getAppHeaderState());

        mTabSupplierObserver =
                new TabSupplierObserver(tabSupplier, true) {
                    @Override
                    public void onTitleUpdated(Tab tab) {
                        onTitleHasBeenUpdated(tab);
                    }

                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        onTitleHasBeenUpdated(tab);
                    }
                };

        mModel.set(
                DesktopPopupHeaderProperties.TITLE_APPEARANCE,
                ChromeColors.getLargeTextPrimaryStyle(mIsIncognito));
    }

    /**
     * {@link DesktopWindowStateManager.AppHeaderObserver} implementation. Dispatches all changes to
     * the layout due to a change in app header state.
     */
    @Override
    public void onAppHeaderStateChanged(@Nullable AppHeaderState newState) {
        if (newState == null) return;
        if (newState.equals(mCurrentHeaderState)) return;
        mCurrentHeaderState = newState;

        final boolean showExplicitTabTitleBar = newState.isInDesktopWindow();
        mModel.set(DesktopPopupHeaderProperties.IS_SHOWN, showExplicitTabTitleBar);

        final boolean showTextInCaptionBar =
                newState.getUnoccludedRectWidth()
                        >= mContext.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.custom_tabs_popup_title_bar_min_width);
        mModel.set(DesktopPopupHeaderProperties.TITLE_VISIBLE, showTextInCaptionBar);

        final int bgColor = ChromeColors.getDefaultBgColor(mContext, mIsIncognito);
        mDesktopWindowStateManager.updateForegroundColor(bgColor);
        mModel.set(DesktopPopupHeaderProperties.BACKGROUND_COLOR, bgColor);

        final int minimumHeaderHeightPx =
                Math.max(
                        mContext.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.custom_tabs_popup_title_bar_min_height),
                        mContext.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.custom_tabs_popup_title_bar_text_height));
        final int finalHeaderHeightPx =
                Math.max(minimumHeaderHeightPx, newState.getAppHeaderHeight());
        mModel.set(DesktopPopupHeaderProperties.HEADER_HEIGHT_PX, finalHeaderHeightPx);

        mModel.set(
                DesktopPopupHeaderProperties.TITLE_SPACING,
                Insets.of(newState.getLeftPadding(), 0, newState.getRightPadding(), 0));
    }

    private void onTitleHasBeenUpdated(@Nullable Tab tab) {
        if (tab == null) return;
        final String newTabTitle = tab.getTitle();
        if (mDisplayedTabTitle.equals(newTabTitle)) return;
        mDisplayedTabTitle = newTabTitle;

        mModel.set(DesktopPopupHeaderProperties.TITLE_TEXT, mDisplayedTabTitle);
    }

    public void destroy() {
        mDesktopWindowStateManager.removeObserver(this);
        mTabSupplierObserver.destroy();
    }
}
