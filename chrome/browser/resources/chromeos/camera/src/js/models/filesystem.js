// Copyright 2018 The Chromium Authors. All rights reserved.
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
 * Creates the file system controller.
 * @constructor
 */
cca.models.FileSystem = function() {
  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * The prefix of thumbnail files.
 * @type {string}
 * @const
 */
cca.models.FileSystem.THUMBNAIL_PREFIX = 'thumb-';

/**
 * Width of thumbnail.
 * @type {number}
 * @const
 */
cca.models.FileSystem.THUMBNAIL_WIDTH = 480;

/**
 * Directory in the internal file system.
 * @type {DirectoryEntry}
 */
cca.models.FileSystem.internalDir = null;

/**
 * Temporary directory in the internal file system.
 * @type {DirectoryEntry}
 */
cca.models.FileSystem.internalTempDir = null;

/**
 * Directory in the external file system.
 * @type {DirectoryEntry}
 */
cca.models.FileSystem.externalDir = null;

/**
 * Initializes the directory in the internal file system.
 * @return {!Promise<DirectoryEntry>} Promise for the directory result.
 * @private
 */
cca.models.FileSystem.initInternalDir_ = function() {
  return new Promise((resolve, reject) => {
    webkitRequestFileSystem(
        window.PERSISTENT, 768 * 1024 * 1024 /* 768MB */,
        (fs) => resolve(fs.root), reject);
  });
};

/**
 * Initializes the temporary directory in the internal file system.
 * @return {!Promise<DirectoryEntry>} Promise for the directory result.
 * @private
 */
cca.models.FileSystem.initInternalTempDir_ = function() {
  return new Promise((resolve, reject) => {
    webkitRequestFileSystem(
        window.TEMPORARY, 768 * 1024 * 1024 /* 768MB */,
        (fs) => resolve(fs.root), reject);
  });
};

/**
 * Initializes the directory in the external file system.
 * @return {!Promise<?DirectoryEntry>} Promise for the directory result.
 * @private
 */
cca.models.FileSystem.initExternalDir_ = function() {
  return new Promise((resolve) => {
    if (!cca.util.isChromeOS()) {
      resolve([null, null]);
      return;
    }
    cca.proxy.browserProxy.getVolumeList((volumes) => {
      if (volumes) {
        for (var i = 0; i < volumes.length; i++) {
          var volumeId = volumes[i].volumeId;
          if (volumeId.indexOf('downloads:Downloads') !== -1 ||
              volumeId.indexOf('downloads:MyFiles') !== -1) {
            cca.proxy.browserProxy.requestFileSystem(
                volumes[i], (fs) => resolve([fs && fs.root, volumeId]));
            return;
          }
        }
      }
      resolve([null, null]);
    });
  }).then(([dir, volumeId]) => {
    if (volumeId && volumeId.indexOf('downloads:MyFiles') !== -1) {
      return cca.models.FileSystem.readDir_(dir).then((entries) => {
        return entries.find(
            (entry) => entry.name == 'Downloads' && entry.isDirectory);
      });
    }
    return dir;
  });
};

/**
 * Initializes file systems, migrating pictures if needed. This function
 * should be called only once in the beginning of the app.
 * @param {function()} promptMigrate Callback to instantiate a promise that
       prompts users to migrate pictures if no acknowledgement yet.
 * @return {!Promise<boolean>} Promise for the external-fs result.
 */
cca.models.FileSystem.initialize = function(promptMigrate) {
  var checkAcked = new Promise((resolve) => {
    // ack 0: User has not yet acknowledged to migrate pictures.
    // ack 1: User acknowledges to migrate pictures to Downloads.
    cca.proxy.browserProxy.localStorageGet(
        {ackMigratePictures: 0},
        (values) => resolve(values.ackMigratePictures >= 1));
  });
  var checkMigrated = new Promise((resolve) => {
    if (chrome.chromeosInfoPrivate) {
      chrome.chromeosInfoPrivate.get(['cameraMediaConsolidated'],
          (values) => resolve(values['cameraMediaConsolidated']));
    } else {
      resolve(false);
    }
  });
  var ackMigrate = () =>
      cca.proxy.browserProxy.localStorageSet({ackMigratePictures: 1});
  var doneMigrate = () => chrome.chromeosInfoPrivate &&
      chrome.chromeosInfoPrivate.set('cameraMediaConsolidated', true);

  return Promise
      .all([
        cca.models.FileSystem.initInternalDir_(),
        cca.models.FileSystem.initInternalTempDir_(),
        cca.models.FileSystem.initExternalDir_(),
        checkAcked,
        checkMigrated,
      ])
      .then(([internalDir, internalTempDir, externalDir, acked, migrated]) => {
        cca.models.FileSystem.internalDir = internalDir;
        cca.models.FileSystem.internalTempDir = internalTempDir;
        cca.models.FileSystem.externalDir = externalDir;
        if (migrated && !externalDir) {
          throw new Error('External file system should be available.');
        }
        // Check if acknowledge-prompt and migrate-pictures are needed.
        if (migrated || !cca.models.FileSystem.externalDir) {
          return [false, false];
        }
        // Check if any internal picture other than thumbnail needs migration.
        // Pictures taken by old Camera App may not have IMG_ or VID_ prefix.
        var dir = cca.models.FileSystem.internalDir;
        return cca.models.FileSystem.readDir_(dir)
            .then((entries) => {
              return entries.some(
                  (entry) => !cca.models.FileSystem.hasThumbnailPrefix_(entry));
            })
            .then((migrateNeeded) => {
              if (migrateNeeded) {
                return [!acked, true];
              }
              // If the external file system is supported and there is already
              // no picture in the internal file system, it implies done
              // migration and then doesn't need acknowledge-prompt.
              ackMigrate();
              doneMigrate();
              return [false, false];
            });
      })
      .then(
          ([promptNeeded, migrateNeeded]) => {  // Prompt to migrate if needed.
            return !promptNeeded ? migrateNeeded : promptMigrate().then(() => {
              ackMigrate();
              return migrateNeeded;
            });
          })
      .then((migrateNeeded) => {  // Migrate pictures if needed.
        const external = cca.models.FileSystem.externalDir != null;
        return !migrateNeeded ? external :
                                cca.models.FileSystem.migratePictures()
                                    .then(doneMigrate)
                                    .then(() => external);
      });
};

/**
 * Reads file entries from the directory.
 * @param {DirectoryEntry} dir Directory entry to be read.
 * @return {!Promise<!Array<FileEntry>>} Promise for the read file entries.
 * @private
 */
cca.models.FileSystem.readDir_ = function(dir) {
  return !dir ? Promise.resolve([]) : new Promise((resolve, reject) => {
    var dirReader = dir.createReader();
    var entries = [];
    var readEntries = () => {
      dirReader.readEntries((inEntries) => {
        if (inEntries.length == 0) {
          resolve(entries);
          return;
        }
        entries = entries.concat(inEntries);
        readEntries();
      }, reject);
    };
    readEntries();
  });
};

/**
 * Migrates all picture-files from internal storage to external storage.
 * @return {!Promise} Promise for the operation.
 */
cca.models.FileSystem.migratePictures = function() {
  var internalDir = cca.models.FileSystem.internalDir;
  var externalDir = cca.models.FileSystem.externalDir;

  var migratePicture = (pictureEntry, thumbnailEntry) => {
    var name = cca.models.FileSystem.regulatePictureName(pictureEntry);
    return cca.models.FileSystem.getFile(
        externalDir, name, true).then((entry) => {
      return new Promise((resolve, reject) => {
        pictureEntry.copyTo(externalDir, entry.name, (result) => {
          if (result.name != pictureEntry.name && thumbnailEntry) {
            // Thumbnails can be recreated later if failing to rename them here.
            thumbnailEntry.moveTo(internalDir,
                cca.models.FileSystem.getThumbnailName(result));
          }
          pictureEntry.remove(() => {});
          resolve();
        }, reject);
      });
    });
  };

  return cca.models.FileSystem.readDir_(internalDir).then((internalEntries) => {
    var pictureEntries = [];
    var thumbnailEntriesByName = {};
    cca.models.FileSystem.parseInternalEntries_(
        internalEntries, thumbnailEntriesByName, pictureEntries);

    var migrated = [];
    for (var index = 0; index < pictureEntries.length; index++) {
      var entry = pictureEntries[index];
      var thumbnailName = cca.models.FileSystem.getThumbnailName(entry);
      var thumbnailEntry = thumbnailEntriesByName[thumbnailName];
      migrated.push(migratePicture(entry, thumbnailEntry));
    }
    return Promise.all(migrated);
  });
};

/**
 * Regulates the picture name to the desired format if it's in legacy formats.
 * @param {FileEntry} entry Picture entry whose name to be regulated.
 * @return {string} Name in the desired format.
 */
cca.models.FileSystem.regulatePictureName = function(entry) {
  if (cca.models.FileSystem.hasVideoPrefix(entry) ||
      cca.models.FileSystem.hasImagePrefix_(entry)) {
    var match = entry.name.match(/(\w{3}_\d{8}_\d{6})(?:_(\d+))?(\..+)?$/);
    if (match) {
      var idx = match[2] ? ' (' + match[2] + ')' : '';
      var ext = match[3] ? match[3].replace(/\.webm$/, '.mkv') : '';
      return match[1] + idx + ext;
    }
  } else {
    // Early pictures are in legacy file name format (crrev.com/c/310064).
    var match = entry.name.match(/(\d+).(?:\d+)/);
    if (match) {
      return (new cca.models.Filenamer(parseInt(match[1], 10))).newImageName();
    }
  }
  return entry.name;
};

/**
 * Saves the blob to the given file name. Name of the actually saved file
 * might be different from the given file name if the file already exists.
 * @param {DirectoryEntry} dir Directory to be written into.
 * @param {string} name Name of the file.
 * @param {!Blob} blob Data of the file to be saved.
 * @return {!Promise<FileEntry>} Promise for the result.
 * @private
 */
cca.models.FileSystem.saveToFile_ = function(dir, name, blob) {
  return cca.models.FileSystem.getFile(dir, name, true).then((entry) => {
    return new Promise((resolve, reject) => {
      entry.createWriter((fileWriter) => {
        fileWriter.onwriteend = () => resolve(entry);
        fileWriter.onerror = reject;
        fileWriter.write(blob);
      }, reject);
    });
  });
};

/**
 * Saves photo blob or metadata blob into predefined default location.
 * @param {!Blob} blob Data of the photo to be saved.
 * @param {string} filename Filename of the photo to be saved.
 * @return {!Promise<FileEntry>} Promise for the result.
 */
cca.models.FileSystem.saveBlob = function(blob, filename) {
  const dir =
      cca.models.FileSystem.externalDir || cca.models.FileSystem.internalDir;
  return cca.models.FileSystem.saveToFile_(dir, filename, blob);
};

/**
 * Creates a file for saving temporary video recording result.
 * @return {!Promise<!FileEntry>} Newly created temporary file.
 * @throws {Error} If failed to create video temp file.
 */
cca.models.FileSystem.createTempVideoFile = async function() {
  const dir =
      cca.models.FileSystem.externalDir || cca.models.FileSystem.internalDir;
  const filename = new cca.models.Filenamer().newVideoName();
  const file = await cca.models.FileSystem.getFile(dir, filename, true);
  if (file === null) {
    throw new Error('Failed to create video temp file.');
  }
  return file;
};

/**
 * @const {string}
 */
cca.models.FileSystem.PRIVATE_TEMPFILE_NAME = 'video-intent.mkv';

/**
 * @return {!Promise<!FileEntry>} Newly created temporary file.
 * @throws {Error} If failed to create video temp file.
 */
cca.models.FileSystem.createPrivateTempVideoFile = async function() {
  // TODO(inker): Handles running out of space case.
  const dir = cca.models.FileSystem.internalTempDir;
  const file = await cca.models.FileSystem.getFile(
      dir, cca.models.FileSystem.PRIVATE_TEMPFILE_NAME, true);
  if (file === null) {
    throw new Error('Failed to create private video temp file.');
  }
  return file;
};

/**
 * Saves temporary video file to predefined default location.
 * @param {FileEntry} tempfile Temporary video file to be saved.
 * @param {string} filename Filename to be saved.
 * @return {Promise<?FileEntry>} Saved video file.
 */
cca.models.FileSystem.saveVideo = async function(tempfile, filename) {
  var dir =
      cca.models.FileSystem.externalDir || cca.models.FileSystem.internalDir;
  if (!dir) {
    return await null;
  }

  // Non-null version for the Closure Compiler.
  let nonNullDir = dir;

  // Assuming content of tempfile contains all recorded chunks appended together
  // and is a well-formed video. The work needed here is just to move the file
  // to the correct directory and rename as the specified filename.
  if (tempfile.name == filename) {
    return tempfile;
  }
  return new Promise(
      (resolve, reject) =>
          tempfile.moveTo(nonNullDir, filename, resolve, reject));
};

/**
 * Gets the thumbnail name of the given picture.
 * @param {FileEntry} entry Picture's file entry.
 * @return {string} Thumbnail name.
 */
cca.models.FileSystem.getThumbnailName = function(entry) {
  var thumbnailName = cca.models.FileSystem.THUMBNAIL_PREFIX + entry.name;
  return (thumbnailName.substr(0, thumbnailName.lastIndexOf('.')) ||
      thumbnailName) + '.jpg';
};

/**
 * Creates and saves the thumbnail of the given picture.
 * @param {boolean} isVideo Picture is a video.
 * @param {FileEntry} entry Picture's file entry whose thumbnail to be saved.
 * @return {!Promise<FileEntry>} Promise for the result.
 */
cca.models.FileSystem.saveThumbnail = function(isVideo, entry) {
  return cca.models.FileSystem.pictureURL(entry)
      .then((url) => {
        return cca.util.scalePicture(
            url, isVideo, cca.models.FileSystem.THUMBNAIL_WIDTH);
      })
      .then((blob) => {
        var thumbnailName = cca.models.FileSystem.getThumbnailName(entry);
        return cca.models.FileSystem.saveToFile_(
            cca.models.FileSystem.internalDir, thumbnailName, blob);
      });
};

/**
 * Checks if the entry's name has the video prefix.
 * @param {FileEntry} entry File entry.
 * @return {boolean} Has the video prefix or not.
 */
cca.models.FileSystem.hasVideoPrefix = function(entry) {
  return entry.name.startsWith(cca.models.Filenamer.VIDEO_PREFIX);
};

/**
 * Checks if the entry's name has the image prefix.
 * @param {FileEntry} entry File entry.
 * @return {boolean} Has the image prefix or not.
 * @private
 */
cca.models.FileSystem.hasImagePrefix_ = function(entry) {
  return entry.name.startsWith(cca.models.Filenamer.IMAGE_PREFIX);
};

/**
 * Checks if the entry's name has the thumbnail prefix.
 * @param {FileEntry} entry File entry.
 * @return {boolean} Has the thumbnail prefix or not.
 * @private
 */
cca.models.FileSystem.hasThumbnailPrefix_ = function(entry) {
  return entry.name.startsWith(cca.models.FileSystem.THUMBNAIL_PREFIX);
};

/**
 * Parses and filters the internal entries to thumbnail and picture entries.
 * @param {Array<FileEntry>} internalEntries Internal file entries.
 * @param {Object<string, FileEntry>} thumbnailEntriesByName Result thumbanil
 *     entries mapped by thumbnail names, initially empty.
 * @param {Array<FileEntry>=} pictureEntries Result picture entries, initially
 *     empty.
 * @private
 */
cca.models.FileSystem.parseInternalEntries_ = function(
    internalEntries, thumbnailEntriesByName, pictureEntries) {
  var isThumbnail = cca.models.FileSystem.hasThumbnailPrefix_;
  var thumbnailEntries = [];
  if (pictureEntries) {
    for (var index = 0; index < internalEntries.length; index++) {
      if (isThumbnail(internalEntries[index])) {
        thumbnailEntries.push(internalEntries[index]);
      } else {
        pictureEntries.push(internalEntries[index]);
      }
    }
  } else {
    thumbnailEntries = internalEntries.filter(isThumbnail);
  }
  for (var index = 0; index < thumbnailEntries.length; index++) {
    var thumbnailEntry = thumbnailEntries[index];
    thumbnailEntriesByName[thumbnailEntry.name] = thumbnailEntry;
  }
};

/**
 * Gets the picture and thumbnail entries.
 * @return {!Promise<!Array<!Array<FileEntry>|!Object<string, FileEntry>>>}
 *     Promise for the picture entries and the thumbnail entries mapped by
 *     thumbnail names.
 */
cca.models.FileSystem.getEntries = function() {
  return Promise.all([
    cca.models.FileSystem.readDir_(cca.models.FileSystem.internalDir),
    cca.models.FileSystem.readDir_(cca.models.FileSystem.externalDir),
  ]).then(([internalEntries, externalEntries]) => {
    var pictureEntries = [];
    var thumbnailEntriesByName = {};

    if (cca.models.FileSystem.externalDir) {
      pictureEntries = externalEntries.filter((entry) => {
        if (!cca.models.FileSystem.hasVideoPrefix(entry) &&
            !cca.models.FileSystem.hasImagePrefix_(entry)) {
          return false;
        }
        return entry.name.match(/_(\d{8})_(\d{6})(?: \((\d+)\))?/);
      });
      cca.models.FileSystem.parseInternalEntries_(
          internalEntries, thumbnailEntriesByName);
    } else {
      cca.models.FileSystem.parseInternalEntries_(
          internalEntries, thumbnailEntriesByName, pictureEntries);
    }
    return [pictureEntries, thumbnailEntriesByName];
  });
};

/**
 * Returns an URL for a picture.
 * @param {FileEntry} entry File entry.
 * @return {!Promise<string>} Promise for the result.
 */
cca.models.FileSystem.pictureURL = function(entry) {
  return new Promise((resolve) => {
    if (cca.models.FileSystem.externalDir) {
      entry.file((file) => resolve(URL.createObjectURL(file)));
    } else {
      resolve(entry.toURL());
    }
  });
};

/**
 * Gets the file by the given name, avoiding name conflicts if necessary.
 * @param {DirectoryEntry} dir Directory to get the file from.
 * @param {string} name File name. Result file may have a different name.
 * @param {boolean} create True to create file, false otherwise.
 * @return {!Promise<?FileEntry>} Promise for the result.
 */
cca.models.FileSystem.getFile = function(dir, name, create) {
  return new Promise((resolve, reject) => {
    var options = create ? {create: true, exclusive: true} : {create: false};
    dir.getFile(name, options, resolve, reject);
  }).catch((error) => {
    if (create && error.name == 'InvalidModificationError') {
      // Avoid name conflicts for creating files.
      return cca.models.FileSystem.getFile(dir,
          cca.models.FileSystem.incrementFileName_(name), create);
    } else if (!create && error.name == 'NotFoundError') {
      return null;
    }
    throw error;
  });
};

/**
 * Increments the file index of a given file name to avoid name conflicts.
 * @param {string} name File name.
 * @return {string} File name with incremented index.
 * @private
 */
cca.models.FileSystem.incrementFileName_ = function(name) {
  var [base, ext] = ['', ''];
  var idx = 0;
  var match = name.match(/^([^.]+)(\..+)?$/);
  if (match) {
    base = match[1];
    ext = match[2];
    match = base.match(/ \((\d+)\)$/);
    if (match) {
      base = base.substring(0, match.index);
      idx = parseInt(match[1], 10);
    }
  }
  return base + ' (' + (idx + 1) + ')' + ext;
};
