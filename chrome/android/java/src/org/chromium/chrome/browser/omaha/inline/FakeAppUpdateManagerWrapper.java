// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;

import androidx.annotation.IntDef;

import com.google.android.play.core.appupdate.AppUpdateInfo;
import com.google.android.play.core.appupdate.testing.FakeAppUpdateManager;
import com.google.android.play.core.install.model.AppUpdateType;
import com.google.android.play.core.tasks.Task;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;

/**
 * A wrapper of FakeAppUpdateManager meant to help automatically trigger more update scenarios.  The
 * wrapper isn't meant to be used for a full integration test, but simulating all of the possible
 * error cases is a bit easier to do here.
 */
public class FakeAppUpdateManagerWrapper extends FakeAppUpdateManager {
    private static final int RESULT_IN_APP_UPDATE_FAILED = 1;
    private static final int STEP_DELAY_MS = 5000;
    private static final int TOAST_DURATION_MS = 2000;

    /** A list of inline update end states that this class can simulate. */
    @IntDef({Type.NO_SIMULATION, Type.NONE, Type.SUCCESS, Type.FAIL_DIALOG_CANCEL,
            Type.FAIL_DIALOG_UPDATE_FAILED, Type.FAIL_DOWNLOAD, Type.FAIL_DOWNLOAD_CANCEL,
            Type.FAIL_INSTALL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        /** No simulation. */
        int NO_SIMULATION = 0;

        /** No update available. */
        int NONE = 1;

        /** The update will be successful. */
        int SUCCESS = 2;

        /** The update will fail because the user will choose cancel when the Play dialog shows. */
        int FAIL_DIALOG_CANCEL = 3;

        /** The update will fail because the dialog will fail with another unknown reason. */
        int FAIL_DIALOG_UPDATE_FAILED = 4;

        /** The update will fail because the download fails. */
        int FAIL_DOWNLOAD = 5;

        /** The update will fail because the download was cancelled.*/
        int FAIL_DOWNLOAD_CANCEL = 6;

        /** The update will fail because it failed to install. */
        int FAIL_INSTALL = 7;
    }

    @IntDef({Event.UPDATE_AVAILABLE, Event.USER_ACCEPTS_UPDATE, Event.USER_REJECTS_UPDATE,
            Event.TRIGGER_DOWNLOAD, Event.DOWNLOAD_STARTS, Event.DOWNLOAD_FAILS,
            Event.USER_CANCELS_DOWNLOAD, Event.DOWNLOAD_COMPLETES, Event.INSTALL_FAILS,
            Event.INSTALL_COMPLETES})
    @Retention(RetentionPolicy.SOURCE)
    @interface Event {
        int UPDATE_AVAILABLE = 1;
        int USER_ACCEPTS_UPDATE = 2;
        int USER_REJECTS_UPDATE = 3;
        int TRIGGER_DOWNLOAD = 4;
        int DOWNLOAD_STARTS = 5;
        int DOWNLOAD_FAILS = 6;
        int USER_CANCELS_DOWNLOAD = 7;
        int DOWNLOAD_COMPLETES = 8;
        int INSTALL_FAILS = 9;
        int INSTALL_COMPLETES = 10;
    }

    /**
     * A helper class to wrap invocations to the Google Play API.
     */
    private static class EventHandler extends Handler {
        // Need to use a weak reference to ensure the handler does not leak the outer object.
        private final WeakReference<FakeAppUpdateManagerWrapper> mWeakWrapper;

        EventHandler(FakeAppUpdateManagerWrapper wrapper) {
            super(Looper.getMainLooper());
            mWeakWrapper = new WeakReference<>(wrapper);
        }

        @Override
        public void handleMessage(Message msg) {
            execute(msg.what);
        }

        public void execute(@Event int event) {
            FakeAppUpdateManagerWrapper w = mWeakWrapper.get();
            if (w == null) return;
            switch (event) {
                case Event.UPDATE_AVAILABLE:
                    w.toast("Making app update available.");
                    w.setUpdateAvailable(10000 /* Figure out a better version? */);
                    return;
                case Event.USER_ACCEPTS_UPDATE:
                    w.toast("User accepts update.");
                    w.userAcceptsUpdate();
                    return;
                case Event.USER_REJECTS_UPDATE:
                    w.toast("User rejects update.");
                    w.userRejectsUpdate();
                    return;
                case Event.TRIGGER_DOWNLOAD:
                    w.toast("Triggering download.");
                    w.triggerDownload();
                    return;
                case Event.DOWNLOAD_STARTS:
                    w.toast("Download has started.");
                    w.downloadStarts();
                    return;
                case Event.DOWNLOAD_FAILS:
                    w.toast("Triggering download failure.");
                    w.downloadFails();
                    return;
                case Event.USER_CANCELS_DOWNLOAD:
                    w.toast("Triggering cancellation of download.");
                    w.userCancelsDownload();
                    return;
                case Event.DOWNLOAD_COMPLETES:
                    w.toast("Download completes.");
                    w.downloadCompletes();
                    return;
                case Event.INSTALL_FAILS:
                    w.toast("Triggering install failure.");
                    w.installFails();
                    return;
                case Event.INSTALL_COMPLETES:
                    w.toast("Triggering install completion.");
                    w.installCompletes();
                    return;
                default:
                    w.toast("Unknown event.");
            }
        }
    }

    private final @Type int mType;
    private final EventHandler mEventHandler;

    /**
     * @param endState at which point should the inline update flow end.
     */
    FakeAppUpdateManagerWrapper(@Type int endState) {
        super(ContextUtils.getApplicationContext());
        mType = endState;
        mEventHandler = new EventHandler(this);

        if (mType != Type.NONE) execute(Event.UPDATE_AVAILABLE);
    }

    // FakeAppUpdateManager implementation.
    @Override
    public boolean startUpdateFlowForResult(AppUpdateInfo appUpdateInfo,
            @AppUpdateType int appUpdateType, Activity activity, int requestCode) {
        toast("Starting update flow.");
        // TODO(dtrainor): Simulate exceptions being thrown or returning false from the super call.
        boolean success =
                super.startUpdateFlowForResult(appUpdateInfo, appUpdateType, activity, requestCode);
        if (!success) return false;

        assert activity instanceof ChromeActivity : "Unexpected triggering activity.";

        final int resultCode;
        if (mType == Type.FAIL_DIALOG_CANCEL) {
            resultCode = Activity.RESULT_CANCELED;
        } else if (mType == Type.FAIL_DIALOG_UPDATE_FAILED) {
            resultCode = RESULT_IN_APP_UPDATE_FAILED;
        } else {
            resultCode = Activity.RESULT_OK;
        }

        PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, () -> {
            triggerDialogResponse((ChromeActivity) activity, requestCode, resultCode);
        }, STEP_DELAY_MS);

        return true;
    }

