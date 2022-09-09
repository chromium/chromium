// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.DimenRes;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.components.autofill_assistant.AssistantProfileImageUtil;

/**
 * Implementation of {@link AssistantProfileImageUtil} for Chrome.
 */
public class AssistantProfileImageUtilChrome
        implements AssistantProfileImageUtil, ProfileDataCache.Observer {
    private final String mSignedInAccountEmail;
    private final ProfileDataCache mProfileCache;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    public AssistantProfileImageUtilChrome(
            Context context, String signedInAccountEmail, @DimenRes int imageSizeRedId) {
        mSignedInAccountEmail = signedInAccountEmail;
        mProfileCache = ProfileDataCache.createWithoutBadge(context, imageSizeRedId);
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
