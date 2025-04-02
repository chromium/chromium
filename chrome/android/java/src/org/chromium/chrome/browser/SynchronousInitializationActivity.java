// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Bundle;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.app.bookmarks.BookmarkActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileIntentUtils;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;

/**
 * Ensures that the native library is loaded by synchronously initializing it on creation.
 *
 * <p>This is needed for Activities that can be started without going through the regular
 * asynchronous browser startup pathway, which could happen if the user restarted Chrome after it
 * died in the background with the Activity visible. One example is {@link BookmarkActivity} and its
 * kin.
 */
public abstract class SynchronousInitializationActivity extends ChromeBaseAppCompatActivity {
    private final OneshotSupplierImpl<Profile> mProfileSupplier = new OneshotSupplierImpl<>();

    @Override
    protected final void onCreate(Bundle savedInstanceState) {
        // Make sure the native is initialized before calling super.onCreate(), as calling
        // super.onCreate() will recreate fragments that might depend on the native code.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
        initProfile();
        super.onCreate(savedInstanceState);
        onCreateInternal(savedInstanceState);

        if (isFinishing()) return;
        mProfileSupplier.runSyncOrOnAvailable(this::onProfileAvailable);
    }

    /**
     * Activity specific implementation corresponding to {@link
     * android.app.Activity#onCreate(Bundle)}
     */
    protected void onCreateInternal(Bundle savedInstanceState) {}

    /**
     * On initial startup, called when the profile is fully loaded and ready to use. This is not
     * guaranteed to be in the same UI loop task as {@link #onCreate(Bundle)}.
     */
    protected void onProfileAvailable(Profile profile) {}

    private void initProfile() {
        assert ProfileManager.isInitialized();
        if (ProfileIntentUtils.hasProfileToken(getIntent())) {
            ProfileIntentUtils.retrieveProfileFromIntent(
                    getIntent(),
                    (profile) -> {
                        if (profile == null) {
                            finish();
                            return;
                        }
                        mProfileSupplier.set(profile);
                    });
        } else {
            // TODO(crbug.com/40254448): Remove this fallback path once all activities are started
            // with a Profile reference passed in. Instead of using the last used profile, these
            // should call finish() instead.
            mProfileSupplier.set(ProfileManager.getLastUsedRegularProfile());
        }
    }

    /** Return the {@link ProfileProvider} for this Activity. */
    public OneshotSupplier<Profile> getProfileSupplier() {
        return mProfileSupplier;
    }
}
