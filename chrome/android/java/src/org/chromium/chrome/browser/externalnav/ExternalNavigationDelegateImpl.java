// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.Manifest.permission;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;
import android.os.StrictMode;
import android.provider.Browser;
import android.provider.Telephony;
import android.text.TextUtils;
import android.view.WindowManager.BadTokenException;
import android.webkit.MimeTypeMap;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.chrome.browser.instantapps.AuthenticatedProxyActivity;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabRedirectHandler;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.util.UrlUtilitiesJni;
import org.chromium.chrome.browser.webapps.WebappActivity;
import org.chromium.chrome.browser.webapps.WebappScopePolicy;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.webapk.lib.client.WebApkValidator;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * The main implementation of the {@link ExternalNavigationDelegate}.
 */
public class ExternalNavigationDelegateImpl implements ExternalNavigationDelegate {
    private static final String PDF_VIEWER = "com.google.android.apps.docs";
    private static final String PDF_MIME = "application/pdf";
    private static final String PDF_SUFFIX = ".pdf";
    private static final String PDF_EXTENSION = "pdf";

    protected final Context mApplicationContext;
    private final Tab mTab;
    private final TabObserver mTabObserver;
    private boolean mIsTabDestroyed;

    public ExternalNavigationDelegateImpl(Tab tab) {
        mTab = tab;
        mApplicationContext = ContextUtils.getApplicationContext();
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onDestroyed(Tab tab) {
                mIsTabDestroyed = true;
            }
        };
        mTab.addObserver(mTabObserver);
    }

    /**
     * Get a {@link Context} linked to this delegate with preference to {@link Activity}.
     * The tab this delegate associates with can swap the {@link Activity} it is hosted in and
     * during the swap, there might not be an available {@link Activity}.
     * @return The activity {@link Context} if it can be reached.
     *         Application {@link Context} if not.
     */
    protected final Context getAvailableContext() {
        if (mTab.getWindowAndroid() == null) return mApplicationContext;
        Context activityContext =
                ContextUtils.activityFromContext(mTab.getWindowAndroid().getContext().get());
        if (activityContext == null) return mApplicationContext;
        return activityContext;
    }

    /**
     * If the intent is for a pdf, resolves intent handlers to find the platform pdf viewer if
     * it is available and force is for the provided |intent| so that the user doesn't need to
     * choose it from Intent picker.
     *
     * @param intent Intent to open.
     */
    public static void forcePdfViewerAsIntentHandlerIfNeeded(Intent intent) {
        if (intent == null || !isPdfIntent(intent)) return;
        resolveIntent(intent, true /* allowSelfOpen (ignored) */);
    }

    /**
     * Retrieve the best activity for the given intent. If a default activity is provided,
     * choose the default one. Otherwise, return the Intent picker if there are more than one
     * capable activities. If the intent is pdf type, return the platform pdf viewer if
     * it is available so user don't need to choose it from Intent picker.
     *
     * Note this function is slow on Android versions less than Lollipop.
     *
     * @param intent Intent to open.
     * @param allowSelfOpen Whether chrome itself is allowed to open the intent.
     * @return true if the intent can be resolved, or false otherwise.
     */
    public static boolean resolveIntent(Intent intent, boolean allowSelfOpen) {
        Context context = ContextUtils.getApplicationContext();
        ResolveInfo info = PackageManagerUtils.resolveActivity(intent, 0);
        if (info == null) return false;

        final String packageName = context.getPackageName();
        if (info.match != 0) {
            // There is a default activity for this intent, use that.
            return allowSelfOpen || !packageName.equals(info.activityInfo.packageName);
        }
        List<ResolveInfo> handlers = PackageManagerUtils.queryIntentActivities(
                intent, PackageManager.MATCH_DEFAULT_ONLY);
        if (handlers == null || handlers.isEmpty()) return false;
        boolean canSelfOpen = false;
        boolean hasPdfViewer = false;
        for (ResolveInfo resolveInfo : handlers) {
            String pName = resolveInfo.activityInfo.packageName;
            if (packageName.equals(pName)) {
                canSelfOpen = true;
            } else if (PDF_VIEWER.equals(pName)) {
                if (isPdfIntent(intent)) {
                    intent.setClassName(pName, resolveInfo.activityInfo.name);
                    Uri referrer = new Uri.Builder()
                                           .scheme(IntentHandler.ANDROID_APP_REFERRER_SCHEME)
                                           .authority(packageName)
                                           .build();
                    intent.putExtra(Intent.EXTRA_REFERRER, referrer);
                    hasPdfViewer = true;
                    break;
                }
            }
        }
        return !canSelfOpen || allowSelfOpen || hasPdfViewer;
    }

    private static boolean isPdfIntent(Intent intent) {
        if (intent == null || intent.getData() == null) return false;
        String filename = intent.getData().getLastPathSegment();
        return (filename != null && filename.endsWith(PDF_SUFFIX))
                || PDF_MIME.equals(intent.getType());
    }

    /**
     * Determines whether Chrome will be handling the given Intent.
     *
     * Note this function is slow on Android versions less than Lollipop.
     *
     * @param intent            Intent that will be fired.
     * @param matchDefaultOnly  See {@link PackageManager#MATCH_DEFAULT_ONLY}.
     * @return                  True if Chrome will definitely handle the intent, false otherwise.
     */
    public static boolean willChromeHandleIntent(Intent intent, boolean matchDefaultOnly) {
        Context context = ContextUtils.getApplicationContext();
        // Early-out if the intent targets Chrome.
        if (context.getPackageName().equals(intent.getPackage())
                || (intent.getComponent() != null
                        && context.getPackageName().equals(
                                intent.getComponent().getPackageName()))) {
            return true;
        }

        // Fall back to the more expensive querying of Android when the intent doesn't target
        // Chrome.
        ResolveInfo info = PackageManagerUtils.resolveActivity(
                intent, matchDefaultOnly ? PackageManager.MATCH_DEFAULT_ONLY : 0);
        return info != null && info.activityInfo.packageName.equals(context.getPackageName());
    }

    @Override
    public List<ResolveInfo> queryIntentActivities(Intent intent) {
        return PackageManagerUtils.queryIntentActivities(
                intent, PackageManager.GET_RESOLVED_FILTER);
    }

    @Override
    public boolean willChromeHandleIntent(Intent intent) {
        return willChromeHandleIntent(intent, false);
    }

    @Override
    public @WebappScopePolicy.NavigationDirective int applyWebappScopePolicyForUrl(String url) {
        Context context = getAvailableContext();
        if (context instanceof WebappActivity) {
            WebappActivity webappActivity = (WebappActivity) context;
            return WebappScopePolicy.applyPolicyForNavigationToUrl(
                    webappActivity.scopePolicy(), webappActivity.getWebappInfo(), url);
        }
        return WebappScopePolicy.NavigationDirective.NORMAL_BEHAVIOR;
    }

    @Override
    public int countSpecializedHandlers(List<ResolveInfo> infos) {
        return getSpecializedHandlersWithFilter(infos, null).size();
    }

    @VisibleForTesting
    public static ArrayList<String> getSpecializedHandlersWithFilter(
            List<ResolveInfo> infos, String filterPackageName) {
        ArrayList<String> result = new ArrayList<>();
        if (infos == null) {
            return result;
        }

        for (ResolveInfo info : infos) {
            if (!matchResolveInfoExceptWildCardHost(info, filterPackageName)) {
                continue;
            }

            if (info.activityInfo != null) {
                if (InstantAppsHandler.getInstance().isInstantAppResolveInfo(info)) {
                    // Don't consider the Instant Apps resolver a specialized application.
                    continue;
                }

                result.add(info.activityInfo.packageName);
            } else {
                result.add("");
            }
        }
        return result;
    }

    private static boolean matchResolveInfoExceptWildCardHost(
            ResolveInfo info, String filterPackageName) {
        IntentFilter intentFilter = info.filter;
        if (intentFilter == null) {
            // Error on the side of classifying ResolveInfo as generic.
            return false;
        }
        if (intentFilter.countDataAuthorities() == 0 && intentFilter.countDataPaths() == 0) {
            // Don't count generic handlers.
            return false;
        }
        boolean isWildCardHost = false;
        Iterator<IntentFilter.AuthorityEntry> it = intentFilter.authoritiesIterator();
        while (it != null && it.hasNext()) {
            IntentFilter.AuthorityEntry entry = it.next();
            if ("*".equals(entry.getHost())) {
                isWildCardHost = true;
                break;
            }
        }
        if (isWildCardHost) {
            return false;
        }
        if (!TextUtils.isEmpty(filterPackageName)
                && (info.activityInfo == null
                           || !info.activityInfo.packageName.equals(filterPackageName))) {
            return false;
        }
        return true;
    }

    /**
     * Check whether the given package is a specialized handler for the given intent
     *
     * @param packageName Package name to check against. Can be null or empty.
     * @param intent The intent to resolve for.
     * @return Whether the given package is a specialized handler for the given intent. If there is
     *         no package name given checks whether there is any specialized handler.
     */
    public static boolean isPackageSpecializedHandler(String packageName, Intent intent) {
        List<ResolveInfo> handlers = PackageManagerUtils.queryIntentActivities(
                intent, PackageManager.GET_RESOLVED_FILTER);
        return !getSpecializedHandlersWithFilter(handlers, packageName).isEmpty();
    }

    @Override
    public void startActivity(Intent intent, boolean proxy) {
        try {
            forcePdfViewerAsIntentHandlerIfNeeded(intent);
            if (proxy) {
                dispatchAuthenticatedIntent(intent);
            } else {
                Context context = getAvailableContext();
                if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                context.startActivity(intent);
            }
            recordExternalNavigationDispatched(intent);
        } catch (RuntimeException e) {
            IntentUtils.logTransactionTooLargeOrRethrow(e, intent);
        }
    }

    @Override
    public boolean startActivityIfNeeded(Intent intent, boolean proxy) {
        boolean activityWasLaunched;
        // Only touches disk on Kitkat. See http://crbug.com/617725 for more context.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            forcePdfViewerAsIntentHandlerIfNeeded(intent);
            if (proxy) {
                dispatchAuthenticatedIntent(intent);
                activityWasLaunched = true;
            } else {
                Context context = getAvailableContext();
                if (context instanceof Activity) {
                    activityWasLaunched = ((Activity) context).startActivityIfNeeded(intent, -1);
                } else {
                    activityWasLaunched = false;
                }
            }
            if (activityWasLaunched) recordExternalNavigationDispatched(intent);
            return activityWasLaunched;
        } catch (SecurityException e) {
            // https://crbug.com/808494: Handle the URL in Chrome if dispatching to another
            // application fails with a SecurityException. This happens due to malformed manifests
            // in another app.
            return false;
        } catch (RuntimeException e) {
            IntentUtils.logTransactionTooLargeOrRethrow(e, intent);
            return false;
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    private void recordExternalNavigationDispatched(Intent intent) {
        ArrayList<String> specializedHandlers = intent.getStringArrayListExtra(
                IntentHandler.EXTRA_EXTERNAL_NAV_PACKAGES);
        if (specializedHandlers != null && specializedHandlers.size() > 0) {
            RecordUserAction.record("MobileExternalNavigationDispatched");
        }
    }

    @Override
    public boolean startIncognitoIntent(final Intent intent, final String referrerUrl,
            final String fallbackUrl, final Tab tab, final boolean needsToCloseTab,
            final boolean proxy) {
        try {
            return startIncognitoIntentInternal(
                    intent, referrerUrl, fallbackUrl, needsToCloseTab, proxy);
        } catch (BadTokenException e) {
            return false;
        }
    }

    private boolean startIncognitoIntentInternal(final Intent intent, final String referrerUrl,
            final String fallbackUrl, final boolean needsToCloseTab, final boolean proxy) {
        if (!hasValidTab()) return false;
        Context context = mTab.getWindowAndroid().getContext().get();
        if (!(context instanceof Activity)) return false;

        Activity activity = (Activity) context;
        new UiUtils.CompatibleAlertDialogBuilder(activity, R.style.Theme_Chromium_AlertDialog)
                .setTitle(R.string.external_app_leave_incognito_warning_title)
                .setMessage(R.string.external_app_leave_incognito_warning)
                .setPositiveButton(R.string.external_app_leave_incognito_leave,
                        new OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                try {
                                    startActivity(intent, proxy);
                                    if (mTab != null && !mTab.isClosing() && mTab.isInitialized()
                                            && needsToCloseTab) {
                                        closeTab();
                                    }
                                } catch (ActivityNotFoundException e) {
                                    // The activity that we thought was going to handle the intent
                                    // no longer exists, so catch the exception and assume Chrome
                                    // can handle it.
                                    loadIntent(intent, referrerUrl, fallbackUrl, mTab,
                                            needsToCloseTab, true);
                                }
                            }
                        })
                .setNegativeButton(R.string.external_app_leave_incognito_stay,
                        new OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                loadIntent(intent, referrerUrl, fallbackUrl, mTab, needsToCloseTab,
                                        true);
                            }
                        })
                .setOnCancelListener(new OnCancelListener() {
                    @Override
                    public void onCancel(DialogInterface dialog) {
                        loadIntent(intent, referrerUrl, fallbackUrl, mTab, needsToCloseTab, true);
                    }
                })
                .show();
        return true;
    }

    @Override
    public boolean shouldRequestFileAccess(String url) {
        // If the tab is null, then do not attempt to prompt for access.
        if (!hasValidTab()) return false;

        // If the url points inside of Chromium's data directory, no permissions are necessary.
        // This is required to prevent permission prompt when uses wants to access offline pages.
        if (url.startsWith(UrlConstants.FILE_URL_PREFIX + PathUtils.getDataDirectory())) {
            return false;
        }

        return !mTab.getWindowAndroid().hasPermission(permission.READ_EXTERNAL_STORAGE)
                && mTab.getWindowAndroid().canRequestPermission(permission.READ_EXTERNAL_STORAGE);
    }

    @Override
    public void startFileIntent(
            final Intent intent, final String referrerUrl, final boolean needsToCloseTab) {
        PermissionCallback permissionCallback = new PermissionCallback() {
            @Override
            public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
                if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED
                        && hasValidTab()) {
                    loadIntent(
                            intent, referrerUrl, null, mTab, needsToCloseTab, mTab.isIncognito());
                } else {
                    // TODO(tedchoc): Show an indication to the user that the navigation failed
                    //                instead of silently dropping it on the floor.
                    if (needsToCloseTab) {
                        // If the access was not granted, then close the tab if necessary.
                        closeTab();
                    }
                }
            }
        };
        if (!hasValidTab()) return;
        mTab.getWindowAndroid().requestPermissions(
                new String[] {permission.READ_EXTERNAL_STORAGE}, permissionCallback);
    }

    private void loadIntent(Intent intent, String referrerUrl, String fallbackUrl, Tab tab,
            boolean needsToCloseTab, boolean launchIncogntio) {
        boolean needsToStartIntent = false;
        if (tab == null || tab.isClosing() || !tab.isInitialized()) {
            needsToStartIntent = true;
            needsToCloseTab = false;
        } else if (needsToCloseTab) {
            needsToStartIntent = true;
        }

        String url = fallbackUrl != null ? fallbackUrl : intent.getDataString();
        if (!UrlUtilities.isAcceptedScheme(url)) {
            if (needsToCloseTab) closeTab();
            return;
        }

        if (needsToStartIntent) {
            intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            String packageName = ContextUtils.getApplicationContext().getPackageName();
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, packageName);
            if (launchIncogntio) intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);
            intent.setClassName(packageName, ChromeLauncherActivity.class.getName());
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentHandler.addTrustedIntentExtras(intent);
            startActivity(intent, false);

            if (needsToCloseTab) closeTab();
            return;
        }

        LoadUrlParams loadUrlParams = new LoadUrlParams(url, PageTransition.AUTO_TOPLEVEL);
        if (!TextUtils.isEmpty(referrerUrl)) {
            Referrer referrer = new Referrer(referrerUrl, ReferrerPolicy.ALWAYS);
            loadUrlParams.setReferrer(referrer);
        }
        tab.loadUrl(loadUrlParams);
    }

    @Override
    public @OverrideUrlLoadingResult int clobberCurrentTab(String url, String referrerUrl) {
        int transitionType = PageTransition.LINK;
        final LoadUrlParams loadUrlParams = new LoadUrlParams(url, transitionType);
        if (!TextUtils.isEmpty(referrerUrl)) {
            Referrer referrer = new Referrer(referrerUrl, ReferrerPolicy.ALWAYS);
            loadUrlParams.setReferrer(referrer);
        }
        if (hasValidTab()) {
            // Loading URL will start a new navigation which cancels the current one
            // that this clobbering is being done for. It leads to UAF. To avoid that,
            // we're loading URL asynchronously. See https://crbug.com/732260.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                @Override
                public void run() {
                    // Tab might be closed when this is run. See https://crbug.com/662877
                    if (!mIsTabDestroyed) mTab.loadUrl(loadUrlParams);
                }
            });
            return OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB;
        } else {
            assert false : "clobberCurrentTab was called with an empty tab.";
            Uri uri = Uri.parse(url);
            Intent intent = new Intent(Intent.ACTION_VIEW, uri);
            String packageName = ContextUtils.getApplicationContext().getPackageName();
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, packageName);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);
            intent.setPackage(packageName);
            startActivity(intent, false);
            return OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT;
        }
    }

    @Override
    public boolean isChromeAppInForeground() {
        return ApplicationStatus.getStateForApplication()
                == ApplicationState.HAS_RUNNING_ACTIVITIES;
    }

    @Override
    public void maybeSetWindowId(Intent intent) {
        Context context = getAvailableContext();
        if (!(context instanceof ChromeTabbedActivity2)) return;
        intent.putExtra(IntentHandler.EXTRA_WINDOW_ID, 2);
    }

    @Override
    public String getDefaultSmsPackageName() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) return null;
        return Telephony.Sms.getDefaultSmsPackage(mApplicationContext);
    }

    private void closeTab() {
        if (!hasValidTab()) return;
        Context context = mTab.getWindowAndroid().getContext().get();
        if (context instanceof ChromeActivity) {
            ((ChromeActivity) context).getTabModelSelector().closeTab(mTab);
        }
    }

    @Override
    public boolean isPdfDownload(String url) {
        String fileExtension = MimeTypeMap.getFileExtensionFromUrl(url);
        if (TextUtils.isEmpty(fileExtension)) return false;

        return PDF_EXTENSION.equals(fileExtension);
    }

    @Override
    public void maybeRecordAppHandlersInIntent(Intent intent, List<ResolveInfo> infos) {
        intent.putExtra(IntentHandler.EXTRA_EXTERNAL_NAV_PACKAGES,
                getSpecializedHandlersWithFilter(infos, null));
    }

    @Override
    public boolean isSerpReferrer() {
        // TODO (thildebr): Investigate whether or not we can use getLastCommittedUrl() instead of
        // the NavigationController.
        if (!hasValidTab() || mTab.getWebContents() == null) return false;

        NavigationController nController = mTab.getWebContents().getNavigationController();
        int index = nController.getLastCommittedEntryIndex();
        if (index == -1) return false;

        NavigationEntry entry = nController.getEntryAtIndex(index);
        if (entry == null) return false;

        return UrlUtilitiesJni.get().isGoogleSearchUrl(entry.getUrl());
    }

    @Override
    public boolean maybeLaunchInstantApp(
            String url, String referrerUrl, boolean isIncomingRedirect) {
        if (!hasValidTab() || mTab.getWebContents() == null) return false;

        InstantAppsHandler handler = InstantAppsHandler.getInstance();
        TabRedirectHandler redirect = TabRedirectHandler.get(mTab);
        Intent intent = redirect != null ? redirect.getInitialIntent() : null;
        // TODO(mariakhomenko): consider also handling NDEF_DISCOVER action redirects.
        if (isIncomingRedirect && intent != null && Intent.ACTION_VIEW.equals(intent.getAction())) {
            // Set the URL the redirect was resolved to for checking the existence of the
            // instant app inside handleIncomingIntent().
            Intent resolvedIntent = new Intent(intent);
            resolvedIntent.setData(Uri.parse(url));
            return handler.handleIncomingIntent(getAvailableContext(), resolvedIntent,
                    LaunchIntentDispatcher.isCustomTabIntent(resolvedIntent), true);
        } else if (!isIncomingRedirect) {
            // Check if the navigation is coming from SERP and skip instant app handling.
            if (isSerpReferrer()) return false;
            return handler.handleNavigation(getAvailableContext(), url,
                    TextUtils.isEmpty(referrerUrl) ? null : Uri.parse(referrerUrl), mTab);
        }
        return false;
    }

    @Override
    public String getPreviousUrl() {
        if (mTab == null || mTab.getWebContents() == null) return null;
        return mTab.getWebContents().getLastCommittedUrl();
    }

    /**
     * Dispatches the intent through a proxy activity, so that startActivityForResult can be used
     * and the intent recipient can verify the caller.
     * @param intent The bare intent we were going to send.
     */
    protected void dispatchAuthenticatedIntent(Intent intent) {
        Intent proxyIntent = new Intent(Intent.ACTION_MAIN);
        proxyIntent.setClass(getAvailableContext(), AuthenticatedProxyActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        proxyIntent.putExtra(AuthenticatedProxyActivity.AUTHENTICATED_INTENT_EXTRA, intent);
        getAvailableContext().startActivity(proxyIntent);
    }

    /**
     * @return Whether or not we have a valid {@link Tab} available.
     */
    private boolean hasValidTab() {
        return mTab != null && !mIsTabDestroyed;
    }

    @Override
    public boolean isIntentForTrustedCallingApp(Intent intent) {
        return false;
    }

    @Override
    public boolean isValidWebApk(String packageName) {
        return WebApkValidator.isValidWebApk(ContextUtils.getApplicationContext(), packageName);
    }
}
