// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.media.projection.MediaProjectionManager;
import android.os.Bundle;
import android.util.SparseArray;

import androidx.activity.result.ActivityResult;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.IntDef;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

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

    /**
     * Task id of the hosting Activity, resolved in {@link #onCreate} while the Activity is live.
     * {@link Activity#getTaskId()} is a binder call that returns -1 once the Activity has been
     * detached from its task, which can already be the case in {@link #onDestroy}.
     */
    private int mTaskId;

    /**
     * Delegate of the prompt showing in each window, keyed by the Activity's task id.
     *
     * <p>Static because the Activity, and this fragment with it, may be recreated while the system
     * prompt is showing; the result is then delivered to the new fragment instance. The task id is
     * stable across recreation and unique per Chrome window. Entries are removed when the result
     * arrives, or when the Activity is destroyed other than for recreation, which also cancels the
     * request. They do not survive process death.
     */
    /* package */ static final SparseArray<Delegate> sNextDelegates = new SparseArray<>();

    public static MediaCapturePickerHeadlessFragment getInstance(FragmentActivity activity) {
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

        mTaskId = requireActivity().getTaskId();
        mMediaProjectionManager =
                (MediaProjectionManager)
                        requireActivity().getSystemService(Context.MEDIA_PROJECTION_SERVICE);

        mLauncher =
                registerForActivityResult(
                        new ActivityResultContracts.StartActivityForResult(),
                        result -> {
                            Delegate delegate = sNextDelegates.get(mTaskId);
                            sNextDelegates.remove(mTaskId);
                            // No pending request: the process was restarted while the prompt was
                            // showing, so there is nothing to deliver the result to.
                            if (delegate == null) return;
                            if (result.getResultCode() == Activity.RESULT_OK
                                    && result.getData() != null) {
                                // There is currently no way to differentiate between window and
                                // screen sharing.
                                delegate.onPicked(CaptureAction.CAPTURE_WINDOW, result);
                            } else {
                                delegate.onPicked(CaptureAction.CAPTURE_CANCELLED, result);
                            }
                        });
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        // Keep the entry across recreate() / a configuration relaunch, where the pending result is
        // delivered to the new instance; otherwise cancel so picker state is cleaned up.
        if (!requireActivity().isChangingConfigurations()) {
            Delegate delegate = sNextDelegates.get(mTaskId);
            sNextDelegates.remove(mTaskId);
            if (delegate != null) {
                delegate.onPicked(
                        CaptureAction.CAPTURE_CANCELLED,
                        new ActivityResult(Activity.RESULT_CANCELED, null));
            }
        }
    }

    /** Returns whether a prompt launched from this window is still awaiting the user's choice. */
    public boolean hasPendingRequest() {
        return sNextDelegates.get(mTaskId) != null;
    }

    public void startAndroidCapturePrompt(Delegate delegate, @Nullable Intent intent) {
        if (hasPendingRequest()) {
            // Callers are expected to check hasPendingRequest() first; cancel the new request
            // rather than overwriting the one that owns the prompt.
            delegate.onPicked(
                    CaptureAction.CAPTURE_CANCELLED,
                    new ActivityResult(Activity.RESULT_CANCELED, null));
            return;
        }
        sNextDelegates.put(mTaskId, delegate);
        mLauncher.launch(
                intent != null ? intent : mMediaProjectionManager.createScreenCaptureIntent());
    }
}
