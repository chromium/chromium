// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

declare global {
  interface FileSystemDirectoryHandle {
    [Symbol.asyncIterator](): AsyncIterableIterator<[string, FileSystemHandle]>;
    entries(): AsyncIterableIterator<[string, FileSystemHandle]>;
    keys(): AsyncIterableIterator<string>;
    values(): AsyncIterableIterator<FileSystemHandle>;
  }
}

/**
 * A data directory to read and write recording files.
 */
export class DataDir {
  constructor(private readonly root: FileSystemDirectoryHandle) {}

  async write(name: string, data: Blob|string): Promise<void> {
    const handle = await this.root.getFileHandle(name, {create: true});
    const writable = await handle.createWritable();
    await writable.write(data);
    await writable.close();
  }

  async read(name: string): Promise<File> {
    const handle = await this.root.getFileHandle(name);
    const file = await handle.getFile();
    return file;
  }

  async list(): Promise<string[]> {
    const names: string[] = [];
    for await (const name of this.root.keys()) {
      names.push(name);
    }
    return names;
  }

  async clear(): Promise<void> {
    const names = await this.list();
    await Promise.all(
      names.map(async (name) => {
        await this.root.removeEntry(name);
      }),
    );
  }

  async remove(name: string): Promise<void> {
    await this.root.removeEntry(name);
  }

  // TODO(shik): Support creating from other directories.
  static async createFromOpfs(): Promise<DataDir> {
    const root = await navigator.storage.getDirectory();
    return new DataDir(root);
  }
}
