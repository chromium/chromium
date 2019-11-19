// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * A class that takes care of communication with NaCL and creates an archive.
 * One instance of this class is created for each pack request. Since multiple
 * compression requests can be in progress at the same time, each instance has
 * a unique compressor id of positive integer. Every communication with NaCL
 * must be done with compressor id.
 * @constructor
 * @param {!Object} naclModule The nacl module.
 * @param {!Array} items The items to be packed.
 * @param {boolean} useTemporaryDirectory Whether to use the temporary
 *     filesystem as work directory for creating a ZIP file. This is used for
 *     filesystem that degrades performance with frequent write operations,
 *     like online storages.
 */
unpacker.Compressor = function(naclModule, items, useTemporaryDirectory) {
  /**
   * @private {Object}
   * @const
   */
  this.naclModule_ = naclModule;

  /**
   * @private {!Array}
   * @const
   */
  this.items_ = items;

  /**
   * @private {!unpacker.types.CompressorId}
   * @const
   */
  this.compressorId_ = unpacker.Compressor.compressorIdCounter++;

  /**
   * @private {string}
   * @const
   */
  this.archiveName_ = this.getArchiveName_();

  /**
   * The counter used to assign a unique id to each entry.
   * @type {number}
   */
  this.entryIdCounter_ = 1;

  /**
   * The set of entry ids waiting for metadata from FileSystem API.
   * These requests needs to be tracked here to tell whether all pack process
   * has finished or not.
   * @type {!Set}
   */
  this.metadataRequestsInProgress_ = new Set();

  /**
   * The queue containing entry ids that have already obtained metadata from
   * FileSystem API and are waiting to be added into archive.
   * @type {!Array}
   */
  this.pendingAddToArchiveRequests_ = [];

  /**
   * The id of the entry that is being compressed and written into archive.
   * Note that packing of each entry should be done one by one unlike
   * unpacking. Thus, at most one entry is processed at once.
   * @type {!unpacker.types.EntryId}
   */
  this.entryIdInProgress_ = 0;

  /**
   * Map from entry ids to entries.
   * @const {!Object<!unpacker.types.EntryId, !FileEntry|!DirectoryEntry>}
   */
  this.entries_ = {};

  /**
   * The file being packed.
   * @type {File}
   */
  this.file_ = null;

  /**
   * Map from entry ids to its metadata.
   * @const {!Object<!unpacker.types.EntryId, !Metadata>}
   */
  this.metadata_ = {};

  /**
   * The offset from which the entry in progress should be read.
   * @type {number}
   */
  this.offset_ = 0;

  /**
   * The total number of bytes of all items.
   * @type {number}
   */
  this.totalSize_ = 0;

  /**
   * The total number of processed bytes.
   * @type {number}
   */
  this.processedSize_ = 0;

  /**
   * Whether to write in-progress zip file in temporary location or not.
   * @type {boolean}
   */
  this.useTemporaryDirectory_ = useTemporaryDirectory;
};

/**
 * The counter which is assigned and incremented every time a new compressor
 * instance is created.
 * @type {number}
 */
unpacker.Compressor.compressorIdCounter = 1;

/**
 * The default archive name.
 * @type {string}
 */
unpacker.Compressor.DEFAULT_ARCHIVE_NAME = 'Archive.zip';

/**
 * The getter function for compressor id.
 * @return {!unpacker.types.CompressorId}
 */
unpacker.Compressor.prototype.getCompressorId = function() {
  return this.compressorId_;
};

/**
 * Returns the archive file name that depends on selected items.
 * @private
 * @return {string}
 */
unpacker.Compressor.prototype.getArchiveName_ = function() {
  // When multiple entries are selected.
  if (this.items_.length !== 1)
    return unpacker.Compressor.DEFAULT_ARCHIVE_NAME;

  var name = this.items_[0].entry.name;
  var idx = name.lastIndexOf('.');
  // When the name does not have extension.
  // TODO(takise): This converts file.tar.gz to file.tar.zip.
  if (idx === -1)
    return name + '.zip';
  // When the name has extension.
  return name.substring(0, idx) + '.zip';
};

