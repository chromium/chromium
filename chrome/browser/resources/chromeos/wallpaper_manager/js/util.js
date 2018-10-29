// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var WallpaperUtil = {
  strings: null,  // Object that contains all the flags
  syncFs: null,   // syncFileSystem handler
  webkitFs: null  // webkitFileSystem handler
};

/**
 * Deletes |wallpaperFileName| and its associated thumbnail from local FS.
 * @param {string} wallpaperFilename Name of the file that will be deleted
 */
WallpaperUtil.deleteWallpaperFromLocalFS = function(wallpaperFilename) {
  WallpaperUtil.requestLocalFS(function(fs) {
    var originalPath =
        Constants.WallpaperDirNameEnum.ORIGINAL + '/' + wallpaperFilename;
    var thumbnailPath =
        Constants.WallpaperDirNameEnum.THUMBNAIL + '/' + wallpaperFilename;
    fs.root.getFile(
        originalPath, {create: false},
        function(fe) {
          fe.remove(function() {}, null);
        },
        // NotFoundError is expected. After we receive a delete
        // event from either original wallpaper or wallpaper
        // thumbnail, we delete both of them in local FS to achieve
        // a faster synchronization. So each file is expected to be
        // deleted twice and the second attempt is a noop.
        function(e) {
          if (e.name != 'NotFoundError')
            WallpaperUtil.onFileSystemError(e);
        });
    fs.root.getFile(
        thumbnailPath, {create: false},
        function(fe) {
          fe.remove(function() {}, null);
        },
        function(e) {
          if (e.name != 'NotFoundError')
            WallpaperUtil.onFileSystemError(e);
        });
  });
};

/**
 * Loads a wallpaper from sync file system and saves it and its thumbnail to
 *     local file system.
 * @param {string} wallpaperFileEntry File name of wallpaper image.
 */
WallpaperUtil.storeWallpaperFromSyncFSToLocalFS = function(wallpaperFileEntry) {
  var filenName = wallpaperFileEntry.name;
  var storeDir = Constants.WallpaperDirNameEnum.ORIGINAL;
  if (filenName.indexOf(Constants.CustomWallpaperThumbnailSuffix) != -1)
    storeDir = Constants.WallpaperDirNameEnum.THUMBNAIL;
  filenName = filenName.replace(Constants.CustomWallpaperThumbnailSuffix, '');
  wallpaperFileEntry.file(function(file) {
    var reader = new FileReader();
    reader.onloadend = function() {
      WallpaperUtil.storeWallpaperToLocalFS(filenName, reader.result, storeDir);
    };
    reader.readAsArrayBuffer(file);
  }, WallpaperUtil.onFileSystemError);
};

/**
 * Deletes |wallpaperFileName| and its associated thumbnail from syncFileSystem.
 * @param {string} wallpaperFilename Name of the file that will be deleted.
 */
WallpaperUtil.deleteWallpaperFromSyncFS = function(wallpaperFilename) {
  var thumbnailFilename =
      wallpaperFilename + Constants.CustomWallpaperThumbnailSuffix;
  var success = function(fs) {
    fs.root.getFile(
        wallpaperFilename, {create: false},
        function(fe) {
          fe.remove(function() {}, null);
        },
        function(e) {
          // NotFoundError is expected under the following scenario:
          // The user uses a same account on device A and device B.
          // The current wallpaper is a third party wallpaper. Then
          // the user changes it to a ONLINE wallpaper on device A.
          // Sync file system change and local file system change
          // will then both be fired on device B, which makes the
          // third party wallpaper be deleted twice from the sync
          // file system. We should ignore this error.
          if (e.name != 'NotFoundError')
            WallpaperUtil.onFileSystemError(e);
        });
    fs.root.getFile(
        thumbnailFilename, {create: false},
        function(fe) {
          fe.remove(function() {}, null);
        },
        function(e) {
          // Same as above.
          if (e.name != 'NotFoundError')
            WallpaperUtil.onFileSystemError(e);
        });
  };
  WallpaperUtil.requestSyncFS(success);
};

