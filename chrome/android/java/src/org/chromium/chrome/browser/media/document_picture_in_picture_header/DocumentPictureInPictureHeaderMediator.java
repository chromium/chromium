// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.document_picture_in_picture_header;

import androidx.annotation.ColorInt;
import androidx.core.graphics.Insets;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator for the Document Picture-in-Picture (PiP) header.
 *
 * <p>This class is responsible for observing state changes and theme color changes and updating the
 * property model accordingly.
 */
@NullMarked
public class DocumentPictureInPictureHeaderMediator
        implements DesktopWindowStateManager.AppHeaderObserver,
                ThemeColorProvider.ThemeColorObserver {
    private final PropertyModel mModel;
    private @Nullable AppHeaderState mCurrentHeaderState;
    private final DesktopWindowStateManager mDesktopWindowStateManager;
    private final ThemeColorProvider mThemeColorProvider;

    public DocumentPictureInPictureHeaderMediator(
            PropertyModel model,
            DesktopWindowStateManager desktopWindowStateManager,
            ThemeColorProvider themeColorProvider) {
        mModel = model;
        mThemeColorProvider = themeColorProvider;

        mDesktopWindowStateManager = desktopWindowStateManager;
        mDesktopWindowStateManager.addObserver(this);
        onAppHeaderStateChanged(mDesktopWindowStateManager.getAppHeaderState());

        mThemeColorProvider.addThemeColorObserver(this);
        onThemeColorChanged(mThemeColorProvider.getThemeColor(), /* shouldAnimate= */ false);
    }

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

    public void destroy() {
        mDesktopWindowStateManager.removeObserver(this);
        mThemeColorProvider.removeThemeColorObserver(this);
    }
}
