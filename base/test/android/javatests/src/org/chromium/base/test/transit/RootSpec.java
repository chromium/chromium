// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.os.IBinder;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.test.espresso.Root;
import androidx.test.espresso.matcher.RootMatchers;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.function.Supplier;

/**
 * Specify restrictions about in which Roots to search for a View.
 *
 * <p>1. An Activity's subwindows. 2. Dialogs. 3. Other Roots.
 */
@NullMarked
public abstract class RootSpec {
    /** Should return whether the given root matches this RootSpec. */
    public abstract boolean matches(Root root);

    /**
     * Should return non-null if the RootSpec isn't ready to match roots (for example the Activity
     * to which roots are restricted doesn't exist yet).
     */
    public @Nullable String getReasonToWaitToMatch() {
        return null;
    }

    /**
     * Should return non-null if the RootSpec will not match any Roots.
     *
     * <p>This saves iterating through windows and provides a better reason message for logging.
     */
    public @Nullable String getReasonWillNotMatch() {
        return null;
    }

    /** Restrict search to a specific Root with the given |decorView|. */
    public static RootSpec specificRoot(View decorView) {
        return new SpecificRootSpec(decorView);
    }

    /**
     * Restrict search to any root:
     *
     * <pre>
     * 1. An Activity's subwindows.
     * 2. Dialogs.
     * 3. Other Roots.
     * </pre>
     */
    public static RootSpec anyRoot() {
        return AnyRootSpec.ANY_ROOT_ROOT_SPEC;
    }

    /**
     * Restrict search to any focused root:
     *
     * <pre>
     * 1. An Activity's subwindows.
     * 2. Dialogs.
     * 3. Other Roots.
     * </pre>
     */
    public static RootSpec focusedRoot() {
        return new FocusedRootSpec();
    }

    /**
     * Restrict search to any focused root:
     *
     * <pre>
     * 2. Dialogs.
     * </pre>
     */
    public static RootSpec dialogRoot() {
        return VersatileRootSpec.DIALOG_ROOT_SPEC;
    }

    /**
     * Restrict search to:
     *
     * <pre>
     * 1. An Activity's subwindows.
     * </pre>
     */
    public static RootSpec activityRoot(Activity activity) {
        return new VersatileRootSpec(
                VersatileRootSpec.RootType.ACTIVITY_ROOT,
                /* allowsFocusedDialogs= */ false,
                () -> activity);
    }

    /**
     * Restrict search to:
     *
     * <pre>
     * 1. An Activity's subwindows.
     * </pre>
     */
    public static RootSpec activityRoot(Supplier<? extends Activity> activitySupplier) {
        return new VersatileRootSpec(
                VersatileRootSpec.RootType.SUPPLIED_ACTIVITY_ROOT,
                /* allowsFocusedDialogs= */ false,
                activitySupplier);
    }

    /**
     * Restrict search to:
     *
     * <pre>
     * 1. An Activity's subwindows.
     * 2. Dialogs.
     * </pre>
     */
    public static RootSpec activityOrDialogRoot(Activity activity) {
        return new VersatileRootSpec(
                VersatileRootSpec.RootType.ACTIVITY_OR_DIALOG_ROOT,
                /* allowsFocusedDialogs= */ true,
                () -> activity);
    }

    /**
     * Restrict search to:
     *
     * <pre>
     * 1. An Activity's subwindows.
     * 2. Dialogs.
     * </pre>
     */
    public static RootSpec activityOrDialogRoot(Supplier<? extends Activity> activitySupplier) {
        return new VersatileRootSpec(
                VersatileRootSpec.RootType.SUPPLIED_ACTIVITY_OR_DIALOG_ROOT,
                /* allowsFocusedDialogs= */ true,
                activitySupplier);
    }

    private static class AnyRootSpec extends RootSpec {
        private static final RootSpec ANY_ROOT_ROOT_SPEC = new AnyRootSpec();

        @Override
        public boolean matches(Root root) {
            return true;
        }
    }

    private static class SpecificRootSpec extends RootSpec {
        private final View mDecorView;

        public SpecificRootSpec(View decorView) {
            mDecorView = decorView;
        }

        @Override
        public boolean matches(Root root) {
            return root.getDecorView() == mDecorView;
        }
    }

    private static class VersatileRootSpec extends RootSpec {
        @IntDef({
            RootType.DIALOG_ROOT,
            RootType.ACTIVITY_ROOT,
            RootType.SUPPLIED_ACTIVITY_ROOT,
            RootType.ACTIVITY_OR_DIALOG_ROOT,
            RootType.SUPPLIED_ACTIVITY_OR_DIALOG_ROOT
        })
        @interface RootType {
            int DIALOG_ROOT = 1;
            int ACTIVITY_ROOT = 2;
            int SUPPLIED_ACTIVITY_ROOT = 3;
            int ACTIVITY_OR_DIALOG_ROOT = 4;
            int SUPPLIED_ACTIVITY_OR_DIALOG_ROOT = 5;
        }

        private static final RootSpec DIALOG_ROOT_SPEC =
                new VersatileRootSpec(
                        VersatileRootSpec.RootType.DIALOG_ROOT,
                        /* allowsFocusedDialogs= */ true,
                        /* activitySupplier= */ null);

        private final @RootType int mType;
        private final boolean mAllowsFocusedDialogs;
        private final @Nullable Supplier<? extends Activity> mActivitySupplier;

        private VersatileRootSpec(
                @RootType int rootType,
                boolean allowsFocusedDialogs,
                @Nullable Supplier<? extends Activity> activitySupplier) {
            mType = rootType;
            mAllowsFocusedDialogs = allowsFocusedDialogs;
            mActivitySupplier = activitySupplier;
        }

        @Override
        public @Nullable String getReasonToWaitToMatch() {
            if (mActivitySupplier != null && mActivitySupplier.get() == null) {
                return String.format("Waiting for Activity from %s", mActivitySupplier);
            }
            return null;
        }

        @Override
        public @Nullable String getReasonWillNotMatch() {
            if (mActivitySupplier == null) {
                return null;
            }

            Activity activity = mActivitySupplier.get();
            assumeNonNull(activity); // Since getReasonToWaitToMatch() didn't return null.

            if (activity.isDestroyed()) {
                return String.format("Activity from %s is destroyed", mActivitySupplier);
            }
            if (activity.isFinishing()) {
                return String.format("Activity from %s is finishing", mActivitySupplier);
            }
            return null;
        }

        @Override
        public boolean matches(Root root) {
            if (RootMatchers.isDialog().matches(root) && root.getDecorView().hasWindowFocus()) {
                return allowsFocusedDialogs();
            } else {
                // Subwindows of the activity.
                return allowsWindowToken(root.getDecorView().getApplicationWindowToken());
            }
        }

        private boolean allowsFocusedDialogs() {
            return mAllowsFocusedDialogs;
        }

        private boolean allowsWindowToken(IBinder applicationWindowToken) {
            if (mActivitySupplier == null) {
                return false;
            }

            Activity activity = mActivitySupplier.get();
            assert activity != null;
            IBinder activityToken = activity.getWindow().getDecorView().getWindowToken();

            return applicationWindowToken == activityToken;
        }
    }

    private static class FocusedRootSpec extends RootSpec {
        @Override
        public boolean matches(Root root) {
            return root.getDecorView().hasWindowFocus();
        }
    }
}
