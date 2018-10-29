// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Converts a c/c++ time_t variable to Date.
 * The time we get from the archive is in local time.
 * @param {number} timestamp A c/c++ time_t variable.
 * @return {!Date}
 */
function DateFromTimeT(timestamp) {
  var local = new Date(1000 * timestamp);
  return new Date(local.getTime() + (local.getTimezoneOffset() * 60000));
}

/**
 * Decode a file name from a raw byte string by given |encoding|.
 *
 * @param {string} data hex-encoded string of the file name. Every 2 characters
 *     represent one byte in the 2-digit hexadecimal number.
 * @param {string} encoding
 */
function decodeBinary(data, encoding) {
  console.assert(data.length % 2 == 0, 'invalid encoding format');
  var codes = [];
  for (var i = 0; i < data.length; i += 2) {
    var s = data.substr(i, 2);
    codes.push(parseInt(s, 16));
  }
  return new TextDecoder(encoding).decode(new Uint8Array(codes));
}

/**
 * Collects |binary_name| of entries as a concatenated string, by recursively
 * scanning files under the subtree.
 *
 * @param {!EntryMetadata} entryMetadata of the root directory.
 * @param {number} limit Stops adding paths if total length exceed this.
 */
function getFileNameSample(entryMetadata, limit) {
  if (entryMetadata.isDirectory) {
    console.assert(
        entryMetadata.entries,
        'The field "entries" is mandatory for directories.');
    var names = '';
    for (var entry in entryMetadata.entries) {
      var subtree = getFileNameSample(entryMetadata.entries[entry], limit);
      names = names.concat(subtree);
      limit -= subtree.length;
      if (limit < 0)
        return names;
    }
    return names;
  } else if (entryMetadata.binary_name) {
    return entryMetadata.binary_name;
  }
  return '';
}

/**
 * Corrects metadata entries fields in order for them to be sent to Files.app.
 * This function runs recursively for every entry in a directory.
 *
 * @param {!Object<string, !EntryMetadata>} entryMetadata The metadata to
 *     correct.
 * @param {string} encoding Coding system which |binary_name| codes the
 *     original file name in the archive. Must be one of the labels supported
 *     by TextDecoder, or empty if |entryMeatadata.name| is already correct.
 */
function correctMetadata(entryMetadata, encoding) {
  entryMetadata.index = parseInt(entryMetadata.index, 10);
  entryMetadata.size = parseInt(entryMetadata.size, 10);
  entryMetadata.modificationTime =
      DateFromTimeT(entryMetadata.modificationTime);
  if (entryMetadata.isDirectory) {
    console.assert(
        entryMetadata.entries,
        'The field "entries" is mandatory for dictionaries.');
    var outputEntries = {};
    for (var entry in entryMetadata.entries) {
      correctMetadata(entryMetadata.entries[entry], encoding);
      var newKey;
      if (!encoding || !entryMetadata.entries[entry].binary_name) {
        newKey = entry;
      } else {
        newKey =
            decodeBinary(entryMetadata.entries[entry].binary_name, encoding);
      }
      if (newKey in outputEntries) {
        // TODO(crbug.com/846195): Tweak output file name to have a unique one.
        // This would not happen unless either the archive or encoding detection
        // was wrong. However an archive that doesn't use CP437 with the
        // language encoding flag (EFS) in the general purpose bit flag unset is
        // already invalid. So handling this case should be best-effort.
        console.warn(
            'Duplicated file names after encoding translation: ' + newKey);
      }
      outputEntries[newKey] = entryMetadata.entries[entry];
    }
    entryMetadata.entries = outputEntries;
  }
  if (entryMetadata.binary_name && encoding) {
    entryMetadata.name = decodeBinary(entryMetadata.binary_name, encoding);
  }
}

/**
 * Defines a volume object that contains information about archives' contents
 * and performs operations on these contents.
 * @constructor
 * @param {!unpacker.Decompressor} decompressor The decompressor used to obtain
 *     data from archives.
 * @param {!Entry} entry The entry corresponding to the volume's archive.
 */
unpacker.Volume = function(decompressor, entry) {
  /**
   * Used for restoring the opened file entry after resuming the event page.
   * @type {!Entry}
   */
  this.entry = entry;

  /**
   * @type {!unpacker.Decompressor}
   */
  this.decompressor = decompressor;

  /**
   * The volume's metadata. The key is the full path to the file on this volume.
   * For more details see
   * https://developer.chrome.com/apps/fileSystemProvider#type-EntryMetadata
   * @type {?Object<string, !EntryMetadata>}
   */
  this.metadata = null;

  /**
   * A map with currently opened files. The key is a requestId value from the
   * openFileRequested event and the value is the open file options.
   * @type {!Object<!unpacker.types.RequestId,
   *                !unpacker.types.OpenFileRequestedOptions>}
   */
  this.openedFiles = {};

  /**
   * Default encoding set for this archive. If empty, then not known.
   * @type {string}
   */
  this.encoding =
      unpacker.Volume.ENCODING_TABLE[chrome.i18n.getUILanguage()] || '';

  /**
   * The default read metadata request id. -1 is ok as the request ids used by
   * flleSystemProvider are greater than 0.
   * @type {number}
   */
  this.DEFAULT_READ_METADATA_REQUEST_ID = -1;
};

