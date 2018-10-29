// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.os.BadParcelableException;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.TransactionTooLargeException;
import android.support.annotation.Nullable;
import android.support.v4.app.BundleCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;

import java.io.Serializable;
import java.util.ArrayList;

/**
 * Utilities dealing with extracting information from intents.
 */
public class IntentUtils {
    private static final String TAG = "IntentUtils";

    /** See {@link #isIntentTooLarge(Intent)}. */
    private static final int MAX_INTENT_SIZE_THRESHOLD = 750000;

    /**
     * Just like {@link Intent#hasExtra(String)} but doesn't throw exceptions.
     */
    public static boolean safeHasExtra(Intent intent, String name) {
        try {
            return intent.hasExtra(name);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "hasExtra failed on intent " + intent);
            return false;
        }
    }

    /**
     * Just like {@link Intent#removeExtra(String)} but doesn't throw exceptions.
     */
    public static void safeRemoveExtra(Intent intent, String name) {
        try {
            intent.removeExtra(name);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "removeExtra failed on intent " + intent);
        }
    }

    /**
     * Just like {@link Intent#getBooleanExtra(String, boolean)} but doesn't throw exceptions.
     */
    public static boolean safeGetBooleanExtra(Intent intent, String name, boolean defaultValue) {
        try {
            return intent.getBooleanExtra(name, defaultValue);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getBooleanExtra failed on intent " + intent);
            return defaultValue;
        }
    }

    /**
     * Just like {@link Bundle#getBoolean(String, boolean)} but doesn't throw exceptions.
     */
    public static boolean safeGetBoolean(Bundle bundle, String name, boolean defaultValue) {
        try {
            return bundle.getBoolean(name, defaultValue);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getBoolean failed on bundle " + bundle);
            return defaultValue;
        }
    }

    /**
     * Just like {@link Intent#getIntExtra(String, int)} but doesn't throw exceptions.
     */
    public static int safeGetIntExtra(Intent intent, String name, int defaultValue) {
        try {
            return intent.getIntExtra(name, defaultValue);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getIntExtra failed on intent " + intent);
            return defaultValue;
        }
    }

    /**
     * Just like {@link Bundle#getInt(String, int)} but doesn't throw exceptions.
     */
    public static int safeGetInt(Bundle bundle, String name, int defaultValue) {
        try {
            return bundle.getInt(name, defaultValue);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getInt failed on bundle " + bundle);
            return defaultValue;
        }
    }

    /**
     * Just like {@link Intent#getIntArrayExtra(String)} but doesn't throw exceptions.
     */
    public static int[] safeGetIntArrayExtra(Intent intent, String name) {
        try {
            return intent.getIntArrayExtra(name);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getIntArrayExtra failed on intent " + intent);
            return null;
        }
    }

    /**
     * Just like {@link Bundle#getIntArray(String)} but doesn't throw exceptions.
     */
    public static int[] safeGetIntArray(Bundle bundle, String name) {
        try {
            return bundle.getIntArray(name);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getIntArray failed on bundle " + bundle);
            return null;
        }
    }

    /**
     * Just like {@link Intent#getLongExtra(String, long)} but doesn't throw exceptions.
     */
    public static long safeGetLongExtra(Intent intent, String name, long defaultValue) {
        try {
            return intent.getLongExtra(name, defaultValue);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getLongExtra failed on intent " + intent);
            return defaultValue;
        }
    }

    /**
     * Just like {@link Intent#getStringExtra(String)} but doesn't throw exceptions.
     */
    public static String safeGetStringExtra(Intent intent, String name) {
        try {
            return intent.getStringExtra(name);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getStringExtra failed on intent " + intent);
            return null;
        }
    }

    /**
     * Just like {@link Bundle#getString(String)} but doesn't throw exceptions.
     */
    public static String safeGetString(Bundle bundle, String name) {
        try {
            return bundle.getString(name);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getString failed on bundle " + bundle);
            return null;
        }
    }

    /**
     * Just like {@link Intent#getBundleExtra(String)} but doesn't throw exceptions.
     */
    public static Bundle safeGetBundleExtra(Intent intent, String name) {
        try {
            return intent.getBundleExtra(name);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getBundleExtra failed on intent " + intent);
            return null;
        }
    }

    /**
     * Just like {@link Bundle#getBundle(String)} but doesn't throw exceptions.
     */
    public static Bundle safeGetBundle(Bundle bundle, String name) {
        try {
            return bundle.getBundle(name);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getBundle failed on bundle " + bundle);
            return null;
        }
    }

    /**
     * Just like {@link Bundle#getParcelable(String)} but doesn't throw exceptions.
     */
    public static <T extends Parcelable> T safeGetParcelable(Bundle bundle, String name) {
        try {
            return bundle.getParcelable(name);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getParcelable failed on bundle " + bundle);
            return null;
        }
    }

    /**
     * Just like {@link Intent#getParcelableExtra(String)} but doesn't throw exceptions.
     */
    public static <T extends Parcelable> T safeGetParcelableExtra(Intent intent, String name) {
        try {
            return intent.getParcelableExtra(name);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getParcelableExtra failed on intent " + intent);
            return null;
        }
    }

    /**
     * Just link {@link Intent#getParcelableArrayListExtra(String)} but doesn't throw exceptions.
     */
    public static <T extends Parcelable> ArrayList<T> getParcelableArrayListExtra(
            Intent intent, String name) {
        try {
            return intent.getParcelableArrayListExtra(name);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getParcelableArrayListExtra failed on intent " + intent);
            return null;
        }
    }

    /**
     * Just link {@link Bundle#getParcelableArrayList(String)} but doesn't throw exceptions.
     */
    public static <T extends Parcelable> ArrayList<T> safeGetParcelableArrayList(
            Bundle bundle, String name) {
        try {
            return bundle.getParcelableArrayList(name);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getParcelableArrayList failed on bundle " + bundle);
            return null;
        }
    }

    /**
     * Just like {@link Intent#getParcelableArrayExtra(String)} but doesn't throw exceptions.
     */
    public static Parcelable[] safeGetParcelableArrayExtra(Intent intent, String name) {
        try {
            return intent.getParcelableArrayExtra(name);
        } catch (Throwable t) {
            Log.e(TAG, "getParcelableArrayExtra failed on intent " + intent);
            return null;
        }
    }

    /**
     * Just like {@link Intent#getStringArrayListExtra(String)} but doesn't throw exceptions.
     */
    public static ArrayList<String> safeGetStringArrayListExtra(Intent intent, String name) {
        try {
            return intent.getStringArrayListExtra(name);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getStringArrayListExtra failed on intent " + intent);
            return null;
        }
    }

    /**
     * Just like {@link Intent#getByteArrayExtra(String)} but doesn't throw exceptions.
     */
    public static byte[] safeGetByteArrayExtra(Intent intent, String name) {
        try {
            return intent.getByteArrayExtra(name);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getByteArrayExtra failed on intent " + intent);
            return null;
        }
    }

    /**
     * Just like {@link Intent#getSerializableExtra(String)} but doesn't throw exceptions.
     */
    @SuppressWarnings("unchecked")
    public static <T extends Serializable> T safeGetSerializableExtra(Intent intent, String name) {
        try {
            return (T) intent.getSerializableExtra(name);
        } catch (ClassCastException ex) {
            Log.e(TAG, "Invalide class for Serializable: " + name, ex);
            return null;
        } catch (Throwable t) {
            // Catches un-serializable exceptions.
            Log.e(TAG, "getSerializableExtra failed on intent " + intent);
            return null;
        }
    }

    /**
     * Just like {@link BundleCompat#getBinder()}, but doesn't throw exceptions.
     */
    public static IBinder safeGetBinder(Bundle bundle, String name) {
        if (bundle == null) return null;
        try {
            return BundleCompat.getBinder(bundle, name);
        } catch (Throwable t) {
            // Catches un-parceling exceptions.
            Log.e(TAG, "getBinder failed on bundle " + bundle);
            return null;
        }
    }

    /**
     * @return a Binder from an Intent, or null.
     *
     * Creates a temporary copy of the extra Bundle, which is required as
     * Intent#getBinderExtra() doesn't exist, but Bundle.getBinder() does.
     */
    @VisibleForTesting
    public static IBinder safeGetBinderExtra(Intent intent, String name) {
        if (!intent.hasExtra(name)) return null;
        Bundle extras = intent.getExtras();
        return safeGetBinder(extras, name);
    }

    /**
     * Inserts a {@link Binder} value into an Intent as an extra.
     *
     * Uses {@link BundleCompat#putBinder()}, but doesn't throw exceptions.
     *
     * @param intent Intent to put the binder into.
     * @param name Key.
     * @param binder Binder object.
     */
    @VisibleForTesting
    public static void safePutBinderExtra(Intent intent, String name, IBinder binder) {
        if (intent == null) return;
        Bundle bundle = new Bundle();
        try {
            BundleCompat.putBinder(bundle, name, binder);
        } catch (Throwable t) {
            // Catches parceling exceptions.
            Log.e(TAG, "putBinder failed on bundle " + bundle);
        }
        intent.putExtras(bundle);
    }

    /** See {@link #safeStartActivity(Context, Intent, Bundle)}. */
    public static boolean safeStartActivity(Context context, Intent intent) {
        return safeStartActivity(context, intent, null);
    }

    /**
     * Catches any failures to start an Activity.
     * @param context Context to use when starting the Activity.
     * @param intent  Intent to fire.
     * @param bundle  Bundle of launch options.
     * @return Whether or not Android accepted the Intent.
     */
    public static boolean safeStartActivity(
            Context context, Intent intent, @Nullable Bundle bundle) {
        try {
            context.startActivity(intent, bundle);
            return true;
        } catch (ActivityNotFoundException e) {
            return false;
        }
    }

    /** Returns whether the intent starts an activity in a new task or a new document. */
    public static boolean isIntentForNewTaskOrNewDocument(Intent intent) {
        int testFlags =
                Intent.FLAG_ACTIVITY_NEW_TASK | ApiCompatibilityUtils.getActivityNewDocumentFlag();
        return (intent.getFlags() & testFlags) != 0;
    }

    /**
     * Returns how large the Intent will be in Parcel form, which is helpful for gauging whether
     * Android will deliver the Intent instead of throwing a TransactionTooLargeException.
     *
     * @param intent Intent to get the size of.
     * @return Number of bytes required to parcel the Intent.
     */
    public static int getParceledIntentSize(Intent intent) {
        Parcel parcel = Parcel.obtain();
        intent.writeToParcel(parcel, 0);
        return parcel.dataSize();
    }

    /**
     * Determines if an Intent's size is bigger than a reasonable threshold.  Having too many large
     * transactions in flight simultaneously (including Intents) causes Android to throw a
     * {@link TransactionTooLargeException}.  According to that class, the limit across all
     * transactions combined is one megabyte.  Best practice is to keep each individual Intent well
     * under the limit to avoid this situation.
     */
    public static boolean isIntentTooLarge(Intent intent) {
        return getParceledIntentSize(intent) > MAX_INTENT_SIZE_THRESHOLD;
    }

    /**
     * Given an exception, check whether it wrapped a {@link TransactionTooLargeException}.  If it
     * does, then log the underlying error.  If not, throw the original exception again.
     *
     * @param e      The caught RuntimeException.
     * @param intent The intent that triggered the RuntimeException to be thrown.
     */
    public static void logTransactionTooLargeOrRethrow(RuntimeException e, Intent intent) {
        // See http://crbug.com/369574.
        if (e.getCause() instanceof TransactionTooLargeException) {
            Log.e(TAG, "Could not resolve Activity for intent " + intent.toString(), e);
        } else {
            throw e;
        }
    }

    private static Intent logInvalidIntent(Intent intent, Exception e) {
        Log.e(TAG, "Invalid incoming intent.", e);
        return intent.replaceExtras((Bundle) null);
    }

    /**
     * Sanitizes an intent. In case the intent cannot be unparcelled, all extras will be removed to
     * make it safe to use.
     * @return A safe to use version of this intent.
     */
    public static Intent sanitizeIntent(final Intent incomingIntent) {
        if (incomingIntent == null) return null;
        try {
            incomingIntent.getBooleanExtra("TriggerUnparcel", false);
            return incomingIntent;
        } catch (BadParcelableException e) {
            return logInvalidIntent(incomingIntent, e);
        } catch (RuntimeException e) {
            if (e.getCause() instanceof ClassNotFoundException) {
                return logInvalidIntent(incomingIntent, e);
            }
            throw e;
        }
    }
}
