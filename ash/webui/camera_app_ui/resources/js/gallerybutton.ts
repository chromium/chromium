// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from './assert.js';
import * as dom from './dom.js';
import {reportError} from './error.js';
import {Filenamer} from './models/file_namer.js';
import * as filesystem from './models/file_system.js';
import {
  DirectoryAccessEntry,
  FileAccessEntry,
} from './models/file_system_access_entry.js';
import {ResultSaver} from './models/result_saver.js';
import {VideoSaver} from './models/video_saver.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import {extractImageFromBlob} from './thumbnailer.js';
import {
  ErrorLevel,
  ErrorType,
  Metadata,
  MimeType,
  VideoType,
} from './type.js';
import {WaitableEvent} from './waitable_event.js';

/**
 * Cover photo of gallery button.
 */
class CoverPhoto {
  /**
   * @param file File entry of cover photo.
   * @param url Url to its cover photo. Might be null if the cover is failed to
   *     load.
   * @param draggable If the file type support share by dragg/drop cover photo.
   */
  constructor(
      readonly file: FileAccessEntry,
      readonly url: string|null,
      readonly draggable: boolean,
  ) {}

  /**
   * File name of the cover photo.
   */
  get name(): string {
    return this.file.name;
  }

  /**
   * Releases resources used by this cover photo.
   */
  release() {
    if (this.url !== null) {
      URL.revokeObjectURL(this.url);
    }
  }

  /**
   * Creates CoverPhoto objects from photo file.
   */
  static async create(file: FileAccessEntry): Promise<CoverPhoto|null> {
    const blob = await file.file();
    if (blob.size === 0) {
      reportError(
          ErrorType.EMPTY_FILE,
          ErrorLevel.ERROR,
          new Error('The file to generate cover photo is empty'),
      );
      return null;
    }

    try {
      const cover = await extractImageFromBlob(blob);
      const draggable = blob.type !== MimeType.MP4;
      return new CoverPhoto(file, URL.createObjectURL(cover), draggable);
    } catch (e) {
      reportError(
          ErrorType.BROKEN_THUMBNAIL, ErrorLevel.ERROR,
          assertInstanceof(e, Error));
      return new CoverPhoto(file, null, false);
    }
  }
}

/**
 * Creates a controller for the gallery-button.
 */
export class GalleryButton implements ResultSaver {
  /**
   * Cover photo from latest saved picture.
   */
  private cover: CoverPhoto|null = null;

  private readonly button = dom.get('#gallery-enter', HTMLButtonElement);

  /**
   * Directory holding saved pictures showing in gallery.
   */
  private directory: DirectoryAccessEntry|null = null;

  private readonly coverPhoto: HTMLImageElement;

  private retryingCheckCover = false;

  constructor() {
    this.coverPhoto = dom.getFrom(this.button, 'img', HTMLImageElement);

    this.button.addEventListener('click', () => {
      if (this.cover !== null) {
        ChromeHelper.getInstance().openFileInGallery(this.cover.file.name);
      }
    });
  }

  /**
   * Initializes the gallery button.
   *
   * @param dir Directory holding saved pictures showing in gallery.
   */
  async initialize(dir: DirectoryAccessEntry): Promise<void> {
    this.directory = dir;
    await this.checkCover();
  }

  /**
   * @param file File to be set as cover photo.
   */
  private async updateCover(file: FileAccessEntry|null): Promise<void> {
    const cover = file === null ? null : await CoverPhoto.create(file);
    if (this.cover === cover) {
      return;
    }
    if (this.cover !== null) {
      this.cover.release();
    }
    this.cover = cover;

    this.button.hidden = cover === null;
    this.coverPhoto.classList.toggle('draggable', cover?.draggable ?? false);
    this.coverPhoto.src = cover?.url ?? '';

    if (file !== null) {
      ChromeHelper.getInstance().monitorFileDeletion(file.name, async () => {
        try {
          await this.checkCover();
        } catch (e) {
          reportError(ErrorType.CHECK_COVER_FAILURE, ErrorLevel.ERROR, e);
        }
      });
    }
  }

  /**
   * Checks validity of cover photo from camera directory.
   */
  private async checkCover(): Promise<void> {
    if (this.directory === null) {
      return;
    }
    const dir = this.directory;

    // Checks existence of cached cover photo.
    if (this.cover !== null) {
      if (await dir.isExist(this.cover.name)) {
        return;
      }
    }

    // Rescan file system. Only select files following CCA naming styles.
    const files = (await filesystem.getEntries())
                      .filter((file) => Filenamer.isCCAFileFormat(file.name));
    if (files.length === 0) {
      await this.updateCover(null);
      return;
    }

    try {
      const filesWithTime = await Promise.all(
          files.map(async (file) => ({
                      file,
                      time: (await file.getLastModificationTime()),
                    })));
      const lastFile =
          filesWithTime.reduce((last, cur) => last.time > cur.time ? last : cur)
              .file;
      await this.updateCover(lastFile);
    } catch (e) {
      // The file might be deleted at any time and cause the operation
      // interrupted. Since it might take a while when doing bulk deletion, only
      // try check cover again if the amount of files become stable.
      if (e instanceof DOMException && !this.retryingCheckCover) {
        this.retryingCheckCover = true;
        try {
          await this.waitUntilCameraFolderStable();
          await this.checkCover();
        } finally {
          this.retryingCheckCover = false;
        }
      } else {
        throw e;
      }
    }
  }

  private async waitUntilCameraFolderStable(): Promise<void> {
    let prevFileCount = (await filesystem.getEntries()).length;
    const cameraFolderStable = new WaitableEvent();

    async function checkFileCount() {
      const newFileCount = (await filesystem.getEntries()).length;
      if (prevFileCount === newFileCount) {
        clearInterval(intervalId);
        cameraFolderStable.signal();
      } else {
        prevFileCount = newFileCount;
      }
    }

    const intervalId = setInterval(checkFileCount, 500);
    return cameraFolderStable.wait();
  }

  async savePhoto(blob: Blob, name: string, metadata: Metadata|null):
      Promise<void> {
    const file = await filesystem.saveBlob(blob, name);
    if (metadata !== null) {
      const metadataBlob =
          new Blob([JSON.stringify(metadata, null, 2)], {type: MimeType.JSON});
      await filesystem.saveBlob(metadataBlob, Filenamer.getMetadataName(name));
    }

    ChromeHelper.getInstance().sendNewCaptureBroadcast(
        {isVideo: false, name: file.name});
    await this.updateCover(file);
  }

  async saveGif(blob: Blob, name: string): Promise<void> {
    const file = await filesystem.saveBlob(blob, name);
    await this.updateCover(file);
  }

  async startSaveVideo(videoRotation: number): Promise<VideoSaver> {
    const file = await filesystem.createVideoFile(VideoType.MP4);
    return VideoSaver.createForFile(file, videoRotation);
  }

  async finishSaveVideo(video: VideoSaver): Promise<void> {
    const file = await video.endWrite();
    assert(file !== null);

    ChromeHelper.getInstance().sendNewCaptureBroadcast(
        {isVideo: true, name: file.name});
    await this.updateCover(file);
  }
}
