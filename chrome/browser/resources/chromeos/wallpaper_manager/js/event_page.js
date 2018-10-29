// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var wallpaperPickerWindow = null;

var surpriseWallpaper = null;

/**
 * Returns the highResolutionSuffix.
 * @param {function} callback A callback function that takes one value:
 *     |highResolutionSuffix|: the suffix to append to the wallpaper urls.
 */
function getHighResolutionSuffix(callback) {
  chrome.wallpaperPrivate.getStrings(strings => {
    callback(strings['highResolutionSuffix']);
  });
}

function SurpriseWallpaper() {}

/**
 * Gets SurpriseWallpaper instance. In case it hasn't been initialized, a new
 * instance is created.
 * @return {SurpriseWallpaper} A SurpriseWallpaper instance.
 */
SurpriseWallpaper.getInstance = function() {
  if (!surpriseWallpaper)
    surpriseWallpaper = new SurpriseWallpaper();
  return surpriseWallpaper;
};

/**
 * Tries to change wallpaper to a new one in the background. May fail due to a
 * network issue.
 */
SurpriseWallpaper.prototype.tryChangeWallpaper = function() {
  var self = this;
  var onFailure = function(status) {
    if (status != 404)
      self.fallbackToLocalRss_();
    else
      self.updateRandomWallpaper_();
  };
  // Try to fetch newest rss as document from server first. If the requested
  // URL is not found (404 error), set a random wallpaper displayed in the
  // wallpaper picker. If any other error occurs, proceed with local copy of
  // rss.
  WallpaperUtil.fetchURL(Constants.WallpaperRssURL, 'document', function(xhr) {
    WallpaperUtil.saveToLocalStorage(
        Constants.AccessLocalRssKey,
        new XMLSerializer().serializeToString(xhr.responseXML));
    self.updateSurpriseWallpaper(xhr.responseXML);
  }, onFailure);
};

/**
 * Retries changing the wallpaper 1 hour later. This is called when fetching the
 * rss or wallpaper from server fails.
 * @private
 */
SurpriseWallpaper.prototype.retryLater_ = function() {
  chrome.alarms.create('RetryAlarm', {delayInMinutes: 60});
};

/**
 * Fetches the cached rss feed from local storage in the event of being unable
 * to download the online feed.
 * @private
 */
SurpriseWallpaper.prototype.fallbackToLocalRss_ = function() {
  var self = this;
  Constants.WallpaperLocalStorage.get(
      Constants.AccessLocalRssKey, function(items) {
        var rssString = items[Constants.AccessLocalRssKey];
        if (rssString) {
          self.updateSurpriseWallpaper(
              new DOMParser().parseFromString(rssString, 'text/xml'));
        } else {
          self.updateSurpriseWallpaper();
        }
      });
};

/**
 * Starts to change wallpaper. Called after rss is fetched.
 * @param {Document=} opt_rss The fetched rss document. If opt_rss is null, uses
 *     a random wallpaper.
 */
SurpriseWallpaper.prototype.updateSurpriseWallpaper = function(opt_rss) {
  if (opt_rss) {
    var items = opt_rss.querySelectorAll('item');
    var date = new Date(new Date().toDateString()).getTime();
    for (var i = 0; i < items.length; i++) {
      item = items[i];
      var disableDate =
          new Date(item.getElementsByTagNameNS(
                           Constants.WallpaperNameSpaceURI, 'disableDate')[0]
                       .textContent)
              .getTime();
      var enableDate =
          new Date(item.getElementsByTagNameNS(
                           Constants.WallpaperNameSpaceURI, 'enableDate')[0]
                       .textContent)
              .getTime();
      var regionsString = item.getElementsByTagNameNS(
                                  Constants.WallpaperNameSpaceURI, 'regions')[0]
                              .textContent;
      var regions = regionsString.split(', ');
      if (enableDate <= date && disableDate > date &&
          regions.indexOf(navigator.language) != -1) {
        var self = this;
        this.setWallpaperFromRssItem_(
            item, function() {},
            function(status) {
              if (status != 404)
                self.retryLater_();
              else
                self.updateRandomWallpaper_();
            });
        return;
      }
    }
  }
  // No surprise wallpaper for today at current locale or fetching rss feed
  // fails. Fallback to use a random one from wallpaper server.
  this.updateRandomWallpaper_();
};

