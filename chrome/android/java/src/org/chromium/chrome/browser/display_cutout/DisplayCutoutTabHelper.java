// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeManager;
import org.chromium.ui.edge_to_edge.EdgeToEdgeTokenHolder;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.url.GURL;

/**
 * Wraps a {@link DisplayCutoutController} for a Chrome {@link Tab}.
 *
 * <p>This will only be created once the tab sets a non-default viewport fit.
 */
@NullMarked
public class DisplayCutoutTabHelper implements UserData {
    private static final Class<DisplayCutoutTabHelper> USER_DATA_KEY = DisplayCutoutTabHelper.class;

    /** The tab that this object belongs to. */
    private final Tab mTab;

    @VisibleForTesting DisplayCutoutController mCutoutController;

    /** Listens to various Tab events. */
    private final TabObserver mTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onShown(Tab tab, @TabSelectionType int type) {
                    assert tab == mTab;

                    // Force a layout update if we are now being shown.
                    mCutoutController.maybeUpdateLayout();
                }

                @Override
                public void onInteractabilityChanged(Tab tab, boolean interactable) {
                    // Force a layout update if the tab is now in the foreground.
                    mCutoutController.maybeUpdateLayout();
                }

                @Override
                public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
                    assert tab == mTab;

                    mCutoutController.onActivityAttachmentChanged(window);
                }

                @Override
                public void onContentChanged(Tab tab) {
                    mCutoutController.onContentChanged();
                }

                @Override
                public void onPageLoadFinished(Tab tab, GURL url) {
                    // Re-push safe-area into Blink once the new document is committed. On
                    // reload, Blink may finish parsing and run page scripts before our
                    // viewport-fit-driven push lands. Pushing again here makes CSS
                    // env(safe-area-inset-*) consistent with the current cover state on Android
                    // versions where the inset observer does not retrigger after refresh.
                    mCutoutController.onPageLoadFinished();
                }
            };

    public static DisplayCutoutTabHelper from(Tab tab) {
        UserDataHost host = tab.getUserDataHost();
        DisplayCutoutTabHelper tabHelper = host.getUserData(USER_DATA_KEY);
        return tabHelper == null
                ? host.setUserData(USER_DATA_KEY, new DisplayCutoutTabHelper(tab))
                : tabHelper;
    }

    @VisibleForTesting
    static class ChromeDisplayCutoutDelegate implements DisplayCutoutController.Delegate {
        private final Tab mTab;
        private @Nullable EdgeToEdgeTokenHolder mEdgeToEdgeTokenHolder;

        ChromeDisplayCutoutDelegate(Tab tab) {
            mTab = tab;
        }

        @Override
        public @Nullable Activity getAttachedActivity() {
            WindowAndroid window = mTab.getWindowAndroid();
            return window == null ? null : window.getActivity().get();
        }

        @Override
        public @Nullable WebContents getWebContents() {
            return mTab.getWebContents();
        }

        @Override
        public @Nullable InsetObserver getInsetObserver() {
            WindowAndroid window = mTab.getWindowAndroid();
            return window == null ? null : window.getInsetObserver();
        }

        @Override
        public @Nullable MonotonicObservableSupplier<Integer>
                getBrowserDisplayCutoutModeSupplier() {
            WindowAndroid window = mTab.getWindowAndroid();
            return window == null ? null : ActivityDisplayCutoutModeSupplier.from(window);
        }

        @Override
        public boolean isInteractable() {
            return mTab.isUserInteractable();
        }

        @Override
        public @DisplayMode.EnumType int getDisplayMode() {
            Activity activity = getAttachedActivity();
            if (!(activity instanceof BaseCustomTabActivity)) {
                return DisplayMode.BROWSER;
            }
            return ((BaseCustomTabActivity) activity)
                    .getIntentDataProvider()
                    .getResolvedDisplayMode();
        }

        @Override
        public boolean isDrawEdgeToEdgeEnabled() {
            return true;
        }

        @Override
        public boolean isShortEdgesCutoutModeEnabled() {
            return ChromeFeatureList.sWebAppShortEdgesCutoutMode.isEnabled();
        }

        /**
         * Returns the activity's {@link EdgeToEdgeManager}, or null if this delegate is not
         * attached to a {@link BaseCustomTabActivity} (or the manager has not been created yet).
         */
        private @Nullable EdgeToEdgeManager getEdgeToEdgeManagerOrNull() {
            Activity activity = getAttachedActivity();
            if (!(activity instanceof BaseCustomTabActivity)) return null;
            return ((BaseCustomTabActivity) activity).getEdgeToEdgeManager();
        }

        @Override
        public void setEdgeToEdgeState(boolean drawEdgeToEdge) {
            EdgeToEdgeManager edgeToEdgeManager = getEdgeToEdgeManagerOrNull();
            if (edgeToEdgeManager == null) return;
            if (mEdgeToEdgeTokenHolder == null) {
                mEdgeToEdgeTokenHolder =
                        new EdgeToEdgeTokenHolder(edgeToEdgeManager.getEdgeToEdgeStateProvider());
            }
            if (drawEdgeToEdge) {
                mEdgeToEdgeTokenHolder.acquireTokenIfEmpty();
            } else {
                mEdgeToEdgeTokenHolder.release();
            }
            edgeToEdgeManager.setContentFitsWindowInsets(!drawEdgeToEdge);
        }
    }

    /**
     * Constructs a new DisplayCutoutTabHelper for a specific tab.
     *
     * @param tab The tab that this object belongs to.
     */
    @VisibleForTesting
    DisplayCutoutTabHelper(Tab tab) {
        mTab = tab;
        tab.addObserver(mTabObserver);
        mCutoutController =
                DisplayCutoutController.createForTab(mTab, new ChromeDisplayCutoutDelegate(mTab));
    }

    /**
     * Set the viewport fit value for the tab.
     *
     * @param value The new viewport fit value.
     */
    public void setViewportFit(@WebContentsObserver.ViewportFitType int value) {
        mCutoutController.setViewportFit(value);
    }

    /**
     * Set whether there are safe area constraint on the current web page.
     *
     * @param hasConstraint Whether there are safe area constraint for the page.
     */
    public void setSafeAreaConstraint(boolean hasConstraint) {
        mCutoutController.setSafeAreaConstraint(hasConstraint);
    }

    @Override
    public void destroy() {
        mTab.removeObserver(mTabObserver);
        mCutoutController.destroy();
    }

    static void initForTesting(Tab tab, DisplayCutoutController controller) {
        DisplayCutoutTabHelper tabHelper = new DisplayCutoutTabHelper(tab);
        tabHelper.mCutoutController = controller;
        tab.getUserDataHost().setUserData(USER_DATA_KEY, tabHelper);
    }

    @VisibleForTesting
    DisplayCutoutController getDisplayCutoutController() {
        return mCutoutController;
    }
}