/**
 * Executes callback after requesting the sync settings.
 * @param {function} callback The callback will be executed.
 */
WallpaperUtil.enabledSyncThemesCallback = function(callback) {
  chrome.wallpaperPrivate.getSyncSetting(function(setting) {
    callback(setting.syncThemes);
  });
};

/**
 * Request a syncFileSystem handle and run callback on it.
 * @param {function} callback The callback to execute after syncFileSystem
 *     handler is available.
 */
WallpaperUtil.requestSyncFS = function(callback) {
  WallpaperUtil.enabledSyncThemesCallback(function(syncEnabled) {
    if (!syncEnabled)
      return;
    if (WallpaperUtil.syncFs) {
      callback(WallpaperUtil.syncFs);
    } else {
      chrome.syncFileSystem.requestFileSystem(function(fs) {
        WallpaperUtil.syncFs = fs;
        callback(WallpaperUtil.syncFs);
      });
    }
  });
};

/**
 * Request a Local Fs handle and run callback on it.
 * @param {function} callback The callback to execute after Local handler is
 *     available.
 */
WallpaperUtil.requestLocalFS = function(callback) {
  if (WallpaperUtil.webkitFs) {
    callback(WallpaperUtil.webkitFs);
  } else {
    window.webkitRequestFileSystem(
        window.PERSISTENT, 1024 * 1024 * 100, function(fs) {
          WallpaperUtil.webkitFs = fs;
          callback(fs);
        });
  }
};

/**
 * Print error to console.error.
 * @param {Event} e The error will be printed to console.error.
 */
// TODO(ranj): Handle different errors differently.
WallpaperUtil.onFileSystemError = function(e) {
  console.error(e);
};

/**
 * Write jpeg/png file data into file entry.
 * @param {FileEntry} fileEntry The file entry that going to be writen.
 * @param {ArrayBuffer} wallpaperData Data for image file.
 * @param {function=} writeCallback The callback that will be executed after
 *     writing data.
 */
WallpaperUtil.writeFile = function(fileEntry, wallpaperData, writeCallback) {
  fileEntry.createWriter(function(fileWriter) {
    var blob = new Blob([new Int8Array(wallpaperData)]);
    fileWriter.write(blob);
    if (writeCallback)
      writeCallback();
  }, WallpaperUtil.onFileSystemError);
};

/**
 * Write jpeg/png file data into syncFileSystem.
 * @param {string} wallpaperFilename The filename that going to be writen.
 * @param {ArrayBuffer} wallpaperData Data for image file.
 */
WallpaperUtil.storeWallpaperToSyncFS = function(
    wallpaperFilename, wallpaperData) {
  var callback = function(fs) {
    fs.root.getFile(
        wallpaperFilename, {create: false}, function() {},  // already exists
        function(e) {  // not exists, create
          fs.root.getFile(wallpaperFilename, {create: true}, function(fe) {
            WallpaperUtil.writeFile(fe, wallpaperData);
          }, WallpaperUtil.onFileSystemError);
        });
  };
  WallpaperUtil.requestSyncFS(callback);
};

/**
 * Stores jpeg/png wallpaper into |localDir| in local file system.
 * @param {string} wallpaperFilename File name of wallpaper image.
 * @param {ArrayBuffer} wallpaperData The wallpaper data.
 * @param {string} saveDir The path to store wallpaper in local file system.
 */
WallpaperUtil.storeWallpaperToLocalFS = function(
    wallpaperFilename, wallpaperData, saveDir) {
  if (!wallpaperData) {
    console.error('wallpaperData is null');
    return;
  }
  var getDirSuccess = function(dirEntry) {
    dirEntry.getFile(
        wallpaperFilename, {create: false}, function() {},  // already exists
        function(e) {  // not exists, create
          dirEntry.getFile(wallpaperFilename, {create: true}, function(fe) {
            WallpaperUtil.writeFile(fe, wallpaperData);
          }, WallpaperUtil.onFileSystemError);
        });
  };
  WallpaperUtil.requestLocalFS(function(fs) {
    fs.root.getDirectory(
        saveDir, {create: true}, getDirSuccess,
        WallpaperUtil.onFileSystemError);
  });
};