/**
 * Sets a new random wallpaper if one has not already been set today.
 * @private
 */
SurpriseWallpaper.prototype.updateRandomWallpaper_ = function() {
  var self = this;
  var onSuccess = function(items) {
    var dateString = new Date().toDateString();
    // At most one random wallpaper per day.
    if (items[Constants.AccessLastSurpriseWallpaperChangedDate] != dateString) {
      self.setRandomWallpaper_(dateString);
    }
  };
  WallpaperUtil.enabledSyncThemesCallback(function(syncEnabled) {
    if (syncEnabled) {
      Constants.WallpaperSyncStorage.get(
          Constants.AccessLastSurpriseWallpaperChangedDate, onSuccess);
    } else {
      Constants.WallpaperLocalStorage.get(
          Constants.AccessLastSurpriseWallpaperChangedDate, onSuccess);
    }
  });
};

/**
 * Sets wallpaper to be a random one. The wallpaper url is retrieved either from
 * the stored manifest file, or by fetching the wallpaper info from the server.
 * @param {string} dateString String representation of current local date.
 * @private
 */
SurpriseWallpaper.prototype.setRandomWallpaper_ = function(dateString) {
  var onSuccess = function(url, layout) {
    WallpaperUtil.saveWallpaperInfo(
        url, layout, Constants.WallpaperSourceEnum.Daily, '');
    WallpaperUtil.saveToLocalStorage(
        Constants.AccessLastSurpriseWallpaperChangedDate, dateString,
        function() {
          WallpaperUtil.saveToSyncStorage(
              Constants.AccessLastSurpriseWallpaperChangedDate, dateString);
        });
  };

  getHighResolutionSuffix(highResolutionSuffix => {
    this.setRandomWallpaperFromServer_(onSuccess, highResolutionSuffix);
  });
};

/**
 * Sets wallpaper to be a random one retrieved from the backend service. If the
 * wallpaper download fails, retry one hour later.
 * @param {function} onSuccess The success callback.
 * @param {string} suffix The url suffix for high resolution wallpaper.
 * @private
 */
SurpriseWallpaper.prototype.setRandomWallpaperFromServer_ = function(
    onSuccess, suffix) {
  var onDailyRefreshInfoReturned = dailyRefreshInfo => {
    var setRandomWallpaperFromServerImpl = dailyRefreshInfo => {
      chrome.wallpaperPrivate.getSurpriseMeImage(
          dailyRefreshInfo.collectionId, dailyRefreshInfo.resumeToken,
          (imageInfo, nextResumeToken) => {
            if (chrome.runtime.lastError) {
              this.retryLater_();
              return;
            }
            dailyRefreshInfo.resumeToken = nextResumeToken;
            WallpaperUtil.saveDailyRefreshInfo(dailyRefreshInfo);

            var wallpaperUrl = imageInfo['imageUrl'] + suffix;
            var layout = Constants.WallpaperThumbnailDefaultLayout;
            WallpaperUtil.setOnlineWallpaperWithoutPreview(
                wallpaperUrl, layout,
                onSuccess.bind(null, wallpaperUrl, layout),
                this.retryLater_.bind(this));
          });
    };

    if (dailyRefreshInfo) {
      if (dailyRefreshInfo.enabled) {
        setRandomWallpaperFromServerImpl(dailyRefreshInfo);
      } else {
        console.error(
            'Daily refresh is disabled when the alarm goes off. ' +
            'This should never happen!');
      }
      return;
    }

    // Migration: we reach here if the old picker set an alarm and by the time
    // the alarm goes off, the new picker is already in use. We should ensure
    // the user transitions to the daily refresh feature.
    chrome.wallpaperPrivate.getCollectionsInfo(collectionsInfo => {
      if (chrome.runtime.lastError) {
        this.retryLater_();
        return;
      }
      if (collectionsInfo.length == 0) {
        // Although the fetch succeeds, it's theoretically possible that the
        // collection list is empty, in this case do nothing.
        return;
      }
      dailyRefreshInfo = {
        enabled: true,
        // Use the first collection (an arbitrary choice).
        collectionId: collectionsInfo[0]['collectionId'],
        resumeToken: null
      };
      setRandomWallpaperFromServerImpl(dailyRefreshInfo);
    });
  };

  WallpaperUtil.getDailyRefreshInfo(onDailyRefreshInfoReturned.bind(null));
};

