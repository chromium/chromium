// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * A class that takes care of communication between NaCl and archive volume.
 * Its job is to handle communication with the naclModule.
 * @constructor
 * @param {!Object} naclModule The nacl module with which the decompressor
 *     communicates.
 * @param {!unpacker.types.FileSystemId} fileSystemId The file system id of for
 *     the archive volume to decompress.
 * @param {!Blob} blob The correspondent file blob for fileSystemId.
 * @param {!unpacker.PassphraseManager} passphraseManager Passphrase manager.
 */
unpacker.Decompressor = function(
    naclModule, fileSystemId, blob, passphraseManager) {
  /**
   * @private {!Object}
   * @const
   */
  this.naclModule_ = naclModule;

  /**
   * @private {!unpacker.types.FileSystemId}
   * @const
   */
  this.fileSystemId_ = fileSystemId;

  /**
   * @private {!Blob}
   * @const
   */
  this.blob_ = blob;

  /**
   * @public {!unpacker.PassphraseManager}
   * @const
   */
  this.passphraseManager = passphraseManager;

  /**
   * Requests in progress. No need to save them onSuspend for now as metadata
   * reads are restarted from start.
   * @public {!Object<!unpacker.types.RequestId, !Object>}
   * @const
   */
  this.requestsInProgress = {};

  /**
   * Number of consecutive times the user has canceled passphrase input.
   *
   * @private {Number}
   */
  this.passphraseCancels_ = 0;
};

/**
 * Maximum number of times the passphrase dialog box is canceled consecutively
 * before no longer requesting a passphrase.
 *
 * @private {Number}
 * @const
 */
unpacker.Decompressor.MAX_PASSPHRASE_CANCEL_THRESHOLD = 2;

/**
 * @return {boolean} True if there is any request in progress.
 */
unpacker.Decompressor.prototype.hasRequestsInProgress = function() {
  return Object.keys(this.requestsInProgress).length > 0;
};

/**
 * Sends a request to NaCl and mark it as a request in progress. onSuccess and
 * onError are the callbacks used when receiving an answer from NaCl.
 * @param {!unpacker.types.RequestId} requestId The operation request id, which
 *     should be unique per every volume.
 * @param {function(...)} onSuccess Callback to execute on success.
 * @param {function(!ProviderError)} onError Callback to execute on error.
 * @param {!Object} naclRequest A request that must be sent to NaCl using
 *     postMessage.
 * @private
 */
unpacker.Decompressor.prototype.addRequest_ = function(
    requestId, onSuccess, onError, naclRequest) {
  console.assert(
      !this.requestsInProgress[requestId],
      'There is already a request with the id ' + requestId + '.');

  this.requestsInProgress[requestId] = {onSuccess: onSuccess, onError: onError};

  this.naclModule_.postMessage(naclRequest);
};

/**
 * Creates a request for reading metadata.
 * @param {!unpacker.types.RequestId} requestId
 * @param {string} encoding Default encoding for the archive's headers.
 * @param {function(!Object<string, !Object>)} onSuccess Callback to execute
 *     once the metadata is obtained from NaCl. It has one parameter, which is
 *     the metadata itself. The metadata has as key the full path to an entry
 *     and as value information about the entry.
 * @param {function(!ProviderError)} onError Callback to execute on error.
 */
unpacker.Decompressor.prototype.readMetadata = function(
    requestId, encoding, onSuccess, onError) {
  this.addRequest_(
      requestId, onSuccess, onError,
      unpacker.request.createReadMetadataRequest(
          this.fileSystemId_, requestId, encoding, this.blob_.size));
};

/**
 * Sends an open file request to NaCl.
 * @param {!unpacker.types.RequestId} requestId
 * @param {number} index Index of the file in the header list.
 * @param {string} encoding Default encoding for the archive's headers.
 * @param {function()} onSuccess Callback to execute on successful open.
 * @param {function(!ProviderError)} onError Callback to execute on error.
 */
unpacker.Decompressor.prototype.openFile = function(
    requestId, index, encoding, onSuccess, onError) {
  this.addRequest_(
      requestId, onSuccess, onError,
      unpacker.request.createOpenFileRequest(
          this.fileSystemId_, requestId, index, encoding, this.blob_.size));
};

