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
                fragment.startAndroidCapturePrompt(
                        (action, result) ->
                                onPickAndroidCapturePrompt(
                                        action, result, params.webContents, delegate, impl),
                        intent);
                return;
            }
        }
        delegate.onCancel();
    }

    private static void onPickAndroidCapturePrompt(
            @CaptureAction int action,
            ActivityResult result,
            WebContents webContents,
            MediaCapturePickerManager.Delegate delegate,
            MediaCapturePickerDelegate impl) {
        if (action == CaptureAction.CAPTURE_CANCELLED) {
            delegate.onCancel();
        } else if (action == CaptureAction.CAPTURE_WINDOW) {
            Tab tab = impl.getPickedTab();
            if (tab != null) {
                // User selected from app provided contents, i.e. a tab.
                tab.loadIfNeeded(TabLoadIfNeededCaller.MEDIA_CAPTURE_PICKER);
                WebContents pickedTabwebContents = tab.getWebContents();
                assert pickedTabwebContents != null;
                delegate.onPickTab(pickedTabwebContents, impl.shouldShareAudio());
            } else {
                // User selected a window or screen.
                ScreenCapture.onPick(webContents, result);
                delegate.onPickWindow();
            }
        } else if (action == CaptureAction.CAPTURE_SCREEN) {
            delegate.onPickScreen();
        }
        impl.onFinish(webContents);
    }
}