/**
 * Sets wallpaper to the wallpaper specified by item from rss. If downloading
 * the wallpaper fails, retry one hour later.
 * @param {Element} item The wallpaper rss item element.
 * @param {function} onSuccess Success callback.
 * @param {function} onFailure Failure callback.
 * @private
 */
SurpriseWallpaper.prototype.setWallpaperFromRssItem_ = function(
    item, onSuccess, onFailure) {
  var url = item.querySelector('link').textContent;
  var layout =
      item.getElementsByTagNameNS(Constants.WallpaperNameSpaceURI, 'layout')[0]
          .textContent;
  var self = this;
  WallpaperUtil.fetchURL(url, 'arraybuffer', function(xhr) {
    if (xhr.response != null) {
      chrome.wallpaperPrivate.setCustomWallpaper(
          xhr.response, layout, false /*generateThumbnail=*/,
          'surprise_wallpaper', false /*previewMode=*/, onSuccess);
      WallpaperUtil.saveWallpaperInfo(
          url, layout, Constants.WallpaperSourceEnum.Daily, '');
      var dateString = new Date().toDateString();
      WallpaperUtil.saveToLocalStorage(
          Constants.AccessLastSurpriseWallpaperChangedDate, dateString,
          function() {
            WallpaperUtil.saveToSyncStorage(
                Constants.AccessLastSurpriseWallpaperChangedDate, dataString);
          });
    } else {
      self.updateRandomWallpaper_();
    }
  }, onFailure);
};

/**
 * Disables the wallpaper surprise me feature. Clear all alarms and states.
 */
SurpriseWallpaper.prototype.disable = function() {
  chrome.alarms.clearAll();
  // Makes last changed date invalid.
  WallpaperUtil.saveToLocalStorage(
      Constants.AccessLastSurpriseWallpaperChangedDate, '', function() {
        WallpaperUtil.saveToSyncStorage(
            Constants.AccessLastSurpriseWallpaperChangedDate, '');
      });
};

/**
 * Changes current wallpaper and sets up an alarm to schedule next change around
 * midnight.
 */
SurpriseWallpaper.prototype.next = function() {
  var nextUpdate = this.nextUpdateTime(new Date());
  chrome.alarms.create({when: nextUpdate});
  this.tryChangeWallpaper();
};

/**
 * Calculates when the next wallpaper change should be triggered.
 * @param {Date} now Current time.
 * @return {number} The time when next wallpaper change should happen.
 */
SurpriseWallpaper.prototype.nextUpdateTime = function(now) {
  var nextUpdate = new Date(now.setDate(now.getDate() + 1)).toDateString();
  return new Date(nextUpdate).getTime();
};

chrome.app.runtime.onLaunched.addListener(function() {
  if (wallpaperPickerWindow && !wallpaperPickerWindow.contentWindow.closed) {
    wallpaperPickerWindow.focus();
    chrome.wallpaperPrivate.minimizeInactiveWindows();
    return;
  }

  var options = {
    frame: 'none',
    innerBounds: {width: 768, height: 512, minWidth: 768, minHeight: 512},
    resizable: true,
    alphaEnabled: true
  };

  chrome.app.window.create('main.html', options, function(window) {
    wallpaperPickerWindow = window;
    chrome.wallpaperPrivate.minimizeInactiveWindows();
    window.onClosed.addListener(function() {
      wallpaperPickerWindow = null;
      // In case the app exits unexpectedly during preview.
      chrome.wallpaperPrivate.cancelPreviewWallpaper(() => {});
      // If the app exits during preview, do not restore the previously active
      // windows. Continue to show the new wallpaper.
      if (!window.contentWindow.document.body.classList.contains(
              'preview-mode')) {
        chrome.wallpaperPrivate.restoreMinimizedWindows();
      }
    });
    // By design, the wallpaper picker should never be shown on top of
    // another window.
    wallpaperPickerWindow.contentWindow.addEventListener('focus', function() {
      chrome.wallpaperPrivate.minimizeInactiveWindows();
    });
    WallpaperUtil.testSendMessage('wallpaper-window-created');
  });
});

