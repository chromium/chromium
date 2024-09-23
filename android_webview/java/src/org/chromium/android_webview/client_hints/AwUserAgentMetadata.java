// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.client_hints;

import androidx.annotation.NonNull;
import androidx.annotation.StringDef;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

/**
 * A class that defines user-agent metadata, it's used to override user-agent client hints.
 *
 * To provide a better experience on using the WebView public API to override user-agent client
 * hints API, this class is implemented a little different from the existing blink Chromium
 * UserAgentMetadata. See: third_party/blink/public/common/user_agent/user_agent_metadata.h.
 */
@JNINamespace("android_webview")
public class AwUserAgentMetadata {
    private String[][] mBrandVersionList;
    private String mFullVersion;
    private String mPlatform;
    private String mPlatformVersion;
    private String mArchitecture;
    private String mModel;
    private boolean mMobile;
    private int mBitness;
    private boolean mWow64;
    private @FormFactors String[] mFormFactors;

    /** Key for the user-agent metadata properties. */
    @StringDef({
        MetadataKeys.BRAND_VERSION_LIST,
        MetadataKeys.FULL_VERSION,
        MetadataKeys.PLATFORM,
        MetadataKeys.PLATFORM_VERSION,
        MetadataKeys.ARCHITECTURE,
        MetadataKeys.MODEL,
        MetadataKeys.MOBILE,
        MetadataKeys.BITNESS,
        MetadataKeys.WOW64,
        MetadataKeys.FORM_FACTORS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface MetadataKeys {
        String BRAND_VERSION_LIST = "BRAND_VERSION_LIST";
        String FULL_VERSION = "FULL_VERSION";
        String PLATFORM = "PLATFORM";
        String PLATFORM_VERSION = "PLATFORM_VERSION";
        String ARCHITECTURE = "ARCHITECTURE";
        String MODEL = "MODEL";
        String MOBILE = "MOBILE";
        String BITNESS = "BITNESS";
        String WOW64 = "WOW64";
        String FORM_FACTORS = "FORM_FACTORS";
    };

    public static final int BITNESS_DEFAULT = 0;

    /** each brand should contains brand, major version and full version. */
    public static final int BRAND_VERSION_LENGTH = 3;

    /**
     * Values for the Sec-CH-UA-Form-Factors header.
     * https://wicg.github.io/ua-client-hints/#sec-ch-ua-form-factors
     */
    // LINT.IfChange
    @StringDef({
        FormFactors.DESKTOP,
        FormFactors.AUTOMOTIVE,
        FormFactors.MOBILE,
        FormFactors.TABLET,
        FormFactors.XR,
        FormFactors.EINK,
        FormFactors.WATCH
    })
    // LINT.ThenChange(/third_party/blink/public/common/user_agent/user_agent_metadata.h)
    @Retention(RetentionPolicy.SOURCE)
    public @interface FormFactors {
        String DESKTOP = "Desktop";
        String AUTOMOTIVE = "Automotive";
        String MOBILE = "Mobile";
        String TABLET = "Tablet";
        String XR = "XR";
        String EINK = "EInk";
        String WATCH = "Watch";
    };

    // To better manage the data within this class, make the constructor as private to avoid
    // creating instances outside of the class.
    private AwUserAgentMetadata() {}

    public AwUserAgentMetadata shallowCopy() {
        AwUserAgentMetadata copy = new AwUserAgentMetadata();
        copy.mBrandVersionList = mBrandVersionList;
        copy.mFullVersion = mFullVersion;
        copy.mPlatform = mPlatform;
        copy.mPlatformVersion = mPlatformVersion;
        copy.mArchitecture = mArchitecture;
        copy.mModel = mModel;
        copy.mMobile = mMobile;
        copy.mBitness = mBitness;
        copy.mWow64 = mWow64;
        copy.mFormFactors = mFormFactors;
        return copy;
    }

    private static int getIntBitnessFromString(String bitness) {
        try {
            return Integer.parseInt(bitness);
        } catch (NumberFormatException e) {
            return BITNESS_DEFAULT;
        }
    }

    private static String getFullVersionFromBandList(
            String[][] brandFullVersionList, String brand) {
        if (brandFullVersionList == null) {
            return "";
        }

        for (String[] bv : brandFullVersionList) {
            if (bv != null && bv.length == 2 && Objects.equals(bv[0], brand)) {
                return bv[1];
            }
        }
        return "";
    }

    @CalledByNative
    private String[][] getBrandVersionList() {
        return mBrandVersionList;
    }

    @CalledByNative
    private String getFullVersion() {
        return mFullVersion;
    }

    @CalledByNative
    private String getPlatform() {
        return mPlatform;
    }

    @CalledByNative
    private String getPlatformVersion() {
        return mPlatformVersion;
    }

    @CalledByNative
    private String getArchitecture() {
        return mArchitecture;
    }

    @CalledByNative
    private String getModel() {
        return mModel;
    }

    @CalledByNative
    private boolean isMobile() {
        return mMobile;
    }

    @CalledByNative
    private int getBitness() {
        return mBitness;
    }

    @CalledByNative
    private boolean isWow64() {
        return mWow64;
    }

    @CalledByNative
    private @FormFactors String[] getFormFactors() {
        return mFormFactors;
    }

    /**
     * Construct a AwUserAgentMetadata instance, and low-entropy client hints should not be null.
     */
    @CalledByNative
    private static AwUserAgentMetadata create(
            @NonNull String[][] brandVersionList,
            String[][] brandFullVersionList,
            String fullVersion,
            @NonNull String platform,
            String platformVersion,
            String architecture,
            String model,
            boolean mobile,
            String bitness,
            boolean wow64,
            @FormFactors String[] formFactors) {
        AwUserAgentMetadata result = new AwUserAgentMetadata();
        result.mBrandVersionList = new String[brandVersionList.length][BRAND_VERSION_LENGTH];
        for (int i = 0; i < brandVersionList.length; i++) {
            result.mBrandVersionList[i][0] = brandVersionList[i][0]; // brand
            result.mBrandVersionList[i][1] = brandVersionList[i][1]; // majorVersion
            result.mBrandVersionList[i][2] =
                    getFullVersionFromBandList(
                            brandFullVersionList, brandVersionList[i][0]); // fullVersion
        }
        result.mFullVersion = fullVersion;
        result.mPlatform = platform;
        result.mPlatformVersion = platformVersion;
        result.mArchitecture = architecture;
        result.mModel = model;
        result.mMobile = mobile;
        result.mBitness = getIntBitnessFromString(bitness);
        result.mWow64 = wow64;
        result.mFormFactors = formFactors;
        return result;
    }

    private static String getValueAsString(
            Map<String, Object> map, @MetadataKeys String key, String defaultValue) {
        Object value = map.get(key);
        if (value != null && !(value instanceof String)) {
            throw new IllegalArgumentException(
                    "AwUserAgentMetadata map does not have "
                            + "right type of value for key: "
                            + key);
        }
        if (value != null) {
            return (String) value;
        }
        return defaultValue;
    }

    private static boolean getValueAsBoolean(
            Map<String, Object> map, @MetadataKeys String key, boolean defaultValue) {
        Object value = map.get(key);
        if (value != null && !(value instanceof Boolean)) {
            throw new IllegalArgumentException(
                    "AwUserAgentMetadata map does not have "
                            + "right type of value for key: "
                            + key);
        }
        if (value != null) {
            return (Boolean) value;
        }
        return defaultValue;
    }

    private static int getValueAsInt(
            Map<String, Object> map, @MetadataKeys String key, int defaultValue) {
        Object value = map.get(key);
        if (value != null && !(value instanceof Integer)) {
            throw new IllegalArgumentException(
                    "AwUserAgentMetadata map does not have "
                            + "right type of value for key: "
                            + key);
        }
        if (value != null) {
            return (Integer) value;
        }
        return defaultValue;
    }

    /**
     * Return an instance based on the provided override user-agent metadata map and the default
     * user-agent metadata settings.
     *
     * Here we only validate some basic requirements for the input, we need to do more strictly
     * validation on Android public API, like check whether brand full version either all empty or
     * all non-empty. Return a boolean indicate whether it needs to update the user-agent metadata.
     *
     * @param uaMetadataMap an object represent what users intend to override user-agent metadata
     *         setting.
     * @param defaultData an object represent system default user-agent metadata.
     * @return For system default override settings, we maintain a shallow copy instance of
     *         AwUserAgentMetadata, while for outside override settings(e.g brand version array)
     *         we will deep copy them when constructing a new instance of AwUserAgentMetadata.
     */
    public static AwUserAgentMetadata fromMap(
            Map<String, Object> uaMetadataMap, @NonNull AwUserAgentMetadata defaultData) {
        if (uaMetadataMap == null || uaMetadataMap.isEmpty()) {
            return defaultData.shallowCopy();
        }

        Object brandVersionValue = uaMetadataMap.get(MetadataKeys.BRAND_VERSION_LIST);
        String[][] brandVersionList = defaultData.mBrandVersionList;
        if (brandVersionValue != null) {
            if (!(brandVersionValue instanceof String[][])) {
                throw new IllegalArgumentException(
                        "AwUserAgentMetadata map does not have "
                                + "right type of value for key: "
                                + MetadataKeys.BRAND_VERSION_LIST);
            }
            String[][] overrideBrandVersionList = (String[][]) brandVersionValue;
            brandVersionList = new String[overrideBrandVersionList.length][];
            for (int i = 0; i < overrideBrandVersionList.length; i++) {
                String[] brandVersionInfo = overrideBrandVersionList[i];
                if (brandVersionInfo.length != BRAND_VERSION_LENGTH) {
                    throw new IllegalArgumentException(
                            "AwUserAgentMetadata map does not have "
                                    + "right type of value for key: "
                                    + MetadataKeys.BRAND_VERSION_LIST
                                    + ", expect brand item length:"
                                    + BRAND_VERSION_LENGTH
                                    + ", actual:"
                                    + brandVersionInfo.length);
                }
                if (brandVersionInfo[0] == null
                        || brandVersionInfo[1] == null
                        || brandVersionInfo[2] == null) {
                    throw new IllegalArgumentException(
                            "AwUserAgentMetadata map does not have "
                                    + "right type of value for key: "
                                    + MetadataKeys.BRAND_VERSION_LIST
                                    + ", brand item should not set as null");
                }
                brandVersionList[i] = Arrays.copyOf(brandVersionInfo, brandVersionInfo.length);
            }
        }

        Object formFactorsValue = uaMetadataMap.get(MetadataKeys.FORM_FACTORS);
        @FormFactors String[] formFactors = defaultData.mFormFactors;
        if (formFactorsValue != null) {
            if (!(formFactorsValue instanceof String[])) {
                throw new IllegalArgumentException(
                        "AwUserAgentMetadata map does not have "
                                + "right type of value for key: "
                                + MetadataKeys.FORM_FACTORS);
            }
            @FormFactors String[] asArray = (String[]) formFactorsValue;
            formFactors = Arrays.copyOf(asArray, asArray.length);
        }

        AwUserAgentMetadata result = new AwUserAgentMetadata();
        result.mBrandVersionList = brandVersionList;
        result.mFullVersion =
                getValueAsString(
                        uaMetadataMap, MetadataKeys.FULL_VERSION, defaultData.mFullVersion);
        result.mPlatform =
                getValueAsString(uaMetadataMap, MetadataKeys.PLATFORM, defaultData.mPlatform);
        result.mPlatformVersion =
                getValueAsString(
                        uaMetadataMap, MetadataKeys.PLATFORM_VERSION, defaultData.mPlatformVersion);
        result.mArchitecture =
                getValueAsString(
                        uaMetadataMap, MetadataKeys.ARCHITECTURE, defaultData.mArchitecture);
        result.mModel = getValueAsString(uaMetadataMap, MetadataKeys.MODEL, defaultData.mModel);
        result.mMobile = getValueAsBoolean(uaMetadataMap, MetadataKeys.MOBILE, defaultData.mMobile);
        result.mBitness = getValueAsInt(uaMetadataMap, MetadataKeys.BITNESS, defaultData.mBitness);
        result.mWow64 = getValueAsBoolean(uaMetadataMap, MetadataKeys.WOW64, defaultData.mWow64);
        result.mFormFactors = formFactors;
        return result;
    }

    public Map<String, Object> toMapObject() {
        Map<String, Object> item = new HashMap<>();
        item.put(MetadataKeys.BRAND_VERSION_LIST, mBrandVersionList);
        item.put(MetadataKeys.FULL_VERSION, mFullVersion);
        item.put(MetadataKeys.PLATFORM, mPlatform);
        item.put(MetadataKeys.PLATFORM_VERSION, mPlatformVersion);
        item.put(MetadataKeys.ARCHITECTURE, mArchitecture);
        item.put(MetadataKeys.MODEL, mModel);
        item.put(MetadataKeys.MOBILE, mMobile);
        item.put(MetadataKeys.BITNESS, mBitness);
        item.put(MetadataKeys.WOW64, mWow64);
        item.put(MetadataKeys.FORM_FACTORS, mFormFactors);
        return item;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof AwUserAgentMetadata)) {
            return false;
        }
        AwUserAgentMetadata that = (AwUserAgentMetadata) o;
        return mMobile == that.mMobile
                && mWow64 == that.mWow64
                && mBitness == that.mBitness
                && Arrays.deepEquals(mBrandVersionList, that.mBrandVersionList)
                && Objects.equals(mFullVersion, that.mFullVersion)
                && Objects.equals(mPlatform, that.mPlatform)
                && Objects.equals(mPlatformVersion, that.mPlatformVersion)
                && Objects.equals(mArchitecture, that.mArchitecture)
                && Objects.equals(mModel, that.mModel)
                && Arrays.deepEquals(mFormFactors, that.mFormFactors);
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                Arrays.deepHashCode(mBrandVersionList),
                mFullVersion,
                mPlatform,
                mPlatformVersion,
                mArchitecture,
                mModel,
                mMobile,
                mBitness,
                mWow64,
                Arrays.deepHashCode(mFormFactors));
    }
}
