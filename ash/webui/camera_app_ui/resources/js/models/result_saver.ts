// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../assert.js';
import {CoverPhoto} from '../cover_photo.js';
import * as dom from '../dom.js';
import {reportError} from '../error.js';
import {GalleryButton} from '../lit/components/gallery-button.js';
import {ChromeHelper} from '../mojo/chrome_helper.js';
import {FileType} from '../mojo/type.js';
import {
  Awaitable,
  ErrorLevel,
  ErrorType,
  ImageFormat,
  Metadata,
  MimeType,
  VideoType,
} from '../type.js';
import {sleep} from '../util.js';

import {Filenamer} from './file_namer.js';
import * as filesystem from './file_system.js';
import {
  DirectoryAccessEntry,
  FileAccessEntry,
} from './file_system_access_entry.js';
import {isCloudDestination} from './load_time_data.js';

/**
 * Handles captured result photos and video.
 */
export interface ResultSaver {
  /**
   * Saves photo capture result.
   *
   * @param blob Data of the photo to be added.
   * @param name Name of the photo to be saved.
   * @param metadata Data of the photo to be added.
   */
  savePhoto(blob: Blob, name: string, metadata: Metadata|null): Promise<void>;

  /**
   * Saves gif capture result.
   *
   * @param blob Data of the gif to be added.
   * @param name Name of the gif to be saved.
   */
  saveGif(blob: Blob, name: string): Promise<void>;

  /**
   * Saves captured video result.
   *
   * @param video Contains the video file to be saved.
   */
  saveVideo(video: FileAccessEntry): Awaitable<void>;
}

/**
 * Default handler for captured result photos and video.
 */
export class DefaultResultSaver implements ResultSaver {
  /**
   * Cover photo from latest saved picture.
   */
  private cover: CoverPhoto|null = null;

  /**
   * Directory holding saved pictures showing in gallery.
   */
  private directory: DirectoryAccessEntry|null = null;

  private readonly galleryButton = dom.get('gallery-button', GalleryButton);

  private retryingCheckCover = false;

  /**
   * Initializes the result saver.
   *
   * @param dir Directory holding saved pictures showing in gallery.
   */
  async initialize(dir: DirectoryAccessEntry): Promise<void> {
    this.directory = dir;
    await this.checkCover();
    this.galleryButton.addEventListener('click', () => {
      if (this.cover !== null) {
        ChromeHelper.getInstance().openFileInGallery(this.cover.name);
      }
    });
  }

  private applyCover(cover: CoverPhoto|null) {
    this.cover = cover;
    this.galleryButton.cover = cover;
  }

  private monitorFileDeletion(fileName: string) {
    void ChromeHelper.getInstance().monitorFileDeletion(fileName, async () => {
      try {
        await this.checkCover();
      } catch (e) {
        reportError(ErrorType.CHECK_COVER_FAILURE, ErrorLevel.ERROR, e);
      }
    });
  }

  /**
   * @param file File to be set as cover photo.
   */
  private async updateCover(file: FileAccessEntry|null): Promise<void> {
    const cover =
        file === null ? null : await CoverPhoto.create(file, ImageFormat.JPEG);
    if (this.cover === cover) {
      return;
    }
    if (this.cover !== null) {
      this.cover.release();
    }
    this.applyCover(cover);
    if (file !== null) {
      this.monitorFileDeletion(file.name);
    }
  }

  private async processCapturedData(file: FileAccessEntry, fileType: FileType) {
    const fileName = file.name;
    if (!isCloudDestination()) {
      void ChromeHelper.getInstance().processCapturedLocalFile(
          fileName, fileType);
      void this.updateCover(file);
      return;
    }
    const cover = await CoverPhoto.create(file, ImageFormat.PNG);
    // Wait for the capture to be processed in the browser process.
    // The returned promise is intentionally not awaited to avoid blocking UI
    // during upload after which this promise is resolved and cover photo
    // updated.
    void ChromeHelper.getInstance()
        .processCapturedFileForCloudUpload(
            fileName,
            fileType,
            cover?.blob ?? new Blob(),
            )
        .then((success) => {
          if (success) {
            this.applyCover(cover);
            this.monitorFileDeletion(fileName);
          }
        });
    return;
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
      if (await dir.exists(this.cover.name)) {
        return;
      }
    }

    // Rescan file system. Only select files following CCA naming styles.
    const files = (await filesystem.getEntries())
                      .filter((file) => Filenamer.isCcaFileFormat(file.name));
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
    while (true) {
      await sleep(500);
      const newFileCount = (await filesystem.getEntries()).length;
      if (prevFileCount === newFileCount) {
        return;
      }
      prevFileCount = newFileCount;
    }
  }

  async savePhoto(blob: Blob, name: string, metadata: Metadata|null):
      Promise<void> {
    const file = await filesystem.saveBlob(blob, name);
    if (metadata !== null) {
      const metadataBlob =
          new Blob([JSON.stringify(metadata, null, 2)], {type: MimeType.JSON});
      await filesystem.saveBlob(metadataBlob, Filenamer.getMetadataName(name));
    }
    await this.processCapturedData(file, FileType.kPhoto);
  }

  async saveGif(blob: Blob, name: string): Promise<void> {
    const file = await filesystem.saveBlob(blob, name);
    await this.processCapturedData(file, FileType.kGif);
  }

  async saveVideo(file: FileAccessEntry): Promise<void> {
    const videoName = (new Filenamer()).newVideoName(VideoType.MP4);
    assert(this.directory !== null);
    await file.moveTo(this.directory, videoName);
    await this.processCapturedData(file, FileType.kVideo);
  }
}
