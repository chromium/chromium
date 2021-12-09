// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '../assert.js';
import {AsyncJobQueue} from '../async_job_queue.js';

import {AsyncWriter} from './async_writer.js';

/**
 * The file entry implementation for SWA.
 */
export class FileAccessEntry {
  /**
   * @param {!FileSystemFileHandle} handle
   * @param {?DirectoryAccessEntryImpl} parent
   */
  constructor(handle, parent = null) {
    /**
     * @type {!FileSystemFileHandle}
     * @private
     */
    this.handle_ = handle;

    /**
     * @type {?DirectoryAccessEntryImpl}
     * @private
     */
    this.parent_ = parent;
  }

  /**
   * Returns the File object which the entry points to.
   * @return {!Promise<!File>}
   */
  async file() {
    return this.handle_.getFile();
  }

  /**
   * Writes |blob| data into the file.
   * @param {!Blob} blob
   * @return {!Promise} The returned promise is resolved once the write
   *     operation is completed.
   */
  async write(blob) {
    const writer = await this.handle_.createWritable();
    await writer.write(blob);
    await writer.close();
  }

  /**
   * Gets a writer to write data into the file.
   * @return {!Promise<!AsyncWriter>}
   */
  async getWriter() {
    const writer = await this.handle_.createWritable();
    // TODO(crbug.com/980846): We should write files in-place so that even the
    // app is accidentally closed or hit any unexpected exceptions, the captured
    // video will not be dropped entirely.
    return new AsyncWriter({
      write: (blob) => writer.write(blob),
      seek: (offset) => writer.seek(offset),
      close: () => writer.close(),
    });
  }

  /**
   * Gets the timestamp of the last modification time of the file.
   * @return {!Promise<number>} The number of milliseconds since the Unix epoch
   *     in UTC.
   */
  async getLastModificationTime() {
    const file = await this.file();
    return file.lastModified;
  }

  /**
   * Deletes the file.
   * @return {!Promise}
   * @throws {!Error} Thrown when trying to delete file with no parent
   *     directory.
   */
  async delete() {
    if (this.parent_ === null) {
      throw new Error('Failed to delete file due to no parent directory');
    }
    return this.parent_.removeEntry(this.name);
  }

  /**
   * @return {string}
   */
  get name() {
    return this.handle_.name;
  }
}

/**
 * Guards from name collision when creating files.
 * @type {!AsyncJobQueue}
 */
const createFileJobs = new AsyncJobQueue();

/**
 * The abstract interface for the directory entry.
 * @interface
 */
export class DirectoryAccessEntry {
  /* eslint-disable getter-return */

  /**
   * Gets the name of the directory.
   * @return {string}
   * @abstract
   */
  get name() {
    return assertNotReached();
  }

  /* eslint-enable getter-return */

  /**
   * Gets files in this directory.
   * @return {!Promise<!Array<!FileAccessEntry>>}
   * @abstract
   */
  async getFiles() {
    assertNotReached();
  }

  /**
   * Gets directories in this directory.
   * @return {!Promise<!Array<!DirectoryAccessEntry>>}
   * @abstract
   */
  async getDirectories() {
    assertNotReached();
  }

  /**
   * Gets the file given by its |name|.
   * @param {string} name The name of the file.
   * @return {!Promise<?FileAccessEntry>} The entry of the found file.
   * @abstract
   */
  async getFile(name) {
    assertNotReached();
  }

  /**
   * Checks if file or directory with the target name exists.
   * @param {string} name
   * @return {!Promise<boolean>}
   * @abstract
   */
  async isExist(name) {
    assertNotReached();
  }

  /**
   * Create the file given by its |name|. If there is already a file with same
   * name, it will try to use a name with index as suffix.
   * e.g. IMG.png => IMG (1).png
   * @param {string} name The name of the file.
   * @return {!Promise<!FileAccessEntry>} The entry of the created file.
   * @abstract
   */
  async createFile(name) {
    assertNotReached();
  }

  /**
   * Gets the directory given by its |name|. If the directory is not found,
   * create one if |createIfNotExist| is true.
   * TODO(crbug.com/1127587): Split this method to getDirectory() and
   * createDirectory().
   * @param {{name: string, createIfNotExist: boolean}} params
   * @return {!Promise<?DirectoryAccessEntry>} The entry of the found/created
   *     directory.
   */
  async getDirectory({name, createIfNotExist}) {
    assertNotReached();
  }

  /**
   * Removes file by given |name| from the directory.
   * @param {string} name The name of the file.
   * @return {!Promise}
   */
  async removeEntry(name) {
    assertNotReached();
  }
}

/**
 * The directory entry implementation for SWA.
 * @implements {DirectoryAccessEntry}
 */
export class DirectoryAccessEntryImpl {
  /**
   * @param {!FileSystemDirectoryHandle} handle
   * @param {?DirectoryAccessEntryImpl} parent
   */
  constructor(handle, parent = null) {
    /**
     * @type {!FileSystemDirectoryHandle}
     * @private
     */
    this.handle_ = handle;

    /**
     * @type {?DirectoryAccessEntryImpl}
     * @private
     */
    this.parent_ = parent;
  }

  /**
   * @override
   */
  get name() {
    return this.handle_.name;
  }

  /**
   * @override
   */
  async getFiles() {
    const results = [];
    for await (const handle of this.handle_.values()) {
      if (handle.kind === 'file') {
        results.push(new FileAccessEntry(handle, this));
      }
    }
    return results;
  }

  /**
   * @override
   */
  async getDirectories() {
    const results = [];
    for await (const handle of this.handle_.values()) {
      if (handle.kind === 'directory') {
        results.push(new DirectoryAccessEntryImpl(handle, this));
      }
    }
    return results;
  }

  /**
   * @override
   */
  async getFile(name) {
    const handle = await this.handle_.getFileHandle(name, {create: false});
    return new FileAccessEntry(handle, this);
  }

  /**
   * @override
   */
  async isExist(name) {
    try {
      await this.getFile(name);
      return true;
    } catch (e) {
      if (e.name === 'NotFoundError') {
        return false;
      }
      if (e.name === 'TypeMismatchError' || e instanceof TypeError) {
        // Directory with same name exists.
        return true;
      }
      throw e;
    }
  }

  /**
   * @override
   */
  async createFile(name) {
    return createFileJobs.push(async () => {
      let uniqueName = name;
      for (let i = 0; await this.isExist(uniqueName);) {
        uniqueName = name.replace(/^(.*?)(?=\.)/, `$& (${++i})`);
      }
      const handle =
          await this.handle_.getFileHandle(uniqueName, {create: true});
      return new FileAccessEntry(handle, this);
    });
  }

  /**
   * @override
   */
  async getDirectory({name, createIfNotExist}) {
    try {
      const handle = await this.handle_.getDirectoryHandle(
          name, {create: createIfNotExist});
      assert(handle !== null);
      return new DirectoryAccessEntryImpl(
          /** @type {!FileSystemDirectoryHandle} */ (handle), this);
    } catch (error) {
      if (!createIfNotExist && error.name === 'NotFoundError') {
        return null;
      }
      throw error;
    }
  }

  /**
   * @override
   */
  async removeEntry(name) {
    return this.handle_.removeEntry(name);
  }
}
