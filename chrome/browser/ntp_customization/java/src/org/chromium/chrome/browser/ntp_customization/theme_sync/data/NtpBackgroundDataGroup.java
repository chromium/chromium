// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import android.content.Context;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.function.Predicate;

/** A class that represents a group of NTP background data. */
@NullMarked
public class NtpBackgroundDataGroup implements Iterable<NtpBackgroundDataBase> {
    static final String DATA_LIST_KEY = "dataList";

    private final List<NtpBackgroundDataBase> mNtpBackgroundDataList;

    public NtpBackgroundDataGroup() {
        this(new ArrayList<>());
    }

    /**
     * @param ntpBackgroundDataList A list of {@link NtpBackgroundDataBase} instances.
     */
    public NtpBackgroundDataGroup(List<NtpBackgroundDataBase> ntpBackgroundDataList) {
        mNtpBackgroundDataList = ntpBackgroundDataList;
    }

    @Override
    public Iterator<NtpBackgroundDataBase> iterator() {
        return mNtpBackgroundDataList.iterator();
    }

    /**
     * Returns the element at the specified position in this list.
     *
     * @param index The index of the element to return.
     * @return The element at the specified position.
     */
    public NtpBackgroundDataBase get(int index) {
        return mNtpBackgroundDataList.get(index);
    }

    /**
     * Adds the given background data to the list.
     *
     * @param data The background data to add.
     */
    public void add(NtpBackgroundDataBase data) {
        mNtpBackgroundDataList.add(data);
    }

    /**
     * Adds the given background data at the specified index.
     *
     * @param index The index at which to add the data.
     * @param data The background data to add.
     */
    public void add(int index, NtpBackgroundDataBase data) {
        mNtpBackgroundDataList.add(index, data);
    }

    /** Returns true if the list is empty. */
    public boolean isEmpty() {
        return mNtpBackgroundDataList.isEmpty();
    }

    /** Returns the size of the list. */
    public int size() {
        return mNtpBackgroundDataList.size();
    }

    /**
     * Returns the index of the first occurrence of the specified element in this list, or -1 if
     * this list does not contain the element.
     *
     * @param data The element to search for.
     * @return The index of the element, or -1 if not found.
     */
    public int indexOf(NtpBackgroundDataBase data) {
        return mNtpBackgroundDataList.indexOf(data);
    }

    /**
     * Removes the element at the specified position in this list.
     *
     * @param index The index of the element to be removed.
     * @return The element that was removed.
     */
    public NtpBackgroundDataBase remove(int index) {
        return mNtpBackgroundDataList.remove(index);
    }

    /**
     * Removes all of the elements of this collection that satisfy the given predicate.
     *
     * @param filter A predicate which returns true for elements to be removed.
     * @return true if any elements were removed.
     */
    public boolean removeIf(Predicate<? super NtpBackgroundDataBase> filter) {
        return mNtpBackgroundDataList.removeIf(filter);
    }

    /** Removes all items in the list. */
    public void clear() {
        mNtpBackgroundDataList.clear();
    }

    /** Returns the JSON representation of the object. */
    public JSONObject toJson() throws JSONException {
        JSONObject json = new JSONObject();
        json.put(DATA_LIST_KEY, toJsonArray());
        return json;
    }

    /** Returns the JSONArray representation of the list of NTP background data. */
    public JSONArray toJsonArray() throws JSONException {
        JSONArray jsonArray = new JSONArray();
        for (NtpBackgroundDataBase data : mNtpBackgroundDataList) {
            jsonArray.put(data.toJson());
        }
        return jsonArray;
    }

    /** Returns the NtpBackgroundDataGroup object from the given JSON representation. */
    public static NtpBackgroundDataGroup fromJson(Context context, JSONObject json)
            throws JSONException {
        return fromJson(context, json.getJSONArray(DATA_LIST_KEY));
    }

    /** Returns the NtpBackgroundDataGroup object from the given JSONArray. */
    public static NtpBackgroundDataGroup fromJson(Context context, JSONArray jsonArray)
            throws JSONException {
        List<NtpBackgroundDataBase> dataList = new ArrayList<>();
        for (int i = 0; i < jsonArray.length(); i++) {
            NtpBackgroundDataBase data =
                    NtpBackgroundDataUtils.fromJson(context, jsonArray.getJSONObject(i));
            if (data != null) {
                dataList.add(data);
            }
        }
        return new NtpBackgroundDataGroup(dataList);
    }
}