/**
 * Returns the archive file name.
 * @return {string}
 */
unpacker.Compressor.prototype.getArchiveName = function() {
  return this.archiveName_;
};

/**
 * Starts actual compressing process.
 * Creates an archive file and requests minizip to create an archive object.
 * @param {function(!unpacker.types.CompressorId)} onSuccess
 * @param {function(!unpacker.types.CompressorId)} onError
 * @param {function(!unpacker.types.CompressorId, number)} onProgress
 * @param {function(!unpacker.types.CompressorId)} onCancel
 */
unpacker.Compressor.prototype.compress = function(
    onSuccess, onError, onProgress, onCancel) {
  this.onSuccess_ = onSuccess;
  this.onError_ = onError;
  this.onProgress_ = onProgress;
  this.onCancel_ = onCancel;

  this.getArchiveFile_();
};

/**
 * Returns archive file entry.
 * @return {FileEntry}
 */
unpacker.Compressor.prototype.archiveFileEntry = function() {
  return this.archiveFileEntry_;
};

/**
 * Gets an archive file with write permission.
 * @private
 */
unpacker.Compressor.prototype.getArchiveFile_ = function() {
  var suggestedName = this.archiveName_;

  var saveZipFile = (rootEntry) => {
    // If parent directory of currently selected files is available then we
    // deduplicate |suggestedName| and save the zip file.
    if (!rootEntry) {
      console.error('rootEntry of selected files is undefined');
      this.onErrorInternal_();
      return;
    }
    fileOperationUtils.deduplicateFileName(suggestedName, rootEntry)
        .then((newName) => {
          // Create an archive file.
          return (new Promise(function(resolve, reject) {
                   rootEntry.getFile(
                       newName, {create: true, exclusive: true}, resolve,
                       reject);
                 }))
              .then((zipEntry) => {
                this.archiveFileEntry_ = zipEntry;
                this.sendCreateArchiveRequest_();
              });
        })
        .catch((error) => {
          console.error('failed to create a ZIP file: ' + error.code);
          this.onErrorInternal_();
        });
  };
  var getWorkRootPromise;
  if (this.useTemporaryDirectory_) {
    getWorkRootPromise = this.getTemporaryRootEntry_();
  } else {
    getWorkRootPromise = this.getParentEntry_();
  }

  getWorkRootPromise.then(saveZipFile).catch((domException) => {
    console.error(domException);
    this.onErrorInternal_();
  });
};

/**
 * Creates a Promise which gives the root entry of a temporary filesystem.
 * @return {!Promise<!Entry>}
 */
unpacker.Compressor.prototype.getTemporaryRootEntry_ = function() {
  return new Promise(function(resolve, reject) {
           navigator.webkitTemporaryStorage
               .queryUsageAndQuota(function(used, granted) {
                 resolve(granted);
               }, reject);
         })
      .then(
          (quota) =>
              new Promise(webkitRequestFileSystem.bind(null, TEMPORARY, quota)))
      .then((fs) => fs.root);
};

/**
 * Sends an create archive request to NaCL.
 * @private
 */
unpacker.Compressor.prototype.sendCreateArchiveRequest_ = function() {
  var request = unpacker.request.createCreateArchiveRequest(this.compressorId_);
  this.naclModule_.postMessage(request);
};

/**
 * A handler of create archive done response.
 * Enumerates entries and requests FileSystem API for their metadata.
 * @private
 */
unpacker.Compressor.prototype.createArchiveDone_ = function() {
  this.items_.forEach(function(item) {
    this.getEntryMetadata_(item.entry);
  }.bind(this));
};

/**
 * Gets metadata of a file or directory.
 * @param {!FileEntry|!DirectoryEntry} entry FileEntry or DirectoryEntry.
 * @private
 */
unpacker.Compressor.prototype.getEntryMetadata_ = function(entry) {
  if (entry.isFile)
    this.getSingleMetadata_(entry);
  else
    this.getDirectoryEntryMetadata_(/** @type {!DirectoryEntry} */ (entry));
};

