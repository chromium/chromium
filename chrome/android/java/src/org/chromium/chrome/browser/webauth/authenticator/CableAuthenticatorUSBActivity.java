// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

/**
 * Phone as a Security Key activity.
 *
 * This activity lives in the main APK and is the target for intents resulting
 * when a USB host tells the device that it wishes to speak CTAP2 over AOA.
 * (See https://source.android.com/devices/accessories/aoa.)
 *
 * This activity is split from {@link CableAuthenticatorActivity} because
 * Android 13 doesn't allow an activity with an intent filter to also receive
 * explicit intents that don't match that filter. However,
 * {@link CableAuthenticatorActivity} needs to receive explicit VIEW intents
 * from Play Services that we don't want to have an intent filter for.
 */
public class CableAuthenticatorUSBActivity extends CableAuthenticatorActivity {}
