// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import android.view.View;

import org.chromium.chrome.browser.incognito.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class IncognitoReauthViewBinder {
    public static void bind(
            PropertyModel model, View incognitoReauthView, PropertyKey propertyKey) {
        if (IncognitoReauthProperties.ON_UNLOCK_INCOGNITO_CLICKED == propertyKey) {
            Runnable unlockIncognito =
                    model.get(IncognitoReauthProperties.ON_UNLOCK_INCOGNITO_CLICKED);
            assert unlockIncognito != null : "Unlock Incognito runnable can't be null.";
            incognitoReauthView.findViewById(R.id.incognito_reauth_unlock_incognito_button)
                    .setOnClickListener(view -> unlockIncognito.run());
        } else if (IncognitoReauthProperties.ON_SEE_OTHER_TABS_CLICKED == propertyKey) {
            Runnable seeOtherTabs = model.get(IncognitoReauthProperties.ON_SEE_OTHER_TABS_CLICKED);
            assert seeOtherTabs != null : "See other tabs runnable can't be null.";
            incognitoReauthView.findViewById(R.id.incognito_reauth_see_other_tabs_label)
                    .setOnClickListener(view -> seeOtherTabs.run());
        } else if (IncognitoReauthProperties.IS_FULL_SCREEN == propertyKey) {
            boolean isFullScreen = model.get(IncognitoReauthProperties.IS_FULL_SCREEN);
            if (isFullScreen) {
                incognitoReauthView.findViewById(R.id.incognito_reauth_see_other_tabs_label)
                        .setVisibility(View.VISIBLE);
                incognitoReauthView.findViewById(R.id.incognito_reauth_menu_button)
                        .setVisibility(View.VISIBLE);
            } else {
                // For non-full screen we have a slight modification on top of the default
                // to remove the incognito icon only for small screen size.
                // TODO(crbug.com/1227656): Add logic to remove Incognito icon for
                // small screen.
            }
        }
    }
}