/**
 * Requests metadata of an entry non-recursively.
 * @param {!FileEntry|!DirectoryEntry} entry FileEntry or DirectoryEntry.
 * @private
 */
unpacker.Compressor.prototype.getSingleMetadata_ = function(entry) {
  var entryId = this.entryIdCounter_++;
  this.metadataRequestsInProgress_.add(entryId);
  this.entries_[entryId] = entry;

  entry.getMetadata(
      function(metadata) {
        this.metadataRequestsInProgress_.delete(entryId);
        this.pendingAddToArchiveRequests_.push(entryId);
        this.metadata_[entryId] = metadata;
        this.totalSize_ += metadata.size;
        this.sendAddToArchiveRequest_();
      }.bind(this),
      function(error) {
        console.error('Failed to get metadata: ' + error.message + '.');
        this.onErrorInternal_();
      }.bind(this));
};

/**
 * Requests metadata of an entry recursively.
 * @param {!DirectoryEntry} dir DirectoryEntry.
 * @private
 */
unpacker.Compressor.prototype.getDirectoryEntryMetadata_ = function(dir) {

  // Read entries in dir and call getEntryMetadata_ for them recursively.
  var dirReader = dir.createReader();

  // Recursive function
  var getEntries = function() {
    dirReader.readEntries(
        function(results) {
          // ReadEntries must be called until it returns nothing, because
          // it does not necessarily return all entries in the directory.
          if (results.length) {
            results.forEach(this.getEntryMetadata_.bind(this));
            getEntries();
          }
        }.bind(this),
        function(error) {
          console.error(
              'Failed to get directory entries: ' + error.message + '.');
          this.onErrorInternal_();
        }.bind(this));
  }.bind(this);

  getEntries();

  // Get the metadata of this dir itself.
  this.getSingleMetadata_(dir);
};

/**
 * Pops an entry from the queue and adds it to the archive.
 * If another entry is in progress, this function does nothing. If there is no
 * entry in the queue, it shifts to close archive process. Otherwise, this sends
 * an add to archive request for a popped entry with its metadata to minizip.
 * @private
 */
unpacker.Compressor.prototype.sendAddToArchiveRequest_ = function() {
  // Another process is in progress.
  if (this.entryIdInProgress_ != 0)
    return;

  // All entries have already been archived.
  if (this.pendingAddToArchiveRequests_.length === 0) {
    if (this.metadataRequestsInProgress_.size === 0)
      this.sendCloseArchiveRequest(false /* hasError */);
    return;
  }

  var entryId = this.pendingAddToArchiveRequests_.shift();
  this.entryIdInProgress_ = entryId;

  // Convert the absolute path on the virtual filesystem to a relative path from
  // the archive root by removing the leading '/' if exists.
  var fullPath = this.entries_[entryId].fullPath;
  if (fullPath.length && fullPath[0] == '/')
    fullPath = fullPath.substring(1);

  // Modification time is set to the archive in local time.
  var utc = this.metadata_[entryId].modificationTime;
  var modificationTime = utc.getTime() - (utc.getTimezoneOffset() * 60000);

  var request = unpacker.request.createAddToArchiveRequest(
      this.compressorId_, entryId, fullPath, this.metadata_[entryId].size,
      modificationTime, this.entries_[entryId].isDirectory);
  this.naclModule_.postMessage(request);
};

/**
 * Sends a release compressor request to NaCl module. Zip Archiver releases
 * objects obtainted in the packing process.
 */
unpacker.Compressor.prototype.sendReleaseCompressor = function() {
  var request =
      unpacker.request.createReleaseCompressorRequest(this.compressorId_);
  this.naclModule_.postMessage(request);
};

/**
 * Sends a close archive request to minizip. minizip writes metadata of
 * the archive itself on the archive and releases objects obtainted in the
 * packing process.
 */
unpacker.Compressor.prototype.sendCloseArchiveRequest = function(hasError) {
  var request =
      unpacker.request.createCloseArchiveRequest(this.compressorId_, hasError);
  this.naclModule_.postMessage(request);
};