chrome.syncFileSystem.onFileStatusChanged.addListener(function(detail) {
  WallpaperUtil.enabledSyncThemesCallback(function(syncEnabled) {
    if (!syncEnabled)
      return;
    if (detail.status != 'synced' || detail.direction != 'remote_to_local')
      return;
    if (detail.action == 'added') {
      // TODO(xdai): Get rid of this setCustomWallpaperFromSyncFS logic.
      // WallpaperInfo might have been saved in the sync filesystem before the
      // corresonding custom wallpaper and thumbnail are saved, thus the
      // onChanged() might not set the custom wallpaper correctly. So we need
      // setCustomWallpaperFromSyncFS() to be called here again to make sure
      // custom wallpaper is set.
      Constants.WallpaperLocalStorage.get(
          Constants.AccessLocalWallpaperInfoKey, function(items) {
            var localData = items[Constants.AccessLocalWallpaperInfoKey];
            if (localData && localData.url == detail.fileEntry.name &&
                localData.source == Constants.WallpaperSourceEnum.Custom) {
              WallpaperUtil.setCustomWallpaperFromSyncFS(
                  localData.url, localData.layout);
            }
          });
      // We only need to store the custom wallpaper if it was set by the
      // built-in wallpaper picker.
      if (!detail.fileEntry.name.startsWith(
              Constants.ThirdPartyWallpaperPrefix)) {
        WallpaperUtil.storeWallpaperFromSyncFSToLocalFS(detail.fileEntry);
      }
    } else if (detail.action == 'deleted') {
      var fileName = detail.fileEntry.name.replace(
          Constants.CustomWallpaperThumbnailSuffix, '');
      WallpaperUtil.deleteWallpaperFromLocalFS(fileName);
    }
  });
});

