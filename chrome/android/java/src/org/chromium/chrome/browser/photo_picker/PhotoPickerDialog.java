// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.app.Activity;
import android.content.Context;
import android.support.v7.app.AlertDialog;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.ui.PhotoPickerListener;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * UI for the photo chooser that shows on the Android platform as a result of
 * &lt;input type=file accept=image &gt; form element.
 */
public class PhotoPickerDialog extends AlertDialog {
    // Our context.
    private Context mContext;

    // The category we're showing photos for.
    private PickerCategoryView mCategoryView;

    // A wrapper around the listener object, watching to see if an external intent is launched.
    private PhotoPickerListenerWrapper mListenerWrapper;

    // Whether the wait for an external intent launch is over.
    private boolean mDoneWaitingForExternalIntent;

    /**
     * A wrapper around {@link PhotoPickerListener} that listens for external intents being
     * launched.
     */
    private static class PhotoPickerListenerWrapper implements PhotoPickerListener {
        // The {@link PhotoPickerListener} to forward the events to.
        PhotoPickerListener mListener;

        // Whether the user selected to launch an external intent.
        private boolean mExternalIntentSelected;

        /**
         * The constructor, supplying the {@link PhotoPickerListener} object to encapsulate.
         */
        public PhotoPickerListenerWrapper(PhotoPickerListener listener) {
            mListener = listener;
        }

        // PhotoPickerListener:
        @Override
        public void onPhotoPickerUserAction(@PhotoPickerAction int action, String[] photos) {
            mExternalIntentSelected = false;
            if (action == PhotoPickerAction.LAUNCH_GALLERY
                    || action == PhotoPickerAction.LAUNCH_CAMERA) {
                mExternalIntentSelected = true;
            }

            mListener.onPhotoPickerUserAction(action, photos);
        }

        /**
         * Returns whether the user picked an external intent to launch.
         */
        public boolean externalIntentSelected() {
            return mExternalIntentSelected;
        }
    }

    /**
     * The PhotoPickerDialog constructor.
     * @param context The context to use.
     * @param listener The listener object that gets notified when an action is taken.
     * @param multiSelectionAllowed Whether the photo picker should allow multiple items to be
     *                              selected.
     * @param mimeTypes A list of mime types to show in the dialog.
     */
    public PhotoPickerDialog(Context context, PhotoPickerListener listener,
            boolean multiSelectionAllowed, List<String> mimeTypes) {
        super(context, R.style.FullscreenWhite);
        mContext = context;
        mListenerWrapper = new PhotoPickerListenerWrapper(listener);

        // Initialize the main content view.
        mCategoryView = new PickerCategoryView(context, multiSelectionAllowed);
        mCategoryView.initialize(this, mListenerWrapper, mimeTypes);
        setView(mCategoryView);
    }

    @Override
    public void dismiss() {
        if (!mListenerWrapper.externalIntentSelected() || mDoneWaitingForExternalIntent) {
            super.dismiss();
            mCategoryView.onDialogDismissed();
        } else {
            ApplicationStatus.registerStateListenerForActivity(new ActivityStateListener() {
                @Override
                public void onActivityStateChange(Activity activity, int newState) {
                    // When an external intent, such as the Camera intent, is launched, this
                    // listener will first receive the PAUSED event. Normally, STOPPED is the next
                    // event, as the Camera intent appears. But if the user presses Back quickly
                    // after the PAUSED event, the STOPPED event will not arrive, and this listener
                    // gets RESUMED instead. However, we are already in teardown mode, so the
                    // safe thing to do is to close the dialog.
                    if (newState == ActivityState.STOPPED || newState == ActivityState.RESUMED) {
                        mDoneWaitingForExternalIntent = true;
                        ApplicationStatus.unregisterActivityStateListener(this);
                        dismiss();
                    }
                }
            }, WindowAndroid.activityFromContext(mContext));
        }
    }

    @VisibleForTesting
    public PickerCategoryView getCategoryViewForTesting() {
        return mCategoryView;
    }
}
