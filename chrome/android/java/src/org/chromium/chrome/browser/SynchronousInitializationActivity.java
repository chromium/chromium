// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.CallSuper;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.bookmarks.BookmarkActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileIntentUtils;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;

/**
 * Ensures that the native library is loaded by synchronously initializing it on creation.
 *
 * <p>This is needed for Activities that can be started without going through the regular
 * asynchronous browser startup pathway, which could happen if the user restarted Chrome after it
 * died in the background with the Activity visible. One example is {@link BookmarkActivity} and its
 * kin.
 */
@NullMarked
public abstract class SynchronousInitializationActivity extends ChromeBaseAppCompatActivity {
    private final OneshotSupplierImpl<Profile> mProfileSupplier = new OneshotSupplierImpl<>();
    private ActivityWindowAndroid mWindowAndroid;

    @Initializer
    @Override
    protected final void onCreate(@Nullable Bundle savedInstanceState) {
        // Make sure the native is initialized before calling super.onCreate(), as calling
        // super.onCreate() will recreate fragments that might depend on the native code.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
        initProfile();
        super.onCreate(savedInstanceState);
        onCreateInternal(savedInstanceState);

        mWindowAndroid = createWindowAndroid();
        if (isFinishing()) return;
        mProfileSupplier.runSyncOrOnAvailable(this::onProfileAvailable);
    }

    @CallSuper
    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent intent) {
        assumeNonNull(mWindowAndroid.getIntentRequestTracker())
                .onActivityResult(requestCode, resultCode, intent);
        super.onActivityResult(requestCode, resultCode, intent);
    }

    @CallSuper
    @Override
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        if (mWindowAndroid.handlePermissionResult(requestCode, permissions, grantResults)) {
            return;
        }
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    }

    @CallSuper
    @Override
    protected void onDestroy() {
        mWindowAndroid.destroy();

        // This must be after destroying ActivityWindowAndroid because it has a reference to
        // the Activity.
        super.onDestroy();
    }

    /**
     * Activity specific implementation corresponding to {@link
     * android.app.Activity#onCreate(Bundle)}
     */
    @Initializer
    protected void onCreateInternal(@Nullable Bundle savedInstanceState) {}

    /**
     * Creates an {@link ActivityWindowAndroid} to delegate calls to, if the Activity requires it.
     */
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(
                this,
                /* listenToActivityState= */ true,
                IntentRequestTracker.createFromActivity(this),
                getInsetObserver(),
                /* occlusionTrackingAllowed= */ true);
    }

    /** Returns the {@link ActivityWindowAndroid} instance attached to the activity. */
    protected ActivityWindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

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