chrome.storage.onChanged.addListener(function(changes, namespace) {
  WallpaperUtil.enabledSyncThemesCallback(function(syncEnabled) {
    var updateDailyRefreshStates = key => {
      if (!changes[key])
        return;
      var oldDailyRefreshInfo = JSON.parse(changes[key].oldValue);
      var newDailyRefreshInfo = JSON.parse(changes[key].newValue);
      // The resume token is expected to change after a new daily refresh
      // wallpaper is set. Ignore it if it's the only change.
      if (oldDailyRefreshInfo.enabled === newDailyRefreshInfo.enabled &&
          oldDailyRefreshInfo.collectionId ===
              newDailyRefreshInfo.collectionId) {
        return;
      }
      // Although the old and new values may both have enabled == true, they can
      // have different collection ids, so the old alarm should always be
      // cleared.
      chrome.alarms.clearAll();
      if (newDailyRefreshInfo.enabled)
        SurpriseWallpaper.getInstance().next();
    };
    updateDailyRefreshStates(
        syncEnabled ? Constants.AccessSyncDailyRefreshInfoKey :
                      Constants.AccessLocalDailyRefreshInfoKey);

    if (syncEnabled) {
      // If sync theme is enabled, use values from chrome.storage.sync to sync
      // wallpaper changes.
      if (changes[Constants.AccessSyncSurpriseMeEnabledKey]) {
        if (changes[Constants.AccessSyncSurpriseMeEnabledKey].newValue) {
          SurpriseWallpaper.getInstance().next();
        } else {
          SurpriseWallpaper.getInstance().disable();
        }
      }

      // If the built-in Wallpaper Picker App is open, update the check mark
      // and the corresponding 'wallpaper-set-by-message' in time.
      var updateCheckMarkAndAppNameIfAppliable = function(appName) {
        if (!wallpaperPickerWindow)
          return;
        var wpDocument = wallpaperPickerWindow.contentWindow.document;

        if (!!appName) {
          chrome.wallpaperPrivate.getStrings(function(strings) {
            var message =
                strings.currentWallpaperSetByMessage.replace(/\$1/g, appName);
            wpDocument.querySelector('#wallpaper-set-by-message').textContent =
                message;
            wpDocument.querySelector('#wallpaper-grid').classList.add('small');
            wpDocument.querySelector('#checkbox').classList.remove('checked');
            wpDocument.querySelector('#categories-list').disabled = false;
            wpDocument.querySelector('#wallpaper-grid').disabled = false;
          });
        } else {
          wpDocument.querySelector('#wallpaper-set-by-message').textContent =
              '';
          wpDocument.querySelector('#wallpaper-grid').classList.remove('small');
          Constants.WallpaperSyncStorage.get(
              Constants.AccessSyncSurpriseMeEnabledKey, function(item) {
                // TODO(crbug.com/810169): Try to combine this part with
                // |WallpaperManager.onSurpriseMeStateChanged_|. The logic is
                // duplicate.
                var enable = item[Constants.AccessSyncSurpriseMeEnabledKey];
                if (enable) {
                  wpDocument.querySelector('#checkbox')
                      .classList.add('checked');
                } else {
                  wpDocument.querySelector('#checkbox')
                      .classList.remove('checked');
                  if (wpDocument.querySelector('.check'))
                    wpDocument.querySelector('.check').style.visibility =
                        'visible';
                }
              });
        }
      };

      if (changes[Constants.AccessLocalWallpaperInfoKey]) {
        // If the old wallpaper is a third party wallpaper we should remove it
        // from the local & sync file system to free space.
        var oldInfo = changes[Constants.AccessLocalWallpaperInfoKey].oldValue;
        if (oldInfo &&
            oldInfo.url.indexOf(Constants.ThirdPartyWallpaperPrefix) != -1) {
          WallpaperUtil.deleteWallpaperFromLocalFS(oldInfo.url);
          WallpaperUtil.deleteWallpaperFromSyncFS(oldInfo.url);
        }

        var newInfo = changes[Constants.AccessLocalWallpaperInfoKey].newValue;
        if (newInfo && newInfo.hasOwnProperty('appName'))
          updateCheckMarkAndAppNameIfAppliable(newInfo.appName);
      }

      if (changes[Constants.AccessSyncWallpaperInfoKey]) {
        var syncInfo = changes[Constants.AccessSyncWallpaperInfoKey].newValue;

        Constants.WallpaperSyncStorage.get(
            Constants.AccessSyncSurpriseMeEnabledKey, function(enabledItems) {
              var syncSurpriseMeEnabled =
                  enabledItems[Constants.AccessSyncSurpriseMeEnabledKey];

              Constants.WallpaperSyncStorage.get(
                  Constants.AccessLastSurpriseWallpaperChangedDate,
                  function(items) {
                    var syncLastSurpriseMeChangedDate =
                        items[Constants.AccessLastSurpriseWallpaperChangedDate];

                    var today = new Date().toDateString();
                    // If SurpriseMe is enabled and surprise wallpaper hasn't
                    // been changed today, we should not sync the change,
                    // instead onAlarm() will be triggered to update a surprise
                    // me wallpaper.
                    if (!syncSurpriseMeEnabled ||
                        (syncSurpriseMeEnabled &&
                         syncLastSurpriseMeChangedDate == today)) {
                      Constants.WallpaperLocalStorage.get(
                          Constants.AccessLocalWallpaperInfoKey,
                          function(infoItems) {
                            var localInfo =
                                infoItems[Constants
                                              .AccessLocalWallpaperInfoKey];
                            // Normally, the wallpaper info saved in local
                            // storage and sync storage are the same. If the
                            // synced value changed by sync service, they may
                            // different. In that case, change wallpaper to the
                            // one saved in sync storage and update the local
                            // value.
                            if (!localInfo || localInfo.url != syncInfo.url ||
                                localInfo.layout != syncInfo.layout ||
                                localInfo.source != syncInfo.source) {
                              if (syncInfo.source ==
                                  Constants.WallpaperSourceEnum.Online) {
                                // TODO(bshe): Consider schedule an alarm to set
                                // online wallpaper later when failed. Note that
                                // we need to cancel the retry if user set
                                // another wallpaper before retry alarm invoked.
                                WallpaperUtil.setOnlineWallpaperWithoutPreview(
                                    syncInfo.url, syncInfo.layout,
                                    function() {}, function() {});
                              } else if (
                                  syncInfo.source ==
                                  Constants.WallpaperSourceEnum.Custom) {
                                WallpaperUtil.setCustomWallpaperFromSyncFS(
                                    syncInfo.url, syncInfo.layout);
                              } else if (
                                  syncInfo.source ==
                                  Constants.WallpaperSourceEnum.Default) {
                                chrome.wallpaperPrivate.resetWallpaper();
                              }

                              // If the old wallpaper is a third party wallpaper
                              // we should remove it from the local & sync file
                              // system to free space.
                              if (localInfo &&
                                  localInfo.url.indexOf(
                                      Constants.ThirdPartyWallpaperPrefix) !=
                                      -1) {
                                WallpaperUtil.deleteWallpaperFromLocalFS(
                                    localInfo.url);
                                WallpaperUtil.deleteWallpaperFromSyncFS(
                                    localInfo.url);
                              }

                              if (syncInfo &&
                                  syncInfo.hasOwnProperty('appName'))
                                updateCheckMarkAndAppNameIfAppliable(
                                    syncInfo.appName);

                              WallpaperUtil.saveToLocalStorage(
                                  Constants.AccessLocalWallpaperInfoKey,
                                  syncInfo);
                            }
                          });
                    }
                  });
            });
      }
    } else {
      // If sync theme is disabled, use values from chrome.storage.local to
      // track wallpaper changes.
      if (changes[Constants.AccessLocalSurpriseMeEnabledKey]) {
        if (changes[Constants.AccessLocalSurpriseMeEnabledKey].newValue) {
          SurpriseWallpaper.getInstance().next();
        } else {
          SurpriseWallpaper.getInstance().disable();
        }
      }
    }
  });
});

