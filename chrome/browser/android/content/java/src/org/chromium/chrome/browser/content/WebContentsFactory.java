// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content;

import dagger.Reusable;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

import javax.inject.Inject;

/**
 * This factory creates WebContents objects and the associated native counterpart. TODO(dtrainor):
 * Move this to the content/ layer if BrowserContext is ever supported in Java.
 */
@Reusable
public class WebContentsFactory {
    @Inject
    public WebContentsFactory() {}

    /**
     * Network handle representing the default network. To be used when a network has not been
     * explicitly set.
     */
    public static final long DEFAULT_NETWORK_HANDLE = -1;

    /** For capturing where WebContentsImpl is created. */
    private static class WebContentsCreationException extends RuntimeException {
        WebContentsCreationException() {
            super("vvv This is where WebContents was created. vvv");
        }
    }

    /**
     * A factory method to build a {@link WebContents} object with an separate and ephemeral
     * StoragePartition. This functionality is for an experiment and is tailored to that
     * experiment's use case. This WebContents is also initially hidden and does not initialize the
     * renderer.
     *
     * @param profile The profile with which the {@link WebContents} should be built.
     * @return A newly created {@link WebContents} object.
     */
    public static WebContents createWebContentsWithSeparateStoragePartitionForExperiment(
            Profile profile) {
        return WebContentsFactoryJni.get()
                .createWebContentsWithSeparateStoragePartitionForExperiment(
                        profile, new WebContentsCreationException());
    }

    /**
     * A factory method to build a {@link WebContents} object.
     *
     * @param profile The profile with which the {@link WebContents} should be built.
     * @param initiallyHidden Whether or not the {@link WebContents} should be initially hidden.
     * @param initializeRenderer Whether or not the {@link WebContents} should initialize renderer.
     * @param networkHandle bound network handle.
     * @return A newly created {@link WebContents} object.
     */
    public static WebContents createWebContents(
            Profile profile,
            boolean initiallyHidden,
            boolean initializeRenderer,
            long networkHandle) {
        return WebContentsFactoryJni.get()
                .createWebContents(
                        profile,
                        initiallyHidden,
                        initializeRenderer,
                        networkHandle,
                        new WebContentsCreationException());
    }

    /**
     * A factory method to build a {@link WebContents} object.
     *
     * @param profile The profile with which the {@link WebContents} should be built.
     * @param initiallyHidden Whether or not the {@link WebContents} should be initially hidden.
     * @param initializeRenderer Whether or not the {@link WebContents} should initialize renderer.
     * @return A newly created {@link WebContents} object.
     */
    public static WebContents createWebContents(
            Profile profile, boolean initiallyHidden, boolean initializeRenderer) {
        return createWebContents(
                profile, initiallyHidden, initializeRenderer, DEFAULT_NETWORK_HANDLE);
    }

    /**
     * A factory method to build a {@link WebContents} object.
     *
     * <p>Also creates and initializes the renderer.
     *
     * @param profile The profile to be used by the WebContents.
     * @param initiallyHidden Whether or not the {@link WebContents} should be initially hidden.
     * @param networkHandle bound network handle.
     * @return A newly created {@link WebContents} object.
     */
    public WebContents createWebContentsWithWarmRenderer(
            Profile profile, boolean initiallyHidden, long networkHandle) {
        return createWebContents(profile, initiallyHidden, true, networkHandle);
    }

    @NativeMethods
    interface Natives {
        WebContents createWebContents(
                @JniType("Profile*") Profile profile,
                boolean initiallyHidden,
                boolean initializeRenderer,
                long networkHandle,
                Throwable javaCreator);

        WebContents createWebContentsWithSeparateStoragePartitionForExperiment(
                @JniType("Profile*") Profile profile, Throwable javaCreator);
    }
}
