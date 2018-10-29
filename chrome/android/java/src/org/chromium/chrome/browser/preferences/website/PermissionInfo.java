// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.support.annotation.IntDef;

import org.chromium.chrome.browser.ContentSettingsType;

import java.io.Serializable;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Permission information for a given origin.
 */
public class PermissionInfo implements Serializable {
    @IntDef({Type.CAMERA, Type.CLIPBOARD, Type.GEOLOCATION, Type.MICROPHONE, Type.MIDI,
            Type.NOTIFICATION, Type.PROTECTED_MEDIA_IDENTIFIER, Type.SENSORS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        // Values used to address index - should be enumerated from 0 and can't have gaps.
        // All updates here must also be reflected in {@link #getContentSettingsType(int)
        // getContentSettingsType} and {@link SingleWebsitePreferences.PERMISSION_PREFERENCE_KEYS}.
        int CAMERA = 0;
        int CLIPBOARD = 1;
        int GEOLOCATION = 2;
        int MICROPHONE = 3;
        int MIDI = 4;
        int NOTIFICATION = 5;
        int PROTECTED_MEDIA_IDENTIFIER = 6;
        int SENSORS = 7;
        /**
         * Number of handled permissions used for example inside for loops.
         */
        int NUM_ENTRIES = 8;
    }

    private final boolean mIsIncognito;
    private final String mEmbedder;
    private final String mOrigin;
    private final @Type int mType;

    public PermissionInfo(@Type int type, String origin, String embedder, boolean isIncognito) {
        mOrigin = origin;
        mEmbedder = embedder;
        mIsIncognito = isIncognito;
        mType = type;
    }

    public @Type int getType() {
        return mType;
    }

    public String getOrigin() {
        return mOrigin;
    }

    public String getEmbedder() {
        return mEmbedder;
    }

    public boolean isIncognito() {
        return mIsIncognito;
    }

    public String getEmbedderSafe() {
        return mEmbedder != null ? mEmbedder : mOrigin;
    }

    /**
     * Returns the ContentSetting value for this origin.
     */
    public ContentSetting getContentSetting() {
        switch (mType) {
            case Type.CAMERA:
                return ContentSetting.fromInt(
                        WebsitePreferenceBridge.nativeGetCameraSettingForOrigin(
                                mOrigin, getEmbedderSafe(), mIsIncognito));
            case Type.CLIPBOARD:
                return ContentSetting.fromInt(
                        WebsitePreferenceBridge.nativeGetClipboardSettingForOrigin(
                                mOrigin, mIsIncognito));
            case Type.GEOLOCATION:
                return ContentSetting.fromInt(
                        WebsitePreferenceBridge.nativeGetGeolocationSettingForOrigin(
                                mOrigin, getEmbedderSafe(), mIsIncognito));
            case Type.MICROPHONE:
                return ContentSetting.fromInt(
                        WebsitePreferenceBridge.nativeGetMicrophoneSettingForOrigin(
                                mOrigin, getEmbedderSafe(), mIsIncognito));
            case Type.MIDI:
                return ContentSetting.fromInt(WebsitePreferenceBridge.nativeGetMidiSettingForOrigin(
                        mOrigin, getEmbedderSafe(), mIsIncognito));
            case Type.NOTIFICATION:
                return ContentSetting.fromInt(
                        WebsitePreferenceBridge.nativeGetNotificationSettingForOrigin(
                                mOrigin, mIsIncognito));
            case Type.PROTECTED_MEDIA_IDENTIFIER:
                return ContentSetting.fromInt(
                        WebsitePreferenceBridge.nativeGetProtectedMediaIdentifierSettingForOrigin(
                                mOrigin, getEmbedderSafe(), mIsIncognito));
            case Type.SENSORS:
                return ContentSetting.fromInt(
                        WebsitePreferenceBridge.nativeGetSensorsSettingForOrigin(
                                mOrigin, getEmbedderSafe(), mIsIncognito));
            default:
                assert false;
                return null;
        }
    }

    /**
     * Sets the native ContentSetting value for this origin.
     */
    public void setContentSetting(ContentSetting value) {
        switch (mType) {
            case Type.CAMERA:
                WebsitePreferenceBridge.nativeSetCameraSettingForOrigin(
                        mOrigin, value.toInt(), mIsIncognito);
                break;
            case Type.CLIPBOARD:
                WebsitePreferenceBridge.nativeSetClipboardSettingForOrigin(
                        mOrigin, value.toInt(), mIsIncognito);
                break;
            case Type.GEOLOCATION:
                WebsitePreferenceBridge.nativeSetGeolocationSettingForOrigin(
                        mOrigin, getEmbedderSafe(), value.toInt(), mIsIncognito);
                break;
            case Type.MICROPHONE:
                WebsitePreferenceBridge.nativeSetMicrophoneSettingForOrigin(
                        mOrigin, value.toInt(), mIsIncognito);
                break;
            case Type.MIDI:
                WebsitePreferenceBridge.nativeSetMidiSettingForOrigin(
                        mOrigin, getEmbedderSafe(), value.toInt(), mIsIncognito);
                break;
            case Type.NOTIFICATION:
                WebsitePreferenceBridge.nativeSetNotificationSettingForOrigin(
                        mOrigin, value.toInt(), mIsIncognito);
                break;
            case Type.PROTECTED_MEDIA_IDENTIFIER:
                WebsitePreferenceBridge.nativeSetProtectedMediaIdentifierSettingForOrigin(
                        mOrigin, getEmbedderSafe(), value.toInt(), mIsIncognito);
                break;
            case Type.SENSORS:
                WebsitePreferenceBridge.nativeSetSensorsSettingForOrigin(
                        mOrigin, getEmbedderSafe(), value.toInt(), mIsIncognito);
                break;
            default:
                assert false;
        }
    }

    public static @ContentSettingsType int getContentSettingsType(@Type int type) {
        switch (type) {
            case Type.CAMERA:
                return ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA;
            case Type.CLIPBOARD:
                return ContentSettingsType.CONTENT_SETTINGS_TYPE_CLIPBOARD_READ;
            case Type.GEOLOCATION:
                return ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION;
            case Type.MICROPHONE:
                return ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC;
            case Type.MIDI:
                return ContentSettingsType.CONTENT_SETTINGS_TYPE_MIDI_SYSEX;
            case Type.NOTIFICATION:
                return ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS;
            case Type.PROTECTED_MEDIA_IDENTIFIER:
                return ContentSettingsType.CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER;
            case Type.SENSORS:
                return ContentSettingsType.CONTENT_SETTINGS_TYPE_SENSORS;
            default:
                assert false;
                return ContentSettingsType.CONTENT_SETTINGS_TYPE_DEFAULT;
        }
    }
}