/**
 * The default read metadata request id. -1 is ok as the request ids used by
 * flleSystemProvider are greater than 0.
 * @const {number}
 */
unpacker.Volume.DEFAULT_READ_METADATA_REQUEST_ID = -1;

/**
 * Map from language codes to default charset encodings.
 * @const {!Object<string, string>}
 */
unpacker.Volume.ENCODING_TABLE = {
  ar: 'CP1256',
  bg: 'CP1251',
  ca: 'CP1252',
  cs: 'CP1250',
  da: 'CP1252',
  de: 'CP1252',
  el: 'CP1253',
  en: 'CP1250',
  en_GB: 'CP1250',
  es: 'CP1252',
  es_419: 'CP1252',
  et: 'CP1257',
  fa: 'CP1256',
  fi: 'CP1252',
  fil: 'CP1252',
  fr: 'CP1252',
  he: 'CP1255',
  hi: 'UTF-8',  // Another one may be better.
  hr: 'CP1250',
  hu: 'CP1250',
  id: 'CP1252',
  it: 'CP1252',
  ja: 'CP932',  // Alternatively SHIFT-JIS.
  ko: 'CP949',  // Alternatively EUC-KR.
  lt: 'CP1257',
  lv: 'CP1257',
  ms: 'CP1252',
  nl: 'CP1252',
  no: 'CP1252',
  pl: 'CP1250',
  pt_BR: 'CP1252',
  pt_PT: 'CP1252',
  ro: 'CP1250',
  ru: 'CP1251',
  sk: 'CP1250',
  sl: 'CP1250',
  sr: 'CP1251',
  sv: 'CP1252',
  th: 'CP874',  // Confirm!
  tr: 'CP1254',
  uk: 'CP1251',
  vi: 'CP1258',
  zh_CN: 'CP936',
  zh_TW: 'CP950'
};

// Map from a preferred MIME name detected by CED to the encoding label
// supproted by TextEncoder class. We may add more encodings but only if there's
// evidence that such archives are widespread.
unpacker.Volume.MIME_TO_ENCODING_TABLE = {
  'Shift_JIS': 'Shift_JIS'
};

/**
 * Size of sampled file name strings for auto-detecting the coding system.
 */
unpacker.Volume.ENCODING_DETECT_SAMPLE_SIZE = 300;

/**
 * @return {boolean} True if volume is ready to be used.
 */
unpacker.Volume.prototype.isReady = function() {
  return !!this.metadata;
};

/**
 * @return {boolean} True if volume is in use.
 */
unpacker.Volume.prototype.inUse = function() {
  return this.decompressor.hasRequestsInProgress() ||
      Object.keys(this.openedFiles).length > 0;
};

function GetEncoding(preferred_mime_name) {
  return unpacker.Volume.MIME_TO_ENCODING_TABLE[preferred_mime_name] || '';
}

/**
 * Initializes the volume by reading its metadata.
 * @param {function()} onSuccess Callback to execute on success.
 * @param {function(!ProviderError)} onError Callback to execute on error.
 */
unpacker.Volume.prototype.initialize = function(onSuccess, onError) {
  var requestId = unpacker.Volume.DEFAULT_READ_METADATA_REQUEST_ID;
  this.decompressor.readMetadata(requestId, this.encoding, function(metadata) {
    // Make a deep copy of metadata.
    this.metadata = /** @type {!Object<string, !EntryMetadata>} */ (
        JSON.parse(JSON.stringify(metadata)));

    var sample = getFileNameSample(
        this.metadata, unpacker.Volume.ENCODING_DETECT_SAMPLE_SIZE);
    chrome.fileManagerPrivate.detectCharacterEncoding(
        sample, function(preferred_mime_name) {
          correctMetadata(
              this.metadata, GetEncoding(preferred_mime_name) || '');
          onSuccess();
        }.bind(this));
  }.bind(this), onError);
};

/**
 * Obtains the metadata for a single entry in the archive. Assumes metadata is
 * loaded.
 * @param {!unpacker.types.GetMetadataRequestedOptions} options Options for
 *     getting the metadata of an entry.
 * @param {function(!EntryMetadata)} onSuccess Callback to execute on success.
 * @param {function(!ProviderError)} onError Callback to execute on error.
 */
unpacker.Volume.prototype.onGetMetadataRequested = function(
    options, onSuccess, onError) {
  console.assert(this.isReady(), 'Metadata must be loaded.');
  var entryMetadata = this.getEntryMetadata_(options.entryPath);
  if (!entryMetadata)
    onError('NOT_FOUND');
  else
    onSuccess(entryMetadata);
};

/**
 * Reads a directory contents from metadata. Assumes metadata is loaded.
 * @param {!unpacker.types.ReadDirectoryRequestedOptions} options Options
 *     for reading the contents of a directory.
 * @param {function(!Array<!EntryMetadata>, boolean)} onSuccess Callback to
 *     execute on success.
 * @param {function(!ProviderError)} onError Callback to execute on error.
 */
