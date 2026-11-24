// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const DB_NAME = 'cca';
const DB_STORE = 'store';

// Key of the camera directory handle.
export const KEY_CAMERA_DIRECTORY_HANDLE = 'CameraDirectoryHandle';

let idbPromise: Promise<IDBDatabase>|null = null;

/**
 * Opens IndexedDB lazily on first use to avoid import-time unhandled
 * rejections under low disk space and skip opening on OPFS/intent paths.
 */
function getIdb(): Promise<IDBDatabase> {
  if (idbPromise !== null) {
    return idbPromise;
  }
  let request: IDBOpenDBRequest;
  try {
    request = indexedDB.open(DB_NAME);
  } catch (e) {
    return Promise.reject(e);
  }
  const promise = new Promise<IDBDatabase>((resolve, reject) => {
    const clearCachedPromise = () => {
      if (idbPromise === promise) {
        idbPromise = null;
      }
    };
    request.onerror = () => {
      clearCachedPromise();
      reject(request.error ?? new Error('Failed to open IndexedDB'));
    };
    request.onupgradeneeded = () => {
      const db = request.result;
      db.createObjectStore(DB_STORE, {
        keyPath: 'id',
      });
    };
    request.onsuccess = () => {
      const db = request.result;
      db.onclose = () => {
        clearCachedPromise();
      };
      resolve(db);
    };
  });
  idbPromise = promise;
  return promise;
}

/**
 * Retrieves serializable object from idb.
 *
 * @param key The key of the object.
 */
export async function get<T>(key: string): Promise<T|null> {
  const transaction = (await getIdb()).transaction(DB_STORE, 'readonly');
  const objStore = transaction.objectStore(DB_STORE);
  const request = objStore.get(key);
  return new Promise<T|null>((resolve, reject) => {
    request.onerror = () => {
      reject(request.error ?? new Error('IndexedDB get request failed'));
    };
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
  const transaction = (await getIdb()).transaction(DB_STORE, 'readwrite');
  const objStore = transaction.objectStore(DB_STORE);
  objStore.put({id: key, value: obj});
  return new Promise((resolve, reject) => {
    // Wait for transaction commit rather than put's onsuccess because
    // low-disk-space quota errors can abort the write during commit, and any
    // unhandled put error also aborts the transaction.
    transaction.onabort = () => {
      reject(
          transaction.error ?? new Error('IndexedDB set transaction failed'));
    };
    transaction.oncomplete = () => resolve();
  });
}
