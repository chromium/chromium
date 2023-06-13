// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const DB_NAME = 'cca';
const DB_STORE = 'store';

// Key of the camera directory handle.
export const KEY_CAMERA_DIRECTORY_HANDLE = 'CameraDirectoryHandle';

const idb = new Promise<IDBDatabase>((resolve, reject) => {
  const request = indexedDB.open(DB_NAME);
  request.onerror = () => {
    reject(request.error);
  };
  request.onupgradeneeded = () => {
    const db = request.result;
    db.createObjectStore(DB_STORE, {
      keyPath: 'id',
    });
  };
  request.onsuccess = () => {
    resolve(request.result);
  };
});

/**
 * Retrieves serializable object from idb.
 *
 * @param key The key of the object.
 */
export async function get<T>(key: string): Promise<T|null> {
  const transaction = (await idb).transaction(DB_STORE, 'readonly');
  const objStore = transaction.objectStore(DB_STORE);
  const request = objStore.get(key);
  return new Promise<T|null>((resolve, reject) => {
    request.onerror = () => reject(request.error);
    request.onsuccess = () => {
      const entry = request.result;
      if (entry === undefined) {
        resolve(null);
        return;
      }
      resolve(entry.value);
    };
  });
}

/**
 * Stores serializable object into idb.
 *
 * @param key The key of the object.
 * @param obj The object to store.
 */
export async function set(key: string, obj: unknown): Promise<void> {
  const transaction = (await idb).transaction(DB_STORE, 'readwrite');
  const objStore = transaction.objectStore(DB_STORE);
  const request = objStore.put({id: key, value: obj});
  return new Promise((resolve, reject) => {
    request.onerror = () => reject(request.error);
    request.onsuccess = () => resolve();
  });
}
