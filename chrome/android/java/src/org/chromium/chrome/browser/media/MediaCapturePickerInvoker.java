// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;

import androidx.activity.result.ActivityResult;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.media.MediaCapturePickerHeadlessFragment.CaptureAction;
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
            android.content.Intent intent = impl.createScreenCaptureIntent(context, params);
            if (intent != null) {
                var fragment = MediaCapturePickerHeadlessFragment.getInstanceForCurrentActivity();
                if (fragment != null) {
                    fragment.startAndroidCapturePrompt(
                            (action, result) ->
                                    onPickAndroidCapturePrompt(
                                            action, result, params.webContents, delegate, impl),
                            intent);
                    return;
                }
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
            return;
        }

        ScreenCapture.onPick(webContents, result);
        if (action == CaptureAction.CAPTURE_WINDOW) {
            WebContents tabWebContents = impl.getPickedWebContents();
            if (tabWebContents != null) {
                // User selected from app provided contents, i.e. a tab.
                delegate.onPickTab(tabWebContents, impl.shouldShareAudio());
            } else {
                // User selected a window or screen.
                delegate.onPickWindow();
            }
        } else if (action == CaptureAction.CAPTURE_SCREEN) {
            delegate.onPickScreen();
        }
    }
}
