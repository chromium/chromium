// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.CallbackUtils;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksFusebox;
import org.chromium.chrome.browser.contextual_tasks.fusebox.ContextualTasksFusebox.ContextualTasksFuseboxConfig;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Factory for creating co-browse content. */
@NullMarked
public class CoBrowseViewFactory {

    private final Activity mActivity;
    private final ContextualTasksFuseboxConfig mFuseboxConfig;
    private final WindowAndroid mWindowAndroid;
    private final NonNullObservableSupplier<Profile> mProfileSupplier;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final SnackbarManager mSnackbarManager;
    private final ContextMenuPopulatorFactory mContextMenuPopulatorFactory;

    /**
     * Factory responsible for creating co-browse content.
     *
     * @param activity The current {@link Activity} instance.
     * @param fuseboxConfig The configuration for the fusebox.
     * @param profileSupplier A supplier for the current {@link Profile}.
     * @param windowAndroid The {@link WindowAndroid} for managing window-level operations.
     * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher} for managing activity
     *     lifecycle.
     * @param snackbarManager The {@link SnackbarManager} for managing snackbar messages.
     * @param contextMenuPopulatorFactory The {@link ContextMenuPopulatorFactory} to show context
     *     menu on the ThinWebView.
     */
    public CoBrowseViewFactory(
            Activity activity,
            ContextualTasksFuseboxConfig fuseboxConfig,
            NonNullObservableSupplier<Profile> profileSupplier,
            WindowAndroid windowAndroid,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            SnackbarManager snackbarManager,
            ContextMenuPopulatorFactory contextMenuPopulatorFactory) {
        mActivity = activity;
        mFuseboxConfig = fuseboxConfig;
        mProfileSupplier = profileSupplier;
        mWindowAndroid = windowAndroid;
        mLifecycleDispatcher = lifecycleDispatcher;
        mSnackbarManager = snackbarManager;
        mContextMenuPopulatorFactory = contextMenuPopulatorFactory;

        TabBottomSheetUtils.attachFactoryToWindow(windowAndroid, this);
    }

    public void destroy() {
        TabBottomSheetUtils.detachFactoryFromWindow(mWindowAndroid);
    }

    /**
     * Builds the co-browse views.
     *
     * @param webContents The {@link WebContents} to be displayed in the thin web view.
     * @param showToolbar Whether to show the toolbar.
     * @param showFusebox Whether to show the fusebox.
     * @return The {@link CoBrowseViews} instance.
     */
    CoBrowseViews buildCoBrowseViews(
            @Nullable WebContents webContents, boolean showToolbar, boolean showFusebox) {
        TabBottomSheetToolbar toolbar =
                showToolbar ? new TabBottomSheetSimpleToolbar(mActivity) : null;
        TabBottomSheetWebUi webUi =
                new TabBottomSheetWebUi(mActivity, mWindowAndroid, mContextMenuPopulatorFactory);
        ContextualTasksFusebox fusebox =
                showFusebox
                        ? new ContextualTasksFusebox(
                                mActivity,
                                mFuseboxConfig.contentView,
                                mFuseboxConfig,
                                mProfileSupplier,
                                mWindowAndroid,
                                mLifecycleDispatcher,
                                /* loadUrlCallback= */ CallbackUtils.emptyCallback(),
                                mSnackbarManager)
                        : null;

        webUi.setWebContents(webContents);

        return new CoBrowseViews(mActivity, toolbar, webUi, fusebox);
    }

    @CalledByNative
    @VisibleForTesting
    public static @Nullable CoBrowseViews buildCoBrowseViews(
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid,
            @Nullable @JniType("content::WebContents*") WebContents webContents,
            boolean showToolbar,
            boolean showFusebox) {
        CoBrowseViewFactory factory = TabBottomSheetUtils.getFactoryFromWindow(windowAndroid);
        if (factory == null) {
            return null;
        }
        return factory.buildCoBrowseViews(webContents, showToolbar, showFusebox);
    }
}