/**
 * Sets wallpaper from synced file system.
 * @param {string} wallpaperFilename File name used to set wallpaper.
 * @param {string} wallpaperLayout Layout used to set wallpaper.
 * @param {function=} onSuccess Callback if set successfully.
 */
WallpaperUtil.setCustomWallpaperFromSyncFS = function(
    wallpaperFilename, wallpaperLayout, onSuccess) {
  var setWallpaperFromSyncCallback = function(fs) {
    if (!wallpaperFilename) {
      console.error('wallpaperFilename is not provided.');
      return;
    }
    if (!wallpaperLayout)
      wallpaperLayout = 'CENTER_CROPPED';
    fs.root.getFile(
        wallpaperFilename, {create: false},
        function(fileEntry) {
          fileEntry.file(function(file) {
            var reader = new FileReader();
            reader.onloadend = function() {
              chrome.wallpaperPrivate.setCustomWallpaper(
                  reader.result, wallpaperLayout, true /*generateThumbnail=*/,
                  wallpaperFilename, false /*previewMode=*/,
                  function(thumbnailData) {
                    // TODO(ranj): Ignore 'canceledWallpaper' error.
                    if (chrome.runtime.lastError) {
                      console.error(chrome.runtime.lastError.message);
                      return;
                    }
                    if (onSuccess)
                      onSuccess();
                  });
            };
            reader.readAsArrayBuffer(file);
          }, WallpaperUtil.onFileSystemError);
        },
        function(e) {}  // fail to read file, expected due to download delay
        );
  };
  WallpaperUtil.requestSyncFS(setWallpaperFromSyncCallback);
};

/**
 * Saves value to local storage that associates with key.
 * @param {string} key The key that associates with value.
 * @param {string} value The value to save to local storage.
 * @param {function=} opt_callback The callback on success.
 */
WallpaperUtil.saveToLocalStorage = function(key, value, opt_callback) {
  var items = {};
  items[key] = value;
  Constants.WallpaperLocalStorage.set(items, opt_callback);
};

/**
 * Saves value to sync storage that associates with key if sync theme is
 * enabled.
 * @param {string} key The key that associates with value.
 * @param {string} value The value to save to sync storage.
 * @param {function=} opt_callback The callback on success.
 */
WallpaperUtil.saveToSyncStorage = function(key, value, opt_callback) {
  var items = {};
  items[key] = value;
  WallpaperUtil.enabledSyncThemesCallback(function(syncEnabled) {
    if (syncEnabled)
      Constants.WallpaperSyncStorage.set(items, opt_callback);
  });
};

/**
 * Saves user's wallpaper infomation to local and sync storage. Note that local
 * value should be saved first.
 * @param {string} url The url address of wallpaper. For custom wallpaper, it is
 *     the file name.
 * @param {string} layout The wallpaper layout.
 * @param {string} source The wallpaper source.
 * @param {string} appName The third party app name. If the current wallpaper is
 *     set by the built-in wallpaper picker, it is set to an empty string.
 */
WallpaperUtil.saveWallpaperInfo = function(url, layout, source, appName) {
  chrome.wallpaperPrivate.recordWallpaperUMA(source);

  // In order to keep the wallpaper sync working across different versions, we
  // have to revert DAILY/THIRDPARTY type wallpaper info to ONLINE/CUSTOM type
  // after record the correct UMA stats.
  source = (source == Constants.WallpaperSourceEnum.Daily) ?
      Constants.WallpaperSourceEnum.Online :
      source;
  source = (source == Constants.WallpaperSourceEnum.ThirdParty) ?
      Constants.WallpaperSourceEnum.Custom :
      source;
  var wallpaperInfo = {
    url: url,
    layout: layout,
    source: source,
    appName: appName,
  };
  WallpaperUtil.saveToLocalStorage(
      Constants.AccessLocalWallpaperInfoKey, wallpaperInfo, function() {
        WallpaperUtil.saveToSyncStorage(
            Constants.AccessSyncWallpaperInfoKey, wallpaperInfo);
      });
};

