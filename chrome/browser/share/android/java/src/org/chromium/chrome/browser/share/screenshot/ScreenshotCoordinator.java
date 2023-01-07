// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.image_editor.ImageEditorDialogCoordinator;
import org.chromium.chrome.browser.modules.ModuleInstallUi;
import org.chromium.chrome.browser.share.BaseScreenshotCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.modules.image_editor.ImageEditorModuleProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/**
 * Handles the screenshot action in the Sharing Hub and launches the screenshot editor.
 */
public class ScreenshotCoordinator extends BaseScreenshotCoordinator {
    // Maximum number of attempts to install the DFM per session.
    @VisibleForTesting
    static final int MAX_INSTALL_ATTEMPTS = 5;
    private static int sInstallAttempts;

    private final WindowAndroid mWindowAndroid;
    private final ScreenshotShareSheetDialog mDialog;
    private final ImageEditorModuleProvider mImageEditorModuleProvider;

    /**
     * Constructs a new ScreenshotCoordinator which may launch the editor, or a fallback.
     *
     * @param activity The parent activity.
     * @param windowAndroid The {@link WindowAndroid} which contains the content to share.
     * @param shareUrl The URL associated with the screenshot.
     * @param chromeOptionShareCallback An interface to share sheet APIs.
     * @param sheetController The {@link BottomSheetController} for the current activity.
     * @param imageEditorModuleProvider An interface to install and/or instantiate the image editor.
     */
    public ScreenshotCoordinator(Activity activity, WindowAndroid windowAndroid, String shareUrl,
            ChromeOptionShareCallback chromeOptionShareCallback,
            BottomSheetController sheetController,
            ImageEditorModuleProvider imageEditorModuleProvider) {
        super(activity, shareUrl, chromeOptionShareCallback, sheetController);
        mWindowAndroid = windowAndroid;
        mDialog = new ScreenshotShareSheetDialog();
        mImageEditorModuleProvider = imageEditorModuleProvider;
    }

    /**
     * Constructor for testing.
     *
     * @param activity The parent activity.
     * @param windowAndroid The WindowAndroid which contains the content to share.
     * @param shareUrl The URL associated with the screenshot.
     * @param screenshotSource The screenshot source.
     * @param dialog The Share Sheet dialog to use as fallback.
     * @param chromeOptionShareCallback An interface to share sheet APIs.
     * @param sheetController The {@link BottomSheetController} for the current activity.
     * @param imageEditorModuleProvider An interface to install and/or instantiate the image editor.
     */
    @VisibleForTesting
    ScreenshotCoordinator(Activity activity, WindowAndroid windowAndroid, String shareUrl,
            EditorScreenshotSource screenshotSource, ScreenshotShareSheetDialog dialog,
            ChromeOptionShareCallback chromeOptionShareCallback,
            BottomSheetController sheetController,
            ImageEditorModuleProvider imageEditorModuleProvider) {
        super(activity, shareUrl, chromeOptionShareCallback, sheetController, screenshotSource);
        mWindowAndroid = windowAndroid;
        mDialog = dialog;
        mImageEditorModuleProvider = imageEditorModuleProvider;
    }

    /**
     * Opens the editor with the captured screenshot if the editor is installed. Otherwise, attempts
     * to install the DFM a set amount of times per session. If MAX_INSTALL_ATTEMPTS is reached,
     * directly opens the screenshot sharesheet instead.
     */
    @Override
    protected void handleScreenshot() {
        if (mScreenshot == null) {
            // TODO(crbug/1024586): Show error message
            return;
        }
        if (mImageEditorModuleProvider != null) {
            if (mImageEditorModuleProvider.isModuleInstalled()) {
                launchEditor();
            } else if (sInstallAttempts < MAX_INSTALL_ATTEMPTS) {
                sInstallAttempts++;
                installEditor(true, /* onSuccessRunnable= */ null);
            } else {
                launchSharesheet();
            }
        } else {
            launchSharesheet();
        }
    }

    /**
     * Launches the screenshot image editor.
     */
    private void launchEditor() {
        assert mImageEditorModuleProvider != null;
        ImageEditorDialogCoordinator editor =
                mImageEditorModuleProvider.getImageEditorDialogCoordinator();
        editor.launchEditor(
                mActivity, mScreenshot, mWindowAndroid, mShareUrl, mChromeOptionShareCallback);
        mScreenshot = null;
    }

    /**
     * Opens the screenshot sharesheet.
     */
    protected void launchSharesheet() {
        ScreenshotShareSheetDialogCoordinator shareSheet =
                new ScreenshotShareSheetDialogCoordinator(mActivity, mDialog, mScreenshot,
                        mWindowAndroid, mShareUrl, mChromeOptionShareCallback,
                        this::retryInstallEditor);
        shareSheet.showShareSheet();
        mScreenshot = null;
    }

    /**
     * Runnable friendly helper function to retry the installation after going to the fallback.
     * @param onSuccess Runnable to run on success.
     */
    protected void retryInstallEditor(Runnable onSuccess) {
        if (mImageEditorModuleProvider == null) {
            // If the module does not exist, nothing to do.
            return;
        }
        if (mImageEditorModuleProvider.isModuleInstalled()) {
            launchEditor();
            return;
        }
        installEditor(false, onSuccess);
    }

    /**
     * Installs the DFM and shows UI (i.e. toasts and a retry dialog) informing the
     * user of the installation status.
     * @param showFallback The fallback will be shown on a unsuccessful installation.
     * @param onSuccessRunnable the runnable to run on a successful install.
     */
    private void installEditor(boolean showFallback, Runnable onSuccessRunnable) {
        assert mImageEditorModuleProvider != null;
        ModuleInstallUi.Delegate moduleInstallUiDelegate = new ModuleInstallUi.Delegate() {
            @Override
            public WindowAndroid getWindowAndroid() {
                return mWindowAndroid;
            }

            @Override
            public Context getContext() {
                return mWindowAndroid != null ? mWindowAndroid.getActivity().get() : null;
            }
        };
        final ModuleInstallUi ui = new ModuleInstallUi(moduleInstallUiDelegate,
                R.string.image_editor_module_title, new ModuleInstallUi.FailureUiListener() {
                    @Override
                    public void onFailureUiResponse(boolean retry) {
                        if (retry) {
                            // User initiated retries are not counted toward the maximum number
                            // of install attempts per session.
                            installEditor(showFallback, onSuccessRunnable);
                        } else if (showFallback) {
                            launchSharesheet();
                        }
                    }
                });
        ui.showInstallStartUi();

        mImageEditorModuleProvider.maybeInstallModule((success) -> {
            if (success) {
                ui.showInstallSuccessUi();
                if (onSuccessRunnable != null) {
                    onSuccessRunnable.run();
                }
                launchEditor();
            } else if (showFallback) {
                launchSharesheet();
            }
        });
    }
}
