// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.installedapp;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Resources;
import android.util.Pair;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.installedapp.mojom.InstalledAppProvider;
import org.chromium.installedapp.mojom.RelatedApplication;
import org.chromium.mojo.system.MojoException;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;

/**
 * Android implementation of the InstalledAppProvider service defined in
 * installed_app_provider.mojom
 */
public class InstalledAppProviderImpl implements InstalledAppProvider {
    @VisibleForTesting
    public static final String ASSET_STATEMENTS_KEY = "asset_statements";
    private static final String ASSET_STATEMENT_FIELD_TARGET = "target";
    private static final String ASSET_STATEMENT_FIELD_NAMESPACE = "namespace";
    private static final String ASSET_STATEMENT_FIELD_SITE = "site";
    @VisibleForTesting
    public static final String ASSET_STATEMENT_NAMESPACE_WEB = "web";
    @VisibleForTesting
    public static final String RELATED_APP_PLATFORM_ANDROID = "play";
    @VisibleForTesting
    public static final String INSTANT_APP_ID_STRING = "instantapp";
    @VisibleForTesting
    public static final String INSTANT_APP_HOLDBACK_ID_STRING = "instantapp:holdback";

    private static final String TAG = "InstalledAppProvider";

    private final FrameUrlDelegate mFrameUrlDelegate;
    private final Context mContext;
    private final InstantAppsHandler mInstantAppsHandler;

    /**
     * Small interface for dynamically getting the URL of the current frame.
     *
     * Abstract to allow for testing.
     */
    public static interface FrameUrlDelegate {
        /**
         * Gets the URL of the current frame. Can return null (if the frame has disappeared).
         */
        public URI getUrl();

        /**
         * Checks if we're in incognito. If the frame has disappeared this returns true.
         */
        public boolean isIncognito();
    }

    public InstalledAppProviderImpl(FrameUrlDelegate frameUrlDelegate, Context context,
            InstantAppsHandler instantAppsHandler) {
        assert instantAppsHandler != null;
        mFrameUrlDelegate = frameUrlDelegate;
        mContext = context;
        mInstantAppsHandler = instantAppsHandler;
    }

