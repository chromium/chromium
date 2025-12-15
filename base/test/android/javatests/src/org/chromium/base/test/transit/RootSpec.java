// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;
import android.os.IBinder;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.function.Supplier;

/**
 * Specify restrictions about in which Roots to search for a View.
 *
 * <p>1. An Activity's subwindows. 2. Dialogs. 3. Other Roots.
 */
@NullMarked
public class RootSpec {
    @IntDef({
        RootType.ANY_ROOT,
        RootType.DIALOG_ROOT,
        RootType.ACTIVITY_ROOT,
        RootType.SUPPLIED_ACTIVITY_ROOT,
        RootType.ACTIVITY_OR_DIALOG_ROOT,
        RootType.SUPPLIED_ACTIVITY_OR_DIALOG_ROOT
    })
    @interface RootType {
        int ANY_ROOT = 0;
        int DIALOG_ROOT = 1;
        int ACTIVITY_ROOT = 2;
        int SUPPLIED_ACTIVITY_ROOT = 3;
        int ACTIVITY_OR_DIALOG_ROOT = 4;
        int SUPPLIED_ACTIVITY_OR_DIALOG_ROOT = 5;
    }

    private static final RootSpec ANY_ROOT_ROOT_SPEC =
            new RootSpec(
                    RootType.ANY_ROOT,
                    /* allowsFocusedDialogs= */ true,
                    /* activitySupplier= */ null);
    private static final RootSpec DIALOG_ROOT_SPEC =
            new RootSpec(
                    RootType.DIALOG_ROOT,
                    /* allowsFocusedDialogs= */ true,
                    /* activitySupplier= */ null);

    private final @RootType int mType;
    private final boolean mAllowsFocusedDialogs;
    private final @Nullable Supplier<? extends Activity> mActivitySupplier;

    /**
     * Restrict search to:
     *
     * <pre>
     * 1. An Activity's subwindows.
     * 2. Dialogs.
     * 3. Other Roots.
     * </pre>
     */
    public static RootSpec anyRoot() {
        return ANY_ROOT_ROOT_SPEC;
    }

    /**
     * Restrict search to:
     *
     * <pre>
     * 2. Dialogs.
     * </pre>
     */
    public static RootSpec dialogRoot() {
        return DIALOG_ROOT_SPEC;
    }

    /**
     * Restrict search to:
     *
     * <pre>
     * 1. An Activity's subwindows.
     * </pre>
     */
    public static RootSpec activityRoot(Activity activity) {
        return new RootSpec(
                RootType.ACTIVITY_ROOT, /* allowsFocusedDialogs= */ false, () -> activity);
    }

    /**
     * Restrict search to:
     *
     * <pre>
     * 1. An Activity's subwindows.
     * </pre>
     */
    public static RootSpec activityRoot(Supplier<? extends Activity> activitySupplier) {
        return new RootSpec(
                RootType.SUPPLIED_ACTIVITY_ROOT,
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
        return new RootSpec(
                RootType.ACTIVITY_OR_DIALOG_ROOT, /* allowsFocusedDialogs= */ true, () -> activity);
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
        return new RootSpec(
                RootType.SUPPLIED_ACTIVITY_OR_DIALOG_ROOT,
                /* allowsFocusedDialogs= */ true,
                activitySupplier);
    }

    private RootSpec(
            @RootType int rootType,
            boolean allowsFocusedDialogs,
            @Nullable Supplier<? extends Activity> activitySupplier) {
        mType = rootType;
        mAllowsFocusedDialogs = allowsFocusedDialogs;
        mActivitySupplier = activitySupplier;
    }

    @RootType
    int getType() {
        return mType;
    }

    boolean allowsFocusedDialogs() {
        return mAllowsFocusedDialogs;
    }

    boolean allowsWindowToken(IBinder applicationWindowToken) {
        if (mType == RootType.ANY_ROOT) {
            return true;
        }

        if (mActivitySupplier == null) {
            return false;
        }

        Activity activity = mActivitySupplier.get();
        assert activity != null;
        IBinder activityToken = activity.getWindow().getDecorView().getWindowToken();

        return applicationWindowToken == activityToken;
    }

    @Nullable Supplier<? extends Activity> getActivitySupplier() {
        return mActivitySupplier;
    }
}
