// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.app.PendingIntent;
import android.content.Intent;
import android.text.TextUtils;
import android.view.InputEvent;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.Predicate;

import java.security.InvalidKeyException;
import java.security.Key;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import java.util.Arrays;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

/**
 * Handles incoming App Attribution intents.
 *
 * The intent flow works as follows:
 * 1. An app sends an "outer" intent to Chrome, which is simply a standard VIEW intent for a URL,
 *    but with the EXTRA_ATTRIBUTION_INTENT set to a Mutable PendingIntent that contains the
 *    attribution parameters and the ACTION_APP_ATTRIBUTION action.
 * 2. Chrome adds the PackageName and PackageName hash from the PendingIntent creator, as well as
 *    the original VIEW intent, as extras to the PendingIntent and sends it.
 * 3. The LauncherActivity receives the ACTION_APP_ATTRIBUTION intent, validates the PackageName
 *    hash to verify the source of the attribution, processes the attribution data, then un-wraps
 *    the original VIEW intent for the LauncherActivity to handle and resume the navigation.
 *
 * Note that the sending app fully controls the contents of both the outer and inner intent - the
 * hash only validates the sender's package, so further validation would be required if we wanted to
 * make any guarantees of data matching between the outer and inner intents.
 *
 * Explainer: https://github.com/WICG/conversion-measurement-api/blob/main/app_to_web.md
 *
 */
public class AttributionIntentHandlerImpl implements AttributionIntentHandler {
    private static final String TAG = "AppAttribution";

    // Static to avoid repeated initialization overhead.
    private static final SecureRandom sRandom = new SecureRandom();

    // Mac that is valid for the life of the process (and no longer).
    @VisibleForTesting
    /* protected */ static final Mac sHasher;
    private static final int SECRET_KEY_NUM_BYTES = 64;
    private static final String MAC_ALGORITHM = "HmacSHA256";

    /* protected */ static final String EXTRA_ORIGINAL_INTENT =
            "com.android.chrome.original_intent";
    /* protected */ static final String EXTRA_PACKAGE_NAME = "com.android.chrome.package_name";
    /* protected */ static final String EXTRA_PACKAGE_MAC = "com.android.chrome.package_mac";
    /* protected */ static final String EXTRA_PENDING_PARAMETERS_TOKEN = "pendingAttributionToken";

    static {
        try {
            byte secret[] = new byte[SECRET_KEY_NUM_BYTES];
            sRandom.nextBytes(secret);
            Key key = new SecretKeySpec(secret, MAC_ALGORITHM);
            sHasher = Mac.getInstance(MAC_ALGORITHM);
            sHasher.init(key);
        } catch (NoSuchAlgorithmException | InvalidKeyException e) {
            // Should never happen.
            throw new RuntimeException(e);
        }
    }

    private Predicate<InputEvent> mInputEventValidator;

    private AttributionParameters mPendingAttributionParameters;
    private byte[] mPendingAttributionToken;

    public AttributionIntentHandlerImpl(Predicate<InputEvent> inputEventValidator) {
        mInputEventValidator = inputEventValidator;
    }

    @Override
    public boolean handleOuterAttributionIntent(Intent intent) {
        PendingIntent outerIntent = IntentUtils.safeGetParcelableExtra(
                intent, AttributionConstants.EXTRA_ATTRIBUTION_INTENT);
        if (outerIntent == null) return false;
        // Remove the PendingIntent so we don't process it again.
        intent.removeExtra(AttributionConstants.EXTRA_ATTRIBUTION_INTENT);

        // Add extras to the Intent sent by the PendingIntent (the 'inner' intent) that will be used
        // for sender validation. We also add the original intent to the inner intent, so that when
        // we receive it we can handle the original view intent after processing the attribution
        // data.
        Intent fillIn = new Intent();
        fillIn.putExtra(EXTRA_ORIGINAL_INTENT, intent);
        String senderPackage = outerIntent.getCreatorPackage();
        byte packageMac[] = sHasher.doFinal(ApiCompatibilityUtils.getBytesUtf8(senderPackage));
        fillIn.putExtra(EXTRA_PACKAGE_NAME, outerIntent.getCreatorPackage());
        fillIn.putExtra(EXTRA_PACKAGE_MAC, packageMac);

        try {
            // By firing the PendingIntent, we can guarantee that only the sending app and Chrome
            // can control the data on the sent Intent. The app could still mistakenly mis-target
            // the sent intent, but there's nothing we can do about that.
            outerIntent.send(ContextUtils.getApplicationContext(), 0, fillIn);
        } catch (PendingIntent.CanceledException e) {
            // PendingIntent was cancelled by the sender, so treat just treat the intent as a
            // regular non-attribution Intent.
            return false;
        }
        return true;
    }

