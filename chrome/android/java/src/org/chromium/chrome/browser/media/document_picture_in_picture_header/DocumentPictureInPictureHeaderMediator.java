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

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.SecurityStatusIcon;
import org.chromium.components.security_state.ConnectionMaliciousContentStatus;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

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
    private static final String TAG = "DocumentPiPHdrMdtr";
    private final PropertyModel mModel;
    private @Nullable AppHeaderState mCurrentHeaderState;
    private final DesktopWindowStateManager mDesktopWindowStateManager;
    private final ThemeColorProvider mThemeColorProvider;
    private final DocumentPictureInPictureHeaderDelegate mDelegate;
    private final Rect mBackToTabRect = new Rect();
    private final Rect mSecurityIconRect = new Rect();
    private final List<Rect> mNonDraggableAreas = new ArrayList<>();

    public DocumentPictureInPictureHeaderMediator(
            PropertyModel model,
            DesktopWindowStateManager desktopWindowStateManager,
            ThemeColorProvider themeColorProvider,
            DocumentPictureInPictureHeaderDelegate delegate,
            boolean isBackToTabShown,
            @ConnectionSecurityLevel int securityLevel,
            @ConnectionMaliciousContentStatus int maliciousContentStatus,
            GURL url) {
        mModel = model;
        mThemeColorProvider = themeColorProvider;
        mDelegate = delegate;
        mModel.set(DocumentPictureInPictureHeaderProperties.IS_BACK_TO_TAB_SHOWN, isBackToTabShown);

        mModel.set(
                DocumentPictureInPictureHeaderProperties.ON_BACK_TO_TAB_CLICK_LISTENER,
                v -> onBackToTab());
        mModel.set(
                DocumentPictureInPictureHeaderProperties.ON_SECURITY_ICON_CLICK_LISTENER,
                v -> onSecurityIconClicked());
        mModel.set(
                DocumentPictureInPictureHeaderProperties.ON_LAYOUT_CHANGE_LISTENER,
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) ->
                        updateNonDraggableAreas(v));

        mDesktopWindowStateManager = desktopWindowStateManager;
        mDesktopWindowStateManager.addObserver(this);
        onAppHeaderStateChanged(mDesktopWindowStateManager.getAppHeaderState());

        mModel.set(
                DocumentPictureInPictureHeaderProperties.SECURITY_ICON,
                SecurityStatusIcon.getSecurityIconResource(
                        securityLevel,
                        () -> maliciousContentStatus,
                        /* isSmallDevice= */ false,
                        /* skipIconForNeutralState= */ false,
                        /* useLockIconForSecureState= */ false));
        mModel.set(
                DocumentPictureInPictureHeaderProperties.SECURITY_ICON_CONTENT_DESCRIPTION_RES_ID,
                SecurityStatusIcon.getSecurityIconContentDescriptionResourceId(securityLevel));
        mModel.set(DocumentPictureInPictureHeaderProperties.URL_STRING, getUrlString(url));

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
        mModel.set(
                DocumentPictureInPictureHeaderProperties.BRANDED_COLOR_SCHEME, brandedColorScheme);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public void onBackToTab() {
        mDelegate.onBackToTab();
    }

    private void onSecurityIconClicked() {
        mDelegate.onSecurityIconClicked();
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

        View securityIcon =
                view.findViewById(R.id.document_picture_in_picture_header_security_icon);
        if (securityIcon != null) {
            securityIcon.getHitRect(mSecurityIconRect);
            mNonDraggableAreas.add(mSecurityIconRect);
        }

        mModel.set(
                DocumentPictureInPictureHeaderProperties.NON_DRAGGABLE_AREAS, mNonDraggableAreas);
    }

    private String getUrlString(GURL url) {
        if (url.getScheme().equals(UrlConstants.FILE_SCHEME)) {
            // File scheme URLs do not have a host, so we use the path instead.
            return url.getPath();
        }

        if (url.getHost().isEmpty()) {
            Log.w(TAG, "URL has an empty host, falling back to the full URL spec.");
            // Fallback to the full URL spec if the host is empty.
            return url.getSpec();
        }

        return url.getHost();
    }

    public void destroy() {
        mDesktopWindowStateManager.removeObserver(this);
        mThemeColorProvider.removeThemeColorObserver(this);
        mThemeColorProvider.removeTintObserver(this);
    }
}