/**
 * Sends a cancel archive request to minizip and interrupts zip process.
 */
unpacker.Compressor.prototype.sendCancelArchiveRequest = function() {
  var request = unpacker.request.createCancelArchiveRequest(this.compressorId_);
  this.naclModule_.postMessage(request);
};

/**
 * Sends a read file chunk done response.
 * @param {number} length The number of bytes read from the entry.
 * @param {!ArrayBuffer} buffer A buffer containing the data that was read.
 * @private
 */
unpacker.Compressor.prototype.sendReadFileChunkDone_ = function(
    length, buffer) {
  var request = unpacker.request.createReadFileChunkDoneResponse(
      this.compressorId_, length, buffer);
  this.naclModule_.postMessage(request);
};

/**
 * A handler of read file chunk messages.
 * Reads 'length' bytes from the entry currently in process.
 * @param {!Object} data
 * @private
 */
unpacker.Compressor.prototype.onReadFileChunk_ = function(data) {
  var entryId = this.entryIdInProgress_;
  var entry = this.entries_[entryId];
  var length = Number(data[unpacker.request.Key.LENGTH]);

  // A function to create a reader and read bytes.
  var readFileChunk = function() {
    var file = this.file_.slice(this.offset_, this.offset_ + length);
    var reader = new FileReader();

    reader.onloadend = function(event) {
      var buffer = event.target.result;

      // The buffer must have 'length' bytes because the byte length which can
      // be read from the file is already calculated on NaCL side.
      if (buffer.byteLength !== length) {
        console.error(
            'Tried to read chunk with length ' + length +
            ', but byte with length ' + buffer.byteLength + ' was returned.');

        // If the first argument(length) is negative, it means that an error
        // occurred in reading a chunk.
        this.sendReadFileChunkDone_(-1, buffer);
        this.onErrorInternal_();
        return;
      }

      this.offset_ += length;
      this.sendReadFileChunkDone_(length, buffer);
    }.bind(this);

    reader.onerror = (event) => {
      console.error(
          'Failed to read file chunk. Name: ' + file.name +
          ', offset: ' + this.offset_ + ', length: ' + length + '.');

      // If the first argument(length) is negative, it means that an error
      // occurred in reading a chunk.
      this.sendReadFileChunkDone_(-1, new ArrayBuffer(0));
      this.onErrorInternal_();
    };

    reader.readAsArrayBuffer(file);
  }.bind(this);

  // When the entry is read for the first time.
  if (!this.file_) {
    entry.file(
        (file) => {
          this.file_ = file;
          readFileChunk();
        },
        (error) => {
          console.error(error);
          this.onErrorInternal_();
        });
    return;
  }

  // From the second time onward.
  readFileChunk();
};

/**
 * A handler of write chunk requests.
 * Writes the data in the given buffer onto the archive file.
 * @param {!Object} data
 * @private
 */
unpacker.Compressor.prototype.onWriteChunk_ = function(data) {
  var offset = Number(data[unpacker.request.Key.OFFSET]);
  var length = Number(data[unpacker.request.Key.LENGTH]);
  var buffer = data[unpacker.request.Key.CHUNK_BUFFER];
  this.writeChunk_(offset, length, buffer, this.sendWriteChunkDone_.bind(this));
};

/**
 * Writes buffer into the archive file (window.archiveFileEntry).
 * @param {number} offset The offset from which date is written.
 * @param {number} length The number of bytes in the buffer to write.
 * @param {!ArrayBuffer} buffer The buffer to write in the archive.
 * @param {function(number)} callback Callback to execute at the end of the
 *     function. This function has one parameter: length, which represents the
 *     length of bytes written on to the archive. If writing a chunk fails,
 *     a negative value must be assigned to this argument.
 * @private
 */
