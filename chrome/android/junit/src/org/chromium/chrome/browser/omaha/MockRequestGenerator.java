// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

/** Mocks out the RequestGenerator for tests. */
public class MockRequestGenerator extends RequestGenerator {
    public enum DeviceType {
        HANDSET,
        TABLET
    }

    public static final String UUID_PHONE = "uuid_phone";
    public static final String UUID_TABLET = "uuid_tablet";
    public static final String SERVER_URL = "http://totallylegitserver.com";

    private static final String BRAND = "MOCK";
    private static final String CLIENT = "mock-client";
    private static final String DEVICE_ID = "some-arbitrary-device-id";
    private static final String LANGUAGE = "zz-ZZ";
    private static final String ADDITIONAL_PARAMETERS = "chromium; manufacturer; model";

    private final boolean mIsOnTablet;

    public MockRequestGenerator(DeviceType deviceType) {
        mIsOnTablet = deviceType == DeviceType.TABLET;
    }

    @Override
    protected String getAppIdHandset() {
        return UUID_PHONE;
    }

    @Override
    protected String getAppIdTablet() {
        return UUID_TABLET;
    }

    @Override
    protected boolean getLayoutIsTablet() {
        return mIsOnTablet;
    }

    @Override
    public String getBrand() {
        return BRAND;
    }

    @Override
    public String getClient() {
        return CLIENT;
    }

    @Override
    public String getDeviceID() {
        return DEVICE_ID;
    }

    @Override
    public String getLanguage() {
        return LANGUAGE;
    }

    @Override
    public String getAdditionalParameters() {
        return ADDITIONAL_PARAMETERS;
    }

    @Override
    public String getServerUrl() {
        return SERVER_URL;
    }
}
