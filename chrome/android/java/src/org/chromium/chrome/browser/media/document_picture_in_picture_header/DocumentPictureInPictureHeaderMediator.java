// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.document_picture_in_picture_header;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;

import org.chromium.base.Log;
import org.chromium.build.annotations.MonotonicNonNull;
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
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
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
    private @MonotonicNonNull AppHeaderState mCurrentHeaderState;
    private final DesktopWindowStateManager mDesktopWindowStateManager;
    private final ThemeColorProvider mThemeColorProvider;
    private final Context mContext;
    private final DocumentPictureInPictureHeaderDelegate mDelegate;
    private final Rect mBackToTabRect = new Rect();
    private final Rect mSecurityIconRect = new Rect();
    private final List<Rect> mNonDraggableAreas = new ArrayList<>();
    private final int mMinHeaderHeight;
    private final int mComponentSize;
    private final WebContents mOpenerWebContents;
    private final WebContents mWebContents;
    private final WebContentsObserver mOpenerWebContentsObserver;
    private final WebContentsObserver mWebContentsObserver;

    public DocumentPictureInPictureHeaderMediator(
            PropertyModel model,
            DesktopWindowStateManager desktopWindowStateManager,
            ThemeColorProvider themeColorProvider,
            Context context,
            DocumentPictureInPictureHeaderDelegate delegate,
            boolean isBackToTabShown,
            WebContents openerWebContents,
            WebContents webContents) {
        mModel = model;
        mThemeColorProvider = themeColorProvider;
        mContext = context;
        mDelegate = delegate;
        mOpenerWebContents = openerWebContents;
        mWebContents = webContents;
        mMinHeaderHeight =
                mContext.getResources()
                        .getDimensionPixelSize(
                                R.dimen.document_picture_in_picture_header_min_height);
        mComponentSize =
                mContext.getResources()
                        .getDimensionPixelSize(
                                R.dimen.document_picture_in_picture_header_component_size);
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

        updateSecurityIcon();
        mModel.set(
                DocumentPictureInPictureHeaderProperties.URL_STRING,
                getUrlString(mOpenerWebContents.getVisibleUrl()));

        mThemeColorProvider.addThemeColorObserver(this);
        mThemeColorProvider.addTintObserver(this);
        onThemeColorChanged(mThemeColorProvider.getThemeColor(), /* shouldAnimate= */ false);
        onTintChanged(
                mThemeColorProvider.getTint(),
                mThemeColorProvider.getActivityFocusTint(),
                mThemeColorProvider.getBrandedColorScheme());

        mWebContentsObserver =
                new WebContentsObserver(mWebContents) {
                    @Override
                    public void didChangeVisibleSecurityState() {
                        updateSecurityIcon();
                    }
                };
        mOpenerWebContentsObserver =
                new WebContentsObserver(mOpenerWebContents) {
                    @Override
                    public void didChangeVisibleSecurityState() {
                        updateSecurityIcon();
                    }
                };
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
        setHeaderHeightAndSpacing();
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

    // The calculations in this method below assumes the header components are in a horizontal
    // linear layout, with all of the inner components sharing the same height = componentSize, like
    // this:
    //
    // ======================================
    // || SecurityIcon || Url || BackToTab ||
    // ======================================
    //
    // We also assume that system controls are centered vertically based on the header height
    // provided by the desktop window state manager, our goal is to align the header components with
    // the system controls.
    //
    // The header has a minimum height property that we want to respect for accessibility, and that
    // value is designed to fit all of the header components without being cut off. If the header
    // from the desktop window state is taller than the minimum height, then the inner components
    // will be centered in the header view, with the space distributed evenly at the top and bottom.
    // PaddingTop = PaddingBottom = (headerHeight - componentSize) / 2.
    //
    // And if the header is shorter than the minimum height, then we change the header height to be
    // the minimum height and try to center the components in the header vertically based on the old
    // header height (not the minimum height), to vertically align header components with the system
    // controls.
    // PaddingTop = (oldHeaderHeight - componentSize) / 2.
    // PaddingBottom = minHeaderHeight - componentSize - PaddingTop.
    //
    // However, if the vertical alignment with the system controls causes the components to be cut
    // off (e.g. header components are too tall compared to the system controls so they need to be
    // shifted up to maintain the same vertical alignment, which causes them to be cut off at the
    // top (PaddingTop < 0)), then we will ignore the center alignment and let the components be
    // aligned to the top (paddingTop = 0) to make sure components are not cut off.
    private void setHeaderHeightAndSpacing() {
        assert mCurrentHeaderState != null;
        var headerHeight = mCurrentHeaderState.getAppHeaderHeight();

        var paddingTop = Math.max((headerHeight - mComponentSize) / 2, 0);
        var paddingBottom = 0;
        if (headerHeight >= mMinHeaderHeight) {
            paddingBottom = paddingTop;
        } else {
            headerHeight = mMinHeaderHeight;
            paddingBottom = mMinHeaderHeight - mComponentSize - paddingTop;
        }

        mModel.set(DocumentPictureInPictureHeaderProperties.HEADER_HEIGHT, headerHeight);
        mModel.set(
                DocumentPictureInPictureHeaderProperties.HEADER_SPACING,
                Insets.of(
                        mCurrentHeaderState.getLeftPadding(),
                        paddingTop,
                        mCurrentHeaderState.getRightPadding(),
                        paddingBottom));
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

    private void updateSecurityIcon() {
        @ConnectionSecurityLevel
        int securityLevel = SecurityStateModel.getSecurityLevelForWebContents(mOpenerWebContents);
        @ConnectionMaliciousContentStatus
        int maliciousContentStatus =
                SecurityStateModel.getMaliciousContentStatusForWebContents(mWebContents);

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
    }

    public void destroy() {
        mDesktopWindowStateManager.removeObserver(this);
        mThemeColorProvider.removeThemeColorObserver(this);
        mThemeColorProvider.removeTintObserver(this);
        mOpenerWebContentsObserver.observe(null);
        mWebContentsObserver.observe(null);
    }
}
