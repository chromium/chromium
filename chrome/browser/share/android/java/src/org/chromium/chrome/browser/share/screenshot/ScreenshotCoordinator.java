// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.app.Activity;
import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.image_editor.ImageEditorDialogCoordinator;
import org.chromium.chrome.browser.modules.ModuleInstallUi;
import org.chromium.chrome.browser.screenshot.EditorScreenshotSource;
import org.chromium.chrome.browser.screenshot.EditorScreenshotTask;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.modules.image_editor.ImageEditorModuleProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Handles the screenshot action in the Sharing Hub and launches the screenshot editor.
 */
public class ScreenshotCoordinator {
    // Maximum number of attempts to install the DFM per session.
    @VisibleForTesting
    static final int MAX_INSTALL_ATTEMPTS = 5;
    private static int sInstallAttempts;

    private final Activity mActivity;
    private final Tab mTab;
    private final ScreenshotShareSheetDialog mDialog;
    private final ChromeOptionShareCallback mChromeOptionShareCallback;
    private final BottomSheetController mBottomSheetController;

    private EditorScreenshotSource mScreenshotSource;
    private Bitmap mScreenshot;

    /**
     * Constructs a new ScreenshotCoordinator which may launch the editor, or a fallback.
     *
     * @param activity The parent activity.
     * @param tab The Tab which contains the content to share.
     * @param screenshotSource The Source interface to use to take a screenshot.
     * @param chromeOptionShareCallback An interface to share sheet APIs.
     */
    public ScreenshotCoordinator(Activity activity, Tab tab,
            ChromeOptionShareCallback chromeOptionShareCallback,
            BottomSheetController sheetController) {
        this(activity, tab, new EditorScreenshotTask(activity, sheetController),
                new ScreenshotShareSheetDialog(), chromeOptionShareCallback, sheetController);
    }

    /**
     * Constructor for testing and inner construction.
     *
     * @param activity The parent activity.
     * @param tab The Tab which contains the content to share.
     * @param screenshotSource The Source interface to use to take a screenshot.
     * @param dialog The Share Sheet dialog to use as fallback.
     * @param chromeOptionShareCallback An interface to share sheet APIs.
     * @param imageEditorModuleProvider An interface to install and/or instantiate the image editor.
     */
    @VisibleForTesting
    public ScreenshotCoordinator(Activity activity, Tab tab,
            EditorScreenshotSource screenshotSource, ScreenshotShareSheetDialog dialog,
            ChromeOptionShareCallback chromeOptionShareCallback,
            BottomSheetController sheetController) {
        mActivity = activity;
        mTab = tab;
        mDialog = dialog;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mScreenshotSource = screenshotSource;
        mBottomSheetController = sheetController;
    }

    /**
     * Takes a screenshot of the current tab and attempts to launch the screenshot image editor.
     */
    public void captureScreenshot() {
        mScreenshotSource.capture(() -> {
            mScreenshot = mScreenshotSource.getScreenshot();
            if (mScreenshot == null) {
                // TODO(crbug/1024586): Show error message
            } else {
                handleScreenshot();
            }
        });
    }

    /**
     * Opens the editor with the captured screenshot if the editor is installed. Otherwise, attempts
     * to install the DFM a set amount of times per session. If MAX_INSTALL_ATTEMPTS is reached,
     * directly opens the screenshot sharesheet instead.
     */
    private void handleScreenshot() {
        if (ImageEditorModuleProvider.get().isModuleInstalled()) {
            launchEditor();
        } else if (sInstallAttempts < MAX_INSTALL_ATTEMPTS) {
            sInstallAttempts++;
            installEditor(true, /* onSuccessRunnable= */ null);
        } else {
            launchSharesheet();
        }
    }

    /**
     * Launches the screenshot image editor.
     */
    private void launchEditor() {
        ImageEditorDialogCoordinator editor = ImageEditorModuleProvider.get()
                                                      .getImageEditorProvider()
                                                      .getImageEditorDialogCoordinator();
        editor.launchEditor(mActivity, mScreenshot, mTab, mChromeOptionShareCallback);
        mScreenshot = null;
    }

    /**
     * Opens the screenshot sharesheet.
     */
    private void launchSharesheet() {
        ScreenshotShareSheetDialogCoordinator shareSheet =
                new ScreenshotShareSheetDialogCoordinator(mActivity, mDialog, mScreenshot, mTab,
                        mChromeOptionShareCallback, this::retryInstallEditor);
        shareSheet.showShareSheet();
        mScreenshot = null;
    }

    /**
     * Runnable friendly helper function to retry the installation after going to the fallback.
     * @param onSuccess. Runnable to run on success.
     */
    protected void retryInstallEditor(Runnable onSuccess) {
        installEditor(false, onSuccess);
    }

    /**
     * Installs the DFM and shows UI (i.e. toasts and a retry dialog) informing the
     * user of the installation status.
     * @param showFallback The fallback will be shown on a unsuccessful installation.
     * @param onSuccessRunnable the runnable to run on a succesfful install.
     */
    private void installEditor(boolean showFallback, Runnable onSuccessRunnable) {
        final ModuleInstallUi ui = new ModuleInstallUi(
                mTab, R.string.image_editor_module_title, new ModuleInstallUi.FailureUiListener() {
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

        ImageEditorModuleProvider.get().maybeInstallModule((success) -> {
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
