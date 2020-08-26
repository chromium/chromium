// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.provider.ContactsContract;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;

import java.io.ByteArrayInputStream;

/**
 * A worker task to retrieve images for contacts.
 */
class FetchIconWorkerTask extends AsyncTask<Bitmap> {
    /**
     * An interface to use to communicate back the results to the client.
     */
    public interface IconRetrievedCallback {
        /**
         * A callback to define to receive the icon for a contact.
         * @param icon The icon retrieved.
         * @param contactId The id of the contact the icon refers to.
         */
        void iconRetrieved(Bitmap icon, String contactId);
    }

    // The ID of the contact to look up.
    private String mContactId;

    // The content resolver to use for looking up
    private ContentResolver mContentResolver;

    // The callback to use to communicate the results.
    private IconRetrievedCallback mCallback;

    /**
     * A FetchIconWorkerTask constructor.
     * @param id The id of the contact to look up.
     * @param contentResolver The ContentResolver to use for the lookup.
     * @param callback The callback to use to communicate back the results.
     */
    public FetchIconWorkerTask(
            String id, ContentResolver contentResolver, IconRetrievedCallback callback) {
        mContactId = id;
        // Avatar icon for own info should not be obtained through the contacts list.
        assert !id.equals(ContactDetails.SELF_CONTACT_ID);
        mContentResolver = contentResolver;
        mCallback = callback;
    }

    /**
     * Fetches the icon of a particular contact (in a background thread).
     * @return The icon representing a contact (returned as Bitmap).
     */
    @Override
    protected Bitmap doInBackground() {
        assert !ThreadUtils.runningOnUiThread();

        if (isCancelled()) return null;

        Uri contactUri = ContentUris.withAppendedId(
                ContactsContract.Contacts.CONTENT_URI, Long.parseLong(mContactId));
        Uri photoUri =
                Uri.withAppendedPath(contactUri, ContactsContract.Contacts.Photo.CONTENT_DIRECTORY);
        Cursor cursor = mContentResolver.query(
                photoUri, new String[] {ContactsContract.Contacts.Photo.PHOTO}, null, null, null);
        if (cursor == null) return null;
        try {
            if (cursor.moveToFirst()) {
                byte[] data = cursor.getBlob(0);
                if (data != null) {
                    return BitmapFactory.decodeStream(new ByteArrayInputStream(data));
                }
            }
        } finally {
            cursor.close();
        }
        return null;
    }

    /**
     * Communicates the results back to the client. Called on the UI thread.
     * @param icon The icon retrieved.
     */
    @Override
    protected void onPostExecute(Bitmap icon) {
        assert ThreadUtils.runningOnUiThread();

        if (isCancelled()) return;

        mCallback.iconRetrieved(icon, mContactId);
    }
}
