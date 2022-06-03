// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.app.Activity;
import android.text.SpannableString;
import android.text.TextUtils;

import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver.PerformanceClass;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ColorUtils;

class ContextMenuHeaderCoordinator {
    private PropertyModel mModel;
    private ContextMenuHeaderMediator mMediator;

    ContextMenuHeaderCoordinator(Activity activity, @PerformanceClass int performanceClass,
            ContextMenuParams params, Profile profile, ContextMenuNativeDelegate nativeDelegate) {
        mModel = buildModel(ContextMenuUtils.getTitle(params), getUrl(activity, params, profile));
        mMediator = new ContextMenuHeaderMediator(
                activity, mModel, performanceClass, params, profile, nativeDelegate);
    }

    private PropertyModel buildModel(String title, CharSequence url) {
        return new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                .with(ContextMenuHeaderProperties.TITLE, title)
                .with(ContextMenuHeaderProperties.TITLE_MAX_LINES, TextUtils.isEmpty(url) ? 2 : 1)
                .with(ContextMenuHeaderProperties.URL, url)
                .with(ContextMenuHeaderProperties.URL_MAX_LINES, TextUtils.isEmpty(title) ? 2 : 1)
                .with(ContextMenuHeaderProperties.URL_PERFORMANCE_CLASS,
                        PerformanceClass.PERFORMANCE_UNKNOWN)
                .with(ContextMenuHeaderProperties.IMAGE, null)
                .with(ContextMenuHeaderProperties.CIRCLE_BG_VISIBLE, false)
                .build();
    }

    private CharSequence getUrl(Activity activity, ContextMenuParams params, Profile profile) {
        CharSequence url = params.getUrl().getSpec();
        if (!TextUtils.isEmpty(url)) {
            boolean useDarkColors = !ColorUtils.inNightMode(activity);

            SpannableString spannableUrl =
                    new SpannableString(ChromeContextMenuPopulator.createUrlText(params));
            ChromeAutocompleteSchemeClassifier chromeAutocompleteSchemeClassifier =
                    new ChromeAutocompleteSchemeClassifier(profile);
            OmniboxUrlEmphasizer.emphasizeUrl(spannableUrl, activity.getResources(),
                    chromeAutocompleteSchemeClassifier, ConnectionSecurityLevel.NONE, false,
                    useDarkColors, false);
            chromeAutocompleteSchemeClassifier.destroy();
            url = spannableUrl;
        }
        return url;
    }

    PropertyModel getModel() {
        return mModel;
    }
}
