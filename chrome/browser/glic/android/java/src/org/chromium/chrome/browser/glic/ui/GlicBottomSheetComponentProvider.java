// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import android.content.Context;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ui.ActorControlCoordinator;
import org.chromium.chrome.browser.actor.ui.ActorControlStateTracker;
import org.chromium.chrome.browser.glic.R;
import org.chromium.chrome.browser.glic.settings.GlicNavigationUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_bottom_sheet.CoBrowseComponentProvider;
import org.chromium.chrome.browser.tab_bottom_sheet.CoBrowseComponentProvider.TabSelectionDelegate;
import org.chromium.chrome.browser.tab_bottom_sheet.LegacyResizingPlaceholderCoordinator;
import org.chromium.chrome.browser.tab_bottom_sheet.PeekViewManager;
import org.chromium.chrome.browser.tab_bottom_sheet.ResizingPlaceholderCoordinator;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetContent;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetSkeletonCoordinator;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetUtils;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;

/**
 * Concrete implementation of {@link CoBrowseComponentProvider} for Glic. Returns specialized
 * components and handles agent task termination.
 */
@JNINamespace("glic")
@NullMarked
public class GlicBottomSheetComponentProvider implements CoBrowseComponentProvider {
    @JniType("::glic::GlicCoBrowseViews")
    interface NativeGlicCoBrowseViews extends JniTypeToken {}

    private final Profile mProfile;
    private @Nullable JniRawPtr<NativeGlicCoBrowseViews> mNativeCoBrowseViews;
    @VisibleForTesting boolean mShowNativeFre;
    private @Nullable ActorControlStateTracker mActorControlStateTracker;
    private @Nullable GlicBottomSheetContent mActiveContent;

    /** JNI static factory method to create the provider. */
    @CalledByNative
    private static GlicBottomSheetComponentProvider createProvider(
            Profile profile, JniRawPtr<NativeGlicCoBrowseViews> nativeCoBrowseViews) {
        return new GlicBottomSheetComponentProvider(profile, nativeCoBrowseViews);
    }

    GlicBottomSheetComponentProvider(
            Profile profile, @Nullable JniRawPtr<NativeGlicCoBrowseViews> nativeCoBrowseViews) {
        mProfile = profile;
        mNativeCoBrowseViews = nativeCoBrowseViews;
    }

    @CalledByNative
    void configureNativeFre(boolean showNativeFre) {
        mShowNativeFre = showNativeFre;
        if (showNativeFre) {
            mountNativeFreOnActiveContent();
        } else if (mActiveContent != null) {
            mActiveContent.teardownNativeFre();
        }
    }

    @Override
    public void destroy() {
        if (mNativeCoBrowseViews != null) {
            mNativeCoBrowseViews.release();
            mNativeCoBrowseViews = null;
        }
        configureNativeFre(false);
        mActiveContent = null;
        if (mActorControlStateTracker != null) {
            mActorControlStateTracker.destroy();
            mActorControlStateTracker = null;
        }
    }

    @Override
    public boolean setupPlaceholderView(TextViewWithCompoundDrawables placeholder) {
        GlicUiUtils.setupPlaceholderView(placeholder);
        return true;
    }

    @Override
    public @Nullable View createLoadingPlaceholderView(
            Context context,
            @ColorInt int backgroundColor,
            @ColorInt int placeholderElemColor,
            Runnable onCloseClicked) {
        return GlicLoadingPlaceholderView.create(
                context, backgroundColor, placeholderElemColor, onCloseClicked);
    }

    @Override
    public ResizingPlaceholderCoordinator createResizingPlaceholderCoordinator(
            Context context, @ColorInt int backgroundColor, @ColorInt int placeholderElemColor) {
        if (TabBottomSheetUtils.PLACEHOLDER_SKELETON.equals(
                TabBottomSheetUtils.getResizingPlaceholderType())) {
            return new TabBottomSheetSkeletonCoordinator(
                    context, backgroundColor, placeholderElemColor, R.drawable.ic_spark_4c_16dp);
        }
        return new LegacyResizingPlaceholderCoordinator(context, backgroundColor);
    }

    @Override
    public TabBottomSheetContent createContent(
            View contentView,
            float defaultHeightRatio,
            float fullHeightRatio,
            @ColorInt int backgroundColor,
            @Px int peekViewHeight,
            @IdRes int peekViewContainerId,
            Runnable onBackPressed) {
        GlicBottomSheetContent content =
                new GlicBottomSheetContent(
                        contentView,
                        defaultHeightRatio,
                        fullHeightRatio,
                        backgroundColor,
                        peekViewHeight,
                        peekViewContainerId,
                        onBackPressed,
                        mProfile);
        if (mActiveContent != null) {
            mActiveContent.teardownNativeFre();
        }
        mActiveContent = content;
        content.setDestroyCallback(
                () -> {
                    if (mActiveContent == content) {
                        mActiveContent = null;
                    }
                });
        if (mShowNativeFre) {
            mountNativeFreOnActiveContent();
        }
        return content;
    }

    private void mountNativeFreOnActiveContent() {
        if (mActiveContent == null) return;
        SettingsCustomTabLauncher customTabLauncher = GlicNavigationUtils.getCustomTabLauncher();
        assert customTabLauncher != null;
        if (customTabLauncher == null) return;
        mActiveContent.mountNativeFre(customTabLauncher, this::notifyNativeFreAccepted);
    }

    @Override
    public @Nullable PeekViewManager createPeekViewManager(
            TabBottomSheetManager tabBottomSheetManager,
            MonotonicObservableSupplier<Profile> profileSupplier,
            NullableObservableSupplier<Tab> tabSupplier,
            TabSelectionDelegate tabSelectionDelegate) {
        ActorControlStateTracker tracker = getOrInitTracker(profileSupplier, tabSupplier);
        return new ActorControlCoordinator(tabBottomSheetManager, tracker, tabSelectionDelegate);
    }

    private ActorControlStateTracker getOrInitTracker(
            MonotonicObservableSupplier<Profile> profileSupplier,
            NullableObservableSupplier<Tab> tabSupplier) {
        if (mActorControlStateTracker == null) {
            mActorControlStateTracker = new ActorControlStateTracker(profileSupplier, tabSupplier);
        }
        return mActorControlStateTracker;
    }

    @VisibleForTesting
    void notifyNativeFreAccepted() {
        mShowNativeFre = false;
        if (mNativeCoBrowseViews != null) {
            GlicBottomSheetComponentProviderJni.get().onNativeFreAccepted(mNativeCoBrowseViews);
        }
    }

    @NativeMethods
    interface Natives {
        void onNativeFreAccepted(JniPtr<NativeGlicCoBrowseViews> self);
    }
}