    @Override
    public void filterInstalledApps(
            final RelatedApplication[] relatedApps, final FilterInstalledAppsResponse callback) {
        if (mFrameUrlDelegate.isIncognito()) {
            callback.call(new RelatedApplication[0]);
            return;
        }

        final URI frameUrl = mFrameUrlDelegate.getUrl();

        // Use an AsyncTask to execute the installed/related checks on a background thread (so as
        // not to block the UI thread).
        new AsyncTask<Pair<RelatedApplication[], Integer>>() {
            @Override
            protected Pair<RelatedApplication[], Integer> doInBackground() {
                return filterInstalledAppsOnBackgroundThread(relatedApps, frameUrl);
            }

            @Override
            protected void onPostExecute(Pair<RelatedApplication[], Integer> result) {
                final RelatedApplication[] installedApps = result.first;
                int delayMillis = result.second;
                // Before calling the callback, delay for the amount of time that has been
                // calculated in |delayMillis|.
                delayThenRun(new Runnable() {
                    @Override
                    public void run() {
                        callback.call(installedApps);
                    }
                }, delayMillis);
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}

    /**
     * Filters a list of apps, returning those that are both installed and match the origin.
     *
     * @param relatedApps A list of applications to be filtered.
     * @param frameUrl The URL of the frame this operation was called from.
     * @return Pair of: A subsequence of applications that meet the criteria, and, the total amount
     *         of time in ms that should be delayed before returning to the user, to mask the
     *         installed state of the requested apps.
     */
    private Pair<RelatedApplication[], Integer> filterInstalledAppsOnBackgroundThread(
            RelatedApplication[] relatedApps, URI frameUrl) {
        ThreadUtils.assertOnBackgroundThread();

        ArrayList<RelatedApplication> installedApps = new ArrayList<RelatedApplication>();
        int delayMillis = 0;
        PackageManager pm = mContext.getPackageManager();
        for (RelatedApplication app : relatedApps) {
            // If the package is of type "play", it is installed, and the origin is associated with
            // package, add the package to the list of valid packages.
            // NOTE: For security, it must not be possible to distinguish (from the response)
            // between the app not being installed and the origin not being associated with the app
            // (otherwise, arbitrary websites would be able to test whether un-associated apps are
            // installed on the user's device).
            if (app.platform.equals(RELATED_APP_PLATFORM_ANDROID) && app.id != null) {
                if (isInstantAppId(app.id)) {
                    if (mInstantAppsHandler.isInstantAppAvailable(frameUrl.toString(),
                                INSTANT_APP_HOLDBACK_ID_STRING.equals(app.id),
                                true /* includeUserPrefersBrowser */)) {
                        installedApps.add(app);
                    }
                    continue;
                }

                delayMillis += calculateDelayForPackageMs(app.id);
                if (isAppInstalledAndAssociatedWithOrigin(app.id, frameUrl, pm)) {
                    installedApps.add(app);
                }
            }
        }

        RelatedApplication[] installedAppsArray = new RelatedApplication[installedApps.size()];
        installedApps.toArray(installedAppsArray);
        return Pair.create(installedAppsArray, delayMillis);
    }

    /**
     * Returns whether or not the app ID is for an instant app/instant app holdback.
     */
    private boolean isInstantAppId(String appId) {
        return INSTANT_APP_ID_STRING.equals(appId) || INSTANT_APP_HOLDBACK_ID_STRING.equals(appId);
    }

    /**
     * Determines how long to artifically delay for, for a particular package name.
     */
    private int calculateDelayForPackageMs(String packageName) {
        // Important timing-attack prevention measure: delay by a pseudo-random amount of time, to
        // add significant noise to the time taken to check whether this app is installed and
        // related. Otherwise, it would be possible to tell whether a non-related app is installed,
        // based on the time this operation takes.
        //
        // Generate a 16-bit hash based on a unique device ID + the package name.
        short hash = PackageHash.hashForPackage(packageName);

        // The time delay is the low 10 bits of the hash in 100ths of a ms (between 0 and 10ms).
        int delayHundredthsOfMs = hash & 0x3ff;
        return delayHundredthsOfMs / 100;
    }

    /**
     * Determines whether a particular app is installed and matches the origin.
     *
     * @param packageName Name of the Android package to check if installed. Returns false if the
     *                    app is not installed.
     * @param frameUrl Returns false if the Android package does not declare association with the
     *                origin of this URL. Can be null.
     */
    public static boolean isAppInstalledAndAssociatedWithOrigin(
            String packageName, URI frameUrl, PackageManager pm) {
        // TODO(yusufo): Move this to a better/shared location before crbug.com/749876 is closed.

        ThreadUtils.assertOnBackgroundThread();

        if (frameUrl == null) return false;

        // Early-exit if the Android app is not installed.
        JSONArray statements;
        try {
            statements = getAssetStatements(packageName, pm);
        } catch (NameNotFoundException e) {
            return false;
        }

        // The installed Android app has provided us with a list of asset statements. If any one of
        // those statements is a web asset that matches the given origin, return true.
        for (int i = 0; i < statements.length(); i++) {
            JSONObject statement;
            try {
                statement = statements.getJSONObject(i);
            } catch (JSONException e) {
                // If an element is not an object, just ignore it.
                continue;
            }

            URI site = getSiteForWebAsset(statement);

            // The URI is considered equivalent if the scheme, host, and port match, according
            // to the DigitalAssetLinks v1 spec.
            if (site != null && statementTargetMatches(frameUrl, site)) {
                return true;
            }
        }

        // No asset matched the origin.
        return false;
    }

    /**
     * Gets the asset statements from an Android app's manifest.
     *
     * This retrieves the list of statements from the Android app's "asset_statements" manifest
     * resource, as specified in Digital Asset Links v1.
     *
     * @param packageName Name of the Android package to get statements from.
     * @return The list of asset statements, parsed from JSON.
     * @throws NameNotFoundException if the application is not installed.
     */
    private static JSONArray getAssetStatements(String packageName, PackageManager pm)
            throws NameNotFoundException {
        // Get the <meta-data> from this app's manifest.
        // Throws NameNotFoundException if the application is not installed.
        ApplicationInfo appInfo = pm.getApplicationInfo(packageName, PackageManager.GET_META_DATA);
        if (appInfo == null || appInfo.metaData == null) {
            return new JSONArray();
        }

        int identifier = appInfo.metaData.getInt(ASSET_STATEMENTS_KEY);
        if (identifier == 0) {
            return new JSONArray();
        }

        // Throws NameNotFoundException in the rare case that the application was uninstalled since
        // getting |appInfo| (or resources could not be loaded for some other reason).
        Resources resources = pm.getResourcesForApplication(appInfo);

        String statements;
        try {
            statements = resources.getString(identifier);
        } catch (Resources.NotFoundException e) {
            // This should never happen, but it could if there was a broken APK, so handle it
            // gracefully without crashing.
            Log.w(TAG,
                    "Android package " + packageName + " missing asset statements resource (0x"
                            + Integer.toHexString(identifier) + ").");
            return new JSONArray();
        }

        try {
            return new JSONArray(statements);
        } catch (JSONException e) {
            // If the JSON is invalid or not an array, assume it is empty.
            Log.w(TAG,
                    "Android package " + packageName
                            + " has JSON syntax error in asset statements resource (0x"
                            + Integer.toHexString(identifier) + ").");
            return new JSONArray();
        }
    }

    /**
     * Gets the "site" URI from an Android asset statement.
     *
     * @return The site, or null if the asset string was invalid or not related to a web site. This
     *         could be because: the JSON string was invalid, there was no "target" field, this was
     *         not a web asset, there was no "site" field, or the "site" field was invalid.
     */
    private static URI getSiteForWebAsset(JSONObject statement) {
        JSONObject target;
        try {
            // Ignore the "relation" field and allow an asset with any relation to this origin.
            // TODO(mgiuca): [Spec issue] Should we require a specific relation string, rather
            // than any or no relation?
            target = statement.getJSONObject(ASSET_STATEMENT_FIELD_TARGET);
        } catch (JSONException e) {
            return null;
        }

        // If it is not a web asset, skip it.
        if (!isAssetWeb(target)) {
            return null;
        }

        try {
            return new URI(target.getString(ASSET_STATEMENT_FIELD_SITE));
        } catch (JSONException | URISyntaxException e) {
            return null;
        }
    }

    /**
     * Determines whether an Android asset statement is for a website.
     *
     * @param target The "target" field of the asset statement.
     */
    private static boolean isAssetWeb(JSONObject target) {
        String namespace;
        try {
            namespace = target.getString(ASSET_STATEMENT_FIELD_NAMESPACE);
        } catch (JSONException e) {
            return false;
        }

        return namespace.equals(ASSET_STATEMENT_NAMESPACE_WEB);
    }

    private static boolean statementTargetMatches(URI frameUrl, URI assetUrl) {
        if (assetUrl.getScheme() == null || assetUrl.getAuthority() == null) {
            return false;
        }

        return assetUrl.getScheme().equals(frameUrl.getScheme())
                && assetUrl.getAuthority().equals(frameUrl.getAuthority());
    }

    /**
     * Runs a Runnable task after a given delay.
     *
     * Protected and non-static for testing.
     *
     * @param r The Runnable that will be executed.
     * @param delayMillis The delay (in ms) until the Runnable will be executed.
     * @return True if the Runnable was successfully placed into the message queue.
     */
    protected void delayThenRun(Runnable r, long delayMillis) {
        ThreadUtils.postOnUiThreadDelayed(r, delayMillis);
    }
}
