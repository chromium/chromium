// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.autofill.LegalMessageLine;

import java.util.LinkedList;

/**
 * Class to represent the fields required to show the {@link AutofillVirtualCardEnrollmentDialog}
 */
@JNINamespace("autofill")
public class VirtualCardEnrollmentFields {
    @VisibleForTesting
    final LinkedList<LegalMessageLine> mGoogleLegalMessages = new LinkedList<>();
    @VisibleForTesting
    final LinkedList<LegalMessageLine> mIssuerLegalMessages = new LinkedList<>();
    private final Bitmap mIssuerCardArt;
    private final String mCardIdentifierString;

    public VirtualCardEnrollmentFields(String cardIdentifierString, Bitmap issuerCardArt) {
        mCardIdentifierString = cardIdentifierString;
        mIssuerCardArt = issuerCardArt;
    }

    public Bitmap getIssuerCardArt() {
        return mIssuerCardArt;
    }

    public String getCardIdentifierString() {
        return mCardIdentifierString;
    }

    public LinkedList<LegalMessageLine> getGoogleLegalMessages() {
        return mGoogleLegalMessages;
    }

    public LinkedList<LegalMessageLine> getIssuerLegalMessages() {
        return mIssuerLegalMessages;
    }

    /**
     * Returns an instance of {@link VirtualCardEnrollmentFields}.
     *
     * @param cardIdentifierString The text to be displayed in the enrollment dialog to help with
     *         identifying the card.
     * @param issuerCardArt The image associated with the card being enrolled.
     */
    @CalledByNative
    @VisibleForTesting
    static VirtualCardEnrollmentFields create(String cardIdentifierString, Bitmap issuerCardArt) {
        return new VirtualCardEnrollmentFields(cardIdentifierString, issuerCardArt);
    }

    /**
     * Adds a line of Google legal message plain text.
     *
     * @param text The legal message plain text.
     */
    @CalledByNative
    private void addGoogleLegalMessageLine(String text) {
        mGoogleLegalMessages.add(new LegalMessageLine(text));
    }

    /**
     * Marks up the last added line of the Google legal message text with a link.
     *
     * @param start The inclusive offset of the start of the link in the text.
     * @param end The exclusive offset of the end of the link in the text.
     * @param url The URL to open when the link is clicked.
     */
    @CalledByNative
    private void addLinkToLastGoogleLegalMessageLine(int start, int end, String url) {
        mGoogleLegalMessages.getLast().links.add(new LegalMessageLine.Link(start, end, url));
    }

    /**
     * Adds a line of issuer legal message plain text.
     *
     * @param text The legal message plain text.
     */
    @CalledByNative
    private void addIssuerLegalMessageLine(String text) {
        mIssuerLegalMessages.add(new LegalMessageLine(text));
    }

    /**
     * Marks up the last added line of the issuer legal message text with a link.
     *
     * @param start The inclusive offset of the start of the link in the text.
     * @param end The exclusive offset of the end of the link in the text.
     * @param url The URL to open when the link is clicked.
     */
    @CalledByNative
    private void addLinkToLastIssuerLegalMessageLine(int start, int end, String url) {
        mIssuerLegalMessages.getLast().links.add(new LegalMessageLine.Link(start, end, url));
    }
}
