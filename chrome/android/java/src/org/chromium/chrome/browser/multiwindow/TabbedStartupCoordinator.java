// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.SessionStartupPolicy;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;

/**
 * Coordinates startup window restoration and crash recovery lifecycle events across {@link
 * TabbedStartupWindowPolicyDelegate} and {@link TabbedCrashRecoveryDelegate}.
 */
@NullMarked
public final class TabbedStartupCoordinator {
    private TabbedStartupCoordinator() {}

    /**
     * Notifies the coordinator that a tabbed window is being created during {@link
     * Activity#onCreate()} pre-inflation startup once its window ID has been allocated. If the
     * window was launched via startup restoration or crash recovery, registers the restoration with
     * the corresponding startup delegate.
     *
     * @param windowId The ID of the created window.
     * @param source The {@link NewWindowAppSource} from the launch intent.
     */
    public static void onWindowCreated(int windowId, @NewWindowAppSource int source) {
        if (source == NewWindowAppSource.CRASH_RECOVERY
                && MultiWindowUtils.isSessionRestoreAfterCrashEnabled()) {
            TabbedCrashRecoveryDelegate.getInstance().registerRestoration(windowId);
        } else if (source == NewWindowAppSource.RELAUNCH
                && MultiWindowUtils.isNewStartupWindowPolicyEnabled()) {
            TabbedStartupWindowPolicyDelegate.getInstance().registerRestoration(windowId);
        }
    }

    /**
     * Resolves the list of startup URLs to launch based on the session startup preference, if
     * applicable for this browser process. This evaluation occurs at most once per browser process.
     *
     * @param incognito Whether the startup is in incognito mode.
     * @return The list of valid startup URLs to open, or an empty list if none apply.
     */
    public static List<String> resolveStartupUrls(boolean incognito) {
        return TabbedStartupWindowPolicyDelegate.getInstance().resolveStartupUrls(incognito);
    }

    /**
     * Initializes the startup policy delegate with native preferences once native is ready. This
     * method is idempotent and can be safely called multiple times across activity lifecycles.
     *
     * @param profile The {@link Profile} associated with the browser session.
     */
    public static void onNativeInitialized(Profile profile) {
        TabbedStartupWindowPolicyDelegate.getInstance().onNativeInitialized(profile);
    }

    /**
     * Shows a crash recovery prompt if applicable, when the {@link ModalDialogManager} for the host
     * activity is available.
     *
     * @param modalDialogManagerSupplier Supplier for {@link ModalDialogManager}.
     * @param activity The host activity where the prompt will be displayed.
     * @return {@code true} if the dialog was shown/triggered; {@code false} otherwise.
     */
    public static boolean maybeShowCrashRecoveryPrompt(
            MonotonicObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            Activity activity) {
        return TabbedCrashRecoveryDelegate.getInstance()
                .maybeShowCrashRecoveryDialog(modalDialogManagerSupplier, activity);
    }

    /**
     * Records session state on termination that determines next session startup behavior.
     *
     * @param startupPolicy The {@link SessionStartupPolicy} to write.
     */
    public static void onSessionTerminated(@SessionStartupPolicy int startupPolicy) {
        TabbedStartupWindowPolicyDelegate.getInstance()
                .maybeSaveSessionStateOnTermination(startupPolicy);
    }

    /* package */ static void onForegroundBrowserProcessInitialized() {
        if (!MultiWindowUtils.isSessionRestoreAfterCrashEnabled()) {
            return;
        }
        TabbedCrashRecoveryDelegate.getInstance().maybeDeferCrashRecovery();
    }

    /**
     * Processes startup restoration policy and crash recovery metadata for the primary cold-startup
     * tabbed window. Invoked during post-inflation startup ({@link
     * MultiInstanceOrchestrator#onInitialize}) rather than pre-inflation startup so that the host
     * window's {@code TabModelSelector} and window ID are registered in {@code TabWindowManager}
     * and host activity creation is guaranteed not to abort.
     *
     * @param activity The primary launching {@link ChromeTabbedActivity}.
     */
    /* package */ static void processStartupWindow(ChromeTabbedActivity activity) {
        // Apply any relaunch startup policy before evaluating crash recovery metadata so relaunch
        // restoration can consume recoverable window state first.
        TabbedStartupWindowPolicyDelegate.getInstance().applyPolicy(activity);
        if (!TabbedCrashRecoveryDelegate.getInstance().initializeCrashRecoveryMetadata()) {
            // Crash recovery only cleans up window state on post-crash startups, and applyPolicy()
            // only clears state for windows it restores. Clear remaining recoverable state after
            // non-crash exits that skip restoration (e.g. task swipes, OS kills, or non-restore
            // startup policies) so stale windows are not restored in future sessions.
            for (int windowId : ChromeMultiInstancePersistentStore.readAllInstanceIds()) {
                ChromeMultiInstancePersistentStore.writeIsRecoverable(
                        windowId, /* isRecoverable= */ false);
            }
        }
    }

    /* package */ static void resetStartupState() {
        TabbedStartupWindowPolicyDelegate.getInstance().resetState();
    }
}
