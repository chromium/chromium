// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Bundle;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.app.bookmarks.BookmarkActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;

/**
 * Ensures that the native library is loaded by synchronously initializing it on creation.
 *
 * This is needed for Activities that can be started without going through the regular asynchronous
 * browser startup pathway, which could happen if the user restarted Chrome after it died in the
 * background with the Activity visible.  One example is {@link BookmarkActivity} and its kin.
 */
public abstract class SynchronousInitializationActivity extends ChromeBaseAppCompatActivity {
    private ProfileProvider mProfileProvider;

    @CallSuper
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Make sure the native is initialized before calling super.onCreate(), as calling
        // super.onCreate() will recreate fragments that might depend on the native code.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
        mProfileProvider = createProfileProvider();
        super.onCreate(savedInstanceState);
    }

    private ProfileProvider createProfileProvider() {
        assert ProfileManager.isInitialized();
        // TODO(crbug.com/40254448): Pass the Profile information via the launching Intent and
        // remove
        // getLastUsedRegularProfile below.
        Profile profile = ProfileManager.getLastUsedRegularProfile();
        return new ProfileProvider() {
            @NonNull
            @Override
            public Profile getOriginalProfile() {
                return profile;
            }

            @Nullable
            @Override
            public Profile getOffTheRecordProfile(boolean createIfNeeded) {
                // TODO(crbug.com/40254448): Instead of using getPrimaryOTRProfile, this should
                // account
                //                      for instances where the incognito profile is using a
                //                      non-primary key. Because the Bookmark model redirects to the
                //                      original profile regardless, this is not a critical issue.
                return profile.getPrimaryOTRProfile(createIfNeeded);
            }

            @Override
            public boolean hasOffTheRecordProfile() {
                return profile.hasPrimaryOTRProfile();
            }
        };
    }

    /** Return the {@link ProfileProvider} for this Activity. */
    public ProfileProvider getProfileProvider() {
        return mProfileProvider;
    }
}
