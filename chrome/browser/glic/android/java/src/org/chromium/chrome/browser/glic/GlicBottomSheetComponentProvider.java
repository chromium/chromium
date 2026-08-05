// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.annotation.Px;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ui.ActorControlCoordinator;
import org.chromium.chrome.browser.actor.ui.ActorControlStateTracker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_bottom_sheet.CoBrowseComponentProvider;
import org.chromium.chrome.browser.tab_bottom_sheet.CoBrowseComponentProvider.TabSelectionDelegate;
import org.chromium.chrome.browser.tab_bottom_sheet.PeekViewManager;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetContent;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;

/**
 * Concrete implementation of {@link CoBrowseComponentProvider} for Glic. Returns specialized
 * components and handles agent task termination.
 */
@JNINamespace("glic")
@NullMarked
public class GlicBottomSheetComponentProvider implements CoBrowseComponentProvider {
    private final Profile mProfile;

    private @Nullable ActorControlStateTracker mActorControlStateTracker;

    /** JNI static factory method to create the provider. */
    @CalledByNative
    private static GlicBottomSheetComponentProvider createProvider(Profile profile) {
        return new GlicBottomSheetComponentProvider(profile);
    }

    GlicBottomSheetComponentProvider(Profile profile) {
        mProfile = profile;
    }

    @Override
    public void destroy() {
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
    public TabBottomSheetContent createContent(
            View contentView,
            float fullHeightRatio,
            @ColorInt int backgroundColor,
            @Px int peekViewHeight,
            @IdRes int peekViewContainerId,
            Runnable onBackPressed) {
        return new GlicBottomSheetContent(
                contentView,
                fullHeightRatio,
                backgroundColor,
                peekViewHeight,
                peekViewContainerId,
                onBackPressed,
                mProfile);
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
}
