// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeClassQualifiedName;
import org.jni_zero.NativeMethods;

/**
 * Provides a context in which to search, and links to the native ContextualSearchContext.
 * Includes the selection, selection offsets, surrounding page content, etc.
 * Requires an override of #onSelectionChanged to call when a non-empty selection is established
 * or changed.
 */
public abstract class ContextualSearchContext {
    static final int INVALID_OFFSET = -1;

    // Non-visible word-break marker.
    private static final int SOFT_HYPHEN_CHAR = '\u00AD';

    // Pointer to the native instance of this class.
    private long mNativePointer;

    // Whether this context has had the required properties set so it can Resolve a Search Term.
    private boolean mHasSetResolveProperties;

    // A shortened version of the actual text content surrounding the selection, or null if not yet
    // established.
    private String mSurroundingText;

    // The start and end offsets of the selection within the text content.
    private int mSelectionStartOffset = INVALID_OFFSET;
    private int mSelectionEndOffset = INVALID_OFFSET;

    // The detected language of the context, or {@code null} if not yet detected, and empty if
    // it cannot be reliably determined.
    private String mDetectedLanguage;

    // The offset of an initial Tap gesture within the text content.
    private int mTapOffset = INVALID_OFFSET;

    // The selection being resolved, or null if no resolve has started.
    private String mSelectionBeingResolved;

    // The original encoding of the base page.
    private String mEncoding;

    // The word that was tapped, as analyzed internally before selection takes place,
    // or {@code null} if no analysis has been done yet.
    private String mWordTapped;

    // The offset of the tapped word within the surrounding text or {@code INVALID_OFFSET} if not
    // yet analyzed.

    // The offset of the tap within the tapped word, or {@code INVALID_OFFSET} if not yet analyzed.
    private int mTapWithinWordOffset = INVALID_OFFSET;

    // Translation members.
    private @NonNull String mTargetLanguage = "";
    private @NonNull String mFluentLanguages = "";

    // The Related Searches stamp - non-empty when Related Searches are being requested.
    private String mRelatedSearchesStamp;

    /** Constructs a context that tracks the selection and some amount of page content. */
    ContextualSearchContext() {
        mNativePointer = ContextualSearchContextJni.get().init(this);
        mHasSetResolveProperties = false;
    }

    /**
     * Updates a context to be able to resolve a search term and have a large amount of
     * page content.
     * @param homeCountry The country where the user usually resides, or an empty string if not
     *        known.
     * @param doSendBasePageUrl Whether the base-page URL should be sent to the server.
     * @param targetLanguage The language to translate into, in case translation might be needed.
     * @param fluentLanguages An ordered comma-separated list of ISO 639 language codes that
     *        the user can read fluently, or an empty string.
     */
    void setResolveProperties(
            @NonNull String homeCountry,
            boolean doSendBasePageUrl,
            @NonNull String targetLanguage,
            @NonNull String fluentLanguages) {
        // TODO(donnd): consider making this a constructor variation.
        mHasSetResolveProperties = true;
        ContextualSearchContextJni.get()
                .setResolveProperties(getNativePointer(), this, homeCountry, doSendBasePageUrl);
        mTargetLanguage = targetLanguage;
        mFluentLanguages = fluentLanguages;
    }

    /**
     * This method should be called to clean up storage when an instance of this class is
     * no longer in use.  The ContextualSearchContextJni.get().destroy will call the destructor on
     * the native instance.
     */
    void destroy() {
        assert mNativePointer != 0;
        ContextualSearchContextJni.get().destroy(mNativePointer, this);
        mNativePointer = 0;

        // Also zero out private data that may be sizable.
        mSurroundingText = null;
    }

    /**
     * Sets the surrounding text and selection offsets assuming UTF-8 and no insertion-point
     * support.
     * @param surroundingText The text from the base page surrounding the selection.
     * @param startOffset The offset of start the selection.
     * @param endOffset The offset of the end of the selection
     */
    @VisibleForTesting
    void setSurroundingText(String surroundingText, int startOffset, int endOffset) {
        setSurroundingText("UTF-8", surroundingText, startOffset, endOffset);
    }

