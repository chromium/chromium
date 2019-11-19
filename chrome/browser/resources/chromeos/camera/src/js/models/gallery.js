// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for models.
 */
cca.models = cca.models || {};

/**
 * Creates the gallery model controller.
 * @constructor
 * @implements {cca.models.ResultSaver}
 */
cca.models.Gallery = function() {
  /**
   * @type {Array<cca.models.Gallery.Observer>}
   * @private
   */
  this.observers_ = [];

  /**
   * @type {Promise<Array<cca.models.Gallery.Picture>>}
   * @private
   */
  this.loaded_ = null;

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Wraps an image/video and its thumbnail as a picture in the model.
 * @param {FileEntry} thumbnailEntry Thumbnail file entry.
 * @param {FileEntry} pictureEntry Picture file entry.
 * @param {boolean} isMotionPicture True if it's a motion picture (video),
 *     false it's a still picture (image).
 * @constructor
 */
cca.models.Gallery.Picture = function(
    thumbnailEntry, pictureEntry, isMotionPicture) {
  /**
   * @type {?FileEntry}
   * @private
   */
  this.thumbnailEntry_ = thumbnailEntry;

  /**
   * @type {FileEntry}
   * @private
   */
  this.pictureEntry_ = pictureEntry;

  /**
   * @type {boolean}
   * @private
   */
  this.isMotionPicture_ = isMotionPicture;

  /**
   * @type {Date}
   * @private
   */
  this.timestamp_ = cca.models.Gallery.Picture.parseTimestamp_(pictureEntry);

  // End of properties. Freeze the object.
  Object.freeze(this);
};

/**
 * Gets a picture's timestamp from its name.
 * @param {FileEntry} pictureEntry Picture file entry.
 * @return {Date} Picture timestamp.
 * @private
 */
cca.models.Gallery.Picture.parseTimestamp_ = function(pictureEntry) {
  var num = function(str) {
    return parseInt(str, 10);
  };

  var name = cca.models.FileSystem.regulatePictureName(pictureEntry);
  // Match numeric parts from filenames, e.g. IMG_'yyyyMMdd_HHmmss (n)'.jpg.
  // Assume no more than one picture taken within one millisecond.
  // Use String.raw instead of /...regex.../ here to avoid breaking syntax
  // highlight on gerrit.
  var match = name.match(
      String.raw`_(\d{4})(\d{2})(\d{2})_(\d{2})(\d{2})(\d{2})(?: \((\d+)\))?/`);
  return match ?
      new Date(
          num(match[1]), num(match[2]) - 1, num(match[3]), num(match[4]),
          num(match[5]), num(match[6]), match[7] ? num(match[7]) : 0) :
      new Date(0);
};

cca.models.Gallery.Picture.prototype = {
  // Assume pictures always have different names as URL API may still point to
  // the deleted file for new files created with the same name.
  get thumbnailURL() {
    return this.thumbnailEntry_ && this.thumbnailEntry_.toURL();
  },
  get pictureEntry() {
    return this.pictureEntry_;
  },
  get isMotionPicture() {
    return this.isMotionPicture_;
  },
  get timestamp() {
    return this.timestamp_;
  },
};

/**
 * Creates and returns an URL for a picture.
 * @return {!Promise<string>} Promise for the result.
 */
cca.models.Gallery.Picture.prototype.pictureURL = function() {
  return cca.models.FileSystem.pictureURL(this.pictureEntry_);
};

/**
 * Observer interface for the pictures' model changes.
 * @constructor
 */
cca.models.Gallery.Observer = function() {
};

/**
 * Notifies about a deleted picture.
 * @param {cca.models.Gallery.Picture} picture Picture deleted.
 */
cca.models.Gallery.Observer.prototype.onPictureDeleted = function(picture) {
};

/**
 * Notifies about an added picture.
 * @param {cca.models.Gallery.Picture} picture Picture added.
 */
cca.models.Gallery.Observer.prototype.onPictureAdded = function(picture) {
};

/**
 * Adds an observer.
 * @param {cca.models.Gallery.Observer} observer Observer to be added.
 */
cca.models.Gallery.prototype.addObserver = function(observer) {
  this.observers_.push(observer);
};

/**
 * Notifies observers about the added or deleted picture.
 * @param {string} fn Observers' callback function name.
 * @param {cca.models.Gallery.Picture} picture Picture added or deleted.
 * @private
 */
cca.models.Gallery.prototype.notifyObservers_ = function(fn, picture) {
  this.observers_.forEach((observer) => observer[fn](picture));
};

/**
 * Loads the pictures from the storages and adds them to the pictures model.
 */
cca.models.Gallery.prototype.load = function() {
  this.loaded_ = cca.models.FileSystem.getEntries().then(
      ([pictureEntries, thumbnailEntriesByName]) => {
        var wrapped =
            pictureEntries.filter((entry) => entry.name).map((entry) => {
              var name = cca.models.FileSystem.getThumbnailName(entry);
              return this.wrapPicture_(entry, thumbnailEntriesByName[name]);
            });
        // Sort pictures by timestamps to make most recent picture at the end.
        // TODO(yuli): Remove unused thumbnails.
        return Promise.all(wrapped).then((pictures) => {
          return pictures.sort((a, b) => {
            if (a.timestamp == null) {
              return -1;
            }
            if (b.timestamp == null) {
              return 1;
            }
            return a.timestamp - b.timestamp;
          });
        });
      });

  this.loaded_.then((pictures) => {
    pictures.forEach((picture) =>
        this.notifyObservers_('onPictureAdded', picture));
  }).catch(console.warn);
};

/**
 * Gets the last picture of the loaded pictures' model.
 * @return {!Promise<cca.models.Gallery.Picture>} Promise for the result.
 */
cca.models.Gallery.prototype.lastPicture = function() {
  return this.loaded_.then((pictures) => pictures[pictures.length - 1]);
};

/**
 * Checks and updates the last picture of the loaded pictures' model.
 * @return {!Promise<cca.models.Gallery.Picture>} Promise for the result.
 */
cca.models.Gallery.prototype.checkLastPicture = function() {
  return this.lastPicture().then((picture) => {
    // Assume only external pictures were removed without updating the model.
    var dir = cca.models.FileSystem.externalDir;
    if (dir && picture) {
      var name = picture.pictureEntry.name;
      return cca.models.FileSystem.getFile(dir, name, false).then(
          (entry) => [picture, (entry != null)]);
    }
    return [picture, (picture != null)];
  }).then(([picture, pictureEntryExist]) => {
    if (pictureEntryExist || !picture) {
      return picture;
    }
    return this.deletePicture(picture, true).then(
        this.checkLastPicture.bind(this));
  });
};

/**
 * Deletes the picture in the pictures' model.
 * @param {cca.models.Gallery.Picture} picture Picture to be deleted.
 * @param {boolean=} pictureEntryDeleted Whether the picture-entry was deleted.
 * @return {!Promise} Promise for the operation.
 */
cca.models.Gallery.prototype.deletePicture = function(
    picture, pictureEntryDeleted) {
  var removed = new Promise((resolve, reject) => {
    if (pictureEntryDeleted) {
      resolve();
    } else {
      picture.pictureEntry.remove(resolve, reject);
    }
  });
  return Promise.all([this.loaded_, removed]).then(([pictures, _]) => {
    var removal = pictures.indexOf(picture);
    if (removal != -1) {
      pictures.splice(removal, 1);
    }
    this.notifyObservers_('onPictureDeleted', picture);
    if (picture.thumbnailEntry_) {
      picture.thumbnailEntry_.remove(() => {});
    }
  });
};

/**
 * Exports the picture to the external storage.
 * @param {cca.models.Gallery.Picture} picture Picture to be exported.
 * @param {FileEntry} entry Target file entry.
 * @return {!Promise} Promise for the operation.
 */
cca.models.Gallery.prototype.exportPicture = function(picture, entry) {
  return new Promise((resolve, reject) => {
    entry.getParent((directory) => {
      picture.pictureEntry.copyTo(directory, entry.name, resolve, reject);
    }, reject);
  });
};

/**
 * Wraps file entries as a picture for the pictures' model.
 * @param {FileEntry} pictureEntry Picture file entry.
 * @param {FileEntry=} thumbnailEntry Thumbnail file entry.
 * @return {!Promise<cca.models.Gallery.Picture>} Promise for the picture.
 * @private
 */
cca.models.Gallery.prototype.wrapPicture_ = function(
    pictureEntry, thumbnailEntry) {
  // Create the thumbnail if it's not cached yet. Ignore errors and proceed to
  // wrap the picture even if unable to save its thumbnail.
  var isMotionPicture = cca.models.FileSystem.hasVideoPrefix(pictureEntry);
  var saved = () => {
    return cca.models.FileSystem.saveThumbnail(
        isMotionPicture, pictureEntry).catch(() => null);
  };
  return Promise.resolve(thumbnailEntry || saved()).then((thumbnailEntry) => {
    return new cca.models.Gallery.Picture(
        thumbnailEntry, pictureEntry, isMotionPicture);
  });
};

/**
 * @override
 */
cca.models.Gallery.prototype.savePhoto = function(blob, name) {
  // TODO(yuli): models.Gallery listens to models.FileSystem's file-added event
  // and then add a new picture into the model.
  var saved = new Promise((resolve) => {
                // Ignore errors since it is better to save something than
                // nothing.
                // TODO(yuli): Support showing images by EXIF orientation
                // instead.
                cca.util.orientPhoto(blob, resolve, () => resolve(blob));
              })
                  .then((blob) => {
                    return cca.models.FileSystem.saveBlob(blob, name);
                  })
                  .then((pictureEntry) => {
                    return this.wrapPicture_(pictureEntry);
                  });

  return saved.then((picture) => this.addPicture_(picture));
};

/**
 * @override
 */
cca.models.Gallery.prototype.startSaveVideo = async function() {
  const tempFile = await cca.models.FileSystem.createTempVideoFile();
  return cca.models.FileVideoSaver.create(tempFile);
};

/**
 * @override
 */
cca.models.Gallery.prototype.finishSaveVideo = async function(video, name) {
  const tempFile = await video.endWrite();
  const savedFile = await cca.models.FileSystem.saveVideo(tempFile, name);
  const picture = await this.wrapPicture_(savedFile);
  await this.addPicture_(picture);
};

/**
 * Adds a picture into gallery.
 * @param {cca.models.Gallery.Picture} picture Picture to be added.
 * @private
 */
cca.models.Gallery.prototype.addPicture_ = async function(picture) {
  const pictures = await this.loaded_;
  // Insert the picture into the sorted pictures' model.
  for (var index = pictures.length - 1; index >= 0; index--) {
    if (picture.timestamp >= pictures[index].timestamp) {
      break;
    }
  }
  pictures.splice(index + 1, 0, picture);
  this.notifyObservers_('onPictureAdded', picture);
};
