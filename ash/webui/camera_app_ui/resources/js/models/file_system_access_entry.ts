// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../assert.js';
import {AsyncJobWithResultQueue} from '../async_job_queue.js';
import {isFileSystemDirectoryHandle, isFileSystemFileHandle} from '../util.js';

import {AsyncWriter} from './async_writer.js';

/**
 * The file entry implementation for SWA.
 */
export class FileAccessEntry {
  constructor(
      private readonly handle: FileSystemFileHandle,
      private readonly parent: DirectoryAccessEntryImpl|null = null) {}

  /**
   * Returns the File object which the entry points to.
   */
  async file(): Promise<File> {
    return this.handle.getFile();
  }

  /**
   * Writes |blob| data into the file.
   *
   * @return The promise is resolved once the write operation is
   *     completed.
   */
  async write(blob: Blob): Promise<void> {
    const writer = await this.handle.createWritable();
    await writer.write(blob);
    await writer.close();
  }

  async getWriter(): Promise<AsyncWriter> {
    const writer = await this.handle.createWritable();
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
   *
   * @return The number of milliseconds since the Unix epoch in UTC.
   */
  async getLastModificationTime(): Promise<number> {
    const file = await this.file();
    return file.lastModified;
  }

  /**
   * @throws Thrown when trying to delete file with no parent directory.
   */
  async remove(): Promise<void> {
    if (this.parent === null) {
      throw new Error('Failed to delete file due to no parent directory');
    }
    return this.parent.removeEntry(this.name);
  }

  /**
   * Moves the file to given directory |dir| and given |name|.
   */
  async moveTo(dir: DirectoryAccessEntry, name: string): Promise<void> {
    const dirHandle = await dir.getHandle();
    await this.handle.move(dirHandle, name);
  }

  get name(): string {
    return this.handle.name;
  }
}

/**
 * Guards from name collision when creating files.
 */
const createFileJobs = new AsyncJobWithResultQueue();

/**
 * The abstract interface for the directory entry.
 */
export interface DirectoryAccessEntry {
  readonly name: string;

  getHandle(): Promise<FileSystemDirectoryHandle>;

  getFiles(): Promise<FileAccessEntry[]>;

  getDirectories(): Promise<DirectoryAccessEntry[]>;

  getFile(name: string): Promise<FileAccessEntry|null>;

  /**
   * Checks if file or directory with the target |name| exists.
   */
  exists(name: string): Promise<boolean>;

  /**
   * Create the file given by its |name|. If there is already a file with same
   * name, it will try to use a name with index as suffix.
   * (e.g. IMG.png => IMG (1).png).
   */
  createFile(name: string): Promise<FileAccessEntry>;

  /**
   * Gets the directory given by its |name|. If the directory is not found,
   * creates one if |createIfNotExist| is true.
   * TODO(crbug.com/1127587): Split this method to getDirectory() and
   * createDirectory().
   */
  getDirectory({name, createIfNotExist}:
                   {name: string, createIfNotExist: boolean}):
      Promise<DirectoryAccessEntry|null>;

  /**
   * Removes file by given |name| from the directory.
   */
  removeEntry(name: string): Promise<void>;
}

/**
 * The directory entry implementation for SWA.
 */
export class DirectoryAccessEntryImpl implements DirectoryAccessEntry {
  constructor(private readonly handle: FileSystemDirectoryHandle) {}

  get name(): string {
    return this.handle.name;
  }

  getHandle(): Promise<FileSystemDirectoryHandle> {
    return Promise.resolve(this.handle);
  }

  async getFiles(): Promise<FileAccessEntry[]> {
    const results = [];
    for await (const handle of this.handle.values()) {
      if (isFileSystemFileHandle(handle)) {
        results.push(new FileAccessEntry(handle, this));
      }
    }
    return results;
  }

  async getDirectories(): Promise<DirectoryAccessEntry[]> {
    const results = [];
    for await (const handle of this.handle.values()) {
      if (isFileSystemDirectoryHandle(handle)) {
        results.push(new DirectoryAccessEntryImpl(handle));
      }
    }
    return results;
  }

  async getFile(name: string): Promise<FileAccessEntry|null> {
    const handle = await this.handle.getFileHandle(name, {create: false});
    return new FileAccessEntry(handle, this);
  }

  async exists(name: string): Promise<boolean> {
    try {
      await this.getFile(name);
      return true;
    } catch (e) {
      assert(e instanceof Error);
      // File doesn't exist.
      if (e.name === 'NotFoundError') {
        return false;
      }
      // Directory with same name exists.
      if (e.name === 'TypeMismatchError') {
        return true;
      }
      throw e;
    }
  }

  async createFile(name: string): Promise<FileAccessEntry> {
    return createFileJobs.push(async () => {
      let uniqueName = name;
      for (let i = 0; await this.exists(uniqueName);) {
        uniqueName = name.replace(/^(.*?)(?=\.)/, `$& (${++i})`);
      }
      const handle =
          await this.handle.getFileHandle(uniqueName, {create: true});
      return new FileAccessEntry(handle, this);
    });
  }

  async getDirectory({name, createIfNotExist}:
                         {name: string, createIfNotExist: boolean}):
      Promise<DirectoryAccessEntry|null> {
    try {
      const handle = await this.handle.getDirectoryHandle(
          name, {create: createIfNotExist});
      assert(handle !== null);
      return new DirectoryAccessEntryImpl(handle);
    } catch (error) {
      if (!createIfNotExist && error instanceof Error &&
          error.name === 'NotFoundError') {
        return null;
      }
      throw error;
    }
  }

  async removeEntry(name: string): Promise<void> {
    if (await this.exists(name)) {
      await this.handle.removeEntry(name);
    }
  }
}