    @Override
    public Task<Void> completeUpdate() {
        toast("Completing update.");
        Task<Void> result = super.completeUpdate();

        if (mType == Type.FAIL_INSTALL) {
            postDelayedEvent(Event.INSTALL_FAILS);
        } else {
            postDelayedEvent(Event.INSTALL_COMPLETES);
            // This doesn't actually restart Chrome in this case.
        }

        return result;
    }

    private void triggerDialogResponse(ChromeActivity activity, int requestCode, int resultCode) {
        if (resultCode == Activity.RESULT_OK) {
            execute(Event.USER_ACCEPTS_UPDATE);
        } else if (resultCode == Activity.RESULT_CANCELED) {
            execute(Event.USER_REJECTS_UPDATE);
        }

        activity.onActivityResult(requestCode, resultCode, null);
        if (resultCode == Activity.RESULT_OK) {
            postDelayedEvent(Event.TRIGGER_DOWNLOAD);
        }
    }

    private void triggerDownload() {
        execute(Event.DOWNLOAD_STARTS);

        if (mType == Type.FAIL_DOWNLOAD) {
            postDelayedEvent(Event.DOWNLOAD_FAILS);
        } else if (mType == Type.FAIL_DOWNLOAD_CANCEL) {
            postDelayedEvent(Event.USER_CANCELS_DOWNLOAD);
        } else {
            postDelayedEvent(Event.DOWNLOAD_COMPLETES);
        }
    }

    private void execute(@Event int event) {
        mEventHandler.execute(event);
    }

    private void postDelayedEvent(@Event int event) {
        mEventHandler.sendEmptyMessageDelayed(event, STEP_DELAY_MS);
    }

    private void toast(CharSequence text) {
        Toast.makeText(ContextUtils.getApplicationContext(), "Play Store Flow: " + text,
                     TOAST_DURATION_MS)
                .show();
    }
}
