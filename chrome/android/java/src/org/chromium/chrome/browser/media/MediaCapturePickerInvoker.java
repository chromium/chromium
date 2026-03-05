// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;

import androidx.activity.result.ActivityResult;
import androidx.fragment.app.FragmentActivity;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.media.MediaCapturePickerHeadlessFragment.CaptureAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLoadIfNeededCaller;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.media.capture.ScreenCapture;

/** Stub for the new media capture picker UI. */
@NullMarked
public class MediaCapturePickerInvoker {
    private static final String TAG = "MediaCapture";

    /**
     * Shows the new media capture picker UI.
     *
     * @param context The {@link Context} to use.
     * @param params The parameters for showing the media capture picker.
     * @param delegate The delegate to notify of the user's choice.
     */
    public static void show(
            Context context,
            MediaCapturePickerManager.Params params,
            MediaCapturePickerManager.Delegate delegate) {
        MediaCapturePickerDelegate impl =
                ServiceLoaderUtil.maybeCreate(MediaCapturePickerDelegate.class);
        if (impl != null) {
            android.content.Intent intent =
                    impl.createScreenCaptureIntent(context, params, delegate);
            if (intent != null) {
                Activity activity = ContextUtils.activityFromContext(context);
                // We should always get a non-null ChromeActivity which is a FragmentActivity.
                // Crash here if this is not true for investigation.
                MediaCapturePickerHeadlessFragment fragment =
                        MediaCapturePickerHeadlessFragment.getInstance(
                                assumeNonNull((FragmentActivity) activity));
                Log.d(
                        TAG,
                        "PickerInvoker: Starting AndroidCapturePrompt for tab/ window/ screen"
                                + " sharing");
                fragment.startAndroidCapturePrompt(
                        (action, result) ->
                                onPickAndroidCapturePrompt(
                                        action, result, params.webContents, delegate, impl),
                        intent);
                return;
            }
            Log.e(TAG, "PickerInvoker: Failed to create ScreenCapture Intent, cancel request");
            // The delegate will record pre-show failure in createScreenCaptureIntent() before
            // returning null.
            delegate.onCancel();
            return;
        }
        Log.e(TAG, "PickerInvoker: No PickerDelegate, cancel request");
        MediaCapturePickerManager.recordPreShowFailure(
                MediaCapturePickerManager.PreShowFailure.PICKER_DELEGATE_NULL_ERROR);
        delegate.onCancel();
    }

    private static void onPickAndroidCapturePrompt(
            @CaptureAction int action,
            ActivityResult result,
            WebContents webContents,
            MediaCapturePickerManager.Delegate delegate,
            MediaCapturePickerDelegate impl) {
        Log.d(TAG, "PickerInvoker: AndroidCapturePrompt received user action %d", action);
        if (action == CaptureAction.CAPTURE_CANCELLED) {
            delegate.onCancel();
            MediaCapturePickerManager.recordResult(MediaCapturePickerManager.Result.CANCELLED);
        } else if (action == CaptureAction.CAPTURE_WINDOW) {
            Tab tab = impl.getPickedTab();
            if (tab != null) {
                // User selected from app provided contents, i.e. a tab.
                Log.d(
                        TAG,
                        "PickerInvoker: Tab %d with title '%s' was picked",
                        tab.getId(),
                        tab.getTitle());
                tab.loadIfNeeded(TabLoadIfNeededCaller.MEDIA_CAPTURE_PICKER);
                WebContents pickedTabwebContents = tab.getWebContents();
                assert pickedTabwebContents != null;
                delegate.onPickTab(pickedTabwebContents, impl.shouldShareAudio());
                MediaCapturePickerManager.recordResult(
                        MediaCapturePickerManager.Result.TAB_SELECTED);
            } else {
                // User selected a window or screen.
                Log.d(TAG, "PickerInvoker: A window or screen was picked");
                ScreenCapture.onPick(webContents, result);
                delegate.onPickWindow();
                MediaCapturePickerManager.recordResult(
                        MediaCapturePickerManager.Result.WINDOW_SELECTED);
            }
        } else if (action == CaptureAction.CAPTURE_SCREEN) {
            delegate.onPickScreen();
            MediaCapturePickerManager.recordResult(
                    MediaCapturePickerManager.Result.SCREEN_SELECTED);
        }
        impl.onFinish(webContents);
    }
}