    /**
     * Sets the surrounding text and selection offsets.
     * @param encoding The original encoding of the base page.
     * @param surroundingText The text from the base page surrounding the selection.
     * @param startOffset The offset of start the selection.
     * @param endOffset The offset of the end of the selection
     */
    @VisibleForTesting
    void setSurroundingText(
            String encoding, String surroundingText, int startOffset, int endOffset) {
        assert startOffset <= endOffset;
        mEncoding = encoding;
        mSurroundingText = surroundingText;
        mSelectionStartOffset = startOffset;
        mSelectionEndOffset = endOffset;
        if (startOffset == endOffset
                && startOffset <= surroundingText.length()
                && !hasAnalyzedTap()) {
            analyzeTap(startOffset);
        }
        // Notify of a changed selection if it's not empty.
        if (endOffset > startOffset) {
            onSelectionChanged();
        }
        // Detect the language of the surroundings or the selection.
        setTranslationLanguages(getDetectedLanguage(), mTargetLanguage, mFluentLanguages);
    }

    /**
     * @return The text that surrounds the selection, or {@code null} if none yet known.
     */
    @Nullable
    String getSurroundingText() {
        return mSurroundingText;
    }

    /**
     * @return The offset into the surrounding text of the start of the selection, or
     *         {@link #INVALID_OFFSET} if not yet established.
     */
    int getSelectionStartOffset() {
        return mSelectionStartOffset;
    }

    /**
     * @return The offset into the surrounding text of the end of the selection, or
     *         {@link #INVALID_OFFSET} if not yet established.
     */
    int getSelectionEndOffset() {
        return mSelectionEndOffset;
    }

    /**
     * @return The original encoding of the base page.
     */
    String getEncoding() {
        return mEncoding;
    }

    /**
     * @return The selection being resolved, or {@code null} if no resolve has been
     * requested.
     */
    @Nullable
    String getSelectionBeingResolved() {
        return mSelectionBeingResolved;
    }

    /**
     * @return The text content that follows the selection (one side of the surrounding text).
     */
    String getTextContentFollowingSelection() {
        if (mSurroundingText != null
                && mSelectionEndOffset > 0
                && mSelectionEndOffset <= mSurroundingText.length()) {
            return mSurroundingText.substring(mSelectionEndOffset);
        } else {
            return "";
        }
    }

    /**
     * @return Whether this context can Resolve the Search Term.
     */
    boolean canResolve() {
        return mHasSetResolveProperties && hasValidSelection();
    }

    /**
     * Prepares the Context to be used in a Resolve request by supplying last minute parameters.
     * If this call is not made before a Resolve then defaults are used (not exact and not a
     * Related Search).
     * @param isExactSearch Specifies whether this search must be exact -- meaning the resolve must
     *        return a non-expanding result that matches the selection exactly.
     * @param relatedSearchesStamp Information to be attached to the Resolve request that is needed
     *        for Related Searches. If this string is empty then no Related Searches results will
     *        be requested.
     */
    void prepareToResolve(boolean isExactSearch, String relatedSearchesStamp) {
        mSelectionBeingResolved = getCurrentSelection();
        mRelatedSearchesStamp = relatedSearchesStamp;
        ContextualSearchContextJni.get()
                .prepareToResolve(mNativePointer, this, isExactSearch, relatedSearchesStamp);
    }

    /**
     * Notifies of an adjustment that has been applied to the start and end of the selection.
     * @param startAdjust A signed value indicating the direction of the adjustment to the start of
     *        the selection (typically a negative value when the selection expands).
     * @param endAdjust A signed value indicating the direction of the adjustment to the end of
     *        the selection (typically a positive value when the selection expands).
     */
    void onSelectionAdjusted(int startAdjust, int endAdjust) {
        // Fully track the selection as it changes.
        mSelectionStartOffset += startAdjust;
        mSelectionEndOffset += endAdjust;
        ContextualSearchContextJni.get()
                .adjustSelection(getNativePointer(), this, startAdjust, endAdjust);
        // Notify of changes.
        onSelectionChanged();
    }

