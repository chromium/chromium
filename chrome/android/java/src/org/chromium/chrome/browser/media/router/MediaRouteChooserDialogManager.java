// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Handler;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.FragmentManager;
import android.support.v7.app.MediaRouteChooserDialog;
import android.support.v7.app.MediaRouteChooserDialogFragment;
import android.support.v7.media.MediaRouteSelector;

/**
 * Manages the dialog responsible for selecting a {@link MediaSink}.
 */
public class MediaRouteChooserDialogManager extends BaseMediaRouteDialogManager {

    private static final String DIALOG_FRAGMENT_TAG =
            "android.support.v7.mediarouter:MediaRouteChooserDialogFragment";

    public MediaRouteChooserDialogManager(
            String sourceId, MediaRouteSelector routeSelector, MediaRouteDialogDelegate delegate) {
        super(sourceId, routeSelector, delegate);
    }

    /**
     * Fragment implementation for MediaRouteChooserDialogManager.
     */
    public static class Fragment extends MediaRouteChooserDialogFragment {
        private final Handler mHandler = new Handler();
        private final SystemVisibilitySaver mVisibilitySaver = new SystemVisibilitySaver();
        private BaseMediaRouteDialogManager mManager;
        private boolean mCancelled;

        public Fragment() {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    Fragment.this.dismiss();
                }
            });
        }

        public Fragment(BaseMediaRouteDialogManager manager) {
            mManager = manager;
        }

        @Override
        public MediaRouteChooserDialog onCreateChooserDialog(
                Context context, Bundle savedInstanceState) {
            MediaRouteChooserDialog dialog =
                    super.onCreateChooserDialog(context, savedInstanceState);
            dialog.setCanceledOnTouchOutside(true);
            return dialog;
        }

        @Override
        public void onStart() {
            mVisibilitySaver.saveSystemVisibility(getActivity());
            super.onStart();
        }

        @Override
        public void onStop() {
            super.onStop();
            mVisibilitySaver.restoreSystemVisibility(getActivity());
        }

        @Override
        public void onCancel(DialogInterface dialog) {
            mCancelled = true;

            mManager.delegate().onDialogCancelled();

            super.onCancel(dialog);
        }

        @Override
        public void onDismiss(DialogInterface dialog) {
            super.onDismiss(dialog);
            if (mManager == null) return;

            mManager.mDialogFragment = null;

            if (mCancelled) return;

            MediaSink newSink =
                    MediaSink.fromRoute(mManager.androidMediaRouter().getSelectedRoute());
            mManager.delegate().onSinkSelected(mManager.sourceId(), newSink);
        }
    }

    @Override
    protected DialogFragment openDialogInternal(FragmentManager fm) {
        if (fm.findFragmentByTag(DIALOG_FRAGMENT_TAG) != null) return null;

        Fragment fragment = new Fragment(this);
        fragment.setRouteSelector(routeSelector());
        fragment.show(fm, DIALOG_FRAGMENT_TAG);
        fm.executePendingTransactions();

        return fragment;
    }
}
