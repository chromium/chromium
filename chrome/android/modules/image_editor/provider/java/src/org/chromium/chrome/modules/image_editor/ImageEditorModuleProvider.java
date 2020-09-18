// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.image_editor;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.module_installer.engine.InstallListener;

/**
 * Installs and loads the module.
 */
public class ImageEditorModuleProvider {
    private static final Object INSTANCE_LOCK = new Object();
    private static ImageEditorModuleProvider sInstance;

    /** @return The singleton instance of {@link ImageEditorModuleProvider}. */
    public static ImageEditorModuleProvider get() {
        synchronized (INSTANCE_LOCK) {
            if (sInstance == null) {
                sInstance = new ImageEditorModuleProvider();
            }
        }
        return sInstance;
    }

    /** Sets the singleton instance of {@link ImageEditorModuleProvider} for testing. */
    @VisibleForTesting
    public static void setInstanceForTesting(ImageEditorModuleProvider provider) {
        synchronized (INSTANCE_LOCK) {
            sInstance = provider;
        }
    }

    /**
     * Returns true if the module is installed.
     */
    public boolean isModuleInstalled() {
        return ImageEditorModule.isInstalled();
    }

    /**
     * Requests deferred installation of the module, i.e. when on unmetered network connection and
     * device is charging.
     */
    public void maybeInstallModuleDeferred() {
        if (isModuleInstalled()) {
            return;
        }

        ImageEditorModule.installDeferred();
    }

    /**
     * Attempts to install the module immediately.
     *
     * @param listener Called when the install has finished.
     */
    public void maybeInstallModule(InstallListener listener) {
        if (isModuleInstalled()) {
            listener.onComplete(false);
            return;
        }

        ImageEditorModule.install(listener);
    }

    /**
     * Returns the image editor provider from inside the module.
     *
     * Can only be called if the module is installed. Maps native resources into memory on first
     * call.
     *
     * TODO(crbug/1024586): Return fallback editor?
     */
    public ImageEditorProvider getImageEditorProvider() {
        return ImageEditorModule.getImpl();
    }
}
