// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.content.Intent;
import android.os.SystemClock;
import android.util.Base64;

import androidx.annotation.Nullable;

import com.google.android.gms.fido.Fido;
import com.google.android.gms.fido.fido2.api.common.ErrorCode;
import com.google.common.io.BaseEncoding;

import org.junit.Assert;

import org.chromium.blink.mojom.AuthenticatorAttachment;
import org.chromium.blink.mojom.AuthenticatorSelectionCriteria;
import org.chromium.blink.mojom.CableAuthentication;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PrfValues;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialDescriptor;
import org.chromium.blink.mojom.PublicKeyCredentialParameters;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRpEntity;
import org.chromium.blink.mojom.PublicKeyCredentialType;
import org.chromium.blink.mojom.PublicKeyCredentialUserEntity;
import org.chromium.blink.mojom.UvmEntry;
import org.chromium.mojo_base.mojom.TimeDelta;
import org.chromium.url.mojom.Url;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.concurrent.TimeUnit;

/* NO_BUILDER:
 *
 * Several comments below describe how binary blobs in this file were produced. However, they use
 * Builder classes that no longer appear to exist. Because of this, it's difficult to update these
 * blobs.
 *
 * The blobs are in Android's Parcel format. This format defines a tag-value object that represents
 * a (tag, bytestring) pair. A tag-value is serialized as a little-endian, uint32 where the bottom
 * 16 bits are the tag, and the top 16 bits are the length of the value. If the length is 0xffff,
 * then a second uint32 is used to store the actual length. (This two-word format appears to be
 * always used in practice, even when the length would fit in 16 bits.) The bytestring contains are
 * then the |length| following bytes.
 *
 * A Parcel consists of a tag-value object with tag 0x4f45 and whose value is the rest of the
 * Parcel data. That value contains a series of tag-values that define the members of the
 * destination object. Unknown tags are skipped over.
 *
 * The semantics of the values of the inner values are tag-specific. In the case of byte[] objects,
 * the value is, itself, a tag-value object. Since this has its own length, there can be padding at
 * the end and they seem to be padded with zeros to the nearest four-byte boundary.
 */

/**
 * A Helper class for testing Fido2ApiHandlerInternal.
 */
public class Fido2ApiTestHelper {
    // Test data.
    private static final String FILLER_ERROR_MSG = "Error Error";

    /**
     * This byte array is produced by
     * com.google.android.gms.fido.fido2.api.common.AuthenticatorAttestationResponse with test data,
     * i.e.:
     * AuthenticatorAttestationResponse response =
     *         new AuthenticatorAttestationResponse.Builder()
     *                 .setAttestationObject(TEST_ATTESTATION_OBJECT)
     *                 .setClientDataJSON(TEST_CLIENT_DATA_JSON)
     *                 .setKeyHandle(TEST_KEY_HANDLE)
     *                 .build().serializeToBytes();
     *
     * NOTE: See NO_BUILDER comment, above.
     */
    private static final byte[] TEST_AUTHENTICATOR_ATTESTATION_RESPONSE = new byte[] {69, 79, -1,
            -1, 44, 1, 0, 0, 2, 0, -1, -1, 36, 0, 0, 0, 32, 0, 0, 0, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6,
            7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 9, 3, 0, -1, -1, 8, 0, 0,
            0, 3, 0, 0, 0, 4, 5, 6, 0, 4, 0, -1, -1, -24, 0, 0, 0, -30, 0, 0, 0, -93, 99, 102, 109,
            116, 100, 110, 111, 110, 101, 103, 97, 116, 116, 83, 116, 109, 116, -96, 104, 97, 117,
            116, 104, 68, 97, 116, 97, 88, -60, 38, -67, 114, 120, -66, 70, 55, 97, -15, -6, -95,
            -79, 10, -76, -60, -8, 38, 112, 38, -100, 65, 12, 114, 106, 31, -42, -32, 88, 85, -31,
            -101, 70, 65, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 124,
            80, -60, -114, 69, -117, 44, -120, 122, -62, 63, 104, 18, -66, 2, -3, -56, 35, -24, 66,
            -4, 74, 48, -128, -52, 80, -100, 46, 97, 93, -25, -21, -53, 40, 123, 90, -107, -20, 111,
            -4, 15, 64, 122, 15, -84, -21, -33, -15, 26, 11, 35, 36, -49, 116, 52, -74, 107, 63,
            113, -59, 125, -27, -120, -63, -91, 1, 2, 3, 38, 32, 1, 33, 88, 32, -75, -80, 118, 102,
            -14, 124, -108, -9, -27, -91, 59, -48, -92, -102, -38, -44, 92, 95, 14, -62, 41, -117,
            -70, 101, 9, 64, 35, 31, -20, 79, -71, -71, 34, 88, 32, -24, -33, 64, 97, -31, -34, 96,
            -83, -119, -25, 21, -14, -56, -70, -37, -116, -21, -33, -128, -66, 61, 41, 107, 16, -25,
            120, 106, -113, 54, -62, -102, 42, 0, 0};