chrome.alarms.onAlarm.addListener(function() {
  SurpriseWallpaper.getInstance().next();
});

chrome.wallpaperPrivate.onWallpaperChangedBy3rdParty.addListener(function(
    wallpaper, thumbnail, layout, appName) {
  WallpaperUtil.saveToLocalStorage(
      Constants.AccessLocalSurpriseMeEnabledKey, false, function() {
        WallpaperUtil.saveToSyncStorage(
            Constants.AccessSyncSurpriseMeEnabledKey, false);
      });

  // Make third party wallpaper syncable through different devices.
  var fileName = Constants.ThirdPartyWallpaperPrefix + new Date().getTime();
  var thumbnailFileName = fileName + Constants.CustomWallpaperThumbnailSuffix;
  WallpaperUtil.storeWallpaperToSyncFS(fileName, wallpaper);
  WallpaperUtil.storeWallpaperToSyncFS(thumbnailFileName, thumbnail);
  WallpaperUtil.saveWallpaperInfo(
      fileName, layout, Constants.WallpaperSourceEnum.ThirdParty, appName);

    WallpaperUtil.saveDailyRefreshInfo(
        {enabled: false, collectionId: null, resumeToken: null});

    if (wallpaperPickerWindow) {
      var event = new CustomEvent(
          Constants.WallpaperChangedBy3rdParty,
          {detail: {wallpaperFileName: fileName}});
      wallpaperPickerWindow.contentWindow.dispatchEvent(event);
    }
});
