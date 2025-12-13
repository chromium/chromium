// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.app.Activity;
import android.content.Context;
import android.text.SpannableString;
import android.text.TextUtils;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.embedder_support.contextmenu.ContextMenuNativeDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuUtils;
import org.chromium.components.embedder_support.contextmenu.ContextMenuUtils.HeaderInfo;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

@NullMarked
class ContextMenuHeaderCoordinator {
    private final PropertyModel mModel;

    private static boolean sDisableForTesting;

    ContextMenuHeaderCoordinator(
            Activity activity,
            ContextMenuParams params,
            Profile profile,
            ContextMenuNativeDelegate nativeDelegate) {
        this(activity, params, profile, nativeDelegate, /* isCustomItemPresent= */ false);
    }

    ContextMenuHeaderCoordinator(
            Activity activity,
            ContextMenuParams params,
            Profile profile,
            ContextMenuNativeDelegate nativeDelegate,
            boolean isCustomItemPresent) {
        if (sDisableForTesting) {
            mModel = new PropertyModel();
            return;
        }

        if (!ChromeFeatureList.sCctContextualMenuItems.isEnabled()) {
            isCustomItemPresent = false;
        }
        HeaderInfo headerInfo = ContextMenuUtils.getHeaderInfo(params, isCustomItemPresent);
        String title = headerInfo.getTitle().toString();
        CharSequence url = getUrl(headerInfo.getUrl(), activity, params, profile);
        boolean hasSecondary = !GURL.isEmptyOrInvalid(headerInfo.getSecondaryUrl());
        boolean hasTertiary = !GURL.isEmptyOrInvalid(headerInfo.getTertiaryUrl());

        CharSequence secondaryUrl =
                hasSecondary ? getUrl(headerInfo.getSecondaryUrl(), activity, params, profile) : "";

        // A tertiary URL is only shown if a secondary one is also present.
        CharSequence tertiaryUrl =
                hasSecondary && hasTertiary
                        ? getUrl(headerInfo.getTertiaryUrl(), activity, params, profile)
                        : "";

        mModel = buildModel(activity, title, url, secondaryUrl, tertiaryUrl);
        new ContextMenuHeaderMediator(activity, mModel, params, profile, nativeDelegate);
    }

    public static void setDisableForTesting(boolean disable) {
        sDisableForTesting = disable;
        ResettersForTesting.register(() -> sDisableForTesting = false);
    }

    @VisibleForTesting
    static PropertyModel buildModel(Context context, String title, CharSequence url) {
        return buildModel(context, title, url, /* secondaryUrl= */ "", /* tertiaryUrl= */ "");
    }

    @VisibleForTesting
    static PropertyModel buildModel(
            Context context,
            String title,
            CharSequence url,
            CharSequence secondaryUrl,
            CharSequence tertiaryUrl) {
        boolean usePopupContextMenu = ContextMenuUtils.isPopupSupported(context);

        int monogramSizeDimen =
                usePopupContextMenu
                        ? R.dimen.context_menu_popup_header_monogram_size
                        : R.dimen.context_menu_header_monogram_size;

        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, title)
                        .with(
                                ContextMenuHeaderProperties.TITLE_MAX_LINES,
                                TextUtils.isEmpty(url) ? 2 : 1)
                        .with(ContextMenuHeaderProperties.URL, url)
                        .with(
                                ContextMenuHeaderProperties.URL_MAX_LINES,
                                TextUtils.isEmpty(title) ? 2 : 1)
                        .with(ContextMenuHeaderProperties.IMAGE, null)
                        .with(ContextMenuHeaderProperties.CIRCLE_BG_VISIBLE, false)
                        .with(
                                ContextMenuHeaderProperties.MONOGRAM_SIZE_PIXEL,
                                context.getResources().getDimensionPixelSize(monogramSizeDimen));

