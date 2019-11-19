// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.text.format.DateUtils;
import android.util.Xml;

import androidx.annotation.VisibleForTesting;

import org.xmlpull.v1.XmlSerializer;

import org.chromium.base.BuildInfo;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.identity.SettingsSecureBasedIdentificationGenerator;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.init.ProcessInitializationHandler;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.base.DeviceFormFactor;

import java.io.IOException;
import java.io.StringWriter;
import java.util.Locale;

/**
 * Generates XML requests to send to the Omaha server.
 */
public abstract class RequestGenerator {
    private static final String TAG = "RequestGenerator";

    // The Omaha specs say that new installs should use "-1".
    public static final int INSTALL_AGE_IMMEDIATELY_AFTER_INSTALLING = -1;

    private static final String SALT = "omahaSalt";
    private static final String URL_OMAHA_SERVER = "https://update.googleapis.com/service/update2";

    private final Context mApplicationContext;

    @VisibleForTesting
    public RequestGenerator(Context context) {
        mApplicationContext = context.getApplicationContext();
        UniqueIdentificationGeneratorFactory.registerGenerator(
                SettingsSecureBasedIdentificationGenerator.GENERATOR_ID,
                new SettingsSecureBasedIdentificationGenerator(getContext()), false);
    }

    /**
     * Determine how long it's been since Chrome was first installed.  Note that this may not
     * accurate for various reasons, but it shouldn't affect stats too much.
     */
    public static long installAge(
            long currentTimestamp, long installTimestamp, boolean sendInstallEvent) {
        if (sendInstallEvent) {
            return INSTALL_AGE_IMMEDIATELY_AFTER_INSTALLING;
        } else {
            return Math.max(0L, (currentTimestamp - installTimestamp) / DateUtils.DAY_IN_MILLIS);
        }
    }

    /**
     * Generates the XML for the current request. Follows the format laid out at
     * https://github.com/google/omaha/blob/master/doc/ServerProtocolV3.md
     * with some additional dummy values supplied.
     */
    public String generateXML(String sessionID, String versionName, long installAge,
            RequestData data) throws RequestFailureException {
        XmlSerializer serializer = Xml.newSerializer();
        StringWriter writer = new StringWriter();
        try {
            serializer.setOutput(writer);
            serializer.startDocument("UTF-8", true);

            // Set up <request protocol=3.0 ...>
            serializer.startTag(null, "request");
            serializer.attribute(null, "protocol", "3.0");
            serializer.attribute(null, "version", "Android-1.0.0.0");
            serializer.attribute(null, "ismachine", "1");
            serializer.attribute(null, "requestid", "{" + data.getRequestID() + "}");
            serializer.attribute(null, "sessionid", "{" + sessionID + "}");
            serializer.attribute(null, "installsource", data.getInstallSource());
            serializer.attribute(null, "userid", "{" + getDeviceID() + "}");

            // Set up <os platform="android"... />
            serializer.startTag(null, "os");
            serializer.attribute(null, "platform", "android");
            serializer.attribute(null, "version", Build.VERSION.RELEASE);
            serializer.attribute(null, "arch", "arm");
            serializer.endTag(null, "os");

            // Set up <app version="" ...>
            serializer.startTag(null, "app");
            serializer.attribute(null, "brand", getBrand());
            serializer.attribute(null, "client", getClient());
            serializer.attribute(null, "appid", getAppId());
            serializer.attribute(null, "version", versionName);
            serializer.attribute(null, "nextversion", "");
            serializer.attribute(null, "lang", getLanguage());
            serializer.attribute(null, "installage", String.valueOf(installAge));
            serializer.attribute(null, "ap", getAdditionalParameters());
            // <code>_numaccounts</code> is actually number of profiles, which is always one for
            // Chrome Android.
            serializer.attribute(null, "_numaccounts", "1");
            serializer.attribute(null, "_numgoogleaccountsondevice",
                    String.valueOf(getNumGoogleAccountsOnDevice()));
            serializer.attribute(null, "_numsignedin", String.valueOf(getNumSignedIn()));
            serializer.attribute(
                    null, "_dl_mgr_disabled", String.valueOf(getDownloadManagerState()));

            if (data.isSendInstallEvent()) {
                // Set up <event eventtype="2" eventresult="1" />
                serializer.startTag(null, "event");
                serializer.attribute(null, "eventtype", "2");
                serializer.attribute(null, "eventresult", "1");
                serializer.endTag(null, "event");
            } else {
                // Set up <updatecheck />
                serializer.startTag(null, "updatecheck");
                serializer.endTag(null, "updatecheck");

                // Set up <ping active="1" />
                serializer.startTag(null, "ping");
                serializer.attribute(null, "active", "1");
                serializer.endTag(null, "ping");
            }

            serializer.endTag(null, "app");
            serializer.endTag(null, "request");

            serializer.endDocument();
        } catch (IOException e) {
            throw new RequestFailureException("Caught an IOException creating the XML: ", e);
        } catch (IllegalArgumentException e) {
            throw new RequestFailureException(
                    "Caught an IllegalArgumentException creating the XML: ", e);
        } catch (IllegalStateException e) {
            throw new RequestFailureException(
                    "Caught an IllegalStateException creating the XML: ", e);
        }

        return writer.toString();
    }

