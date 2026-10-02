// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import android.app.PendingIntent;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Network;
import android.os.Bundle;
import android.widget.RemoteViews;

import androidx.annotation.Px;
import androidx.browser.customtabs.CustomTabsIntent.ActivitySideSheetDecorationType;
import androidx.browser.customtabs.CustomTabsIntent.ActivitySideSheetRoundedCornersPosition;
import androidx.browser.customtabs.CustomTabsIntent.OpenInBrowserState;
import androidx.browser.trusted.TrustedWebActivityDisplayMode;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.TitleVisibility;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.CustomTabProfileType;

import java.util.List;

/**
 * Stores Custom Tab specific information on behalf of {@link BrowserServicesIntentDataProvider}, in
 * a form that can be put into the saved instance state.
 */
@NullMarked
public class CustomTabIntentDataHolder {
    public final @Nullable SessionHolder mSessionHolder;
    public final @Nullable String mClientPackageName;
    public final boolean mIsTrustedIntent;
    public final @Nullable Bundle mAnimationBundle;
    public final @Nullable Intent mKeepAliveServiceIntent;
    public final @Nullable Network mNetwork;
    public final boolean mIsOpenedByChrome;
    public final @CustomTabsUiType int mUiType;
    public final @TitleVisibility int mTitleVisibilityState;
    public final @Px int mInitialActivityHeight;
    public final @Px int mInitialActivityWidth;
    public final int mBreakPointDp;
    public final @Px int mPartialTabToolbarCornerRadius;
    public final @ActivityType int mActivityType;
    public final @CustomTabProfileType int mCustomTabMode;
    public final @Nullable String mMediaViewerUrl;
    public final boolean mEnableEmbeddedMediaExperience;
    public final boolean mIsFromMediaLauncherActivity;
    public final boolean mDisableStar;
    public final boolean mDisableDownload;
    public final @Nullable List<String> mTwaAdditionalOrigins;
    public final @Nullable TrustedWebActivityDisplayMode mTwaDisplayMode;
    public final List<TrustedWebActivityDisplayMode> mTwaDisplayOverrideMode;
    public final boolean mEnableUrlBarHiding;
    public final boolean mIsCloseButtonEnabled;
    // Stored as a Bitmap rather than a Drawable because Drawable is not Parcelable.
    public final @Nullable Bitmap mCloseButtonIcon;
    public final @Nullable RemoteViews mRemoteViews;
    public final @ActivitySideSheetDecorationType int mSideSheetDecorationType;
    public final @ActivitySideSheetRoundedCornersPosition int mSideSheetRoundedCornersPosition;
    public final int @Nullable [] mClickableViewIds;
    public final @Nullable PendingIntent mRemoteViewsPendingIntent;
    public final @Nullable PendingIntent mSecondaryToolbarSwipeUpPendingIntent;
    public final @OpenInBrowserState int mOpenInBrowserState;
    public final @Nullable String mTranslateLanguage;
    public final @Nullable String mAutoTranslateLanguage;
    public final int mDefaultOrientation;
    public final int @Nullable [] mGsaExperimentIds;
    public final boolean mIsPartialCustomTabFixedHeight;
    public final boolean mContentScrollMayResizeTab;
    public final boolean mInteractWithBackground;
    public final boolean mCctTabSwitcherEnabledForChromeExperiment;
    public final boolean mCctTabSwitcherEnabledForEmbedderExperiment;
    public final @Nullable String mAuthRedirectScheme;
    public final @Nullable String mAuthRedirectHost;
    public final @Nullable String mAuthRedirectPath;