    /**
     * This byte array is produced by
     * com.google.android.gms.fido.fido2.api.common.AuthenticatorAssertionResponse with test data,
     * i.e.:
     * AuthenticatorAssertionResponse.Builder()
     *         .setAuthenticatorData(TEST_AUTHENTICATOR_DATA)
     *         .setSignature(TEST_SIGNATURE)
     *         .setClientDataJSON(TEST_CLIENT_DATA_JSON)
     *         .setKeyHandle(TEST_KEY_HANDLE)
     *         .build().serializeToBytes();
     *
     * NOTE: See NO_BUILDER comment, above.
     */
    private static final byte[] TEST_AUTHENTICATOR_ASSERTION_RESPONSE = new byte[] {69, 79, -1, -1,
            92, 0, 0, 0, 2, 0, -1, -1, 36, 0, 0, 0, 32, 0, 0, 0, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8,
            5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 9, 3, 0, -1, -1, 8, 0, 0, 0, 3,
            0, 0, 0, 4, 5, 6, 0, 4, 0, -1, -1, 8, 0, 0, 0, 3, 0, 0, 0, 7, 8, 9, 0, 5, 0, -1, -1, 8,
            0, 0, 0, 3, 0, 0, 0, 10, 11, 12, 0};

    /**
     * This byte array is produced by
     * com.google.android.gms.fido.fido2.api.common.PublicKeyCredential with test data,
     * i.e.:
     * AuthenticatorAssertionResponse response =
     *         new AuthenticatorAssertionResponse.Builder()
     *                 .setAuthenticatorData(TEST_AUTHENTICATOR_DATA)
     *                 .setSignature(TEST_SIGNATURE)
     *                 .setClientDataJSON(TEST_CLIENT_DATA_JSON)
     *                 .setKeyHandle(TEST_KEY_HANDLE)
     *                 .build();
     * UvmEntry uvmEntry0 =
     *         new UvmEntry.Builder()
     *                 .setUserVerificationMethod(TEST_USER_VERIFICATION_METHOD[0])
     *                 .setKeyProtectionType(TEST_KEY_PROTECTION_TYPE[0])
     *                 .setMatcherProtectionType(TEST_MATCHER_PROTECTION_TYPE[0])
     *                 .build();
     * UvmEntry uvmEntry1 =
     *         new UvmEntry.Builder()
     *                 .setUserVerificationMethod(TEST_USER_VERIFICATION_METHOD[1])
     *                 .setKeyProtectionType(TEST_KEY_PROTECTION_TYPE[1])
     *                 .setMatcherProtectionType(TEST_MATCHER_PROTECTION_TYPE[1])
     *                 .build();
     * UvmEntries uvmEntries =
     *         new UvmEntries.Builder().addUvmEntry(uvmEntry0).addUvmEntry(uvmEntry1).build();
     * AuthenticationExtensionsClientOutputs
     *         authenticationExtensionsClientOutputs =
     *                 new AuthenticationExtensionsClientOutputs.Builder()
     *                         .setUserVerificationMethodEntries(uvmEntries)
     *                         .build();
     * PublicKeyCredential publicKeyCredential =
     *         new PublicKeyCredential.Builder()
     *                 .setResponse(response)
     *                 .setAuthenticationExtensionsClientOutputs(
     *                         authenticationExtensionsClientOutputs)
     *                 .build().serializeToBytes();
     *
     * NOTE: See NO_BUILDER comment, above.
     */
    private static final byte[] TEST_ASSERTION_PUBLIC_KEY_CREDENTIAL_WITH_UVM = new byte[] {69, 79,
            -1, -1, 4, 1, 0, 0, 2, 0, -1, -1, 28, 0, 0, 0, 10, 0, 0, 0, 112, 0, 117, 0, 98, 0, 108,
            0, 105, 0, 99, 0, 45, 0, 107, 0, 101, 0, 121, 0, 0, 0, 0, 0, 5, 0, -1, -1, 100, 0, 0, 0,
            69, 79, -1, -1, 92, 0, 0, 0, 2, 0, -1, -1, 36, 0, 0, 0, 32, 0, 0, 0, 5, 6, 7, 8, 5, 6,
            7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 9, 3, 0, -1,
            -1, 8, 0, 0, 0, 3, 0, 0, 0, 4, 5, 6, 0, 4, 0, -1, -1, 8, 0, 0, 0, 3, 0, 0, 0, 7, 8, 9,
            0, 5, 0, -1, -1, 8, 0, 0, 0, 3, 0, 0, 0, 10, 11, 12, 0, 7, 0, -1, -1, 108, 0, 0, 0, 69,
            79, -1, -1, 100, 0, 0, 0, 1, 0, -1, -1, 92, 0, 0, 0, 69, 79, -1, -1, 84, 0, 0, 0, 1, 0,
            -1, -1, 76, 0, 0, 0, 2, 0, 0, 0, 32, 0, 0, 0, 69, 79, -1, -1, 24, 0, 0, 0, 1, 0, 4, 0,
            2, 0, 0, 0, 2, 0, 4, 0, 2, 0, 0, 0, 3, 0, 4, 0, 4, 0, 0, 0, 32, 0, 0, 0, 69, 79, -1, -1,
            24, 0, 0, 0, 1, 0, 4, 0, 0, 2, 0, 0, 2, 0, 4, 0, 1, 0, 0, 0, 3, 0, 4, 0, 1, 0, 0, 0};

