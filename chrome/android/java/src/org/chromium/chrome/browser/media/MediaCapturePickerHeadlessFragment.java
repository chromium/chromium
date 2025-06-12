// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.media.projection.MediaProjectionManager;
import android.os.Bundle;

import androidx.activity.result.ActivityResult;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.IntDef;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.ApplicationStatus;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Headless fragment used to invoke the MediaProjection API.
 *
 * <p>This is created on a per-Activity basis.
 */
@NullMarked
public class MediaCapturePickerHeadlessFragment extends Fragment {
    private static final String TAG = "MediaCapturePickerHeadlessFragment";

    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    @IntDef({
        CaptureAction.CAPTURE_CANCELLED,
        CaptureAction.CAPTURE_WINDOW,
        CaptureAction.CAPTURE_SCREEN
    })
    @interface CaptureAction {
        int CAPTURE_CANCELLED = 0;
        int CAPTURE_WINDOW = 1;
        int CAPTURE_SCREEN = 2;
    }

    interface Delegate {
        void onPicked(@CaptureAction int action, ActivityResult result);
    }

    private MediaProjectionManager mMediaProjectionManager;
    private ActivityResultLauncher<Intent> mLauncher;
    private @Nullable Delegate mNextDelegate;

    public static @Nullable MediaCapturePickerHeadlessFragment getInstanceForCurrentActivity() {
        var activity = (FragmentActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity == null) {
            return null;
        }
        FragmentManager fragmentManager = activity.getSupportFragmentManager();
        var fragment =
                (MediaCapturePickerHeadlessFragment)
                        fragmentManager.findFragmentByTag(MediaCapturePickerHeadlessFragment.TAG);

        if (fragment == null) {
            fragment = new MediaCapturePickerHeadlessFragment();
            fragmentManager
                    .beginTransaction()
                    .add(fragment, MediaCapturePickerHeadlessFragment.TAG)
                    .commitNowAllowingStateLoss();
        }
        return fragment;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mMediaProjectionManager =
                (MediaProjectionManager)
                        requireActivity().getSystemService(Context.MEDIA_PROJECTION_SERVICE);

        mLauncher =
                registerForActivityResult(
                        new ActivityResultContracts.StartActivityForResult(),
                        result -> {
                            assert mNextDelegate != null;
                            if (result.getResultCode() == Activity.RESULT_OK
                                    && result.getData() != null) {
                                // There is currently no way to differentiate between window and
                                // screen sharing.
                                mNextDelegate.onPicked(CaptureAction.CAPTURE_WINDOW, result);
                            } else {
                                mNextDelegate.onPicked(CaptureAction.CAPTURE_CANCELLED, result);
                            }
                            mNextDelegate = null;
                        });

        // This fragment has no UI so we can retain it.
        setRetainInstance(true);
    }

    public void startAndroidCapturePrompt(Delegate delegate) {
        assert mNextDelegate == null;
        mNextDelegate = delegate;
        mLauncher.launch(mMediaProjectionManager.createScreenCaptureIntent());
    }
}