unpacker.Volume.prototype.onReadDirectoryRequested = function(
    options, onSuccess, onError) {
  console.assert(this.isReady(), 'Metadata must be loaded.');
  var directoryMetadata = this.getEntryMetadata_(options.directoryPath);
  if (!directoryMetadata) {
    onError('NOT_FOUND');
    return;
  }
  if (!directoryMetadata.isDirectory) {
    onError('NOT_A_DIRECTORY');
    return;
  }

  // Convert dictionary entries to an array.
  var entries = [];
  for (var entry in directoryMetadata.entries) {
    entries.push(directoryMetadata.entries[entry]);
  }

  onSuccess(entries, false /* Last call. */);
};

/**
 * Opens a file for read or write.
 * @param {!unpacker.types.OpenFileRequestedOptions} options Options for
 *     opening a file.
 * @param {function()} onSuccess Callback to execute on success.
 * @param {function(!ProviderError)} onError Callback to execute on error.
 */
unpacker.Volume.prototype.onOpenFileRequested = function(
    options, onSuccess, onError) {
  console.assert(this.isReady(), 'Metadata must be loaded.');
  if (options.mode != 'READ') {
    onError('INVALID_OPERATION');
    return;
  }

  var metadata = this.getEntryMetadata_(options.filePath);
  if (!metadata) {
    onError('NOT_FOUND');
    return;
  }

  this.openedFiles[options.requestId] = options;

  this.decompressor.openFile(
      options.requestId, metadata.index, this.encoding,
      function() {
        onSuccess();
      }.bind(this),
      function(error) {
        delete this.openedFiles[options.requestId];
        onError('FAILED');
      }.bind(this));
};

/**
 * Closes a file identified by options.openRequestId.
 * @param {!unpacker.types.CloseFileRequestedOptions} options Options for
 *     closing a file.
 * @param {function()} onSuccess Callback to execute on success.
 * @param {function(!ProviderError)} onError Callback to execute on error.
 */
unpacker.Volume.prototype.onCloseFileRequested = function(
    options, onSuccess, onError) {
  console.assert(this.isReady(), 'Metadata must be loaded.');
  var openRequestId = options.openRequestId;
  var openOptions = this.openedFiles[openRequestId];
  if (!openOptions) {
    onError('INVALID_OPERATION');
    return;
  }

  this.decompressor.closeFile(options.requestId, openRequestId, function() {
    delete this.openedFiles[openRequestId];
    onSuccess();
  }.bind(this), onError);
};

/**
 * Reads the contents of a file identified by options.openRequestId.
 * @param {!unpacker.types.ReadFileRequestedOptions} options Options for
 *     reading a file's contents.
 * @param {function(!ArrayBuffer, boolean)} onSuccess Callback to execute on
 *     success.
 * @param {function(!ProviderError)} onError Callback to execute on error.
 */
unpacker.Volume.prototype.onReadFileRequested = function(
    options, onSuccess, onError) {
  console.assert(this.isReady(), 'Metadata must be loaded.');
  var openOptions = this.openedFiles[options.openRequestId];
  if (!openOptions) {
    onError('INVALID_OPERATION');
    return;
  }

  var offset = options.offset;
  var length = options.length;
  // Offset and length should be validated by the API.
  console.assert(offset >= 0, 'Offset should be >= 0.');
  console.assert(length >= 0, 'Length should be >= 0.');

  var fileSize = this.getEntryMetadata_(openOptions.filePath).size;
  if (offset >= fileSize || length == 0) {  // No more data.
    onSuccess(new ArrayBuffer(0), false /* Last call. */);
    return;
  }
  length = Math.min(length, fileSize - offset);

  this.decompressor.readFile(
      options.requestId, options.openRequestId, offset, length, onSuccess,
      onError);
};

/**
 * Gets the metadata for an entry based on its path.
 * @param {string} entryPath The full path to the entry.
 * @return {?Object} The correspondent metadata.
 * @private
 */
unpacker.Volume.prototype.getEntryMetadata_ = function(entryPath) {
  var pathArray = entryPath.split('/');

  // Remove empty strings resulted after split. As paths start with '/' we will
  // have an empty string at the beginning of pathArray and possible an
  // empty string at the end for directories (e.g. /path/to/dir/). The code
  // assumes entryPath cannot have consecutive '/'.
  pathArray.splice(0, 1);

  if (pathArray.length > 0) {  // In case of 0 this is root directory.
    var lastIndex = pathArray.length - 1;
    if (pathArray[lastIndex] == '')
      pathArray.splice(lastIndex);
  }

  // Get the actual metadata by iterating through every directory metadata
  // on the path to the entry.
  var entryMetadata = this.metadata;
  for (var i = 0, limit = pathArray.length; i < limit; i++) {
    if (!entryMetadata ||
        !entryMetadata.isDirectory && i != limit - 1 /* Parent directory. */)
      return null;
    entryMetadata = entryMetadata.entries[pathArray[i]];
  }

  return entryMetadata;
};