unpacker.Compressor.prototype.writeChunk_ = function(
    offset, length, buffer, callback) {
  // TODO(takise): Use the same instance of FileWriter over multiple calls of
  // this function instead of creating new ones.
  this.archiveFileEntry_.createWriter(
      function(fileWriter) {
        fileWriter.onwriteend = function(event) {
          callback(length);
        };

        fileWriter.onerror = function(event) {
          console.error(
              'Failed to write chunk to ' + this.archiveFileEntry_ + '.');
          // If the first argument(length) is negative, it means that an error
          // occurred in writing a chunk.
          callback(-1 /* length */);
          this.onErrorInternal_();
        }.bind(this);

        // Create a new Blob and append it to the archive file.
        var blob = new Blob([buffer], {});
        fileWriter.seek(offset);
        fileWriter.write(blob);
      }.bind(this),
      function(event) {
        console.error(
            'Failed to create writer for ' + this.archiveFileEntry_ + '.');
        this.onErrorInternal_();
      }.bind(this));
};

/**
 * Sends a write chunk done response.
 * @param {number} length The number of bytes written onto the entry.
 * @private
 */
unpacker.Compressor.prototype.sendWriteChunkDone_ = function(length) {
  var request =
      unpacker.request.createWriteChunkDoneResponse(this.compressorId_, length);
  this.naclModule_.postMessage(request);
};

/**
 * A handler of add to archive done responses.
 * Resets information on the current entry and starts processing another entry.
 * @private
 */
unpacker.Compressor.prototype.onAddToArchiveDone_ = function() {
  // Reset information on the current entry.
  this.entryIdInProgress_ = 0;
  this.file_ = null;
  this.offset_ = 0;

  // Start processing another entry.
  this.sendAddToArchiveRequest_();
};

/**
 * Moves the temporary file to actual destination folder.
 * @param {function()} onFinished called when file move is finished.
 */
unpacker.Compressor.prototype.moveZipFileToActualDestination = function(
    onFinished) {
  var suggestedName = this.getArchiveName_();
  const moveZipFileToParentDir = (rootEntry) => {
    // If parent directory of currently selected files is available then we
    // deduplicate |suggestedName| and save the zip file.
    if (!rootEntry)
      return Promise.reject('rootEntry of selected files is undefined');

    return fileOperationUtils.deduplicateFileName(suggestedName, rootEntry)
        .then((newName) => new Promise((resolve, reject) => {
                this.archiveFileEntry_.moveTo(
                    rootEntry, newName, resolve, (error) => {
                      reject('Failed to move the file to destination.');
                    });
              }));
  };
  this.getParentEntry_()
      .then(moveZipFileToParentDir)
      .then(onFinished)
      .catch((error) => {
        this.onErrorInternal_();
        console.error(error);
      });
};

/**
 * Creates a Promise which gives the parent entry of the files to be zipped.
 * @rerturn {!Promise<!Entry>}
 * @private
 */
unpacker.Compressor.prototype.getParentEntry_ = function() {
  return new Promise((resolve, reject) => {
    // Get all accessible volumes with their metadata
    chrome.fileManagerPrivate.getVolumeMetadataList((volumeMetadataList) => {
      // Here we call chrome.fileSystem.requestFileSystem on each volume's
      // metadata entry to be able to sucessfully execute
      // resolveIsolatedEntries later.
      Promise.all(this.requestAccessPermissionForVolumes_(volumeMetadataList))
          .then((result) => {
            chrome.fileManagerPrivate.resolveIsolatedEntries(
                [this.items_[0].entry], (result) => {
                  if (result && result.length >= 1) {
                    result[0].getParent(resolve);
                  } else {
                    console.error('Failed to resolve isolated entries!');
                    if (chrome.runtime.lastError)
                      console.error(chrome.runtime.lastError.message);

                    reject();
                  }
                });
          })
          .catch((error) => {
            console.error(error);
            reject();
          });
    });
  });
};

/**
 * A handler of close archive responses.
 * Receiving this response means the entire packing process has finished.
 * @private
 */
unpacker.Compressor.prototype.onCloseArchiveDone_ = function() {
  this.onSuccess_(this.compressorId_);
};

/**
 * A handler of cancel archive response. Receiving this response means that we
 * do not expect new requests from Zip Archiver.
 * @private
 */
