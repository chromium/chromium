// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;

import java.util.ArrayList;
import java.util.List;

import javax.inject.Inject;

/**
 * Requests {@link WebappDataStorage} during deferred startup. For WebAPKs only, creates
 * {@link WebappDataStorage} if the WebAPK is not registered. Runs tasks once the
 * {@link WebappDataStorage} has been fetched (and perhaps also created).
 */
@ActivityScope
public class WebappDeferredStartupWithStorageHandler {
    /** Interface for deferred startup task callbacks. */
    public interface Task {
        /**
         * Called to run task.
         * @param storage Null if there is no {@link WebappDataStorage} registered for the webapp
         *                and a new entry was not created.
         * @param didCreateStorage Whether a new {@link WebappDataStorage} entry was created.
         */
        void run(@Nullable WebappDataStorage storage, boolean didCreateStorage);
    }

    private final Activity mActivity;
    private final @Nullable String mWebappId;
    private final boolean mIsWebApk;
    private final List<Task> mDeferredWithStorageTasks = new ArrayList<>();

    @Inject
    public WebappDeferredStartupWithStorageHandler(
            Activity activity, BrowserServicesIntentDataProvider intentDataProvider) {
        mActivity = activity;
        WebappExtras webappExtras = intentDataProvider.getWebappExtras();
        mWebappId = (webappExtras != null) ? webappExtras.id : null;
        mIsWebApk = intentDataProvider.isWebApkActivity();
    }

    /** Invoked to add deferred startup task to queue. */
    public void initDeferredStartupForActivity() {
        DeferredStartupHandler.getInstance()
                .addDeferredTask(
                        () -> {
                            runDeferredTask();
                        });
    }

    public void addTask(Task task) {
        mDeferredWithStorageTasks.add(task);
    }

    public void addTaskToFront(Task task) {
        mDeferredWithStorageTasks.add(0, task);
    }

    private void runDeferredTask() {
        if (mActivity.isFinishing() || mActivity.isDestroyed()) return;

        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(mWebappId);
        if (storage != null || !mIsWebApk) {
            runTasks(storage, /* didCreateStorage= */ false);
            return;
        }

        WebappRegistry.getInstance()
                .register(
                        mWebappId,
                        new WebappRegistry.FetchWebappDataStorageCallback() {
                            @Override
                            public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
                                runTasks(storage, /* didCreateStorage= */ true);
                            }
                        });
    }

    public void runTasks(@Nullable WebappDataStorage storage, boolean didCreateStorage) {
        for (Task task : mDeferredWithStorageTasks) {
            task.run(storage, didCreateStorage);
        }
        mDeferredWithStorageTasks.clear();
    }
}