    /**
     * Returns the application context.
     */
    protected Context getContext() {
        return mApplicationContext;
    }

    @VisibleForTesting
    public String getAppId() {
        return getLayoutIsTablet() ? getAppIdTablet() : getAppIdHandset();
    }

    /**
     * Returns the current Android language and region code (e.g. en-GB or de-DE).
     *
     * Note: the region code depends only on the language the user selected in Android settings.
     * It doesn't depend on the user's physical location.
     */
    public String getLanguage() {
        Locale locale = Locale.getDefault();
        if (locale.getCountry().isEmpty()) {
            return locale.getLanguage();
        } else {
            return locale.getLanguage() + "-" + locale.getCountry();
        }
    }

    /**
     * Sends additional info that might be useful for statistics generation,
     * including information about channel and device type.
     * This string is partially sanitized for dashboard viewing and because people randomly set
     * these strings when building their own custom Android ROMs.
     */
    public String getAdditionalParameters() {
        String applicationLabel =
                StringSanitizer.sanitize(BuildInfo.getInstance().hostPackageLabel);
        String brand = StringSanitizer.sanitize(Build.BRAND);
        String model = StringSanitizer.sanitize(Build.MODEL);
        return applicationLabel + ";" + brand + ";" + model;
    }

    /**
     * Returns the number of accounts on the device, bucketed into:
     * 0 accounts, 1 account, or 2+ accounts.
     *
     * @return Number of accounts on the device, bucketed as above.
     */
    @VisibleForTesting
    public int getNumGoogleAccountsOnDevice() {
        // RequestGenerator may be invoked from JobService or AlarmManager (through OmahaService),
        // so have to make sure AccountManagerFacade instance is initialized.
        int numAccounts = 0;
        try {
            // TODO(waffles@chromium.org): Ideally, this should be asynchronous.
            PostTask.runSynchronously(UiThreadTaskTraits.DEFAULT,
                    () -> ProcessInitializationHandler.getInstance().initializePreNative());
            numAccounts = AccountManagerFacade.get().getGoogleAccounts().size();
        } catch (Exception e) {
            Log.e(TAG, "Can't get number of accounts.", e);
        }
        switch (numAccounts) {
            case 0:
                return 0;
            case 1:
                return 1;
            default:
                return 2;
        }
    }

    /**
     * Determine number of accounts signed in.
     */
    @VisibleForTesting
    public int getNumSignedIn() {
        // We only have a single account.
        return ChromeSigninController.get().isSignedIn() ? 1 : 0;
    }

    /**
     * Returns DownloadManager system service enabled state as
     * -1 - manager state unknown
     *  0 - manager enabled
     *  1 - manager disabled by user
     *  2 - manager disabled by unknown source
     */
    @VisibleForTesting
    public int getDownloadManagerState() {
        PackageInfo info;
        try {
            info = getContext().getPackageManager().getPackageInfo(
                    "com.android.providers.downloads", 0);
        } catch (PackageManager.NameNotFoundException e) {
            // DownloadManager Package not found.
            return -1;
        }
        int state = getContext().getPackageManager().getApplicationEnabledSetting(info.packageName);
        switch (state) {
            case PackageManager.COMPONENT_ENABLED_STATE_DEFAULT:
                // Service enable state is taken directly from the manifest.
                if (info.applicationInfo.enabled) {
                    return 0;
                } else {
                    // Service enable state set to disabled in the manifest.
                    return 2;
                }
            case PackageManager.COMPONENT_ENABLED_STATE_ENABLED:
                return 0;
            case PackageManager.COMPONENT_ENABLED_STATE_DISABLED_USER:
                // Service enable state has been explicitly disabled by the user.
                return 1;
            case PackageManager.COMPONENT_ENABLED_STATE_DISABLED:
            case PackageManager.COMPONENT_ENABLED_STATE_DISABLED_UNTIL_USED:
                // Service enable state has been explicitly disabled. Source unknown.
                return 2;
            default:
                // Illegal value returned by getApplicationEnabledSetting(). Should never happen.
                return -1;
        }
    }

    /**
     * Return a device-specific ID.
     */
    public String getDeviceID() {
        return UniqueIdentificationGeneratorFactory
                .getInstance(SettingsSecureBasedIdentificationGenerator.GENERATOR_ID)
                .getUniqueId(SALT);
    }

    /**
     * Determine whether we're on the phone or the tablet. Extracted to a separate method to
     * facilitate testing.
     */
    @VisibleForTesting
    protected boolean getLayoutIsTablet() {
        return DeviceFormFactor.isTablet();
    }

    /** URL for the Omaha server. */
    public String getServerUrl() {
        return URL_OMAHA_SERVER;
    }

    /** Returns the UUID of the Chrome version we're running when the device is a handset. */
    protected abstract String getAppIdHandset();

    /** Returns the UUID of the Chrome version we're running when the device is a tablet. */
    protected abstract String getAppIdTablet();

    /** Returns the brand code. If one can't be retrieved, return "". */
    protected abstract String getBrand();

    /** Returns the current client ID. */
    protected abstract String getClient();
}