    /**
     * The following byte arrays are produced by
     * com.google.android.gms.fido.fido2.api.common.AuthenticatorErrorResponse with
     * com.google.android.gms.fido.fido2.api.common.ErrorCode and error message;
     * i.e.:
     * AuthenticatorErrorResponse.Builder()
     *                           .setErrorCode(errorCode)
     *                           .setErrorMessage(errorMsg)
     *                           .build().serializeToBytes();
     *
     * NOTE: See NO_BUILDER comment, above.
     */
    private static final byte[] TEST_ERROR_WITH_FILLER_ERROR_MSG_RESPONSE_FRONT =
            new byte[] {69, 79, -1, -1, 44, 0, 0, 0, 2, 0, 4, 0};
    private static final byte[] TEST_ERROR_WITH_FILLER_ERROR_MSG_RESPONSE_TAIL =
            new byte[] {0, 0, 0, 3, 0, -1, -1, 28, 0, 0, 0, 11, 0, 0, 0, 69, 0, 114, 0, 114, 0, 111,
                    0, 114, 0, 32, 0, 69, 0, 114, 0, 114, 0, 111, 0, 114, 0, 0, 0};
    private static final byte[] TEST_ERROR_WITH_NULL_ERROR_MSG_RESPONSE_FRONT = {
            69, 79, -1, -1, 8, 0, 0, 0, 2, 0, 4, 0};
    private static final byte[] TEST_ERROR_WITH_NULL_ERROR_MSG_RESPONSE_TAIL = {0, 0, 0};
    private static final byte[] TEST_CONSTRAINTERROR_NOSCREENLOCK_RESPONSE =
            new byte[] {69, 79, -1, -1, 116, 0, 0, 0, 2, 0, 4, 0, 29, 0, 0, 0, 3, 0, -1, -1, 100, 0,
                    0, 0, 46, 0, 0, 0, 84, 0, 104, 0, 101, 0, 32, 0, 100, 0, 101, 0, 118, 0, 105, 0,
                    99, 0, 101, 0, 32, 0, 105, 0, 115, 0, 32, 0, 110, 0, 111, 0, 116, 0, 32, 0, 115,
                    0, 101, 0, 99, 0, 117, 0, 114, 0, 101, 0, 100, 0, 32, 0, 119, 0, 105, 0, 116, 0,
                    104, 0, 32, 0, 97, 0, 110, 0, 121, 0, 32, 0, 115, 0, 99, 0, 114, 0, 101, 0, 101,
                    0, 110, 0, 32, 0, 108, 0, 111, 0, 99, 0, 107, 0, 0, 0, 0, 0};
    private static final byte[] TEST_EMPTYALLOWCRED_RESPONSE1 = new byte[] {69, 79, -1, -1, -128, 0,
            0, 0, 2, 0, 4, 0, 35, 0, 0, 0, 3, 0, -1, -1, 112, 0, 0, 0, 52, 0, 0, 0, 65, 0, 117, 0,
            116, 0, 104, 0, 101, 0, 110, 0, 116, 0, 105, 0, 99, 0, 97, 0, 116, 0, 105, 0, 111, 0,
            110, 0, 32, 0, 114, 0, 101, 0, 113, 0, 117, 0, 101, 0, 115, 0, 116, 0, 32, 0, 109, 0,
            117, 0, 115, 0, 116, 0, 32, 0, 104, 0, 97, 0, 118, 0, 101, 0, 32, 0, 110, 0, 111, 0,
            110, 0, 45, 0, 101, 0, 109, 0, 112, 0, 116, 0, 121, 0, 32, 0, 97, 0, 108, 0, 108, 0,
            111, 0, 119, 0, 76, 0, 105, 0, 115, 0, 116, 0, 0, 0, 0, 0};
    private static final byte[] TEST_EMPTYALLOWCRED_RESPONSE2 = new byte[] {69, 79, -1, -1, -120, 0,
            0, 0, 2, 0, 4, 0, 35, 0, 0, 0, 3, 0, -1, -1, 120, 0, 0, 0, 57, 0, 0, 0, 82, 0, 101, 0,
            113, 0, 117, 0, 101, 0, 115, 0, 116, 0, 32, 0, 100, 0, 111, 0, 101, 0, 115, 0, 110, 0,
            39, 0, 116, 0, 32, 0, 104, 0, 97, 0, 118, 0, 101, 0, 32, 0, 97, 0, 32, 0, 118, 0, 97, 0,
            108, 0, 105, 0, 100, 0, 32, 0, 108, 0, 105, 0, 115, 0, 116, 0, 32, 0, 111, 0, 102, 0,
            32, 0, 97, 0, 108, 0, 108, 0, 111, 0, 119, 0, 101, 0, 100, 0, 32, 0, 99, 0, 114, 0, 101,
            0, 100, 0, 101, 0, 110, 0, 116, 0, 105, 0, 97, 0, 108, 0, 115, 0, 46, 0, 0, 0};
    private static final byte[] TEST_INVALIDSTATEERROR_DUPLICATE_REGISTRATION_RESPONSE =
            new byte[] {69, 79, -1, -1, -116, 0, 0, 0, 2, 0, 4, 0, 11, 0, 0, 0, 3, 0, -1, -1, 124,
                    0, 0, 0, 58, 0, 0, 0, 79, 0, 110, 0, 101, 0, 32, 0, 111, 0, 102, 0, 32, 0, 116,
                    0, 104, 0, 101, 0, 32, 0, 101, 0, 120, 0, 99, 0, 108, 0, 117, 0, 100, 0, 101, 0,
                    100, 0, 32, 0, 99, 0, 114, 0, 101, 0, 100, 0, 101, 0, 110, 0, 116, 0, 105, 0,
                    97, 0, 108, 0, 115, 0, 32, 0, 101, 0, 120, 0, 105, 0, 115, 0, 116, 0, 115, 0,
                    32, 0, 111, 0, 110, 0, 32, 0, 116, 0, 104, 0, 101, 0, 32, 0, 108, 0, 111, 0, 99,
                    0, 97, 0, 108, 0, 32, 0, 100, 0, 101, 0, 118, 0, 105, 0, 99, 0, 101, 0, 0, 0, 0,
                    0};
    private static final byte[] TEST_UNKNOWNERROR_CRED_NOT_RECOGNIZED_RESPONSE =
            new byte[] {69, 79, -1, -1, 68, 0, 0, 0, 2, 0, 4, 0, 28, 0, 0, 0, 3, 0, -1, -1, 52, 0,
                    0, 0, 22, 0, 0, 0, 76, 0, 111, 0, 119, 0, 32, 0, 108, 0, 101, 0, 118, 0, 101, 0,
                    108, 0, 32, 0, 101, 0, 114, 0, 114, 0, 111, 0, 114, 0, 32, 0, 48, 0, 120, 0, 54,
                    0, 97, 0, 56, 0, 48, 0, 0, 0, 0, 0};