/**
 * Downloads resources from url. Calls onSuccess and opt_onFailure accordingly.
 * @param {string} url The url address where we should fetch resources.
 * @param {string} type The response type of XMLHttprequest.
 * @param {function} onSuccess The success callback. It must be called with
 *     current XMLHttprequest object.
 * @param {function} onFailure The failure callback.
 * @param {XMLHttpRequest=} opt_xhr The XMLHttpRequest object.
 */
WallpaperUtil.fetchURL = function(url, type, onSuccess, onFailure, opt_xhr) {
  var xhr;
  if (opt_xhr)
    xhr = opt_xhr;
  else
    xhr = new XMLHttpRequest();

  try {
    // Do not use loadend here to handle both success and failure case. It gets
    // complicated with abortion. Unexpected error message may show up. See
    // http://crbug.com/242581.
    xhr.addEventListener('load', function(e) {
      if (this.status == 200) {
        onSuccess(this);
      } else {
        onFailure(this.status);
      }
    });
    xhr.addEventListener('error', onFailure);
    xhr.open('GET', url, true);
    xhr.responseType = type;
    xhr.send(null);
  } catch (e) {
    onFailure();
  }
};

/**
 * A convenience wrapper for setting online wallpapers with preview disabled.
 * @param {string} url The url address where we should fetch resources.
 * @param {string} layout The layout of online wallpaper.
 * @param {function} onSuccess The success callback.
 * @param {function} onFailure The failure callback.
 */
WallpaperUtil.setOnlineWallpaperWithoutPreview = function(
    url, layout, onSuccess, onFailure) {
  chrome.wallpaperPrivate.setWallpaperIfExists(
      url, layout, false /*previewMode=*/, exists => {
        if (exists) {
          onSuccess();
          return;
        }

        this.fetchURL(url, 'arraybuffer', xhr => {
          if (xhr.response != null) {
            chrome.wallpaperPrivate.setWallpaper(
                xhr.response, layout, url, false /*previewMode=*/, onSuccess);
          } else {
            onFailure();
          }
        }, onFailure);
      });
};

/**
 * Creates a blob of type 'image/png'.
 * @param {string} data The image data.
 */
WallpaperUtil.createPngBlob = function(data) {
  return new Blob([new Int8Array(data)], {'type': 'image/png'});
};

/**
 * Displays the image by creating an image blob.
 * @param {Object} imageElement The image element.
 * @param {string} data The image data.
 * @param {function} opt_callback An optional callback, called after the image
 *     finishes loading.
 */
WallpaperUtil.displayImage = function(imageElement, data, opt_callback) {
  imageElement.src =
      window.URL.createObjectURL(WallpaperUtil.createPngBlob(data));
  imageElement.addEventListener('load', function(e) {
    if (opt_callback)
      opt_callback();
    // Revoke the url since it won't be used anymore after the image is loaded.
    window.URL.revokeObjectURL(imageElement.src);
  });
};

/**
 * Sets the value of the daily refresh toggle.
 * @param {boolean} checked The value used to set the checkbox.
 */
WallpaperUtil.setSurpriseMeCheckboxValue = function(checked) {
    document.querySelectorAll('.daily-refresh-slider').forEach(element => {
      element.classList.toggle('checked', checked);
    });
};

/**
 * Gets the state of the daily refresh toggle.
 * @return {boolean} The value of the checkbox.
 */
WallpaperUtil.getSurpriseMeCheckboxValue = function() {
    return document.querySelector('.daily-refresh-slider')
        .classList.contains('checked');
};

/**
 * A convenience wrapper for displaying the thumbnail image.
 * @param {Object} imageElement The image element.
 * @param {string} url The base url of the wallpaper.
 * @param {string} source The source of the wallpaper corresponding to
 *     |WallpaperSourceEnum|.
 */
