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

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.embedder_support.contextmenu.ContextMenuNativeDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.ui.modelutil.PropertyModel;

class ContextMenuHeaderCoordinator {
    private PropertyModel mModel;
    private ContextMenuHeaderMediator mMediator;

    ContextMenuHeaderCoordinator(
            Activity activity,
            ContextMenuParams params,
            Profile profile,
            ContextMenuNativeDelegate nativeDelegate) {
        mModel =
                buildModel(
                        activity,
                        ContextMenuUtils.getTitle(params),
                        getUrl(activity, params, profile));
        mMediator =
                new ContextMenuHeaderMediator(activity, mModel, params, profile, nativeDelegate);
    }

    @VisibleForTesting
    static PropertyModel buildModel(Context context, String title, CharSequence url) {
        boolean usePopupContextMenu = ContextMenuUtils.usePopupContextMenuForContext(context);

        int monogramSizeDimen =
                usePopupContextMenu
                        ? R.dimen.context_menu_popup_header_monogram_size
                        : R.dimen.context_menu_header_monogram_size;

        PropertyModel model =
                new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                        .with(ContextMenuHeaderProperties.TITLE, title)
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
                                context.getResources().getDimensionPixelSize(monogramSizeDimen))
                        .build();

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

    private CharSequence getUrl(Activity activity, ContextMenuParams params, Profile profile) {
        CharSequence url = params.getUrl().getSpec();
        if (!TextUtils.isEmpty(url)) {
            SpannableString spannableUrl =
                    new SpannableString(ChromeContextMenuPopulator.createUrlText(params));
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
            url = spannableUrl;
        }
        return url;
    }

    PropertyModel getModel() {
        return mModel;
    }
}