        // This is limited to CCTs and the secondary url is only set in CCT sessions with present
        // custom context menu items.
        if (ChromeFeatureList.sCctContextualMenuItems.isEnabled()
                && !TextUtils.isEmpty(secondaryUrl)) {
            // The secondary URL is guaranteed to be visible in this flow.
            int visibleProperties = 1;
            if (!TextUtils.isEmpty(title)) visibleProperties++;
            if (!TextUtils.isEmpty(url)) visibleProperties++;

            boolean isTertiaryUrlPresent = !TextUtils.isEmpty(tertiaryUrl);
            if (isTertiaryUrlPresent) {
                visibleProperties++;
            }

            int maxSecondaryUrlLines = 1;
            int maxTertiaryUrlLines = 1;

            // If there are few enough items, they can expand to take up more space.
            if (visibleProperties == 1) {
                // Should only happen if title, url, and tertiaryUrl are all empty.
                maxSecondaryUrlLines = 3;
            } else if (visibleProperties == 2) {
                // Two items are visible (e.g., secondary + tertiary, or secondary + title).
                // We give the secondary URL priority with more lines.
                maxSecondaryUrlLines = 2;
                // The other item (tertiary, title, or url) implicitly gets 1 line.
            }
            // If visibleProperties is 3 or more, all items get 1 line each (the default).

            modelBuilder
                    .with(ContextMenuHeaderProperties.SECONDARY_URL, secondaryUrl)
                    .with(
                            ContextMenuHeaderProperties.SECONDARY_URL_MAX_LINES,
                            maxSecondaryUrlLines);

            if (isTertiaryUrlPresent) {
                modelBuilder
                        .with(ContextMenuHeaderProperties.TERTIARY_URL, tertiaryUrl)
                        .with(
                                ContextMenuHeaderProperties.TERTIARY_URL_MAX_LINES,
                                maxTertiaryUrlLines);
            }
        }
        PropertyModel model = modelBuilder.build();

        if (usePopupContextMenu) {
            int maxImageSize =
                    context.getResources()
                            .getDimensionPixelSize(
                                    R.dimen.context_menu_popup_header_image_max_size);

            // Popup context menu leaves the same size for image and monogram.
            model.set(
                    ContextMenuHeaderProperties.OVERRIDE_HEADER_IMAGE_MAX_SIZE_PIXEL, maxImageSize);
            model.set(
                    ContextMenuHeaderProperties.OVERRIDE_HEADER_CIRCLE_BG_SIZE_PIXEL, maxImageSize);
            model.set(ContextMenuHeaderProperties.OVERRIDE_HEADER_CIRCLE_BG_MARGIN_PIXEL, 0);
        } else {
            // Use invalid override instead of 0, so view binder will not override layout params.
            model.set(
                    ContextMenuHeaderProperties.OVERRIDE_HEADER_IMAGE_MAX_SIZE_PIXEL,
                    ContextMenuHeaderProperties.INVALID_OVERRIDE);
            model.set(
                    ContextMenuHeaderProperties.OVERRIDE_HEADER_CIRCLE_BG_SIZE_PIXEL,
                    ContextMenuHeaderProperties.INVALID_OVERRIDE);
            model.set(
                    ContextMenuHeaderProperties.OVERRIDE_HEADER_CIRCLE_BG_MARGIN_PIXEL,
                    ContextMenuHeaderProperties.INVALID_OVERRIDE);
        }

        return model;
    }

    private CharSequence getUrl(
            GURL url, Activity activity, ContextMenuParams params, Profile profile) {
        CharSequence pageUrl = params.getUrl().getSpec();
        if (!TextUtils.isEmpty(pageUrl)) {
            SpannableString spannableUrl =
                    new SpannableString(ChromeContextMenuPopulator.createUrlText(url));
            ChromeAutocompleteSchemeClassifier chromeAutocompleteSchemeClassifier =
                    new ChromeAutocompleteSchemeClassifier(profile);
            @ColorInt
            int nonEmphasizedColor =
                    OmniboxResourceProvider.getUrlBarSecondaryTextColor(
                            activity, BrandedColorScheme.APP_DEFAULT);
            @ColorInt
            int emphasizedColor =
                    OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                            activity, BrandedColorScheme.APP_DEFAULT);
            @ColorInt
            int dangerColor =
                    OmniboxResourceProvider.getUrlBarDangerColor(
                            activity, BrandedColorScheme.APP_DEFAULT);
            @ColorInt
            int secureColor =
                    OmniboxResourceProvider.getUrlBarSecureColor(
                            activity, BrandedColorScheme.APP_DEFAULT);
            OmniboxUrlEmphasizer.emphasizeUrl(
                    spannableUrl,
                    chromeAutocompleteSchemeClassifier,
                    ConnectionSecurityLevel.NONE,
                    /* emphasizeScheme= */ false,
                    nonEmphasizedColor,
                    emphasizedColor,
                    dangerColor,
                    secureColor);
            chromeAutocompleteSchemeClassifier.destroy();
            return spannableUrl;
        }
        return pageUrl;
    }

    PropertyModel getModel() {
        return mModel;
    }
}