/**
 * Sends a close file request to NaCl.
 * @param {!unpacker.types.RequestId} requestId
 * @param {!unpacker.types.RequestId} openRequestId The request id of the
 *     corresponding open file operation for the file to close.
 * @param {function()} onSuccess Callback to execute on successful open.
 * @param {function(!ProviderError)} onError Callback to execute on error.
 */
unpacker.Decompressor.prototype.closeFile = function(
    requestId, openRequestId, onSuccess, onError) {
  this.addRequest_(
      requestId, onSuccess, onError,
      unpacker.request.createCloseFileRequest(
          this.fileSystemId_, requestId, openRequestId));
};

/**
 * Sends a read file request to NaCl.
 * @param {!unpacker.types.RequestId} requestId
 * @param {!unpacker.types.RequestId} openRequestId The request id of the
 *     corresponding open file operation for the file to read.
 * @param {number} offset The offset from where read operation should start.
 * @param {number} length The number of bytes to read.
 * @param {function(!ArrayBuffer, boolean)} onSuccess Callback to execute on
 *     success.
 * @param {function(!ProviderError)} onError Callback to execute on error.
 */
unpacker.Decompressor.prototype.readFile = function(
    requestId, openRequestId, offset, length, onSuccess, onError) {
  this.addRequest_(
      requestId, onSuccess, onError,
      unpacker.request.createReadFileRequest(
          this.fileSystemId_, requestId, openRequestId, offset, length));
};

/**
 * Processes messages from NaCl module.
 * @param {!Object} data The data contained in the message from NaCl. Its
 *     types depend on the operation of the request.
 * @param {!unpacker.request.Operation} operation An operation from request.js.
 * @param {number} requestId The request id, which should be unique per every
 *     volume.
 */
unpacker.Decompressor.prototype.processMessage = function(
    data, operation, requestId) {
  // Create a request reference for asynchronous calls as sometimes we delete
  // some requestsInProgress from this.requestsInProgress.
  var requestInProgress = this.requestsInProgress[requestId];
  console.assert(
      requestInProgress,
      'No request with id <' + requestId + '> for: ' + this.fileSystemId_ +
          '.');

  switch (operation) {
    case unpacker.request.Operation.READ_METADATA_DONE:
      var metadata = data[unpacker.request.Key.METADATA];
      console.assert(metadata, 'No metadata.');
      requestInProgress.onSuccess(metadata);
      break;

    case unpacker.request.Operation.READ_CHUNK:
      this.readChunk_(data, requestId);
      // this.requestsInProgress_[requestId] should be valid as long as NaCL
      // can still make READ_CHUNK requests.
      return;

    case unpacker.request.Operation.READ_PASSPHRASE:
      this.readPassphrase_(data, requestId);
      // this.requestsInProgress_[requestId] should be valid as long as NaCL
      // can still make READ_PASSPHRASE requests.
      return;

    case unpacker.request.Operation.OPEN_FILE_DONE:
      requestInProgress.onSuccess();
      // this.requestsInProgress_[requestId] should be valid until closing the
      // file so NaCL can make READ_CHUNK requests.
      return;

    case unpacker.request.Operation.CLOSE_FILE_DONE:
      var openRequestId = data[unpacker.request.Key.OPEN_REQUEST_ID];
      console.assert(openRequestId, 'No open request id.');

      openRequestId = Number(openRequestId);  // Received as string.
      delete this.requestsInProgress[openRequestId];
      requestInProgress.onSuccess();
      break;

    case unpacker.request.Operation.READ_FILE_DONE:
      var buffer = data[unpacker.request.Key.READ_FILE_DATA];
      console.assert(buffer, 'No buffer for read file operation.');
      var hasMoreData = data[unpacker.request.Key.HAS_MORE_DATA];
      console.assert(
          buffer !== undefined,
          'No HAS_MORE_DATA boolean value for file operation.');

      requestInProgress.onSuccess(buffer, hasMoreData /* Last call. */);
      if (hasMoreData)
        return;  // Do not delete requestInProgress.
      break;

    case unpacker.request.Operation.FILE_SYSTEM_ERROR:
      console.error(
          'File system error for <' + this.fileSystemId_ +
          '>: ' + data[unpacker.request.Key.ERROR]);  // The error contains
                                                      // the '.' at the end.
      requestInProgress.onError('FAILED');
      break;

    case unpacker.request.Operation.CONSOLE_LOG:
    case unpacker.request.Operation.CONSOLE_DEBUG:
      var srcFile = data[unpacker.request.Key.SRC_FILE];
      var srcLine = data[unpacker.request.Key.SRC_LINE];
      var srcFunc = data[unpacker.request.Key.SRC_FUNC];
      var msg = data[unpacker.request.Key.MESSAGE];
      var log = operation == unpacker.request.Operation.CONSOLE_LOG ?
          console.log :
          console.debug;
      log(srcFile + ':' + srcFunc + ':' + srcLine + ': ' + msg);
      break;

    default:
      console.error('Invalid NaCl operation: ' + operation + '.');
      requestInProgress.onError('FAILED');
      break;
  }

  delete this.requestsInProgress[requestId];
};

