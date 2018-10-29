// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

/**
 * Structured representation of the JSON payload of a suggestion with an answer.  An answer has
 * exactly two image lines, so called because they are a combination of text and an optional
 * image.  Each image line has 0 or more text fields, each of which is required to contain a string
 * and an integer type.  The text fields are contained in a list and two optional named properties,
 * referred to as "additional text" and "status text".  The image, if present, contains a single
 * string, which may be a URL or base64-encoded image data.
 *
 * When represented in the UI, these elements should be styled and laid out according to the
 * specification at http://goto.google.com/ais_api.
 */
public class SuggestionAnswer {
    private static final String TAG = "SuggestionAnswer";

    private ImageLine mFirstLine;
    private ImageLine mSecondLine;

    private static final String ANSWERS_JSON_LINE = "l";
    private static final String ANSWERS_JSON_IMAGE_LINE = "il";
    private static final String ANSWERS_JSON_TEXT = "t";
    private static final String ANSWERS_JSON_ADDITIONAL_TEXT = "at";
    private static final String ANSWERS_JSON_STATUS_TEXT = "st";
    private static final String ANSWERS_JSON_TEXT_TYPE = "tt";
    private static final String ANSWERS_JSON_IMAGE = "i";
    private static final String ANSWERS_JSON_IMAGE_DATA = "d";
    private static final String ANSWERS_JSON_NUMBER_OF_LINES = "ln";

    private SuggestionAnswer() {}

    /**
     * Parses the JSON representation of an answer and constructs a SuggestionAnswer from the
     * contents.
     *
     * @param answerContents The JSON representation of an answer.
     * @return A SuggestionAnswer with the answer contents or null if the contents are malformed or
     *         missing required elements.
     */
    public static SuggestionAnswer parseAnswerContents(String answerContents) {
        SuggestionAnswer answer = new SuggestionAnswer();

        try {
            JSONObject jsonAnswer = new JSONObject(answerContents);
            JSONArray jsonLines = jsonAnswer.getJSONArray(ANSWERS_JSON_LINE);

            if (jsonLines.length() != 2) {
                Log.e(TAG, "Answer JSON doesn't contain exactly two lines: " + jsonAnswer);
                return null;
            }

            answer.mFirstLine = new SuggestionAnswer.ImageLine(
                    jsonLines.getJSONObject(0).getJSONObject(ANSWERS_JSON_IMAGE_LINE));
            answer.mSecondLine = new SuggestionAnswer.ImageLine(
                    jsonLines.getJSONObject(1).getJSONObject(ANSWERS_JSON_IMAGE_LINE));
        } catch (JSONException e) {
            Log.e(TAG, "Problem parsing answer JSON: " + e.getMessage());
            return null;
        }

        return answer;
    }

    /**
     * Returns the first of the two required image lines.
     */
    public ImageLine getFirstLine() {
        return mFirstLine;
    }

    /**
     * Returns the second of the two required image lines.
     */
    public ImageLine getSecondLine() {
        return mSecondLine;
    }

    /**
     * Represents a single line of an answer, containing any number of typed text fields and an
     * optional image.
     */
    public static class ImageLine {
        private final List<TextField> mTextFields;
        private final TextField mAdditionalText;
        private final TextField mStatusText;
        private final String mImage;

        ImageLine(JSONObject jsonLine) throws JSONException {
            mTextFields = new ArrayList<TextField>();

            JSONArray textValues = jsonLine.getJSONArray(ANSWERS_JSON_TEXT);
            for (int i = 0; i < textValues.length(); i++) {
                mTextFields.add(new TextField(textValues.getJSONObject(i)));
            }

            mAdditionalText = jsonLine.has(ANSWERS_JSON_ADDITIONAL_TEXT)
                    ? new TextField(jsonLine.getJSONObject(ANSWERS_JSON_ADDITIONAL_TEXT))
                    : null;

            mStatusText = jsonLine.has(ANSWERS_JSON_STATUS_TEXT)
                    ? new TextField(jsonLine.getJSONObject(ANSWERS_JSON_STATUS_TEXT))
                    : null;

            String jsonImageData = jsonLine.has(ANSWERS_JSON_IMAGE)
                    ? jsonLine.getJSONObject(ANSWERS_JSON_IMAGE).getString(ANSWERS_JSON_IMAGE_DATA)
                    : null;
            if (jsonImageData != null) {
                jsonImageData = "https:" + jsonImageData.replace("\\/", "/");
            }
            mImage = jsonImageData;
        }

        /**
         * Return an unnamed list of text fields.  These represent the main content of the line.
         */
        public List<TextField> getTextFields() {
            return mTextFields;
        }

        /**
         * Returns true if the line contains an "additional text" field.
         */
        public boolean hasAdditionalText() {
            return mAdditionalText != null;
        }

        /**
         * Return the "additional text" field.
         */
        public TextField getAdditionalText() {
            return mAdditionalText;
        }

        /**
         * Returns true if the line contains an "status text" field.
         */
        public boolean hasStatusText() {
            return mStatusText != null;
        }

        /**
         * Return the "status text" field.
         */
        public TextField getStatusText() {
            return mStatusText;
        }

        /**
         * Returns true if the line contains an image.
         */
        public boolean hasImage() {
            return mImage != null;
        }

        /**
         * Return the optional image (URL or base64-encoded image data).
         */
        public String getImage() {
            return mImage;
        }
    }

    /**
     * Represents one text field of an answer, containing a integer type and a string.
     */
    public static class TextField {
        private final int mType;
        private final String mText;
        private final int mNumLines;

        TextField(JSONObject jsonTextField) throws JSONException {
            mType = jsonTextField.getInt(ANSWERS_JSON_TEXT_TYPE);
            mText = jsonTextField.getString(ANSWERS_JSON_TEXT);
            mNumLines = jsonTextField.has(ANSWERS_JSON_NUMBER_OF_LINES)
                    ? jsonTextField.getInt(ANSWERS_JSON_NUMBER_OF_LINES)
                    : -1;
        }

        public int getType() {
            return mType;
        }

        public String getText() {
            return mText;
        }

        public boolean hasNumLines() {
            return mNumLines != -1;
        }

        public int getNumLines() {
            return mNumLines;
        }
    }
}
