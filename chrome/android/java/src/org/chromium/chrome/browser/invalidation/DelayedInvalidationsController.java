// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import android.accounts.Account;
import android.content.ContentResolver;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.support.v4.util.ObjectsCompat;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.invalidation.PendingInvalidation;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.sync.AndroidSyncSettings;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

/**
 * A class for controlling whether an invalidation should be notified immediately, or should be
 * delayed until Chrome comes to the foreground again.
 */
public class DelayedInvalidationsController {
    private static final String TAG = "invalidation";
    private static final String DELAYED_ACCOUNT_NAME = "delayed_account";
    private static final String DELAYED_INVALIDATIONS = "delayed_invalidations";

    private static class LazyHolder {
        private static final DelayedInvalidationsController INSTANCE =
                new DelayedInvalidationsController();
    }

    public static DelayedInvalidationsController getInstance() {
        return LazyHolder.INSTANCE;
    }

    @VisibleForTesting
    DelayedInvalidationsController() {}

    /**
     * Notify any invalidations that were delayed while Chromium was backgrounded.
     * @return whether there were any invalidations pending to be notified.
     */
    public boolean notifyPendingInvalidations() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        String accountName = prefs.getString(DELAYED_ACCOUNT_NAME, null);
        if (accountName == null) {
            Log.d(TAG, "No pending invalidations.");
            return false;
        } else {
            Log.d(TAG, "Handling pending invalidations.");
            Account account = AccountManagerFacade.createAccountFromName(accountName);
            List<Bundle> bundles = popPendingInvalidations();
            notifyInvalidationsOnBackgroundThread(account, bundles);
            return true;
        }
    }

    /**
     * Calls ContentResolver.requestSync() in a separate thread as it performs some blocking
     * IO operations.
     */
    @VisibleForTesting
    void notifyInvalidationsOnBackgroundThread(final Account account, final List<Bundle> bundles) {
        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                String contractAuthority = AndroidSyncSettings.getContractAuthority();
                for (Bundle bundle : bundles) {
                    ContentResolver.requestSync(account, contractAuthority, bundle);
                }
                return null;
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Stores preferences to indicate that an invalidation has arrived, but dropped on the floor.
     */
    @VisibleForTesting
    void addPendingInvalidation(String account, PendingInvalidation invalidation) {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        String oldAccount = prefs.getString(DELAYED_ACCOUNT_NAME, null);
        // Make sure to construct a new set so it can be modified safely. See crbug.com/568369.
        Set<String> invals = new HashSet<String>(
                prefs.getStringSet(DELAYED_INVALIDATIONS, Collections.<String>emptySet()));
        assert invals.isEmpty() || oldAccount != null;
        boolean invalidateAllTypes = false;
        // We invalidate all types if:
        // - the account has changed
        // - we were in "invalidate all types" mode already
        // - new invalidation indicates to invalidate all types by setting source to 0
        // - adding invalidation to the current set failed
        if (oldAccount != null && !oldAccount.equals(account)) invalidateAllTypes = true;
        if (oldAccount != null && invals.isEmpty()) invalidateAllTypes = true;
        if (invalidation.mObjectSource == 0) invalidateAllTypes = true;
        if (!invalidateAllTypes && !addInvalidationToSet(invalidation, invals)) {
            invalidateAllTypes = true;
        }

        SharedPreferences.Editor editor = prefs.edit();
        editor.putString(DELAYED_ACCOUNT_NAME, account);
        if (invalidateAllTypes) {
            editor.remove(DELAYED_INVALIDATIONS);
        } else {
            editor.putStringSet(DELAYED_INVALIDATIONS, invals);
        }
        editor.apply();
    }

    /**
     * Adds newInvalidation into set of encoded invalidations. Invalidations with the same id/source
     * and lower version are removed from the set. If invalidation with same or higher version is
     * is present, then new invalidation is discarded.
     * @return true if update is successful, false when decoding invalidation from string fails.
     */
    private boolean addInvalidationToSet(
            PendingInvalidation newInvalidation, Set<String> invalidations) {
        for (Iterator<String> iter = invalidations.iterator(); iter.hasNext();) {
            String encodedInvalidation = iter.next();
            PendingInvalidation invalidation =
                    PendingInvalidation.decodeToPendingInvalidation(encodedInvalidation);
            if (invalidation == null) return false;
            if (ObjectsCompat.equals(invalidation.mObjectId, newInvalidation.mObjectId)
                    && invalidation.mObjectSource == newInvalidation.mObjectSource) {
                if (invalidation.mVersion >= newInvalidation.mVersion) return true;
                iter.remove();
            }
        }
        invalidations.add(newInvalidation.encodeToString());
        return true;
    }

    private List<Bundle> popPendingInvalidations() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assert prefs.contains(DELAYED_ACCOUNT_NAME);
        Set<String> savedInvalidations = prefs.getStringSet(DELAYED_INVALIDATIONS, null);
        clearPendingInvalidations();
        // Absence of specific invalidations indicates invalidate all types.
        if (savedInvalidations == null) return Arrays.asList(new Bundle());

        List<Bundle> bundles = new ArrayList<Bundle>(savedInvalidations.size());
        for (String invalidation : savedInvalidations) {
            Bundle bundle = PendingInvalidation.decodeToBundle(invalidation);
            if (bundle == null) {
                Log.e(TAG, "Error parsing saved invalidation. Invalidating all.");
                return Arrays.asList(new Bundle());
            }
            bundles.add(bundle);
        }
        return bundles;
    }

    /**
     * If there are any pending invalidations, they will be cleared.
     */
    @VisibleForTesting
    public void clearPendingInvalidations() {
        SharedPreferences.Editor editor =
                ContextUtils.getAppSharedPreferences().edit();
        editor.putString(DELAYED_ACCOUNT_NAME, null);
        editor.putStringSet(DELAYED_INVALIDATIONS, null);
        editor.apply();
    }

    @VisibleForTesting
    boolean shouldNotifyInvalidation(Bundle extras) {
        return isManualRequest(extras) || ApplicationStatus.hasVisibleActivities();
    }

    private static boolean isManualRequest(Bundle extras) {
        if (extras.getBoolean(ContentResolver.SYNC_EXTRAS_MANUAL, false)) {
            Log.d(TAG, "Manual sync requested.");
            return true;
        }
        return false;
    }
}