    private CustomTabIntentDataHolder(Builder builder) {
        mSessionHolder = builder.mSessionHolder;
        mClientPackageName = builder.mClientPackageName;
        mIsTrustedIntent = builder.mIsTrustedIntent;
        mAnimationBundle = builder.mAnimationBundle;
        mKeepAliveServiceIntent = builder.mKeepAliveServiceIntent;
        mNetwork = builder.mNetwork;
        mIsOpenedByChrome = builder.mIsOpenedByChrome;
        mUiType = builder.mUiType;
        mTitleVisibilityState = builder.mTitleVisibilityState;
        mInitialActivityHeight = builder.mInitialActivityHeight;
        mInitialActivityWidth = builder.mInitialActivityWidth;
        mBreakPointDp = builder.mBreakPointDp;
        mPartialTabToolbarCornerRadius = builder.mPartialTabToolbarCornerRadius;
        mActivityType = builder.mActivityType;
        mCustomTabMode = builder.mCustomTabMode;
        mMediaViewerUrl = builder.mMediaViewerUrl;
        mEnableEmbeddedMediaExperience = builder.mEnableEmbeddedMediaExperience;
        mIsFromMediaLauncherActivity = builder.mIsFromMediaLauncherActivity;
        mDisableStar = builder.mDisableStar;
        mDisableDownload = builder.mDisableDownload;
        mTwaAdditionalOrigins = builder.mTwaAdditionalOrigins;
        mTwaDisplayMode = builder.mTwaDisplayMode;
        mTwaDisplayOverrideMode = builder.mTwaDisplayOverrideMode;
        mEnableUrlBarHiding = builder.mEnableUrlBarHiding;
        mIsCloseButtonEnabled = builder.mIsCloseButtonEnabled;
        mCloseButtonIcon = builder.mCloseButtonIcon;
        mRemoteViews = builder.mRemoteViews;
        mSideSheetDecorationType = builder.mSideSheetDecorationType;
        mSideSheetRoundedCornersPosition = builder.mSideSheetRoundedCornersPosition;
        mClickableViewIds = builder.mClickableViewIds;
        mRemoteViewsPendingIntent = builder.mRemoteViewsPendingIntent;
        mSecondaryToolbarSwipeUpPendingIntent = builder.mSecondaryToolbarSwipeUpPendingIntent;
        mOpenInBrowserState = builder.mOpenInBrowserState;
        mTranslateLanguage = builder.mTranslateLanguage;
        mAutoTranslateLanguage = builder.mAutoTranslateLanguage;
        mDefaultOrientation = builder.mDefaultOrientation;
        mGsaExperimentIds = builder.mGsaExperimentIds;
        mIsPartialCustomTabFixedHeight = builder.mIsPartialCustomTabFixedHeight;
        mContentScrollMayResizeTab = builder.mContentScrollMayResizeTab;
        mInteractWithBackground = builder.mInteractWithBackground;
        mCctTabSwitcherEnabledForChromeExperiment =
                builder.mCctTabSwitcherEnabledForChromeExperiment;
        mCctTabSwitcherEnabledForEmbedderExperiment =
                builder.mCctTabSwitcherEnabledForEmbedderExperiment;
        mAuthRedirectScheme = builder.mAuthRedirectScheme;
        mAuthRedirectHost = builder.mAuthRedirectHost;
        mAuthRedirectPath = builder.mAuthRedirectPath;
    }

    public static final class Builder {
        private @Nullable SessionHolder mSessionHolder;
        private @Nullable String mClientPackageName;
        private boolean mIsTrustedIntent;
        private @Nullable Bundle mAnimationBundle;
        private @Nullable Intent mKeepAliveServiceIntent;
        private @Nullable Network mNetwork;
        private boolean mIsOpenedByChrome;
        private @CustomTabsUiType int mUiType;
        private @TitleVisibility int mTitleVisibilityState;
        private @Px int mInitialActivityHeight;
        private @Px int mInitialActivityWidth;
        private int mBreakPointDp;
        private @Px int mPartialTabToolbarCornerRadius;
        private @ActivityType int mActivityType;
        private @CustomTabProfileType int mCustomTabMode = CustomTabProfileType.REGULAR;
        private @Nullable String mMediaViewerUrl;
        private boolean mEnableEmbeddedMediaExperience;
        private boolean mIsFromMediaLauncherActivity;
        private boolean mDisableStar;
        private boolean mDisableDownload;
        private @Nullable List<String> mTwaAdditionalOrigins;
        private @Nullable TrustedWebActivityDisplayMode mTwaDisplayMode;
        private List<TrustedWebActivityDisplayMode> mTwaDisplayOverrideMode = List.of();
        private boolean mEnableUrlBarHiding;
        private boolean mIsCloseButtonEnabled;
        private @Nullable Bitmap mCloseButtonIcon;
        private @Nullable RemoteViews mRemoteViews;
        private @ActivitySideSheetDecorationType int mSideSheetDecorationType;
        private @ActivitySideSheetRoundedCornersPosition int mSideSheetRoundedCornersPosition;
        private int @Nullable [] mClickableViewIds;
        private @Nullable PendingIntent mRemoteViewsPendingIntent;
        private @Nullable PendingIntent mSecondaryToolbarSwipeUpPendingIntent;
        private @OpenInBrowserState int mOpenInBrowserState;
        private @Nullable String mTranslateLanguage;
        private @Nullable String mAutoTranslateLanguage;
        private int mDefaultOrientation;
        private int @Nullable [] mGsaExperimentIds;
        private boolean mIsPartialCustomTabFixedHeight;
        private boolean mContentScrollMayResizeTab;
        private boolean mInteractWithBackground;
        private boolean mCctTabSwitcherEnabledForChromeExperiment;
        private boolean mCctTabSwitcherEnabledForEmbedderExperiment;
        private @Nullable String mAuthRedirectScheme;
        private @Nullable String mAuthRedirectHost;
        private @Nullable String mAuthRedirectPath;

