// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.view.View;

/**
 * Interface that is needed to manage a VR dialogs.
 */
public interface VrDialogManager {
    /**
     * Sets the top View that is placed inside of a dialog. This View should be
     * shown on as a texture on a quad in VR.
     */
    void setDialogView(View view);

    /*
     * Close the popup Dialog in VR.
     */
    void closeVrDialog();

    /**
     * Set size of the Dialog in VR. The width and height are used to make sure
     * that events are sent to the correct location, and the dialog has the
     * correct size.
     *
     * @param width the dialog's width in pixels
     * @param height the dialog's height in pixels
     */
    void setDialogSize(int width, int height);

    /**
     * Set size of the Dialog in VR.
     * @param x the dialog x offset in pixels.
     * @param y the dialog y offset in pixels.
     */
    void setDialogLocation(int x, int y);

    /**
     * Initialize the Dialog in VR. The width and height are used to make sure
     * that events are sent to the correct location, and the dialog has the
     * correct size.
     *
     * @param width the dialog's width in pixels
     * @param height the dialog's height in pixels
     */
    void initVrDialog(int width, int height);

    /**
     * Set dialog as floating or not floating. Floting means that Dialog can change its position.
     *
     * @param floating indicates if the dialog is floating.
     */
    void setDialogFloating(boolean floating);

    /**
     * Set dismiss handler for dialog. When VrDialogManager wants to close the
     * dialog should call dismissHandler. This is used to close the dialog when
     * native wants to dismiss.
     *
     * @param dismissHandler is called when VrDialogManager wants to dismiss the
     * current dialog.
     */
    void setVrDialogDismissHandler(Runnable dismissHandler);
}