WallpaperUtil.displayThumbnail = function(imageElement, url, source) {
  chrome.wallpaperPrivate.getThumbnail(url, source, data => {
    if (data) {
      WallpaperUtil.displayImage(imageElement, data, null /*opt_callback=*/);
    } else {
      // The only known case for hitting this branch is when showing the
      // wallpaper picker for the first time after OOBE, the |saveThumbnail|
      // operation within |WallpaperThumbnailsGridItem.decorate| hasn't
      // completed. See http://crbug.com/792829.
      var xhr = new XMLHttpRequest();
      xhr.open('GET', url, true);
      xhr.responseType = 'arraybuffer';
      xhr.send(null);
      xhr.addEventListener('load', function(e) {
        if (xhr.status === 200) {
          WallpaperUtil.displayImage(
              imageElement, xhr.response, null /*opt_callback=*/);
        }
      });
    }
  });
};

/**
 * Runs chrome.test.sendMessage in test environment. Does nothing if running
 * in production environment.
 *
 * @param {string} message Test message to send.
 */
WallpaperUtil.testSendMessage = function(message) {
  var test = chrome.test || window.top.chrome.test;
  if (test)
    test.sendMessage(message);
};

/**
 * Gets the daily refresh info from sync storage, or local storage if the former
 * is not available.
 * @param {function} callback A callback that takes the value of the info, or
 *     null if the value is invalid.
 */
WallpaperUtil.getDailyRefreshInfo = function(callback) {
  WallpaperUtil.enabledSyncThemesCallback(syncEnabled => {
    var parseInfo = dailyRefreshInfoJson => {
      if (!dailyRefreshInfoJson) {
        callback(null);
        return;
      }

      var dailyRefreshInfo = JSON.parse(dailyRefreshInfoJson);
      if (!dailyRefreshInfo || !dailyRefreshInfo.hasOwnProperty('enabled') ||
          !dailyRefreshInfo.hasOwnProperty('collectionId') ||
          !dailyRefreshInfo.hasOwnProperty('resumeToken')) {
        callback(null);
        return;
      }
      callback(dailyRefreshInfo);
    };

    if (syncEnabled) {
      Constants.WallpaperSyncStorage.get(
          Constants.AccessSyncDailyRefreshInfoKey, items => {
            var dailyRefreshInfoJson =
                items[Constants.AccessSyncDailyRefreshInfoKey];
            if (dailyRefreshInfoJson) {
              parseInfo(dailyRefreshInfoJson);
            } else {
              Constants.WallpaperLocalStorage.get(
                  Constants.AccessLocalDailyRefreshInfoKey, items => {
                    dailyRefreshInfoJson =
                        items[Constants.AccessLocalDailyRefreshInfoKey];
                    parseInfo(dailyRefreshInfoJson);
                    if (dailyRefreshInfoJson) {
                      WallpaperUtil.saveToSyncStorage(
                          Constants.AccessSyncDailyRefreshInfoKey,
                          dailyRefreshInfoJson);
                    }
                  });
            }
          });
    } else {
      Constants.WallpaperLocalStorage.get(
          Constants.AccessLocalDailyRefreshInfoKey, items => {
            parseInfo(items[Constants.AccessLocalDailyRefreshInfoKey]);
          });
    }
  });
};

/**
 * Saves the daily refresh info to local and sync storage.
 * @param {Object} dailyRefreshInfo The daily refresh info.
 */
WallpaperUtil.saveDailyRefreshInfo = function(dailyRefreshInfo) {
  // Discard |resumeToken| to prevent the server from potentially fingerprinting
  // the end user. Therefore, |resumeToken| will always be null when sending
  // |getSurpriseMeImage| requests.
  // TODO(crbug.com/810169): Implement the mechanism to avoid duplicate
  // wallpapers on the client side.
  dailyRefreshInfo.resumeToken = null;
  var dailyRefreshInfoJson = JSON.stringify(dailyRefreshInfo);
  WallpaperUtil.saveToLocalStorage(
      Constants.AccessLocalDailyRefreshInfoKey, dailyRefreshInfoJson,
      null /*opt_callback=*/);
  WallpaperUtil.saveToSyncStorage(
      Constants.AccessSyncDailyRefreshInfoKey, dailyRefreshInfoJson,
      null /*opt_callback=*/);
};
