// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.incognito.R;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class IncognitoReauthViewBinder {
    public static void bind(
            PropertyModel model, View incognitoReauthView, PropertyKey propertyKey) {
        if (IncognitoReauthProperties.ON_UNLOCK_INCOGNITO_CLICKED == propertyKey) {
            Runnable unlockIncognito =
                    model.get(IncognitoReauthProperties.ON_UNLOCK_INCOGNITO_CLICKED);
            assert unlockIncognito != null : "Unlock Incognito runnable can't be null.";
            incognitoReauthView
                    .findViewById(R.id.incognito_reauth_unlock_incognito_button)
                    .setOnClickListener(view -> unlockIncognito.run());
        } else if (IncognitoReauthProperties.ON_SEE_OTHER_TABS_CLICKED == propertyKey) {
            Runnable seeOtherTabs = model.get(IncognitoReauthProperties.ON_SEE_OTHER_TABS_CLICKED);
            assert seeOtherTabs != null : "See other tabs runnable can't be null.";
            incognitoReauthView
                    .findViewById(R.id.incognito_reauth_see_other_tabs_label)
                    .setOnClickListener(view -> seeOtherTabs.run());
        } else if (IncognitoReauthProperties.IS_FULL_SCREEN == propertyKey) {
            boolean isFullScreen = model.get(IncognitoReauthProperties.IS_FULL_SCREEN);
            updateViewVisibility(incognitoReauthView, isFullScreen);
        } else if (IncognitoReauthProperties.MENU_BUTTON_DELEGATE == propertyKey) {
            updateMenuButton(
                    incognitoReauthView, model.get(IncognitoReauthProperties.MENU_BUTTON_DELEGATE));
        } else {
            assert false : "Property not found.";
        }
    }

    private static void updateViewVisibility(View incognitoReauthView, boolean isFullscreen) {
        incognitoReauthView
                .findViewById(R.id.incognito_reauth_see_other_tabs_label)
                .setVisibility(isFullscreen ? View.VISIBLE : View.GONE);
        // For non-full screen we have a slight modification on top of the default
        // to remove the incognito icon only for small screen size.
        // TODO(crbug.com/40056462): Add logic to remove Incognito icon for
        // small screen.
    }

    private static void updateMenuButton(
            View incognitoReauthView, @Nullable ListMenuButtonDelegate menuButtonDelegate) {
        ListMenuButton menuButton =
                incognitoReauthView.findViewById(R.id.incognito_reauth_menu_button);
        menuButton.setDelegate(menuButtonDelegate);
        menuButton.setVisibility(menuButtonDelegate != null ? View.VISIBLE : View.GONE);
    }
}
