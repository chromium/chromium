// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from './assert.js';
import * as dom from './dom.js';
import {reportError} from './error.js';
import * as filesystem from './models/file_system.js';
import {
  DirectoryAccessEntry,  // eslint-disable-line no-unused-vars
  FileAccessEntry,       // eslint-disable-line no-unused-vars
} from './models/file_system_access_entry.js';
// eslint-disable-next-line no-unused-vars
import {ResultSaver} from './models/result_saver.js';
import {VideoSaver} from './models/video_saver.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import {extractImageFromBlob} from './thumbnailer.js';
import {
  ErrorLevel,
  ErrorType,
  MimeType,
  VideoType,
} from './type.js';

/**
 * Cover photo of gallery button.
 */
class CoverPhoto {
  /**
   * @param {!FileAccessEntry} file File entry of cover photo.
   * @param {?string} url Url to its cover photo. Might be null if the cover is
   *     failed to load.
   * @param {boolean} draggable If the file type support share by dragg/drop
   *     cover photo.
   */
  constructor(file, url, draggable) {
    /**
     * @type {!FileAccessEntry}
     * @const
     */
    this.file = file;

    /**
     * @type {?string}
     * @const
     */
    this.url = url;

    /**
     * @const {boolean}
     */
    this.draggable = draggable;
  }

  /**
   * File name of the cover photo.
   * @return {string}
   */
  get name() {
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
   * @param {!FileAccessEntry} file
   * @return {!Promise<?CoverPhoto>}
   */
  static async create(file) {
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
 * @implements {ResultSaver}
 */
export class GalleryButton {
  /**
   * @public
   */
  constructor() {
    /**
     * Cover photo from latest saved picture.
     * @type {?CoverPhoto}
     * @private
     */
    this.cover_ = null;

    /**
     * @type {!HTMLButtonElement}
     * @private
     */
    this.button_ = dom.get('#gallery-enter', HTMLButtonElement);

    /**
     * @type {!HTMLImageElement}
     * @private
     */
    this.coverPhoto_ = dom.getFrom(this.button_, 'img', HTMLImageElement);

    /**
     * Directory holding saved pictures showing in gallery.
     * @type {?DirectoryAccessEntry}
     * @private
     */
    this.directory_ = null;

    this.button_.addEventListener('click', async () => {
      if (this.cover_ !== null) {
        await ChromeHelper.getInstance().openFileInGallery(
            this.cover_.file.name);
      }
    });
  }

  /**
   * Initializes the gallery button.
   * @param {!DirectoryAccessEntry} dir Directory holding saved pictures
   *     showing in gallery.
   */
  async initialize(dir) {
    this.directory_ = dir;
    await this.checkCover_();
  }

  /**
   * @param {?FileAccessEntry} file File to be set as cover photo.
   * @return {!Promise}
   * @private
   */
  async updateCover_(file) {
    const cover = file === null ? null : await CoverPhoto.create(file);
    if (this.cover_ === cover) {
      return;
    }
    if (this.cover_ !== null) {
      this.cover_.release();
    }
    this.cover_ = cover;

    this.button_.hidden = cover === null;
    this.coverPhoto_.classList.toggle('draggable', cover?.draggable ?? false);
    this.coverPhoto_.src = cover?.url ?? '';

    if (cover !== null) {
      ChromeHelper.getInstance().monitorFileDeletion(file.name, () => {
        this.checkCover_();
      });
    }
  }

  /**
   * Checks validity of cover photo from camera directory.
   * @private
   */
  async checkCover_() {
    if (this.directory_ === null) {
      return;
    }
    const dir = this.directory_;

    // Checks existence of cached cover photo.
    if (this.cover_ !== null) {
      if (await dir.isExist(this.cover_.name)) {
        return;
      }
    }

    // Rescan file system.
    const files = await filesystem.getEntries();
    if (files.length === 0) {
      await this.updateCover_(null);
      return;
    }
    const filesWithTime = await Promise.all(
        files.map(async (file) => ({
                    file,
                    time: (await file.getLastModificationTime()),
                  })));
    const lastFile =
        filesWithTime.reduce((last, cur) => last.time > cur.time ? last : cur)
            .file;
    await this.updateCover_(lastFile);
  }

  /**
   * @override
   */
  async savePhoto(blob, name) {
    const file = await filesystem.saveBlob(blob, name);

    ChromeHelper.getInstance().sendNewCaptureBroadcast(
        {isVideo: false, name: file.name});
    await this.updateCover_(file);
  }

  /**
   * @override
   */
  async saveGif(blob, name) {
    const file = await filesystem.saveBlob(blob, name);
    await this.updateCover_(file);
  }

  /**
   * @override
   */
  async startSaveVideo(videoRotation) {
    const file = await filesystem.createVideoFile(VideoType.MP4);
    return VideoSaver.createForFile(file, videoRotation);
  }

  /**
   * @override
   */
  async finishSaveVideo(video) {
    const file = await video.endWrite();
    assert(file !== null);

    ChromeHelper.getInstance().sendNewCaptureBroadcast(
        {isVideo: true, name: file.name});
    await this.updateCover_(file);
  }
}
