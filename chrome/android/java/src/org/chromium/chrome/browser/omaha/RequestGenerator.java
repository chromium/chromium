// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.os.Build;
import android.text.format.DateUtils;
import android.util.Xml;

import androidx.annotation.VisibleForTesting;

import org.xmlpull.v1.XmlSerializer;

import org.chromium.base.BuildInfo;
import org.chromium.chrome.browser.uid.SettingsSecureBasedIdentificationGenerator;
import org.chromium.chrome.browser.uid.UniqueIdentificationGeneratorFactory;
import org.chromium.ui.base.DeviceFormFactor;

import java.io.IOException;
import java.io.StringWriter;
import java.util.Locale;

/** Generates XML requests to send to the Omaha server. */
public abstract class RequestGenerator {
    // The Omaha specs say that new installs should use "-1".
    public static final int INSTALL_AGE_IMMEDIATELY_AFTER_INSTALLING = -1;

    private static final String SALT = "omahaSalt";
    private static final String URL_OMAHA_SERVER = "https://update.googleapis.com/service/update2";

    protected RequestGenerator() {
        UniqueIdentificationGeneratorFactory.registerGenerator(
                SettingsSecureBasedIdentificationGenerator.GENERATOR_ID,
                new SettingsSecureBasedIdentificationGenerator(),
                false);
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
     * with some additional placeholder values supplied.
     */
    public String generateXML(
            String sessionID,
            String versionName,
            long installAge,
            int lastCheckDate,
            RequestData data)
            throws RequestFailureException {
        XmlSerializer serializer = Xml.newSerializer();
        StringWriter writer = new StringWriter();
        try {
            serializer.setOutput(writer);
            serializer.startDocument("UTF-8", true);

            // Set up <request protocol=3.0 ...>
            serializer.startTag(null, "request");
            serializer.attribute(null, "protocol", "3.0");
            serializer.attribute(null, "updater", "Android");
            serializer.attribute(null, "updaterversion", versionName);
            serializer.attribute(
                    null,
                    "updaterchannel",
                    StringSanitizer.sanitize(BuildInfo.getInstance().hostPackageLabel));
            serializer.attribute(null, "ismachine", "1");
            serializer.attribute(null, "requestid", "{" + data.getRequestID() + "}");
            serializer.attribute(null, "sessionid", "{" + sessionID + "}");
            serializer.attribute(null, "installsource", data.getInstallSource());
            serializer.attribute(null, "dedup", "cr");

            // Set up <os platform="android"... />
            serializer.startTag(null, "os");
            serializer.attribute(null, "platform", "android");
            serializer.attribute(null, "version", Build.VERSION.RELEASE);
            serializer.attribute(null, "arch", BuildInfo.getArch());
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

                // Set up <ping active="1" rd="..." ad="..." />
                serializer.startTag(null, "ping");
                serializer.attribute(null, "active", "1");
                serializer.attribute(null, "ad", String.valueOf(lastCheckDate));
                serializer.attribute(null, "rd", String.valueOf(lastCheckDate));
                serializer.endTag(null, "ping");
            }

            serializer.endTag(null, "app");
            serializer.endTag(null, "request");

            serializer.endDocument();
        } catch (IOException e) {
            throw new RequestFailureException("Caught an IOException creating the XML: ", e);
        }

        return writer.toString();
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

    /** Return a device-specific ID. */
    public String getDeviceID() {
        try {
            return UniqueIdentificationGeneratorFactory.getInstance(
                            SettingsSecureBasedIdentificationGenerator.GENERATOR_ID)
                    .getUniqueId(SALT);
        } catch (SecurityException unused) {
            // In some cases the browser lacks permission to get the ID. Consult crbug.com/1158707.
            return "";
        }
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
