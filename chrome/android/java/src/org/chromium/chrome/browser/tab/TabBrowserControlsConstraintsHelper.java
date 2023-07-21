// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.UserData;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Manages the state of tab browser controls.
 */
public class TabBrowserControlsConstraintsHelper implements UserData {
    private static final Class<TabBrowserControlsConstraintsHelper> USER_DATA_KEY =
            TabBrowserControlsConstraintsHelper.class;

    private final TabImpl mTab;
    private final Callback<Integer> mConstraintsChangedCallback;

    private long mNativeTabBrowserControlsConstraintsHelper; // Lazily initialized in |update|
    private BrowserControlsVisibilityDelegate mVisibilityDelegate;

    public static void createForTab(Tab tab) {
        tab.getUserDataHost().setUserData(
                USER_DATA_KEY, new TabBrowserControlsConstraintsHelper(tab));
    }

    public static TabBrowserControlsConstraintsHelper get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    /**
     * Returns the current visibility constraints for the display of browser controls.
     * {@link BrowserControlsState} defines the valid return options.
     * @param tab Tab whose browser controls state is looked into.
     * @return The current visibility constraints.
     */
    public static @BrowserControlsState int getConstraints(Tab tab) {
        if (tab == null || get(tab) == null) return BrowserControlsState.BOTH;
        return get(tab).getConstraints();
    }

    /**
     * Returns the constraints delegate for a particular tab. The returned supplier will always be
     * associated with that tab, even if it stops being the active tab.
     * @param tab Tab whose browser controls state is looked into.
     * @return Observable supplier for the current visibility constraints.
     */
    public static ObservableSupplier<Integer> getObservableConstraints(Tab tab) {
        if (tab == null) {
            return null;
        }
        TabBrowserControlsConstraintsHelper helper = get(tab);
        if (helper == null) {
            return null;
        }
        return helper.mVisibilityDelegate;
    }

    /**
     * Push state about whether or not the browser controls can show or hide to the renderer.
     * @param tab Tab object.
     */
    public static void updateEnabledState(Tab tab) {
        if (tab == null || get(tab) == null) return;
        get(tab).updateEnabledState();
    }

    /**
     * Updates the browser controls state for this tab.  As these values are set at the renderer
     * level, there is potential for this impacting other tabs that might share the same
     * process.
     *
     * @param tab Tab whose browser constrol state is looked into.
     * @param current The desired current state for the controls.  Pass
     *         {@link BrowserControlsState#BOTH} to preserve the current position.
     * @param animate Whether the controls should animate to the specified ending condition or
     *         should jump immediately.
     */
    public static void update(Tab tab, int current, boolean animate) {
        if (tab == null || get(tab) == null) return;
        get(tab).update(current, animate);
    }

    /** Constructor */
    private TabBrowserControlsConstraintsHelper(Tab tab) {
        mTab = (TabImpl) tab;
        mConstraintsChangedCallback = (constraints) -> updateEnabledState();
        mTab.addObserver(new EmptyTabObserver() {
            @Override
            public void onInitialized(Tab tab, String appId) {
                updateVisibilityDelegate();
            }

            @Override
            public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
                if (window != null) updateVisibilityDelegate();
            }

            @Override
            public void onDestroyed(Tab tab) {
                tab.removeObserver(this);
            }

            private void updateAfterRendererProcessSwitch(Tab tab, boolean hasCommitted) {
                int constraints = getConstraints();
                if (constraints == BrowserControlsState.SHOWN && hasCommitted
                        && TabBrowserControlsOffsetHelper.get(tab).topControlsOffset() == 0) {
                    // If the browser controls were already fully visible on the previous page, then
                    // avoid an animation to keep the controls from jumping around.
                    update(BrowserControlsState.SHOWN, false);
                } else {
                    updateEnabledState();
                }
            }

            @Override
            public void onDidFinishNavigationInPrimaryMainFrame(
                    Tab tab, NavigationHandle navigationHandle) {
                // At this point, we might have switched renderer processes, so push the existing
                // constraints to the new renderer (has the potential to be slightly spammy, but
                // the renderer has logic to suppress duplicate calls).
                updateAfterRendererProcessSwitch(tab, navigationHandle.hasCommitted());
            }

            @Override
            public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                updateAfterRendererProcessSwitch(tab, true);
            }
        });
        if (mTab.isInitialized() && !TabUtils.isDetached(mTab)) updateVisibilityDelegate();
    }

    @Override
    public void destroy() {
        if (mNativeTabBrowserControlsConstraintsHelper != 0) {
            TabBrowserControlsConstraintsHelperJni.get().onDestroyed(
                    mNativeTabBrowserControlsConstraintsHelper,
                    TabBrowserControlsConstraintsHelper.this);
        }
    }

    private void updateVisibilityDelegate() {
        if (mVisibilityDelegate != null) {
            mVisibilityDelegate.removeObserver(mConstraintsChangedCallback);
        }
        mVisibilityDelegate =
                mTab.getDelegateFactory().createBrowserControlsVisibilityDelegate(mTab);
        if (mVisibilityDelegate != null) {
            mVisibilityDelegate.addObserver(mConstraintsChangedCallback);
        }
    }

    private void updateEnabledState() {
        if (mTab.isFrozen()) return;
        update(BrowserControlsState.BOTH, getConstraints() != BrowserControlsState.HIDDEN);
    }

    /**
     * Updates the browser controls state for this tab.  As these values are set at the renderer
     * level, there is potential for this impacting other tabs that might share the same
     * process.
     *
     * @param current The desired current state for the controls.  Pass
     *                {@link BrowserControlsState#BOTH} to preserve the current position.
     * @param animate Whether the controls should animate to the specified ending condition or
     *                should jump immediately.
     */
    public void update(int current, boolean animate) {
        int constraints = getConstraints();

        // Do nothing if current and constraints conflict to avoid error in renderer.
        if ((constraints == BrowserControlsState.HIDDEN && current == BrowserControlsState.SHOWN)
                || (constraints == BrowserControlsState.SHOWN
                        && current == BrowserControlsState.HIDDEN)) {
            return;
        }
        if (mNativeTabBrowserControlsConstraintsHelper == 0) {
            mNativeTabBrowserControlsConstraintsHelper =
                    TabBrowserControlsConstraintsHelperJni.get().init(
                            TabBrowserControlsConstraintsHelper.this);
        }
        TabBrowserControlsConstraintsHelperJni.get().updateState(
                mNativeTabBrowserControlsConstraintsHelper,
                TabBrowserControlsConstraintsHelper.this, mTab.getWebContents(), constraints,
                current, animate);
    }

    private @BrowserControlsState int getConstraints() {
        return mVisibilityDelegate == null ? BrowserControlsState.BOTH : mVisibilityDelegate.get();
    }

    public static void setForTesting(Tab tab, TabBrowserControlsConstraintsHelper helper) {
        tab.getUserDataHost().setUserData(USER_DATA_KEY, helper);
    }

    @NativeMethods
    interface Natives {
        long init(TabBrowserControlsConstraintsHelper caller);
        void onDestroyed(long nativeTabBrowserControlsConstraintsHelper,
                TabBrowserControlsConstraintsHelper caller);
        void updateState(long nativeTabBrowserControlsConstraintsHelper,
                TabBrowserControlsConstraintsHelper caller, WebContents webContents, int contraints,
                int current, boolean animate);
    }
}