/**
 * Reads a chunk of data from this.blob_ for READ_CHUNK operation.
 * @param {!Object} data The data received from the NaCl module.
 * @param {number} requestId The request id, which should be unique per every
 *     volume.
 * @private
 */
unpacker.Decompressor.prototype.readChunk_ = function(data, requestId) {
  // Offset and length are received as strings. See request.js.
  var offsetStr = data[unpacker.request.Key.OFFSET];
  var lengthStr = data[unpacker.request.Key.LENGTH];

  // Explicit check if offset is undefined as it can be 0.
  console.assert(
      offsetStr !== undefined && !isNaN(offsetStr) && Number(offsetStr) >= 0 &&
          Number(offsetStr) < this.blob_.size,
      'Invalid offset.');
  console.assert(
      lengthStr && !isNaN(lengthStr) && Number(lengthStr) > 0,
      'Invalid length.');

  var offset = Number(offsetStr);
  var length = Math.min(this.blob_.size - offset, Number(lengthStr));

  // Read a chunk from offset to offset + length.
  var blob = this.blob_.slice(offset, offset + length);
  var fileReader = new FileReader();

  fileReader.onload = function(event) {
    this.naclModule_.postMessage(unpacker.request.createReadChunkDoneResponse(
        this.fileSystemId_, requestId, event.target.result, offset));
  }.bind(this);

  fileReader.onerror = function(event) {
    console.error('Failed to read a chunk of data from the archive.');
    this.naclModule_.postMessage(unpacker.request.createReadChunkErrorResponse(
        this.fileSystemId_, requestId));
    // Reading from the source file failed. Assume that the file is gone and
    // unmount the archive.
    // TODO(523195): Show a notification that the source file is gone.
    unpacker.app.unmountVolume(this.fileSystemId_, true);
  }.bind(this);

  fileReader.readAsArrayBuffer(blob);
};

/**
 * Reads a passphrase from user input for READ_PASSPHRASE operation.
 * @param {!Object} data The data received from the NaCl module.
 * @param {number} requestId The request id, which should be unique per every
 *     volume.
 * @private
 */
unpacker.Decompressor.prototype.readPassphrase_ = function(data, requestId) {
  if (this.passphraseCancels_ >=
      unpacker.Decompressor.MAX_PASSPHRASE_CANCEL_THRESHOLD) {
    this.naclModule_.postMessage(
        unpacker.request.createReadPassphraseErrorResponse(
            this.fileSystemId_, requestId));
    return;
  }
  this.passphraseManager.getPassphrase()
      .then(function(passphrase) {
        this.passphraseCancels_ = 0;
        this.naclModule_.postMessage(
            unpacker.request.createReadPassphraseDoneResponse(
                this.fileSystemId_, requestId, passphrase));
      }.bind(this))
      .catch(function(error) {
        console.error(error.stack || error);
        this.naclModule_.postMessage(
            unpacker.request.createReadPassphraseErrorResponse(
                this.fileSystemId_, requestId));
        this.passphraseCancels_++;
      }.bind(this));
};