    private static final byte[] TEST_KEY_HANDLE = BaseEncoding.base16().decode(
            "0506070805060708050607080506070805060708050607080506070805060709");
    private static final String TEST_ENCODED_KEY_HANDLE = Base64.encodeToString(
            TEST_KEY_HANDLE, Base64.URL_SAFE | Base64.NO_PADDING | Base64.NO_WRAP);
    private static final byte[] TEST_ATTESTATION_OBJECT = new byte[] {-93, 99, 102, 109, 116, 100,
            110, 111, 110, 101, 103, 97, 116, 116, 83, 116, 109, 116, -96, 104, 97, 117, 116, 104,
            68, 97, 116, 97, 88, -60, 38, -67, 114, 120, -66, 70, 55, 97, -15, -6, -95, -79, 10,
            -76, -60, -8, 38, 112, 38, -100, 65, 12, 114, 106, 31, -42, -32, 88, 85, -31, -101, 70,
            65, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 124, 80, -60,
            -114, 69, -117, 44, -120, 122, -62, 63, 104, 18, -66, 2, -3, -56, 35, -24, 66, -4, 74,
            48, -128, -52, 80, -100, 46, 97, 93, -25, -21, -53, 40, 123, 90, -107, -20, 111, -4, 15,
            64, 122, 15, -84, -21, -33, -15, 26, 11, 35, 36, -49, 116, 52, -74, 107, 63, 113, -59,
            125, -27, -120, -63, -91, 1, 2, 3, 38, 32, 1, 33, 88, 32, -75, -80, 118, 102, -14, 124,
            -108, -9, -27, -91, 59, -48, -92, -102, -38, -44, 92, 95, 14, -62, 41, -117, -70, 101,
            9, 64, 35, 31, -20, 79, -71, -71, 34, 88, 32, -24, -33, 64, 97, -31, -34, 96, -83, -119,
            -25, 21, -14, -56, -70, -37, -116, -21, -33, -128, -66, 61, 41, 107, 16, -25, 120, 106,
            -113, 54, -62, -102, 42};
    private static final byte[] TEST_CLIENT_DATA_JSON = new byte[] {4, 5, 6};
    private static final byte[] TEST_AUTHENTICATOR_DATA = new byte[] {7, 8, 9};
    private static final byte[] TEST_SIGNATURE = new byte[] {10, 11, 12};
    private static final long TIMEOUT_SAFETY_MARGIN_MS = scaleTimeout(TimeUnit.SECONDS.toMillis(1));
    private static final long TIMEOUT_MS = scaleTimeout(TimeUnit.SECONDS.toMillis(1));
    private static final String FIDO2_KEY_CREDENTIAL_EXTRA = "FIDO2_CREDENTIAL_EXTRA";
    private static final int[] TEST_USER_VERIFICATION_METHOD = new int[] {0x00000002, 0x00000200};
    private static final short[] TEST_KEY_PROTECTION_TYPE = new short[] {0x0002, 0x0001};
    private static final short[] TEST_MATCHER_PROTECTION_TYPE = new short[] {0x0004, 0x0001};
    private static Url createUrl(String s) {
        Url url = new Url();
        url.url = s;
        return url;
    }

