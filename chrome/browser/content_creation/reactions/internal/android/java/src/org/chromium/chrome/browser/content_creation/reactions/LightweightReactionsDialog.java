// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions;

import android.app.Dialog;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.chrome.browser.content_creation.reactions.internal.R;
import org.chromium.chrome.browser.content_creation.reactions.scene.SceneCoordinator;

/**
 * Dialog for the reactions creation.
 */
public class LightweightReactionsDialog extends DialogFragment {
    interface ReactionsDialogObserver {
        void onViewCreated(View view);
    }

    private View mContentView;
    private Bitmap mScreenshot;
    private SceneCoordinator mSceneCoordinator;
    private ReactionsDialogObserver mDialogObserver;
    private int mCurrentOrientation;

    /**
     * Initialize the dialog outside of the constructor as fragments require default constructor.
     * @param screenshot A {@link Bitmap} of the screenshot of the page to set as the background.
     * @param sceneCoordinator A {@link SceneCoordinator} for coordinating with the scene.
     * @param obs A class implementing the {@link ReactionsDialogObserver} interface.
     */
    void init(Bitmap screenshot, SceneCoordinator sceneCoordinator, ReactionsDialogObserver obs) {
        mScreenshot = screenshot;
        mSceneCoordinator = sceneCoordinator;
        mDialogObserver = obs;
    }

    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        AlertDialog.Builder builder =
                new AlertDialog.Builder(getActivity(), R.style.ThemeOverlay_BrowserUI_Fullscreen);

        // There is a known issue where if the app was sent to the background and some OS settings
        // were changed (battery saver, dark / light theme, etc), then when the app comes back to
        // the foreground the dialog gets partially recreated. At that point the scene coordinator
        // is null, which is an unrecoverable state. If this happens, dismiss the dialog to avoid
        // a crash. A {@link Dialog} still needs to be returned by this method, so return an empty
        // one.
        if (mSceneCoordinator == null) {
            dismiss();
            return builder.create();
        }

        mCurrentOrientation = getResources().getConfiguration().orientation;
        mContentView = getActivity().getLayoutInflater().inflate(R.layout.reactions_dialog, null);
        setBackgroundImage();
        mSceneCoordinator.setSceneViews(mContentView.findViewById(R.id.lightweight_reactions_scene),
                mContentView.findViewById(R.id.lightweight_reactions_background));
        builder.setView(mContentView);

        if (mDialogObserver != null) {
            mDialogObserver.onViewCreated(mContentView);
        }

        return builder.create();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        if (mCurrentOrientation != newConfig.orientation) {
            mCurrentOrientation = newConfig.orientation;
            LightweightReactionsMetrics.recordOrientationChange(newConfig.orientation);
            mSceneCoordinator.handleOrientationChange();
        }
    }

    private void setBackgroundImage() {
        ImageView sceneBackground =
                mContentView.findViewById(R.id.lightweight_reactions_background);
        Drawable background = new BitmapDrawable(getResources(), mScreenshot);
        sceneBackground.setImageDrawable(background);
    }
}