unpacker.Compressor.prototype.onCancelArchiveDone_ = function() {
  console.warn('Archive for "' + this.compressorId_ + '" has been canceled.');
  this.removeTemporaryFileIfExists_();
  this.onCancel_(this.compressorId_);
};

/**
 * Processes messages from NaCl module.
 * @param {!Object} data The data contained in the message from NaCl. Its
 *     types depend on the operation of the request.
 * @param {!unpacker.request.Operation} operation An operation from request.js.
 */
unpacker.Compressor.prototype.processMessage = function(data, operation) {
  switch (operation) {
    case unpacker.request.Operation.CREATE_ARCHIVE_DONE:
      this.createArchiveDone_();
      break;

    case unpacker.request.Operation.READ_FILE_CHUNK:
      this.onReadFileChunk_(data);
      // We are updating progress in READ and WRITE part because in some cases
      // when compression ratio is very high we will rarely get WRITE
      // operation. Therefore we need to update in both operations.
      this.onProgress_(
          data.compressor_id, this.processedSize_ / this.totalSize_);
      this.processedSize_ += parseInt(data.length);
      break;

    case unpacker.request.Operation.WRITE_CHUNK:
      this.onWriteChunk_(data);
      this.onProgress_(
          data.compressor_id, this.processedSize_ / this.totalSize_);
      break;

    case unpacker.request.Operation.ADD_TO_ARCHIVE_DONE:
      this.onAddToArchiveDone_();
      break;

    case unpacker.request.Operation.CLOSE_ARCHIVE_DONE:
      this.sendReleaseCompressor();
      if (this.useTemporaryDirectory_)
        this.moveZipFileToActualDestination(() => this.onCloseArchiveDone_());
      else
        this.onCloseArchiveDone_();
      break;

    case unpacker.request.Operation.CANCEL_ARCHIVE_DONE:
      this.sendReleaseCompressor();
      this.onCancelArchiveDone_();
      break;

    case unpacker.request.Operation.COMPRESSOR_ERROR:
      console.error(
          'Compressor error for compressor id ' + this.compressorId_ + ': ' +
          data[unpacker.request.Key.ERROR]);  // The error contains
                                              // the '.' at the end.
      this.sendReleaseCompressor();
      this.onErrorInternal_();
      break;

    default:
      console.error('Invalid NaCl operation: ' + operation + '.');
      this.sendReleaseCompressor();
      this.onErrorInternal_();
  }
};

/**
 * Requests access permissions for |volumeMetadataList| file systems.
 * @param {!Array<!Object>} volumeMetadataList The metadata for each mounted
 *     volume.
 * @return {!Array<!Promise<!FileSystem>>}
 * @private
 */
unpacker.Compressor.prototype.requestAccessPermissionForVolumes_ = function(
    volumeMetadataList) {
  var promises = [];
  volumeMetadataList.forEach(function(volumeMetadata) {
    if (volumeMetadata.isReadOnly)
      return;

    promises.push(new Promise(function(resolve, reject) {
      chrome.fileSystem.requestFileSystem(
          {
            volumeId: volumeMetadata.volumeId,
            writable: !volumeMetadata.isReadOnly
          },
          function(isolatedFileSystem) {
            if (chrome.runtime.lastError)
              reject(chrome.runtime.lastError.message);
            else
              resolve(isolatedFileSystem);
          });
    }));
  });

  return promises;
};

/**
 * Cleans up the temporary file and notify error.
 */
unpacker.Compressor.prototype.onErrorInternal_ = function() {
  this.removeTemporaryFileIfExists_();
  this.onError_(this.compressorId_);
};

/**
 * Removes the temporary zip file in the local storage.
 */
unpacker.Compressor.prototype.removeTemporaryFileIfExists_ = function() {
  if (!this.archiveFileEntry_)
    return;
  const entry = this.archiveFileEntry_;
  this.archiveFileEntry_ = null;
  entry.remove(
      function() {},
      function(error) {
        console.error('failed to remove temporary file.');
      });
};