    /**
     * Builds a test intent to be returned by a successful call to makeCredential.
     * @return Intent containing the response from the Fido2 API.
     */
    public static Intent createSuccessfulMakeCredentialIntent() {
        Intent intent = new Intent();
        intent.putExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA, TEST_AUTHENTICATOR_ATTESTATION_RESPONSE);
        return intent;
    }

    /**
     * Construct default options for a makeCredential request.
     * @return Options for the Fido2 API.
     * @throws Exception
     */
    public static PublicKeyCredentialCreationOptions createDefaultMakeCredentialOptions()
            throws Exception {
        PublicKeyCredentialCreationOptions options = new PublicKeyCredentialCreationOptions();
        options.challenge = "climb a mountain".getBytes("UTF8");

        options.relyingParty = new PublicKeyCredentialRpEntity();
        options.relyingParty.id = "subdomain.example.test";
        options.relyingParty.name = "Acme";
        options.relyingParty.icon = createUrl("https://icon.example.test");

        options.user = new PublicKeyCredentialUserEntity();
        options.user.id = "1098237235409872".getBytes("UTF8");
        options.user.name = "avery.a.jones@example.com";
        options.user.displayName = "Avery A. Jones";
        options.user.icon = createUrl("https://usericon.example.test");

        options.timeout = new TimeDelta();
        options.timeout.microseconds = TimeUnit.MILLISECONDS.toMicros(TIMEOUT_MS);

        PublicKeyCredentialParameters parameters = new PublicKeyCredentialParameters();
        parameters.algorithmIdentifier = -7;
        parameters.type = PublicKeyCredentialType.PUBLIC_KEY;
        options.publicKeyParameters = new PublicKeyCredentialParameters[] {parameters};

        PublicKeyCredentialDescriptor descriptor = new PublicKeyCredentialDescriptor();
        descriptor.type = 0;
        descriptor.id = new byte[] {8, 7, 6};
        descriptor.transports = new int[] {0};
        options.excludeCredentials = new PublicKeyCredentialDescriptor[] {descriptor};

        options.authenticatorSelection = new AuthenticatorSelectionCriteria();
        /* TODO add UserVerificationRequirement and ResidentKeyRequirement when the FIDO2 API
         * supports it */
        options.authenticatorSelection.authenticatorAttachment =
                AuthenticatorAttachment.CROSS_PLATFORM;
        return options;
    }

    /**
     * Verifies that the returned response matches expected values.
     * @param response The response from the Fido2 API.
     */
    public static void validateMakeCredentialResponse(
            MakeCredentialAuthenticatorResponse response) {
        Assert.assertArrayEquals(response.attestationObject, TEST_ATTESTATION_OBJECT);
        Assert.assertArrayEquals(response.info.rawId, TEST_KEY_HANDLE);
        Assert.assertEquals(response.info.id, TEST_ENCODED_KEY_HANDLE);
        Assert.assertArrayEquals(response.info.clientDataJson, TEST_CLIENT_DATA_JSON);
    }

    /**
     * Constructs default options for a getAssertion request.
     * @return Options for the Fido2 API
     * @throws Exception
     */
    public static PublicKeyCredentialRequestOptions createDefaultGetAssertionOptions()
            throws Exception {
        PublicKeyCredentialRequestOptions options = new PublicKeyCredentialRequestOptions();
        options.challenge = "climb a mountain".getBytes("UTF8");
        options.timeout = new TimeDelta();
        options.timeout.microseconds = TimeUnit.MILLISECONDS.toMicros(TIMEOUT_MS);
        options.relyingPartyId = "subdomain.example.test";

        PublicKeyCredentialDescriptor descriptor = new PublicKeyCredentialDescriptor();
        descriptor.type = 0;
        descriptor.id = new byte[] {8, 7, 6};
        descriptor.transports = new int[] {0};
        options.allowCredentials = new PublicKeyCredentialDescriptor[] {descriptor};

        options.cableAuthenticationData = new CableAuthentication[] {};
        options.prfInputs = new PrfValues[] {};
        return options;
    }

    /**
     * Builds a test intent without uvm extension to be returned by a successful call to
     * makeCredential.
     * @return Intent containing the response from the Fido2 API.
     */
    public static Intent createSuccessfulGetAssertionIntent() {
        Intent intent = new Intent();
        intent.putExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA, TEST_AUTHENTICATOR_ASSERTION_RESPONSE);
        return intent;
    }

    /**
     * Builds a test intent with uvm extension to be returned by a successful call to
     * makeCredential.
     * @return Intent containing the response from the Fido2 API.
     */
    public static Intent createSuccessfulGetAssertionIntentWithUvm() {
        Intent intent = new Intent();
        intent.putExtra(FIDO2_KEY_CREDENTIAL_EXTRA, TEST_ASSERTION_PUBLIC_KEY_CREDENTIAL_WITH_UVM);
        return intent;
    }

    /**
     * Verifies that the returned userVerificationMethod matches expected values.
     * @param userVerificationMethods The userVerificationMethods from the Fido2 API.
     */
    public static void validateUserVerificationMethods(
            boolean echoUserVerificationMethods, UvmEntry[] userVerificationMethods) {
        if (echoUserVerificationMethods) {
            Assert.assertEquals(
                    userVerificationMethods.length, TEST_USER_VERIFICATION_METHOD.length);
            for (int i = 0; i < userVerificationMethods.length; i++) {
                Assert.assertEquals(userVerificationMethods[i].userVerificationMethod,
                        TEST_USER_VERIFICATION_METHOD[i]);
                Assert.assertEquals(
                        userVerificationMethods[i].keyProtectionType, TEST_KEY_PROTECTION_TYPE[i]);
                Assert.assertEquals(userVerificationMethods[i].matcherProtectionType,
                        TEST_MATCHER_PROTECTION_TYPE[i]);
            }
        }
    }

    /**
     * Verifies that the returned response matches expected values.
     * @param response The response from the Fido2 API.
     */
    public static void validateGetAssertionResponse(GetAssertionAuthenticatorResponse response) {
        Assert.assertArrayEquals(response.info.authenticatorData, TEST_AUTHENTICATOR_DATA);
        Assert.assertArrayEquals(response.signature, TEST_SIGNATURE);
        Assert.assertArrayEquals(response.info.rawId, TEST_KEY_HANDLE);
        Assert.assertEquals(response.info.id, TEST_ENCODED_KEY_HANDLE);
        Assert.assertArrayEquals(response.info.clientDataJson, TEST_CLIENT_DATA_JSON);
        validateUserVerificationMethods(
                response.echoUserVerificationMethods, response.userVerificationMethods);
    }

    /**
     * Verifies that the response did not return before timeout.
     * @param startTimeMs The start time of the operation.
     */
    public static void verifyRespondedBeforeTimeout(long startTimeMs) {
        long elapsedTime = SystemClock.elapsedRealtime() - startTimeMs;
        Assert.assertTrue(elapsedTime < TIMEOUT_MS);
    }

    /**
     * Generates error response byte array with error message that only differs by a single
     * errorCode byte.
     * @return Error response byte array.
     */
    private static byte[] generateErrorResponseBytesWithErrorMessage(Integer errorCode) {
        byte errorByte = errorCode.byteValue();
        ByteArrayOutputStream error_response_output = new ByteArrayOutputStream();
        try {
            error_response_output.write(TEST_ERROR_WITH_FILLER_ERROR_MSG_RESPONSE_FRONT);
        } catch (IOException e) {
            e.printStackTrace();
        }
        try {
            error_response_output.write(new byte[] {errorByte});
        } catch (IOException e) {
            e.printStackTrace();
        }
        try {
            error_response_output.write(TEST_ERROR_WITH_FILLER_ERROR_MSG_RESPONSE_TAIL);
        } catch (IOException e) {
            e.printStackTrace();
        }
        return error_response_output.toByteArray();
    }

    /**
     * Generates error response byte array without error message that only differs by a single
     * errorCode byte.
     * @return Error response byte array.
     */
    private static byte[] generateErrorResponseBytesWithoutErrorMessage(Integer errorCode) {
        byte errorByte = errorCode.byteValue();
        ByteArrayOutputStream error_response_output = new ByteArrayOutputStream();
        try {
            error_response_output.write(TEST_ERROR_WITH_NULL_ERROR_MSG_RESPONSE_FRONT);
        } catch (IOException e) {
            e.printStackTrace();
        }
        try {
            error_response_output.write(new byte[] {errorByte});
        } catch (IOException e) {
            e.printStackTrace();
        }
        try {
            error_response_output.write(TEST_ERROR_WITH_NULL_ERROR_MSG_RESPONSE_TAIL);
        } catch (IOException e) {
            e.printStackTrace();
        }
        return error_response_output.toByteArray();
    }

    /**
     * Constructs corresponding error response byte array based on errorCode and errorMsg.
     * @return Error response byte array.
     */
    private static byte[] constructErrorResponseBytes(
            ErrorCode errorCode, @Nullable String errorMsg) {
        if (errorMsg == null) {
            return generateErrorResponseBytesWithoutErrorMessage(errorCode.getCode());
        }
        if (FILLER_ERROR_MSG.equals(errorMsg)) {
            return generateErrorResponseBytesWithErrorMessage(errorCode.getCode());
        }
        switch (errorMsg) {
            case "The device is not secured with any screen lock":
                return TEST_CONSTRAINTERROR_NOSCREENLOCK_RESPONSE;
            case "One of the excluded credentials exists on the local device":
                return TEST_INVALIDSTATEERROR_DUPLICATE_REGISTRATION_RESPONSE;
            case "Authentication request must have non-empty allowList":
                return TEST_EMPTYALLOWCRED_RESPONSE1;
            case "Request doesn't have a valid list of allowed credentials.":
                return TEST_EMPTYALLOWCRED_RESPONSE2;
            case "Low level error 0x6a80":
                return TEST_UNKNOWNERROR_CRED_NOT_RECOGNIZED_RESPONSE;
            default:
                return new byte[] {};
        }
    }

    /**
     * Constructs an intent that returns an error response from the Fido2 API.
     * @param errorCode Numeric values corresponding to a Fido2 error.
     * @return an Intent containing the error response.
     */
    public static Intent createErrorIntent(ErrorCode errorCode, @Nullable String errorMsg) {
        Intent intent = new Intent();
        intent.putExtra(
                Fido.FIDO2_KEY_ERROR_EXTRA, constructErrorResponseBytes(errorCode, errorMsg));
        return intent;
    }
}