        public Builder setSessionHolder(@Nullable SessionHolder sessionHolder) {
            mSessionHolder = sessionHolder;
            return this;
        }

        public Builder setClientPackageName(@Nullable String clientPackageName) {
            mClientPackageName = clientPackageName;
            return this;
        }

        public Builder setIsTrustedIntent(boolean isTrustedIntent) {
            mIsTrustedIntent = isTrustedIntent;
            return this;
        }

        public Builder setAnimationBundle(@Nullable Bundle animationBundle) {
            mAnimationBundle = animationBundle;
            return this;
        }

        public Builder setKeepAliveServiceIntent(@Nullable Intent keepAliveServiceIntent) {
            mKeepAliveServiceIntent = keepAliveServiceIntent;
            return this;
        }

        public Builder setNetwork(@Nullable Network network) {
            mNetwork = network;
            return this;
        }

        public Builder setIsOpenedByChrome(boolean isOpenedByChrome) {
            mIsOpenedByChrome = isOpenedByChrome;
            return this;
        }

        public Builder setUiType(@CustomTabsUiType int uiType) {
            mUiType = uiType;
            return this;
        }

        public Builder setTitleVisibilityState(@TitleVisibility int titleVisibilityState) {
            mTitleVisibilityState = titleVisibilityState;
            return this;
        }

        public Builder setInitialActivityHeight(@Px int initialActivityHeight) {
            mInitialActivityHeight = initialActivityHeight;
            return this;
        }

        public Builder setInitialActivityWidth(@Px int initialActivityWidth) {
            mInitialActivityWidth = initialActivityWidth;
            return this;
        }

        public Builder setBreakPointDp(int breakPointDp) {
            mBreakPointDp = breakPointDp;
            return this;
        }

        public Builder setPartialTabToolbarCornerRadius(@Px int partialTabToolbarCornerRadius) {
            mPartialTabToolbarCornerRadius = partialTabToolbarCornerRadius;
            return this;
        }

        public Builder setActivityType(@ActivityType int activityType) {
            mActivityType = activityType;
            return this;
        }

        public Builder setCustomTabMode(@CustomTabProfileType int customTabMode) {
            mCustomTabMode = customTabMode;
            return this;
        }

        public Builder setMediaViewerUrl(@Nullable String mediaViewerUrl) {
            mMediaViewerUrl = mediaViewerUrl;
            return this;
        }

        public Builder setEnableEmbeddedMediaExperience(boolean enableEmbeddedMediaExperience) {
            mEnableEmbeddedMediaExperience = enableEmbeddedMediaExperience;
            return this;
        }

        public Builder setIsFromMediaLauncherActivity(boolean isFromMediaLauncherActivity) {
            mIsFromMediaLauncherActivity = isFromMediaLauncherActivity;
            return this;
        }

        public Builder setDisableStar(boolean disableStar) {
            mDisableStar = disableStar;
            return this;
        }

        public Builder setDisableDownload(boolean disableDownload) {
            mDisableDownload = disableDownload;
            return this;
        }

        public Builder setTwaAdditionalOrigins(@Nullable List<String> twaAdditionalOrigins) {
            mTwaAdditionalOrigins = twaAdditionalOrigins;
            return this;
        }

        public Builder setTwaDisplayMode(@Nullable TrustedWebActivityDisplayMode twaDisplayMode) {
            mTwaDisplayMode = twaDisplayMode;
            return this;
        }

        public Builder setTwaDisplayOverrideMode(
                List<TrustedWebActivityDisplayMode> twaDisplayOverrideMode) {
            mTwaDisplayOverrideMode = twaDisplayOverrideMode;
            return this;
        }

