// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.document_picture_in_picture_header;

import android.content.res.ColorStateList;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Mediator for the Document Picture-in-Picture (PiP) header.
 *
 * <p>This class is responsible for observing state changes and theme color changes and updating the
 * property model accordingly.
 */
@NullMarked
public class DocumentPictureInPictureHeaderMediator
        implements DesktopWindowStateManager.AppHeaderObserver,
                ThemeColorProvider.ThemeColorObserver,
                ThemeColorProvider.TintObserver {
    private final PropertyModel mModel;
    private @Nullable AppHeaderState mCurrentHeaderState;
    private final DesktopWindowStateManager mDesktopWindowStateManager;
    private final ThemeColorProvider mThemeColorProvider;
    private final DocumentPictureInPictureHeaderDelegate mDelegate;
    private final Rect mBackToTabRect = new Rect();
    private final List<Rect> mNonDraggableAreas = new ArrayList<>();

    public DocumentPictureInPictureHeaderMediator(
            PropertyModel model,
            DesktopWindowStateManager desktopWindowStateManager,
            ThemeColorProvider themeColorProvider,
            DocumentPictureInPictureHeaderDelegate delegate,
            boolean isBackToTabShown) {
        mModel = model;
        mThemeColorProvider = themeColorProvider;
        mDelegate = delegate;
        mModel.set(DocumentPictureInPictureHeaderProperties.IS_BACK_TO_TAB_SHOWN, isBackToTabShown);

        mModel.set(
                DocumentPictureInPictureHeaderProperties.ON_BACK_TO_TAB_CLICK_LISTENER,
                v -> onBackToTab());
        mModel.set(
                DocumentPictureInPictureHeaderProperties.ON_LAYOUT_CHANGE_LISTENER,
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) ->
                        updateNonDraggableAreas(v));

        mDesktopWindowStateManager = desktopWindowStateManager;
        mDesktopWindowStateManager.addObserver(this);
        onAppHeaderStateChanged(mDesktopWindowStateManager.getAppHeaderState());

        mThemeColorProvider.addThemeColorObserver(this);
        mThemeColorProvider.addTintObserver(this);
        onThemeColorChanged(mThemeColorProvider.getThemeColor(), /* shouldAnimate= */ false);
        onTintChanged(
                mThemeColorProvider.getTint(),
                mThemeColorProvider.getActivityFocusTint(),
                mThemeColorProvider.getBrandedColorScheme());
    }

    // TODO(crbug.com/477855428): Resize pip window if width doesn't fit header content.
    @Override
    public void onAppHeaderStateChanged(@Nullable AppHeaderState newState) {
        if (newState == null) return;
        if (newState.equals(mCurrentHeaderState)) return;

        mCurrentHeaderState = newState;

        // TODO(crbug.com/475181474): The header should always be shown, we need to handle the case
        // where the caption bars are not available to draw into.
        mModel.set(
                DocumentPictureInPictureHeaderProperties.IS_SHOWN,
                mCurrentHeaderState.isInDesktopWindow());
        // TODO(crbug.com/474041659): Account for header heights != 48dp set by different vendors.
        mModel.set(
                DocumentPictureInPictureHeaderProperties.HEADER_HEIGHT,
                mCurrentHeaderState.getAppHeaderHeight());
        mModel.set(
                DocumentPictureInPictureHeaderProperties.HEADER_SPACING,
                Insets.of(
                        mCurrentHeaderState.getLeftPadding(),
                        0,
                        mCurrentHeaderState.getRightPadding(),
                        0));
    }

    @Override
    public void onThemeColorChanged(@ColorInt int color, boolean shouldAnimate) {
        mDesktopWindowStateManager.updateForegroundColor(color);
        mModel.set(DocumentPictureInPictureHeaderProperties.BACKGROUND_COLOR, color);
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        mModel.set(DocumentPictureInPictureHeaderProperties.TINT_COLOR_LIST, activityFocusTint);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public void onBackToTab() {
        mDelegate.onBackToTab();
    }

    private void updateNonDraggableAreas(View view) {
        // TODO(crbug.com/478159763): Create a supplier for the non-draggable areas to not leak
        // Android views to the mediator.
        mNonDraggableAreas.clear();
        View backToTab = view.findViewById(R.id.document_picture_in_picture_header_back_to_tab);
        if (backToTab != null) {
            backToTab.getHitRect(mBackToTabRect);
            mNonDraggableAreas.add(mBackToTabRect);
        }
        mModel.set(
                DocumentPictureInPictureHeaderProperties.NON_DRAGGABLE_AREAS, mNonDraggableAreas);
    }

    public void destroy() {
        mDesktopWindowStateManager.removeObserver(this);
        mThemeColorProvider.removeThemeColorObserver(this);
        mThemeColorProvider.removeTintObserver(this);
    }
}
