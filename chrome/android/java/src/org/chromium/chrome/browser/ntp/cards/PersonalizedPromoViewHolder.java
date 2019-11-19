// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.chrome.browser.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.signin.SigninPromoController;
import org.chromium.chrome.browser.signin.SigninPromoUtil;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.ui.widget.displaystyle.UiConfig;

/**
 * View Holder for {@link SignInPromo} if the personalized promo is to be shown.
 */
public class PersonalizedPromoViewHolder extends CardViewHolder {
    private @Nullable ProfileDataCache mProfileDataCache;
    private @Nullable SigninPromoController mSigninPromoController;

    public PersonalizedPromoViewHolder(SuggestionsRecyclerView parent,
            ContextMenuManager contextMenuManager, UiConfig config) {
        super(R.layout.personalized_signin_promo_view_modern_content_suggestions, parent, config,
                contextMenuManager);
    }

    public void onBindViewHolder(
            SigninPromoController signinPromoController, ProfileDataCache profileDataCache) {
        super.onBindViewHolder();
        mSigninPromoController = signinPromoController;
        mProfileDataCache = profileDataCache;
        updatePersonalizedSigninPromo();
    }

    @Override
    public void recycle() {
        if (mSigninPromoController != null) {
            mSigninPromoController.detach();
            mSigninPromoController = null;
        }
        mProfileDataCache = null;
        super.recycle();
    }

    /**
     * Triggers an update of the personalized signin promo. Intended to be used as
     * {@link NewTabPageViewHolder.PartialBindCallback}.
     */
    public static void update(NewTabPageViewHolder viewHolder) {
        ((PersonalizedPromoViewHolder) viewHolder).updatePersonalizedSigninPromo();
    }

    private void updatePersonalizedSigninPromo() {
        SigninPromoUtil.setupPromoViewFromCache(mSigninPromoController, mProfileDataCache,
                (PersonalizedSigninPromoView) itemView, null);
    }

    /**
     * Binds the view and sets the profile data directly. Used for testing purposes.
     * @param profileData The profile data which will be used to configure the personalized
     *         signin promo.
     */
    @VisibleForTesting
    public void bindAndConfigureViewForTests(@Nullable DisplayableProfileData profileData) {
        super.onBindViewHolder();
        PersonalizedSigninPromoView view = (PersonalizedSigninPromoView) itemView;
        mSigninPromoController.setupPromoView(view.getContext(), view, profileData, null);
    }

    @VisibleForTesting
    public void setSigninPromoControllerForTests(@Nullable SigninPromoController controller) {
        mSigninPromoController = controller;
    }
}