        public Builder setEnableUrlBarHiding(boolean enableUrlBarHiding) {
            mEnableUrlBarHiding = enableUrlBarHiding;
            return this;
        }

        public Builder setIsCloseButtonEnabled(boolean isCloseButtonEnabled) {
            mIsCloseButtonEnabled = isCloseButtonEnabled;
            return this;
        }

        public Builder setCloseButtonIcon(@Nullable Bitmap closeButtonIcon) {
            mCloseButtonIcon = closeButtonIcon;
            return this;
        }

        public Builder setRemoteViews(@Nullable RemoteViews remoteViews) {
            mRemoteViews = remoteViews;
            return this;
        }

        public Builder setSideSheetDecorationType(
                @ActivitySideSheetDecorationType int sideSheetDecorationType) {
            mSideSheetDecorationType = sideSheetDecorationType;
            return this;
        }

        public Builder setSideSheetRoundedCornersPosition(
                @ActivitySideSheetRoundedCornersPosition int sideSheetRoundedCornersPosition) {
            mSideSheetRoundedCornersPosition = sideSheetRoundedCornersPosition;
            return this;
        }

        public Builder setClickableViewIds(int @Nullable [] clickableViewIds) {
            mClickableViewIds = clickableViewIds;
            return this;
        }

        public Builder setRemoteViewsPendingIntent(
                @Nullable PendingIntent remoteViewsPendingIntent) {
            mRemoteViewsPendingIntent = remoteViewsPendingIntent;
            return this;
        }

        public Builder setSecondaryToolbarSwipeUpPendingIntent(
                @Nullable PendingIntent secondaryToolbarSwipeUpPendingIntent) {
            mSecondaryToolbarSwipeUpPendingIntent = secondaryToolbarSwipeUpPendingIntent;
            return this;
        }

        public Builder setOpenInBrowserState(@OpenInBrowserState int openInBrowserState) {
            mOpenInBrowserState = openInBrowserState;
            return this;
        }

        public Builder setTranslateLanguage(@Nullable String translateLanguage) {
            mTranslateLanguage = translateLanguage;
            return this;
        }

        public Builder setAutoTranslateLanguage(@Nullable String autoTranslateLanguage) {
            mAutoTranslateLanguage = autoTranslateLanguage;
            return this;
        }

        public Builder setDefaultOrientation(int defaultOrientation) {
            mDefaultOrientation = defaultOrientation;
            return this;
        }

        public Builder setGsaExperimentIds(int @Nullable [] gsaExperimentIds) {
            mGsaExperimentIds = gsaExperimentIds;
            return this;
        }

        public Builder setIsPartialCustomTabFixedHeight(boolean isPartialCustomTabFixedHeight) {
            mIsPartialCustomTabFixedHeight = isPartialCustomTabFixedHeight;
            return this;
        }

        public Builder setContentScrollMayResizeTab(boolean contentScrollMayResizeTab) {
            mContentScrollMayResizeTab = contentScrollMayResizeTab;
            return this;
        }

        public Builder setInteractWithBackground(boolean interactWithBackground) {
            mInteractWithBackground = interactWithBackground;
            return this;
        }

        public Builder setCctTabSwitcherEnabledForChromeExperiment(
                boolean cctTabSwitcherEnabledForChromeExperiment) {
            mCctTabSwitcherEnabledForChromeExperiment = cctTabSwitcherEnabledForChromeExperiment;
            return this;
        }

        public Builder setCctTabSwitcherEnabledForEmbedderExperiment(
                boolean cctTabSwitcherEnabledForEmbedderExperiment) {
            mCctTabSwitcherEnabledForEmbedderExperiment =
                    cctTabSwitcherEnabledForEmbedderExperiment;
            return this;
        }

        public Builder setAuthRedirectScheme(@Nullable String authRedirectScheme) {
            mAuthRedirectScheme = authRedirectScheme;
            return this;
        }

        public Builder setAuthRedirectHost(@Nullable String authRedirectHost) {
            mAuthRedirectHost = authRedirectHost;
            return this;
        }

        public Builder setAuthRedirectPath(@Nullable String authRedirectPath) {
            mAuthRedirectPath = authRedirectPath;
            return this;
        }

        public CustomTabIntentDataHolder build() {
            return new CustomTabIntentDataHolder(this);
        }
    }
}