    /** Returns the current selection, or an empty string if there is no valid selection. */
    private String getCurrentSelection() {
        if (TextUtils.isEmpty(mSurroundingText)
                || mSelectionEndOffset < mSelectionStartOffset
                || mSelectionStartOffset < 0
                || mSelectionEndOffset > mSurroundingText.length()) {
            return "";
        }
        return mSurroundingText.substring(mSelectionStartOffset, mSelectionEndOffset);
    }

    /** @return the current selection, or an empty string if data is invalid or nothing selected. */
    String getSelection() {
        if (TextUtils.isEmpty(mSurroundingText)
                || mSelectionEndOffset < mSelectionStartOffset
                || mSelectionStartOffset < 0
                || mSelectionEndOffset > mSurroundingText.length()) {
            return "";
        }
        return mSurroundingText.substring(mSelectionStartOffset, mSelectionEndOffset);
    }

    /** Notifies this instance that the selection has been changed. */
    abstract void onSelectionChanged();

    /**
     * Gets the language of the current context's content by calling the native CLD3 detector if
     * needed.
     * @return An ISO 639 language code string, or an empty string if the language cannot be
     *         reliably determined.
     */
    @NonNull
    String getDetectedLanguage() {
        assert mSurroundingText != null;
        if (mDetectedLanguage == null) {
            mDetectedLanguage =
                    ContextualSearchContextJni.get().detectLanguage(mNativePointer, this);
        }
        return mDetectedLanguage;
    }

    /**
     * Pushes the given languages down to the native ContextualSearchContext.
     * @param detectedLanguage An ISO 639 language code string for the language to translate from.
     * @param targetLanguage An ISO 639 language code string to translation into.
     * @param fluentLanguages An ordered comma-separated list of ISO 639 language codes that
     *        the user can read fluently, or an empty string.
     */
    @VisibleForTesting
    void setTranslationLanguages(
            @NonNull String detectedLanguage,
            @NonNull String targetLanguage,
            @NonNull String fluentLanguages) {
        // Set redundant languages to empty strings.
        fluentLanguages = targetLanguage.equals(fluentLanguages) ? "" : fluentLanguages;
        // The target language is essential in order to provide results the user can read, and if
        // not specified the server may fallback onto a guess based on location, which isn't
        // always a good experience.
        ContextualSearchContextJni.get()
                .setTranslationLanguages(
                        mNativePointer, this, detectedLanguage, targetLanguage, fluentLanguages);
    }

    // ============================================================================================
    // Content Analysis.
    // ============================================================================================

    /**
     * @return Whether this context has valid Surrounding text and initial Tap offset.
     */
    @VisibleForTesting
    boolean hasValidTappedText() {
        return !TextUtils.isEmpty(mSurroundingText)
                && mTapOffset >= 0
                && mTapOffset <= mSurroundingText.length();
    }

    /**
     * @return Whether this context has a valid selection, which may be an insertion point.
     */
    @VisibleForTesting
    boolean hasValidSelection() {
        return !TextUtils.isEmpty(mSurroundingText)
                && mSelectionStartOffset != INVALID_OFFSET
                && mSelectionEndOffset != INVALID_OFFSET
                && mSelectionStartOffset < mSelectionEndOffset
                && mSelectionEndOffset < mSurroundingText.length();
    }

    /**
     * @return Whether a Tap gesture has occurred and been analyzed.
     */
    @VisibleForTesting
    boolean hasAnalyzedTap() {
        return mTapOffset >= 0;
    }

    /**
     * @return The word tapped, or {@code null} if the word that was tapped cannot be identified by
     *         the current limited parsing capability.
     * @see #analyzeTap(int)
     */
    String getWordTapped() {
        return mWordTapped;
    }

    /**
     * @return The offset of the tap within the tapped word, or {@code INVALID_OFFSET} if the tapped
     *         word cannot be identified by the current parsing capability.
     * @see #analyzeTap(int)
     */
    int getTapOffsetWithinTappedWord() {
        return mTapWithinWordOffset;
    }

