// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.provider.ContactsContract;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/**
 * A worker task to retrieve images for contacts.
 */
class ContactsFetcherWorkerTask extends AsyncTask<ArrayList<ContactDetails>> {
    private static final String[] PROJECTION = {
            ContactsContract.Contacts._ID, ContactsContract.Contacts.LOOKUP_KEY,
            ContactsContract.Contacts.DISPLAY_NAME_PRIMARY,
    };

    /**
     * An interface to use to communicate back the results to the client.
     */
    public interface ContactsRetrievedCallback {
        /**
         * A callback to define to receive the contact details.
         * @param contacts The contacts retrieved.
         */
        void contactsRetrieved(ArrayList<ContactDetails> contacts);
    }

    // The content resolver to use for looking up contacts.
    private ContentResolver mContentResolver;

    // The callback to use to communicate the results.
    private ContactsRetrievedCallback mCallback;

    /**
     * A ContactsFetcherWorkerTask constructor.
     * @param callback The callback to use to communicate back the results.
     */
    public ContactsFetcherWorkerTask(
            ContentResolver contentResolver, ContactsRetrievedCallback callback) {
        mContentResolver = contentResolver;
        mCallback = callback;
    }

    /**
     * Fetches the details for all contacts (in a background thread).
     * @return The icon representing a contact.
     */
    @Override
    protected ArrayList<ContactDetails> doInBackground() {
        assert !ThreadUtils.runningOnUiThread();

        if (isCancelled()) return null;

        return getAllContacts();
    }

    /**
     * Fetches specific details for contacts.
     * @param source The source URI to use for the lookup.
     * @param idColumn The name of the id column.
     * @param idColumn The name of the data column.
     * @param sortOrder The sort order. Data must be sorted by CONTACT_ID but can be additionally
     *                  sorted also.
     * @return A map of ids to contact details (as ArrayList).
     */
    private Map<String, ArrayList<String>> getDetails(
            Uri source, String idColumn, String dataColumn, String sortOrder) {
        Map<String, ArrayList<String>> map = new HashMap<String, ArrayList<String>>();

        Cursor cursor = mContentResolver.query(source, null, null, null, sortOrder);
        ArrayList<String> list = new ArrayList<String>();
        String key = "";
        String value;
        while (cursor.moveToNext()) {
            String id = cursor.getString(cursor.getColumnIndex(idColumn));
            value = cursor.getString(cursor.getColumnIndex(dataColumn));
            if (key.isEmpty()) {
                key = id;
                list.add(value);
            } else {
                if (key.equals(id)) {
                    list.add(value);
                } else {
                    map.put(key, list);
                    list = new ArrayList<String>();
                    list.add(value);
                    key = id;
                }
            }
        }
        map.put(key, list);
        cursor.close();

        return map;
    }

    /**
     * Fetches all known contacts.
     * @return The contact list as an array.
     */
    public ArrayList<ContactDetails> getAllContacts() {
        Map<String, ArrayList<String>> emailMap =
                getDetails(ContactsContract.CommonDataKinds.Email.CONTENT_URI,
                        ContactsContract.CommonDataKinds.Email.CONTACT_ID,
                        ContactsContract.CommonDataKinds.Email.DATA,
                        ContactsContract.CommonDataKinds.Email.CONTACT_ID + " ASC, "
                                + ContactsContract.CommonDataKinds.Email.DATA + " ASC");

        Map<String, ArrayList<String>> phoneMap =
                getDetails(ContactsContract.CommonDataKinds.Phone.CONTENT_URI,
                        ContactsContract.CommonDataKinds.Phone.CONTACT_ID,
                        ContactsContract.CommonDataKinds.Email.DATA,
                        ContactsContract.CommonDataKinds.Email.CONTACT_ID + " ASC, "
                                + ContactsContract.CommonDataKinds.Phone.NUMBER + " ASC");

        // A cursor containing the raw contacts data.
        Cursor cursor = mContentResolver.query(ContactsContract.Contacts.CONTENT_URI, PROJECTION,
                null, null, ContactsContract.Contacts.DISPLAY_NAME_PRIMARY + " ASC");

        ArrayList<ContactDetails> contacts = new ArrayList<ContactDetails>(cursor.getCount());
        if (!cursor.moveToFirst()) return contacts;
        do {
            String id = cursor.getString(cursor.getColumnIndex(ContactsContract.Contacts._ID));
            String name = cursor.getString(
                    cursor.getColumnIndex(ContactsContract.Contacts.DISPLAY_NAME_PRIMARY));
            contacts.add(new ContactDetails(id, name, emailMap.get(id), phoneMap.get(id)));
        } while (cursor.moveToNext());

        cursor.close();
        return contacts;
    }

    /**
     * Communicates the results back to the client. Called on the UI thread.
     * @param contacts The contacts retrieved.
     */
    @Override
    protected void onPostExecute(ArrayList<ContactDetails> contacts) {
        assert ThreadUtils.runningOnUiThread();

        if (isCancelled()) return;

        mCallback.contactsRetrieved(contacts);
    }
}
