// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.chromium.base.ObserverList;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;

/**
 * Implementation of {@link AssistantProfileImageUtil} for Chrome.
 */
public class AssistantProfileImageUtilChrome
        implements AssistantProfileImageUtil, ProfileDataCache.Observer {
    private final String mSignedInAccountEmail;
    private final ProfileDataCache mProfileCache;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    public AssistantProfileImageUtilChrome(Context context, String signedInAccountEmail) {
        mSignedInAccountEmail = signedInAccountEmail;
        mProfileCache = ProfileDataCache.createWithoutBadge(
                context, R.dimen.autofill_assistant_profile_size);
    }

    @Override
    public void addObserver(Observer observer) {
        if (mObservers.isEmpty() && mObservers.addObserver(observer)) {
            mProfileCache.addObserver(this);
        }
    }

    @Override
    public void removeObserver(Observer observer) {
        if (mObservers.removeObserver(observer) && mObservers.isEmpty()) {
            mProfileCache.removeObserver(this);
        }
    }

    @Override
    public void onProfileDataUpdated(String accountEmail) {
        if (!mSignedInAccountEmail.equals(accountEmail)) {
            return;
        }

        Drawable profileImage =
                mProfileCache.getProfileDataOrDefault(mSignedInAccountEmail).getImage();

        for (Observer observer : mObservers) {
            observer.onProfileImageChanged(profileImage);
        }
    }
}
