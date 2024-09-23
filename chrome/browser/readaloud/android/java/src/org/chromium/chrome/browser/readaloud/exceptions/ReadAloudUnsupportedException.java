// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.exceptions;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.readaloud.exceptions.ReadAloudException.ReadAloudErrorCode;

/** A ReadAloudException representing an issue with readability. */
public class ReadAloudUnsupportedException extends ReadAloudException {

    @IntDef({
        RejectionReason.UNKNOWN_REJECTION_REASON, RejectionReason.PAGE_DOWNLOAD_FAILURE,
                RejectionReason.UNSUPPORTED_VOICE, RejectionReason.UNSUPPORTED_LANGUAGE,
                RejectionReason.PAYWALL, RejectionReason.PORN, RejectionReason.VIDEO,
                RejectionReason.NON_ARTICLE,
        RejectionReason.TEXT_TOO_SHORT, RejectionReason.NON_TEXTUAL,
                RejectionReason.UNSUPPORTED_ARTICLE_QUALITY, RejectionReason.URL_BLACKLISTED,
                RejectionReason.CONTENT_TOO_LARGE, RejectionReason.INVALID_URL,
                RejectionReason.DISALLOWED_FOR_READOUT, RejectionReason.BAD_REQUEST,
        RejectionReason.EXPIRED_CONTENT_VERSION, RejectionReason.UNSUPPORTED_EMAIL_FORMAT,
                RejectionReason.UNSUPPORTED_TUPLE_DATA_EXTRACTION,
                RejectionReason.UNSUPPORTED_LATTICE_REVIEW_DATA_EXTRACTION,
                RejectionReason.DISALLOWED_FOR_TRANSLATION, RejectionReason.COUNT
    })
    // This must be kept in sync with readaloud/enums.xml values
    public @interface RejectionReason {
        int UNKNOWN_REJECTION_REASON = 0;
        // The page cannot be fetched.
        int PAGE_DOWNLOAD_FAILURE = 1;
        // The specified voice is not supported in general, not supported for the user
        // or does not match the page language.
        int UNSUPPORTED_VOICE = 2;
        // The specified language is not supported.
        int UNSUPPORTED_LANGUAGE = 3;
        // The page contains paid content.
        int PAYWALL = 4;
        // Porn is not allowed.
        int PORN = 5;
        // Video content, not suitable for narration even if it has short text in
        // addition to the video.
        int VIDEO = 6;
        // The page cannot be parsed to an article, for whatever reason.
        int NON_ARTICLE = 7;
        // The text is too short.
        int TEXT_TOO_SHORT = 8;
        // The content is classified as data (rather than textual).
        int NON_TEXTUAL = 9;
        // The article quality isn't sufficient for the user.
        int UNSUPPORTED_ARTICLE_QUALITY = 10;
        // The URL is blacklisted.
        int URL_BLACKLISTED = 11;
        // The content is too large.
        int CONTENT_TOO_LARGE = 12;
        // The URL format is invalid or unsupported.
        int INVALID_URL = 13;
        // The publisher has explicitly marked the page so that it shouldn't be read
        // aloud.
        int DISALLOWED_FOR_READOUT = 14;
        // A generic code for cases where the input is invalid.
        int BAD_REQUEST = 15;
        // The requested content version has expired and is no longer available.
        int EXPIRED_CONTENT_VERSION = 16;
        // We don't support this kind of email.
        int UNSUPPORTED_EMAIL_FORMAT = 17;
        // There is no Tuple data available for this url.
        int UNSUPPORTED_TUPLE_DATA_EXTRACTION = 18;
        // There is no LatticeReview data available for this url.
        int UNSUPPORTED_LATTICE_REVIEW_DATA_EXTRACTION = 19;
        // The content is not allowed to be translated.
        int DISALLOWED_FOR_TRANSLATION = 20;
        int COUNT = 21;
    }

    private final @RejectionReason int mRejectionReason;

    public ReadAloudUnsupportedException(
            String message, @Nullable Throwable cause, @RejectionReason int rejectionReason) {
        super(message, cause, ReadAloudErrorCode.FAILED_PRECONDITION);
        mRejectionReason = rejectionReason;
    }

    /** Returns the rejection reason. */
    public @RejectionReason int getRejectionReason() {
        return mRejectionReason;
    }
}