    @Override
    public Intent handleInnerAttributionIntent(Intent intent) {
        if (!AttributionConstants.ACTION_APP_ATTRIBUTION.equals(intent.getAction())) return null;

        String senderPackage = IntentUtils.safeGetStringExtra(intent, EXTRA_PACKAGE_NAME);
        byte packageMac[] = IntentUtils.safeGetByteArrayExtra(intent, EXTRA_PACKAGE_MAC);
        Intent originalIntent = IntentUtils.safeGetParcelableExtra(intent, EXTRA_ORIGINAL_INTENT);
        String sourceEventId = IntentUtils.safeGetStringExtra(
                intent, AttributionConstants.EXTRA_ATTRIBUTION_SOURCE_EVENT_ID);
        String attributionDestination = IntentUtils.safeGetStringExtra(
                intent, AttributionConstants.EXTRA_ATTRIBUTION_DESTINATION);
        String reportTo = IntentUtils.safeGetStringExtra(
                intent, AttributionConstants.EXTRA_ATTRIBUTION_REPORT_TO);
        long expiry = IntentUtils.safeGetLongExtra(
                intent, AttributionConstants.EXTRA_ATTRIBUTION_EXPIRY, 0);
        InputEvent inputEvent =
                IntentUtils.safeGetParcelableExtra(intent, AttributionConstants.EXTRA_INPUT_EVENT);

        AttributionParameters params = new AttributionParameters(
                senderPackage, sourceEventId, attributionDestination, reportTo, expiry);
        if (!isValidAttributionIntent(params, packageMac, originalIntent, inputEvent)) {
            Log.w(TAG, "Invalid APP_ATTRIBUTION intent: " + intent.toUri(0));
            // Even if the attribution intent was invalid, we can still handle the original view
            // intent, which shouldn't be null unless the sending app is intentionally removing it.
            return originalIntent;
        }
        mPendingAttributionToken = new byte[32];
        sRandom.nextBytes(mPendingAttributionToken);
        mPendingAttributionParameters = params;

        originalIntent.putExtra(EXTRA_PENDING_PARAMETERS_TOKEN, mPendingAttributionToken);
        return originalIntent;
    }

    @VisibleForTesting
    public boolean isValidAttributionIntent(AttributionParameters params, byte[] packageMac,
            Intent originalIntent, InputEvent inputEvent) {
        if (params.getSourcePackageName() == null || packageMac == null || originalIntent == null
                || TextUtils.isEmpty(params.getSourceEventId())
                || TextUtils.isEmpty(params.getDestination()) || inputEvent == null) {
            Log.d(TAG, "Attribution intent missing attributes.");
            return false;
        }

        byte correctPackageMac[] =
                sHasher.doFinal(ApiCompatibilityUtils.getBytesUtf8(params.getSourcePackageName()));
        if (!Arrays.equals(correctPackageMac, packageMac)) {
            Log.d(TAG, "Attribution intent package MAC incorrect.");
            return false;
        }

        if (!mInputEventValidator.test(inputEvent)) {
            Log.d(TAG, "InputEvent was not valid.");
            return false;
        }

        return true;
    }

    @Override
    public AttributionParameters getAndClearPendingAttributionParameters(Intent intent) {
        if (!intent.hasExtra(EXTRA_PENDING_PARAMETERS_TOKEN)) return null;
        byte[] token = IntentUtils.safeGetByteArrayExtra(intent, EXTRA_PENDING_PARAMETERS_TOKEN);
        if (!Arrays.equals(token, mPendingAttributionToken)) return null;
        AttributionParameters params = mPendingAttributionParameters;
        mPendingAttributionParameters = null;
        mPendingAttributionToken = null;
        return params;
    }
}