    /**
     * Finds the words around the initial Tap offset by expanding and looking for word-breaks.
     * This mimics the Blink word-segmentation invoked by SelectWordAroundCaret and similar
     * selection logic, but is only appropriate for limited use.  Does not work on ideographic
     * languages and possibly many other cases.  Should only be used only for ML signal evaluation.
     * @param tapOffset The offset of the Tap within the surrounding text.
     */
    private void analyzeTap(int tapOffset) {
        mTapOffset = tapOffset;
        mWordTapped = null;
        mTapWithinWordOffset = INVALID_OFFSET;

        assert hasValidTappedText();

        int wordStartOffset = findWordStartOffset(mTapOffset);
        int wordEndOffset = findWordEndOffset(mTapOffset);
        if (wordStartOffset == INVALID_OFFSET || wordEndOffset == INVALID_OFFSET) return;

        mWordTapped = mSurroundingText.substring(wordStartOffset, wordEndOffset);
        mTapWithinWordOffset = mTapOffset - wordStartOffset;
    }

    /**
     * @return The start of the word that contains the given initial offset, within the surrounding
     *         text, or {@code INVALID_OFFSET} if not found.
     */
    private int findWordStartOffset(int initial) {
        // Scan before, aborting if we hit any ideographic letter.
        for (int offset = initial - 1; offset >= 0; offset--) {
            if (isWordBreakAtIndex(offset)) {
                // The start of the word is after this word break.
                return offset + 1;
            }
        }

        return INVALID_OFFSET;
    }

    /**
     * Finds the offset of the end of the word that includes the given initial offset.
     * NOTE: this is the index of the character just past the last character of the word,
     * so a 3 character word "who" has start index 0 and end index 3.
     * The character at the initial offset is examined and each one after that too until a non-word
     * character is encountered, and that offset will be returned.
     * @param initial The initial offset to scan from.
     * @return The end of the word that contains the given initial offset, within the surrounding
     *         text.
     */
    private int findWordEndOffset(int initial) {
        // Scan after, aborting if we hit any CJKV letter.
        for (int offset = initial; offset < mSurroundingText.length(); offset++) {
            if (isWordBreakAtIndex(offset)) {
                // The end of the word is the offset of this word break.
                return offset;
            }
        }
        return INVALID_OFFSET;
    }

    /**
     * @return Whether the character at the given index is a word-break.
     */
    private boolean isWordBreakAtIndex(int index) {
        return !Character.isLetterOrDigit(mSurroundingText.charAt(index))
                && mSurroundingText.charAt(index) != SOFT_HYPHEN_CHAR;
    }

    // ============================================================================================
    // Test support.
    // ============================================================================================

    @VisibleForTesting
    String getRelatedSearchesStamp() {
        return mRelatedSearchesStamp;
    }

    // ============================================================================================
    // Native callback support.
    // ============================================================================================

    @CalledByNative
    private long getNativePointer() {
        assert mNativePointer != 0;
        return mNativePointer;
    }

    @NativeMethods
    interface Natives {
        @NativeClassQualifiedName("NativeContextualSearchContext")
        long init(ContextualSearchContext caller);

        @NativeClassQualifiedName("NativeContextualSearchContext")
        void destroy(long nativeContextualSearchContext, ContextualSearchContext caller);

        @NativeClassQualifiedName("NativeContextualSearchContext")
        void setResolveProperties(
                long nativeContextualSearchContext,
                ContextualSearchContext caller,
                String homeCountry,
                boolean doSendBasePageUrl);

        @NativeClassQualifiedName("NativeContextualSearchContext")
        void adjustSelection(
                long nativeContextualSearchContext,
                ContextualSearchContext caller,
                int startAdjust,
                int endAdjust);

        @NativeClassQualifiedName("NativeContextualSearchContext")
        String detectLanguage(long nativeContextualSearchContext, ContextualSearchContext caller);

        @NativeClassQualifiedName("NativeContextualSearchContext")
        void setTranslationLanguages(
                long nativeContextualSearchContext,
                ContextualSearchContext caller,
                String detectedLanguage,
                String targetLanguage,
                String fluentLanguages);

        @NativeClassQualifiedName("NativeContextualSearchContext")
        void prepareToResolve(
                long nativeContextualSearchContext,
                ContextualSearchContext caller,
                boolean isExactSearch,
                String relatedSearchesStamp);
    }
}
