// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Interfaces to decouple Coordinator / Mediator / View for the Custom Tile Edit Dialog. */
@NullMarked
class CustomTileEditDelegates {
    @IntDef({
        DialogMode.ADD_SHORTCUT,
        DialogMode.EDIT_SHORTCUT,
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    public @interface DialogMode {
        int ADD_SHORTCUT = 0;
        int EDIT_SHORTCUT = 1;
        int NUM_ENTRIES = 2;
    }

    @IntDef({
        UrlErrorCode.NONE,
        UrlErrorCode.INVALID_URL,
        UrlErrorCode.DUPLICATE_URL,
        UrlErrorCode.NUM_ENTRIES
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    public @interface UrlErrorCode {
        int NONE = 0;
        int INVALID_URL = 1;
        int DUPLICATE_URL = 2;
        int NUM_ENTRIES = 3;
    }

    /** Interface to access functionalities provided by the Browser. */
    interface MediatorToBrowser {
        /** Shows the edit dialog. */
        void showEditDialog();

        /**
         * Closes the edit dialog.
         *
         * @param isSubmit True if this is after a successful submit, false otherwise.
         */
        void closeEditDialog(boolean isSubmit);

        /**
         * Checks whether the a URL already exists in a custom tile.
         *
         * @param url The URL to check.
         * @return True if the URL is a duplicate, false otherwise.
         */
        boolean isUrlDuplicate(GURL url);

        /**
         * Submits the changes to the tile.
         *
         * @param name The new name of the tile.
         * @param url The new URL of the tile.
         * @return True if the submission was successful, false otherwise.
         */
        boolean submitChange(String name, GURL url);
    }

    /**
     * Interface to access functionalities provided by the View. This is used instead of Model and
     * ViewBinder for simplicity: We need to update focus and error on input fields, but these are
     * readily updated by user action. Thus the semantics of Model state assignment is tenuous.
     */
    interface MediatorToView {
        /** Adds a {@param task} to run once the window gains focus. */
        void addOnWindowFocusGainedTask(Runnable task);

        /**
         * Sets the mode under which the dialog is used.
         *
         * @param mode The mode to use.
         */
        void setDialogMode(@DialogMode int mode);

        /**
         * Sets the value in the Name input field.
         *
         * @param name The value set.
         */
        void setName(String name);

        /**
         * Sets the error message for the URL input field. Note that the error clears automatically
         * on text change.
         *
         * @param errorCode The error code..
         */
        void setUrlErrorByCode(@UrlErrorCode int urlErrorCode);

        /**
         * Sets the URL text in the URL input field.
         *
         * @param urlText The URL text to set.
         */
        void setUrlText(String urlText);

        /**
         * Enables or disables the "Save" button.
         *
         * @param enable True to enable, false to disable.
         */
        void toggleSaveButton(boolean enable);

        /** Sets the focus to the Name input field and shows the soft keyboard. */
        void focusOnName();

        /**
         * Sets the focus to the URL input field, shows the soft keyboard, and optionally selects
         * all text.
         *
         * @param selectAll Whether to also select all text.
         */
        void focusOnUrl(boolean selectAll);
    }

    /** Interface to handle interactions from the View. */
    interface ViewToMediator {
        /**
         * Notifies the delegate when the URL text has changed.
         *
         * @param urlText The new URL text.
         */
        void onUrlTextChanged(String urlText);

        /**
         * Notifies the delegate when the Save button is clicked.
         *
         * @param name The tile name entered by the user.
         * @param urlText The tile URL entered by the user.
         */
        void onSave(String name, String urlText);

        /** Notifies the delegate when the Close button is clicked. */
        void onCancel();
    }
}
