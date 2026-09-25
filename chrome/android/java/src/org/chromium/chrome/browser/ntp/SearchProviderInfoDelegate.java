// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.search_engines.AiModeButtonUiConfig;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.url.GURL;

/** Delegate to hold search provider information for the New Tab Page. */
@NullMarked
public class SearchProviderInfoDelegate {
    private final TemplateUrlService mTemplateUrlService;

    private boolean mSearchProviderHasLogo = true;
    private boolean mSearchProviderIsGoogle;
    private boolean mShowingNonStandardGoogleLogo;
    private @Nullable AiModeButtonUiConfig mAiModeButtonUiConfig;

    /**
     * @param templateUrlService The {@link TemplateUrlService} of the current profile.
     */
    public SearchProviderInfoDelegate(TemplateUrlService templateUrlService) {
        mTemplateUrlService = templateUrlService;
    }

    /**
     * Sets whether the search provider has a logo and whether it is Google.
     *
     * @param hasLogo Whether the search provider has a logo.
     * @param isGoogle Whether the search provider is Google.
     * @return True if any parameter is changed, false otherwise.
     */
    public boolean setSearchProviderInfo(boolean hasLogo, boolean isGoogle) {
        if (mSearchProviderHasLogo == hasLogo && mSearchProviderIsGoogle == isGoogle) {
            return false;
        }

        mSearchProviderHasLogo = hasLogo;
        mSearchProviderIsGoogle = isGoogle;
        if (!isGoogle) {
            mShowingNonStandardGoogleLogo = false;
        }
        return true;
    }

    /** Returns whether the search provider has a logo. */
    public boolean getSearchProviderHasLogo() {
        return mSearchProviderHasLogo;
    }

    /** Sets whether the search provider has a logo. */
    public void setSearchProviderHasLogo(boolean hasLogo) {
        mSearchProviderHasLogo = hasLogo;
    }

    /** Returns whether the search provider is Google. */
    public boolean getSearchProviderIsGoogle() {
        return mSearchProviderIsGoogle;
    }

    /** Sets whether the search provider is Google. */
    public void setSearchProviderIsGoogle(boolean isGoogle) {
        mSearchProviderIsGoogle = isGoogle;
        if (!mSearchProviderIsGoogle) {
            mShowingNonStandardGoogleLogo = false;
        }
    }

    /** Returns whether a non-standard Google logo (e.g., doodle) is being shown. */
    public boolean getShowingNonStandardGoogleLogo() {
        return mShowingNonStandardGoogleLogo;
    }

    /** Sets whether a non-standard Google logo (e.g., doodle) is being shown. */
    public void setShowingNonStandardGoogleLogo(boolean showingNonStandardGoogleLogo) {
        mShowingNonStandardGoogleLogo = showingNonStandardGoogleLogo;
    }

    /** Returns the composeplate URL of the current search provider, or null if there isn't one. */
    public @Nullable GURL getComposeplateUrl() {
        return mTemplateUrlService.getComposeplateUrl();
    }

    /**
     * Sets the {@link AiModeButtonUiConfig} of the current search provider, which is null when the
     * provider doesn't offer an AI Mode entry point, or when this client isn't permitted to surface
     * one.
     *
     * @param aiModeButtonUiConfig The config of the current search provider.
     * @return True if the config is changed, false otherwise.
     */
    public boolean setAiModeButtonUiConfig(@Nullable AiModeButtonUiConfig aiModeButtonUiConfig) {
        if (mAiModeButtonUiConfig == aiModeButtonUiConfig) return false;

        mAiModeButtonUiConfig = aiModeButtonUiConfig;
        return true;
    }

    /**
     * Returns the {@link AiModeButtonUiConfig} of the current search provider, or null if there
     * isn't one.
     */
    public @Nullable AiModeButtonUiConfig getAiModeButtonUiConfig() {
        return mAiModeButtonUiConfig;
    }

    /** Returns whether the current search provider offers an AI Mode entry point. */
    public boolean hasAiModeEntryPoint() {
        return mAiModeButtonUiConfig != null;
    }
}
