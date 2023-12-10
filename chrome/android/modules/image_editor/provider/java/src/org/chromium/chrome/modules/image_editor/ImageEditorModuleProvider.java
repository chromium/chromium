// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.image_editor;

import org.chromium.chrome.browser.image_editor.ImageEditorDialogCoordinator;
import org.chromium.components.module_installer.engine.InstallListener;

/** Interface for installing and loading the image_editor module. */
public interface ImageEditorModuleProvider {
    /** Returns true if the module is installed. */
    public boolean isModuleInstalled();

    /**
     * Requests deferred installation of the module, i.e. when on unmetered network connection and
     * device is charging.
     */
    public void maybeInstallModuleDeferred();

    /**
     * Attempts to install the module immediately.
     *
     * @param listener Called when the install has finished.
     */
    public void maybeInstallModule(InstallListener listener);

    /**
     * Creates and returns the instance tied to the image editor dialog.
     *
     * Can only be called if the module is installed. Maps native resources into memory on first
     * call.
     */
    public ImageEditorDialogCoordinator getImageEditorDialogCoordinator();
}
