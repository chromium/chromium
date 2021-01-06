// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/800945): Rename it to WallpaperPicker for consistency.

/**
 * WallpaperManager constructor.
 *
 * WallpaperManager objects encapsulate the functionality of the wallpaper
 * manager extension.
 *
 * @constructor
 * @param {HTMLElement} dialogDom The DOM node containing the prototypical
 *     extension UI.
 */

function WallpaperManager(dialogDom) {
  this.dialogDom_ = dialogDom;
  this.document_ = dialogDom.ownerDocument;
  this.selectedItem_ = null;
  this.progressManager_ = new ProgressManager();
  this.customWallpaperData_ = null;
  this.currentWallpaper_ = null;
  this.wallpaperRequest_ = null;
  this.preDownloadDomInit_();

  // The wallpaper picker has two steps of fetching the online images: it first
  // fetches a list of collection names (ie. categories such as Art,
  // Landscape etc.) via extension API, and then fetches the info specific to
  // each collection and caches the info in a map.
  // After the url and relevant info of the images are fetched, it passes the
  // info to |WallpaperThumbnailsGridItem| to display the images.

  // |collectionsInfo_| represents the list of wallpaper collections. Each
  // collection contains the display name and a unique id.
  this.collectionsInfo_ = null;
  // |imagesInfoMap_| caches the mapping between each collection id and the
  // images that belong to this collection. Each image is represented by a set
  // of info including the image url, author name, layout etc. Such info will
  // be used by |WallpaperThumbnailsGridItem| to display the images.
  this.imagesInfoMap_ = {};
  // The total count of images whose info has been fetched.
  this.imagesInfoCount_ = 0;
  // |dailyRefreshInfo_| stores the info related to the daily refresh feature.
  // Its value should be consistent with the sync/local storage.
  this.dailyRefreshInfo_ = null;
  // |pendingDailyRefreshInfo_| stores the up-to-date daily refresh info that
  // hasn't been confirmed by user (e.g. when user is previewing the image).
  // Its value will either replace |dailyRefreshInfo_| or be discarded.
  this.pendingDailyRefreshInfo_ = null;
  this.placeWallpaperPicker_();
  this.getCollectionsInfo_();
}

// Anonymous 'namespace'.
// TODO(bshe): Get rid of anonymous namespace.
(function() {

// Default wallpaper url
var OEM_DEFAULT_WALLPAPER_URL = 'OemDefaultWallpaper';

/**
 * The following values should be kept in sync with the style sheet.
 */
var GRID_IMAGE_WIDTH_CSS = 160;
var GRID_IMAGE_PADDING_CSS = 8;
var DIALOG_TOP_BAR_WIDTH = 192;

/**
 * Returns a translated string.
 *
 * Wrapper function to make dealing with translated strings more concise.
 *
 * @param {string} id The id of the string to return.
 * @return {string} The translated string.
 */
function str(id) {
  return loadTimeData.getString(id);
}

/**
 * Helper function to center the element.
 * @param {Object} element The element to be centered.
 * @param {number} totalWidth The total width. An empty value disables centering
 *     horizontally.
 * @param {number} totalHeight The total height. An empty value disables
 *     centering vertically.
 */
function centerElement(element, totalWidth, totalHeight) {
  if (totalWidth)
    element.style.left = (totalWidth - element.offsetWidth) / 2 + 'px';
  if (totalHeight)
    element.style.top = (totalHeight - element.offsetHeight) / 2 + 'px';
}

/**
 * Helper function to give an element the ripple effect capability.
 * @param {String} elementID The element to be centered.
 */
function addRippleOverlay(elementID) {
  if(!$(elementID))
    return;
  $(elementID).querySelectorAll('.ink').forEach(e => e.remove());
  var inkEl = document.createElement('span');
  inkEl.classList.add('ink');
  $(elementID).appendChild(inkEl);
  $(elementID).addEventListener('mousedown', e => {
    var currentTarget = e.currentTarget;
    var inkEl = currentTarget.querySelector('.ink');
    inkEl.style.width = inkEl.style.height =
        currentTarget.offsetWidth + 'px';
    // If target is on contained child, the offset of child must be added.
    inkEl.style.left =
        (e.offsetX +
         (e.target.id != elementID ? e.target.offsetLeft : 0) -
         0.5 * inkEl.offsetWidth) +
        'px';
    inkEl.style.top =
        (e.offsetY +
         (e.target.id != elementID ? e.target.offsetTop : 0) -
         0.5 * inkEl.offsetHeight) +
        'px';
    inkEl.classList.add('ripple-category-list-item-animation');
  });
  inkEl.addEventListener('animationend', e => {
    var inkTarget = e.target;
    inkTarget.classList.remove('ripple-category-list-item-animation');
  });
}

/**
 * Loads translated strings.
 */
WallpaperManager.initStrings = function(callback) {
  chrome.wallpaperPrivate.getStrings(function(strings) {
    loadTimeData.data = strings;
    if (callback)
      callback();
  });
};

/**
 * Fetches wallpaper collection info.
 * @private
 */
WallpaperManager.prototype.getCollectionsInfo_ = function() {
  // Prefer to use the saved image info in local storage (if it exists) for
  // faster loading.
  Constants.WallpaperLocalStorage
      .get(
          Constants.AccessLocalImagesInfoKey, items => {
            var imagesInfoJson = items[Constants.AccessLocalImagesInfoKey];
            var currentLanguage = loadTimeData.data_.language;
            var previousLanguage = null;
            if (imagesInfoJson) {
              var imagesInfo = JSON.parse(imagesInfoJson);
              var imagesInfoMap =
                  imagesInfo[Constants.LastUsedLocalImageMappingKey];
              previousLanguage = imagesInfo[Constants.LastUsedLanguageKey];
              if (imagesInfoMap) {
                Object.entries(imagesInfoMap).forEach(([
                                                        collectionId, imagesInfo
                                                      ]) => {
                  var wallpapersDataModel =
                      new cr.ui.ArrayDataModel(imagesInfo.array_);
                  this.imagesInfoMap_[collectionId] = wallpapersDataModel;
                  if (wallpapersDataModel.length > 0) {
                    var imageInfo = wallpapersDataModel.item(0);
                    // Prefer to build |collectionsInfo_| from |imagesInfoMap_|
                    // than to save it in local storage separately, in case of
                    // version mismatch.
                    if (!this.collectionsInfo_)
                      this.collectionsInfo_ = [];
                    this.collectionsInfo_.push({
                      collectionId: imageInfo.collectionId,
                      collectionName: imageInfo.collectionName
                    });
                  }
                });
              }
            }

            // There're four cases to consider:
            // 1) First-time user / Network error: only show the "My images"
            // category.
            // 2) First-time user / No network error: show the "My images"
            // category first (to avoid empty UI in case of slow connection),
            // and call |postDownloadDomInit_| again to show the complete
            // category list after the server responds.
            // 3) Non-first-time user / Network error: show the complete
            // cateogry list based on the image info in local storage.
            // 4) Non-first-time user / No network error: the same with 3),
            // unless updates reflect system language changes.  If there is
            // a change in system language, the same path as 2) occurs.  For
            // other updates, save them to local storage to be used next time
            // (avoid updating the already initialized category list).
            if (!previousLanguage || previousLanguage == currentLanguage) {
              this.postDownloadDomInit_();
            }
            chrome.wallpaperPrivate.getCollectionsInfo(collectionsInfo => {
              if (chrome.runtime.lastError) {
                // TODO(crbug.com/800945): Distinguish the error types and show
                // custom error messages.
                this.showError_(str('connectionFailed'));
                $('wallpaper-grid').classList.add('image-picker-offline');
                return;
              }

              var imagesInfoMap = {};
              var getIndividualCollectionInfo = index => {
                var collectionId = collectionsInfo[index]['collectionId'];
                chrome.wallpaperPrivate.getImagesInfo(
                    collectionId, imagesInfo => {
                      var wallpapersDataModel = new cr.ui.ArrayDataModel([]);

                      if (!chrome.runtime.lastError) {
                        for (var i = 0; i < imagesInfo.length; ++i) {
                          var wallpaperInfo = {
                            // Use the next available unique id.
                            wallpaperId: this.imagesInfoCount_,
                            baseURL: imagesInfo[i]['imageUrl'],
                            highResolutionURL: imagesInfo[i]['imageUrl'] +
                                str('highResolutionSuffix'),
                            layout: Constants.WallpaperThumbnailDefaultLayout,
                            source: Constants.WallpaperSourceEnum.Online,
                            availableOffline: false,
                            displayText: imagesInfo[i]['displayText'],
                            authorWebsite: imagesInfo[i]['actionUrl'],
                            collectionName:
                                collectionsInfo[index]['collectionName'],
                            collectionId: collectionId,
                            ariaLabel: imagesInfo[i]['displayText'][0],
                            // The display order of the collections.
                            collectionIndex: index,
                            previewable: true
                          };
                          wallpapersDataModel.push(wallpaperInfo);
                          ++this.imagesInfoCount_;
                        }
                      }
                      // Save info to the map. The data model is empty if
                      // there's a |chrome.runtime.lastError|.
                      imagesInfoMap[collectionId] = wallpapersDataModel;

                      ++index;
                      if (index >= collectionsInfo.length) {
                        if (!this.collectionsInfo_ ||
                            previousLanguage != currentLanguage) {
                          // Update the UI to show the complete category list,
                          // corresponding to case 2) and 4) above.
                          this.collectionsInfo_ = collectionsInfo;
                          this.imagesInfoMap_ = imagesInfoMap;
                          this.postDownloadDomInit_();
                        }
                        var imagesInfo = {};
                        imagesInfo[Constants.LastUsedLocalImageMappingKey] =
                            imagesInfoMap;
                        imagesInfo[Constants.LastUsedLanguageKey] =
                            currentLanguage;
                        WallpaperUtil.saveToLocalStorage(
                            Constants.AccessLocalImagesInfoKey,
                            JSON.stringify(imagesInfo));
                      } else {
                        // Fetch the info for the next collection.
                        getIndividualCollectionInfo(index);
                      }
                    });
              };

              getIndividualCollectionInfo(0 /*index=*/);
            });
          });
};

/**
 * Displays images that belong to the particular collection.
 * @param {number} index The index of the collection in |collectionsInfo_| list.
 * @private
 */
WallpaperManager.prototype.showCollection_ = function(index) {
  this.wallpaperGrid_.dataModel = null;
  var collectionId = this.collectionsInfo_[index]['collectionId'];
  if (!(collectionId in this.imagesInfoMap_)) {
    console.error('Attempt to display images with an unknown collection id.');
    this.updateNoImagesVisibility_(true);
    return;
  }
  if (this.imagesInfoMap_[collectionId].length == 0) {
    this.updateNoImagesVisibility_(true);
    return;
  }
  this.updateNoImagesVisibility_(false);
  this.wallpaperGrid_.dataModel = this.imagesInfoMap_[collectionId];
};

/**
 * Places the main dialog in the center of the screen, and the header bar at the
 * top.
 * @private
 */
WallpaperManager.prototype.placeWallpaperPicker_ = function() {
  // Wallpaper preview must always be in full screen. Exit preview if the
  // window is not in full screen for any reason (e.g. when device locks).
  if (!chrome.app.window.current().isFullscreen() && this.isDuringPreview_())
    $('cancel-preview-wallpaper').click();

  var totalWidth = this.document_.body.offsetWidth;
  var totalHeight = this.document_.body.offsetHeight;
  centerElement($('preview-spinner'), totalWidth, totalHeight);

  centerElement(
      $('no-images-message'), $('no-images-message').parentNode.offsetWidth,
      $('no-images-message').parentNode.offsetHeight);
  var icon = $('no-images-message').querySelector('.icon');
  var text = $('no-images-message').querySelector('.text');
  // Adjust the relative position of the "no images" icon and text.
  if (text.offsetWidth > icon.offsetWidth) {
    icon.style.marginInlineStart =
        (text.offsetWidth - icon.offsetWidth) / 2 + 'px';
  } else {
    text.style.marginInlineStart =
        (icon.offsetWidth - text.offsetWidth) / 2 + 'px';
  }

  // Position the entire image grid.
  var isHorizontal =
      chrome.app.window.current().isFullscreen() && totalWidth > totalHeight;
  var totalPadding = DIALOG_TOP_BAR_WIDTH + (isHorizontal ? 88 : 48);
  var columnWidth = GRID_IMAGE_WIDTH_CSS + GRID_IMAGE_PADDING_CSS;
  var columnCount = Math.floor((totalWidth - totalPadding) / columnWidth);
  var imageGridTotalWidth = columnCount * columnWidth;
  this.document_.querySelector('.dialog-main').style.marginInlineStart =
      (totalWidth - imageGridTotalWidth - totalPadding) + 'px';

  $('current-wallpaper-info-bar').style.width =
      (imageGridTotalWidth - GRID_IMAGE_PADDING_CSS) + 'px';
  var moreInfoColumnPadding;
  if (isHorizontal) {
    moreInfoColumnPadding = 96;
  } else if (chrome.app.window.current().isFullscreen()) {
    moreInfoColumnPadding = 24;
  } else {
    moreInfoColumnPadding = 24 +
        (this.document_.body.offsetWidth -
         chrome.app.window.current().innerBounds.minWidth) /
            6;
  }
  // The current wallpaper more info column should occupy as much remaining
  // space as possible.
  $('current-wallpaper-more-info').style.width =
      (imageGridTotalWidth - GRID_IMAGE_PADDING_CSS - moreInfoColumnPadding -
       $('current-wallpaper-image').offsetWidth -
       $('current-wallpaper-image').style.marginInlineEnd -
       $('current-wallpaper-more-options').offsetWidth) +
      'px';
};

/**
 * Shows error message in a centered dialog.
 * @private
 * @param {string} errroMessage The string to show in the error dialog.
 */
WallpaperManager.prototype.showError_ = function(errorMessage) {
    $('message-content').textContent = errorMessage;
    $('message-container').style.display = 'block';
};

/**
 * One-time initialization of various DOM nodes. Fetching manifest or the
 * collection info may take a long time due to slow connection. Dom nodes that
 * do not depend on the download should be initialized here.
 */
WallpaperManager.prototype.preDownloadDomInit_ = function() {
  // Ensure that wallpaper manager exits preview if the window gets hidden.
  document.addEventListener('visibilitychange', () => {
    if (this.isDuringPreview_() && document.hidden)
      $('cancel-preview-wallpaper').click();
  });
  this.document_.defaultView.addEventListener(
      'resize', this.onResize_.bind(this));
  this.document_.defaultView.addEventListener(
      'keydown', this.onKeyDown_.bind(this));
  $('minimize-button').addEventListener('click', function() {
    chrome.app.window.current().minimize();
  });
  $('close-button').addEventListener('click', function() {
    window.close();
  });
  window.addEventListener(Constants.WallpaperChangedBy3rdParty, e => {
    this.currentWallpaper_ = e.detail.wallpaperFileName;
    this.decorateCurrentWallpaperInfoBar_();
    // Clear the check mark (if any). Do not try to locate the new wallpaper
    // in the picker to avoid changing the selected category abruptly.
    this.wallpaperGrid_.selectedItem = null;
    this.disableDailyRefresh_();
  });
  var imagePicker = this.document_.body.querySelector('.image-picker');
  imagePicker.addEventListener('scroll', function() {
    var scrollTimer;
    return () => {
      imagePicker.classList.add('show-scroll-bar');
      window.clearTimeout(scrollTimer);
      scrollTimer = window.setTimeout(() => {
        imagePicker.classList.remove('show-scroll-bar');
      }, 500);
    };
  }());

  var dialogTopbar = this.document_.body.querySelector('.dialog-topbar');
  dialogTopbar.addEventListener('scroll', function() {
    var scrollTimer;
    return () => {
      dialogTopbar.classList.add('show-scroll-bar');
      window.clearTimeout(scrollTimer);
      scrollTimer = window.setTimeout(() => {
        dialogTopbar.classList.remove('show-scroll-bar');
      }, 500);
    };
  }());
};

/**
 * One-time initialization of various DOM nodes. Dom nodes that do depend on
 * the download should be initialized here.
 */
WallpaperManager.prototype.postDownloadDomInit_ = function() {
  i18nTemplate.process(this.document_, loadTimeData);
  this.initCategoriesList_();
  this.initThumbnailsGrid_();
  this.presetCategory_();

  // Always prefer the value from local filesystem to avoid the time window
  // of setting the third party app name and the third party wallpaper.
  var getThirdPartyAppName = function(callback) {
    Constants.WallpaperLocalStorage.get(
        Constants.AccessLocalWallpaperInfoKey, function(items) {
          var localInfo = items[Constants.AccessLocalWallpaperInfoKey];
          if (localInfo && localInfo.hasOwnProperty('appName'))
            callback(localInfo.appName);
          else
            callback('');
        });
  };

  getThirdPartyAppName(function(appName) {
    if (appName) {
      $('message-content').textContent =
          loadTimeData.getStringF('currentWallpaperSetByMessage', appName);
      $('message-container').style.display = 'block';
    } else {
      $('message-container').style.display = 'none';
    }
  });

  this.initializeDailyRefreshStates_();

  window.addEventListener('offline', () => {
    $('wallpaper-grid').classList.add('image-picker-offline');
    this.showError_(str('connectionFailed'));
    $('wallpaper-grid').highlightOfflineWallpapers();
  });
  window.addEventListener('online', () => {
    // Fetch the collection info (if not yet) when device gets online.
    if (!this.collectionsInfo_)
      this.getCollectionsInfo_();
    // Force refreshing the images.
    this.wallpaperGrid_.dataModel = null;
    this.onCategoriesChange_();
    $('message-container').style.display = 'none';
    this.downloadedListMap_ = null;
    $('wallpaper-grid').classList.remove('image-picker-offline');
  });

  this.decorateCurrentWallpaperInfoBar_();
  this.onResize_();
  WallpaperUtil.testSendMessage('launched');
};

/**
 * Preset to the category which contains current wallpaper.
 */
WallpaperManager.prototype.presetCategory_ = function() {
  // |currentWallpaper| is either a url containing |highResolutionSuffix| or a
  // custom wallpaper file name.
  this.currentWallpaper_ = str('currentWallpaper');
  this.currentWallpaperLayout_ = str('currentWallpaperLayout');
  // The default category is the last one (the custom category).
  var categoryIndex = this.categoriesList_.dataModel.length - 1;
  Object.entries(this.imagesInfoMap_).forEach(([collectionId, imagesInfo]) => {
    for (var i = 0; i < imagesInfo.length; ++i) {
      if (this.currentWallpaper_.includes(imagesInfo.item(i).baseURL)) {
        for (var index = 0; index < this.collectionsInfo_.length; ++index) {
          // Find the index of the category which the current wallpaper
          // belongs to based on the collection id.
          if (this.collectionsInfo_[index]['collectionId'] == collectionId)
            categoryIndex = index;
        }
      }
    }
  });
  this.categoriesList_.selectionModel.selectedIndex = categoryIndex;
};

/**
 * Decorate the info bar for current wallpaper which shows the image thumbnail,
 * title and description.
 * @private
 */
WallpaperManager.prototype.decorateCurrentWallpaperInfoBar_ = function() {
  var decorateCurrentWallpaperInfoBarImpl =
      currentWallpaperInfo => {
        // Initialize the "more options" buttons.
        var isOnlineWallpaper = !!currentWallpaperInfo;
        var isDefaultWallpaper = !this.currentWallpaper_ ||
            this.currentWallpaper_ == OEM_DEFAULT_WALLPAPER_URL;
        var visibleItemList = [];
        $('refresh').hidden = !isOnlineWallpaper || !this.dailyRefreshInfo_ ||
            !this.dailyRefreshInfo_.enabled;
        if (!$('refresh').hidden) {
          this.addEventToButton_($('refresh'), () => {
            if (this.pendingDailyRefreshInfo_) {
              // There's already a refresh in progress, ignore this request.
              return;
            }
            this.pendingDailyRefreshInfo_ = this.dailyRefreshInfo_;
            this.setDailyRefreshWallpaper_();
          });
          visibleItemList.push($('refresh'));
        }

        $('explore').hidden =
            !isOnlineWallpaper || !currentWallpaperInfo.authorWebsite;
        if (!$('explore').hidden) {
          this.addEventToButton_($('explore'), () => {
            window.open(currentWallpaperInfo.authorWebsite);
          });
          visibleItemList.push($('explore'));
        }

        $('center').hidden = isOnlineWallpaper || isDefaultWallpaper;
        if (!$('center').hidden) {
          this.addEventToButton_(
              $('center'), this.setCustomWallpaperLayout_.bind(this, 'CENTER'));
          visibleItemList.push($('center'));
        }

        $('center-cropped').hidden = isOnlineWallpaper || isDefaultWallpaper;
        if (!$('center-cropped').hidden) {
          this.addEventToButton_(
              $('center-cropped'),
              this.setCustomWallpaperLayout_.bind(this, 'CENTER_CROPPED'));
          visibleItemList.push($('center-cropped'));
        }

        if (visibleItemList.length == 1) {
          visibleItemList[0].style.marginTop =
              ($('current-wallpaper-info-bar').offsetHeight -
               visibleItemList[0].offsetHeight) /
                  2 +
              'px';
        } else if (visibleItemList.length == 2) {
          // There are at most two visible elements.
          var topMargin = ($('current-wallpaper-info-bar').offsetHeight -
                           visibleItemList[0].offsetHeight -
                           visibleItemList[1].offsetHeight) *
              0.4;
          visibleItemList[0].style.marginTop = topMargin + 'px';
          visibleItemList[1].style.marginTop = topMargin / 2 + 'px';
        }
        // Add necessary padding and make sure all the texts are centered. Clear
        // the existing padding first.
        for (var item of visibleItemList) {
          item.style.paddingLeft = item.style.paddingRight = '0px';
        }
        var totalWidth = $('current-wallpaper-more-options').offsetWidth;
        for (var item of visibleItemList) {
          var padding = 15 +
              (totalWidth -
               (item.querySelector('.icon').offsetWidth +
                item.querySelector('.text').offsetWidth)) /
                  2;
          item.style.paddingLeft = item.style.paddingRight = padding + 'px';
        }

        // Clear the existing contents (needed if the wallpaper changes while
        // the picker is open).
        $('current-wallpaper-description').innerHTML = '';
        if (isOnlineWallpaper) {
          // Set the image title and description.
          $('current-wallpaper-title').textContent =
              currentWallpaperInfo.displayText[0];
          $('current-wallpaper-description')
              .classList.toggle(
                  'small-font',
                  currentWallpaperInfo.displayText.length > 2 &&
                      !chrome.app.window.current().isFullscreen() &&
                      !chrome.app.window.current().isMaximized());
          for (var i = 1; i < currentWallpaperInfo.displayText.length; ++i) {
            $('current-wallpaper-description')
                .appendChild(document.createTextNode(
                    currentWallpaperInfo.displayText[i]));
            $('current-wallpaper-description')
                .appendChild(document.createElement('br'));
          }
        }

        var imageElement = $('current-wallpaper-image');
        var isFromOldPicker =
            currentWallpaperInfo && currentWallpaperInfo.isFromOldPicker;
        if (isOnlineWallpaper && !isFromOldPicker) {
          WallpaperUtil.displayThumbnail(
              imageElement, currentWallpaperInfo.baseURL,
              Constants.WallpaperSourceEnum.Online);
        } else {
          // Request the thumbnail of the current wallpaper as fallback, since
          // the picker doesn't have access to thumbnails of other types of
          // wallpapers.
          var currentWallpaper = this.currentWallpaper_;
          chrome.wallpaperPrivate.getCurrentWallpaperThumbnail(
              imageElement.offsetHeight, imageElement.offsetWidth,
              thumbnail => {
                // If the current wallpaper already changed when this function
                // returns, do nothing.
                if (currentWallpaper != this.currentWallpaper_)
                  return;

                // If the current wallpaper is third_party, remove checkmark.
                if (this.currentWallpaper_.includes('third_party'))
                  this.wallpaperGrid_.activeItem = null;

                WallpaperUtil.displayImage(
                    imageElement, thumbnail, null /*opt_callback=*/);
                // Show a placeholder as the image title.
                $('current-wallpaper-title').textContent =
                    str('customCategoryLabel');
              });
        }

        this.toggleLayoutButtonStates_(this.currentWallpaperLayout_);
        this.placeWallpaperPicker_();
        $('current-wallpaper-info-bar').classList.add('show-info-bar');
      };

  // Try finding the current wallpaper in the online wallpaper collection.
  var currentWallpaperInfo;
  Object.values(this.imagesInfoMap_).forEach(imagesInfo => {
    for (var i = 0; i < imagesInfo.length; ++i) {
      if (this.currentWallpaper_.includes(imagesInfo.item(i).baseURL))
        currentWallpaperInfo = imagesInfo.item(i);
    }
  });

  if (currentWallpaperInfo) {
    // Update active item to current online wallpaper.
    this.wallpaperGrid_.activeItem = currentWallpaperInfo;
    decorateCurrentWallpaperInfoBarImpl(currentWallpaperInfo);
  } else {
    // Migration: it's possible that the wallpaper was selected from the online
    // collection of the old picker. Try finding its info from local storage.
    var accessManifestKey = Constants.AccessLocalManifestKey;
    Constants.WallpaperLocalStorage.get(accessManifestKey, items => {
      var manifest = items[accessManifestKey];
      if (manifest) {
        for (var i = 0; i < manifest.wallpaper_list.length; i++) {
          if (this.currentWallpaper_.includes(
                  manifest.wallpaper_list[i].base_url)) {
            currentWallpaperInfo = {
              displayText: ['', manifest.wallpaper_list[i].author],
              authorWebsite: manifest.wallpaper_list[i].author_website,
              isFromOldPicker: true
            };
          }
        }
      }
      decorateCurrentWallpaperInfoBarImpl(currentWallpaperInfo);
    });
  }
};

/**
 * Constructs the thumbnails grid.
 */
WallpaperManager.prototype.initThumbnailsGrid_ = function() {
  this.wallpaperGrid_ = $('wallpaper-grid');
  wallpapers.WallpaperThumbnailsGrid.decorate(this.wallpaperGrid_);

  this.wallpaperGrid_.addEventListener('change', this.onChange_.bind(this));
};

/**
 * Handles change event dispatched by wallpaper grid.
 */
WallpaperManager.prototype.onChange_ = function() {
  // splice may dispatch a change event because the position of selected
  // element changing. But the actual selected element may not change after
  // splice. Check if the new selected element equals to the previous selected
  // element before continuing. Otherwise, wallpaper may reset to previous one
  // as described in http://crbug.com/229036.
  if (this.selectedItem_ == this.wallpaperGrid_.selectedItem)
    return;
  this.selectedItem_ = this.wallpaperGrid_.selectedItem;
  this.onSelectedItemChanged_();
};

/**
 * Closes window if no pending wallpaper request.
 */
WallpaperManager.prototype.onClose_ = function() {
  if (this.wallpaperRequest_) {
    this.wallpaperRequest_.addEventListener('loadend', function() {
      // Close window on wallpaper loading finished.
      window.close();
    });
  } else {
    window.close();
  }
};

/**
 * Moves the check mark to |activeItem| and hides the wallpaper set by third
 * party message if any. And saves the wallpaper's information to local & sync
 * storage. Called when wallpaper changed successfully.
 * @param {?Object} activeItem The active item in WallpaperThumbnailsGrid's
 *     data model.
 * @param {?string} currentWallpaperURL The URL or filename of current
 *     wallpaper.
 */
WallpaperManager.prototype.onWallpaperChanged_ = function(
    activeItem, currentWallpaperURL) {
  $('message-container').style.display = 'none';
  this.wallpaperGrid_.activeItem = activeItem;
  this.currentWallpaper_ = currentWallpaperURL;
  this.decorateCurrentWallpaperInfoBar_();
  this.wallpaperGrid_.checkmark.focus();

  // Disables daily refresh if user selects a non-daily wallpaper.
  if (activeItem && activeItem.source !== Constants.WallpaperSourceEnum.Daily)
    this.disableDailyRefresh_();

  if (activeItem) {
    WallpaperUtil.saveWallpaperInfo(
        currentWallpaperURL, activeItem.layout, activeItem.source, '');
  } else {
    WallpaperUtil.saveWallpaperInfo(
        '', '', Constants.WallpaperSourceEnum.Default, '');
  }
};

/**
 * Sets wallpaper to the corresponding wallpaper of selected thumbnail.
 * @param {Object} selectedItem The selected item in WallpaperThumbnailsGrid's
 *     data model.
 * @private
 */
WallpaperManager.prototype.setSelectedWallpaper_ = function(selectedItem) {
  switch (selectedItem.source) {
    case Constants.WallpaperSourceEnum.Custom:
      this.setSelectedCustomWallpaper_(selectedItem);
      break;
    case Constants.WallpaperSourceEnum.OEM:
      // Resets back to default wallpaper.
      chrome.wallpaperPrivate.resetWallpaper();
      this.onWallpaperChanged_(selectedItem, selectedItem.baseURL);
      break;
    case Constants.WallpaperSourceEnum.Online:
      var previewMode = this.shouldPreviewWallpaper_();
      var successCallback = () => {
        this.updateSpinnerVisibility_(false);
        if (previewMode) {
          this.onPreviewModeStarted_(
              selectedItem,
              this.onWallpaperChanged_.bind(
                  this, selectedItem, selectedItem.highResolutionURL),
              /*optCancelCallback=*/null, /*optOnRefreshClicked=*/null);
        } else {
          this.onWallpaperChanged_(
              selectedItem, selectedItem.highResolutionURL);
        }
      };
      this.setSelectedOnlineWallpaper_(selectedItem, successCallback, () => {
        this.updateSpinnerVisibility_(false);
      }, previewMode);
      break;
    case Constants.WallpaperSourceEnum.Daily:
    case Constants.WallpaperSourceEnum.ThirdParty:
    default:
      console.error('Unsupported wallpaper source.');
  }
};

/**
 * Implementation of |setSelectedWallpaper_| for custom wallpapers.
 * @param {Object} selectedItem The selected item in WallpaperThumbnailsGrid's
 *     data model.
 * @param {function} successCallback The success callback.
 * @private
 */
WallpaperManager.prototype.setSelectedCustomWallpaper_ = function(
    selectedItem, successCallback) {
  if (selectedItem.source != Constants.WallpaperSourceEnum.Custom) {
    console.error(
        '|setSelectedCustomWallpaper_| is called but the wallpaper source ' +
        'is not custom.');
    return;
  }

  var successCallback = (imageData, optThumbnailData) => {
    this.onWallpaperChanged_(selectedItem, selectedItem.baseURL);
    WallpaperUtil.storeWallpaperToSyncFS(selectedItem.baseURL, imageData);
  };
  this.setCustomWallpaperImpl_(selectedItem, successCallback);
};

/**
 * Implementation of |setSelectedCustomWallpaper_|.
 * @param {Object} selectedItem The selected item in WallpaperThumbnailsGrid's
 *     data model.
 * @param {function} successCallback The success callback.
 * @private
 */
WallpaperManager.prototype.setCustomWallpaperImpl_ = function(
    selectedItem, successCallback) {
  // Read the image data from |filePath| and set the wallpaper with the data.
  chrome.wallpaperPrivate.getLocalImageData(
      selectedItem.filePath, imageData => {
        if (chrome.runtime.lastError || !imageData) {
          this.showError_(str('downloadFailed'));
          return;
        }
        var previewMode = this.shouldPreviewWallpaper_();
        if (!previewMode) {
          chrome.wallpaperPrivate.setCustomWallpaper(
              imageData, selectedItem.layout, false /*generateThumbnail=*/,
              selectedItem.baseURL, false /*previewMode=*/,
              optThumbnailData => {
                if (chrome.runtime.lastError) {
                  this.showError_(str('downloadFailed'));
                  return;
                }
                successCallback(imageData, optThumbnailData);
              });
          return;
        }

        var decorateLayoutButton = layout => {
          if (layout != 'CENTER' && layout != 'CENTER_CROPPED') {
            console.error('Wallpaper layout ' + layout + ' is not supported.');
            return;
          }
          var layoutButton = this.document_.querySelector(
              layout == 'CENTER' ? '#center-button' : '#center-cropped-button');
          this.addEventToButton_(layoutButton, () => {
            chrome.wallpaperPrivate.setCustomWallpaper(
                imageData, layout, false /*generateThumbnail=*/,
                selectedItem.baseURL, true /*previewMode=*/,
                optThumbnailData => {
                  if (chrome.runtime.lastError) {
                    this.showError_(str('downloadFailed'));
                    return;
                  }
                  this.currentlySelectedLayout_ = layout;
                  this.document_.querySelector('#center-button')
                      .classList.toggle('disabled', layout == 'CENTER');
                  this.document_.querySelector('#center-cropped-button')
                      .classList.toggle('disabled', layout == 'CENTER_CROPPED');
                  this.onPreviewModeStarted_(
                      selectedItem,
                      successCallback.bind(null, imageData, optThumbnailData),
                      /*optCancelCallback=*/null,
                      /*optOnRefreshClicked=*/null);
                });
          });
        };

        decorateLayoutButton('CENTER');
        decorateLayoutButton('CENTER_CROPPED');
        // The default layout is CENTER_CROPPED.
        this.document_.querySelector('#center-cropped-button').click();
      });
};

/**
 * Implementation of |setSelectedWallpaper_| for online wallpapers.
 * @param {Object} selectedItem The selected item in WallpaperThumbnailsGrid's
 *     data model.
 * @param {function} successCallback The callback after the wallpaper is set
 *     successfully.
 * @param {function} failureCallback The callback after setting the wallpaper
 *     fails.
 * @param {boolean} previewMode True if the wallpaper should be previewed.
 * @private
 */
WallpaperManager.prototype.setSelectedOnlineWallpaper_ = function(
    selectedItem, successCallback, failureCallback, previewMode) {
  // Cancel any ongoing wallpaper request, otherwise the wallpaper being set in
  // the end may not be the one that the user selected the last, because the
  // time needed to set each wallpaper may vary (e.g. some wallpapers already
  // exist in the local file system but others need to be fetched from server).
  if (this.wallpaperRequest_) {
    this.wallpaperRequest_.abort();
    this.wallpaperRequest_ = null;
  }

  var selectedGridItem = this.wallpaperGrid_.getListItem(selectedItem);
  chrome.wallpaperPrivate.setWallpaperIfExists(
      selectedItem.highResolutionURL, selectedItem.layout, previewMode,
      exists => {
        if (exists) {
          successCallback();
          return;
        }

        // Falls back to request wallpaper from server.
        this.wallpaperRequest_ = new XMLHttpRequest();
        this.progressManager_.reset(this.wallpaperRequest_, selectedGridItem);

        var onSuccess =
            xhr => {
              var image = xhr.response;
              chrome.wallpaperPrivate.setWallpaper(
                  image, selectedItem.layout, selectedItem.highResolutionURL,
                  previewMode, () => {
                    this.progressManager_.hideProgressBar(selectedGridItem);

                    if (chrome.runtime.lastError != undefined &&
                        chrome.runtime.lastError.message !=
                            str('canceledWallpaper')) {
                      // The user doesn't need to distinguish this error (most
                      // likely due to a decode failure) from a download
                      // failure.
                      this.showError_(str('downloadFailed'));
                      failureCallback();
                      return;
                    }
                    successCallback();
                  });
              this.wallpaperRequest_ = null;
            };
        var onFailure = status => {
          this.progressManager_.hideProgressBar(selectedGridItem);
          this.showError_(str('downloadFailed'));
          this.wallpaperRequest_ = null;
          failureCallback();
        };
        WallpaperUtil.fetchURL(
            selectedItem.highResolutionURL, 'arraybuffer', onSuccess, onFailure,
            this.wallpaperRequest_);
      });
};

/**
 * Handles the UI changes when the preview mode is started.
 * @param {Object} wallpaperInfo The info related to the wallpaper image.
 * @param {function} optConfirmCallback The callback after preview
 *     wallpaper is set.
 * @param {function} optCancelCallback The callback after preview
 *     is canceled.
 * @param {function} optOnRefreshClicked The event listener for the refresh
 *     button. Must be non-null when the wallpaper type is daily.
 * @private
 */
WallpaperManager.prototype.onPreviewModeStarted_ = function(
    wallpaperInfo, optConfirmCallback, optCancelCallback, optOnRefreshClicked) {
  if (this.isDuringPreview_())
    return;

  addRippleOverlay("center-button");
  addRippleOverlay("center-cropped-button");

  this.document_.body.classList.add('preview-animation');
  chrome.wallpaperPrivate.minimizeInactiveWindows();
  window.setTimeout(() => {
    chrome.app.window.current().fullscreen();
    this.document_.body.classList.add('preview-mode');
    this.document_.body.classList.toggle(
        'custom-wallpaper',
        wallpaperInfo.source == Constants.WallpaperSourceEnum.Custom);
    this.document_.body.classList.toggle(
        'daily-wallpaper',
        wallpaperInfo.source == Constants.WallpaperSourceEnum.Daily);
  }, 800);

  var onConfirmClicked = () => {
    chrome.wallpaperPrivate.confirmPreviewWallpaper(() => {
      if (optConfirmCallback)
        optConfirmCallback();
      this.showSuccessMessageAndQuit_();
    });
  };
  this.addEventToButton_($('confirm-preview-wallpaper'), onConfirmClicked);

  var onRefreshClicked = () => {
    if (optOnRefreshClicked)
      optOnRefreshClicked();
  };
  this.addEventToButton_($('refresh-wallpaper'), onRefreshClicked);

  // Enable swiping to show the previous/next preview wallpaper.
  var onTouchStarted = e => {
    // Do not enable swiping if a non-null |optOnRefreshClicked| is provided.
    if (optOnRefreshClicked)
      return;
    this.xStart_ = e.touches[0].clientX;
    this.yStart_ = e.touches[0].clientY;
  };
  $('preview-canvas').addEventListener('touchstart', onTouchStarted);

  var onTouchMoved = e => {
    if (!this.xStart_ || !this.yStart_)
      return;

    var xDiff = e.touches[0].clientX - this.xStart_;
    var yDiff = e.touches[0].clientY - this.yStart_;
    // Reset these to prevent duplicate handling of a single swipe event.
    this.xStart_ = null;
    this.yStart_ = null;
    // Ignore vertical swipes.
    if (Math.abs(xDiff) <= Math.abs(yDiff))
      return;

    // When swiping to the left(right), the next(previous) wallpaper should
    // be previewed.
    // TODO(crbug.com/837355): Consider flipping this for RTL languages.
    var onScreenSwiped = left => {
      var dataModel = this.wallpaperGrid_.dataModel;
      // Get the index of the wallpaper that's being previewed. This is only
      // needed for the first swipe event.
      if (this.currentPreviewIndex_ == null) {
        for (var i = 0; i < dataModel.length; ++i) {
          if (dataModel.item(i) == wallpaperInfo) {
            this.currentPreviewIndex_ = i;
            break;
          }
        }
      }
      // The wallpaper being previewed may not come from the data model
      // (e.g. when it's from the current wallpaper info bar). In this case
      // do nothing.
      if (this.currentPreviewIndex_ == null)
        return;

      // Find the previous/next wallpaper to be previewed based on the swipe
      // direction.
      var getNextPreviewIndex = (index, left) => {
        var getNextPreviewIndexImpl = (index, left) => {
          index += left ? 1 : -1;
          index = index % dataModel.length;
          if (index < 0)
            index += dataModel.length;
          return index;
        };

        index = getNextPreviewIndexImpl(index, left);
        var firstFoundIndex = index;
        // The item must be previewable.
        while (!dataModel.item(index).previewable) {
          index = getNextPreviewIndexImpl(index, left);
          // Return null if none of the items within the data model is
          // previewable.
          if (firstFoundIndex === index)
            return null;
        }
        return index;
      };

      // Start previewing the next wallpaper.
      var nextPreviewIndex =
          getNextPreviewIndex(this.currentPreviewIndex_, left);
      if (nextPreviewIndex === null) {
        console.error(
            'Cannot find any previewable wallpaper. This should never happen.');
        return;
      }
      var nextPreviewImage = dataModel.item(nextPreviewIndex);
      if (nextPreviewImage.source == Constants.WallpaperSourceEnum.Online)
        this.updateSpinnerVisibility_(true);
      $('message-container').style.display = 'none';
      this.setWallpaperAttribution(nextPreviewImage);
      this.setSelectedWallpaper_(nextPreviewImage);
      this.currentPreviewIndex_ = nextPreviewIndex;
    };

    // |xDiff < 0| indicates a left swipe.
    onScreenSwiped(xDiff < 0);
  };
  $('preview-canvas').addEventListener('touchmove', onTouchMoved);

  var onCancelClicked = () => {
    $('preview-canvas').removeEventListener('touchstart', onTouchStarted);
    $('preview-canvas').removeEventListener('touchmove', onTouchMoved);

    chrome.wallpaperPrivate.cancelPreviewWallpaper(() => {
      if (optCancelCallback)
        optCancelCallback();
      // Deselect the image.
      this.wallpaperGrid_.selectedItem = null;
      this.currentlySelectedLayout_ = null;
      this.currentPreviewIndex_ = null;
      this.document_.body.classList.remove('preview-mode');
      this.document_.body.classList.remove('preview-animation');
      this.updateSpinnerVisibility_(false);
      // Exit full screen, but the window should still be maximized.
      if (chrome.app.window.current().isFullscreen())
        chrome.app.window.current().maximize();
      // TODO(crbug.com/841968): Force refreshing the images. This is a
      // workaround until the issue is fixed.
      this.wallpaperGrid_.dataModel = null;
      this.onCategoriesChange_();
    });
  };
  this.addEventToButton_($('cancel-preview-wallpaper'), onCancelClicked);
  window.addEventListener(Constants.ClosePreviewWallpaper, onCancelClicked);

  $('message-container').style.display = 'none';
};

/*
 * Shows a success message and closes the window.
 * @private
 */
WallpaperManager.prototype.showSuccessMessageAndQuit_ = function() {
  this.document_.body.classList.add('wallpaper-set-successfully');
  $('message-content').textContent = str('setSuccessfullyMessage');
  // Success message must be shown in full screen mode.
  chrome.app.window.current().fullscreen();
  $('message-container').style.display = 'block';
  // Close the window after showing the success message.
  window.setTimeout(() => {
    window.close();
  }, 800);
};

/**
 * Handles changing of selectedItem in wallpaper manager.
 */
WallpaperManager.prototype.onSelectedItemChanged_ = function() {
  if (!this.selectedItem_)
    return;
  this.setWallpaperAttribution(this.selectedItem_);

  if (this.selectedItem_.baseURL) {
    if (this.selectedItem_.source == Constants.WallpaperSourceEnum.Custom) {
      var items = {};
      var key = this.selectedItem_.baseURL;
      var self = this;
      Constants.WallpaperLocalStorage.get(key, function(items) {
        self.selectedItem_.layout =
            items[key] ? items[key] : Constants.WallpaperThumbnailDefaultLayout;
        self.setSelectedWallpaper_(self.selectedItem_);
      });
    } else {
      this.setSelectedWallpaper_(this.selectedItem_);
    }
  }
};

/**
 * Set attributions of wallpaper with given URL. If URL is not valid, clear
 * the attributions.
 * @param {Object} selectedItem the selected item in WallpaperThumbnailsGrid's
 *     data model.
 */
WallpaperManager.prototype.setWallpaperAttribution = function(selectedItem) {
    $('image-title').textContent = '';
    $('wallpaper-description').textContent = '';
    if (selectedItem) {
      // The first element in |displayText| is used as title.
      if (selectedItem.displayText)
        $('image-title').textContent = selectedItem.displayText[0];

      if (selectedItem.displayText && selectedItem.displayText.length > 1) {
        for (var i = 1; i < selectedItem.displayText.length; ++i) {
          $('wallpaper-description').textContent +=
              selectedItem.displayText[i] + ' ';
        }
      } else if (selectedItem.collectionName) {
        // Display the collection name as backup.
        $('wallpaper-description').textContent = selectedItem.collectionName;
      }
    }
};

/**
 * Resize thumbnails grid and categories list to fit the new window size.
 */
WallpaperManager.prototype.onResize_ = function() {
  // Resize events should be ignored during preview mode, since the app
  // should be fullscreen and transparent, hiding the elements that are
  // otherwise redrawn when preview mode is off (when the picker should
  // be visible).  While chrome.app.window.current().fullscreen() is
  // running, the bit for chrome.app.window.current().isFullscreen() may
  // not have been flipped on yet, causing placeWallpaperPicker_() to
  // initiate an unintended early cancellation of preview mode.
  if (this.isDuringPreview_())
    return;
  this.placeWallpaperPicker_();
  this.wallpaperGrid_.redraw();
  this.categoriesList_.redraw();
};

/**
 * Close the last opened overlay or app window on pressing the Escape key.
 * @param {Event} event A keydown event.
 */
WallpaperManager.prototype.onKeyDown_ = function(event) {
  if (event.keyCode == 27) {
    // The last opened overlay coincides with the first match of querySelector
    // because the Error Container is declared in the DOM before the Wallpaper
    // Selection Container.
    // TODO(bshe): Make the overlay selection not dependent on the DOM.
    var closeButtonSelector = '.overlay-container:not([hidden]) .close';
    var closeButton = this.document_.querySelector(closeButtonSelector);
    if (closeButton) {
      closeButton.click();
      event.preventDefault();
    } else {
      this.onClose_();
    }
  }
};

/**
 * Constructs the categories list.
 */
WallpaperManager.prototype.initCategoriesList_ = function() {
  this.categoriesList_ = $('categories-list');
  wallpapers.WallpaperCategoriesList.decorate(this.categoriesList_);

  this.categoriesList_.selectionModel.addEventListener(
      'change', this.onCategoriesChange_.bind(this));

    if (this.collectionsInfo_) {
      for (var colletionInfo of this.collectionsInfo_)
        this.categoriesList_.dataModel.push(colletionInfo['collectionName']);
    }
  // Adds custom category as last category.
  this.categoriesList_.dataModel.push(str('customCategoryLabel'));
};

/**
 * Updates the layout of the currently set custom wallpaper. No-op if the
 * current wallpaper is not a custom wallpaper.
 * @param {string} newLayout The new wallpaper layout.
 * @private
 */
WallpaperManager.prototype.setCustomWallpaperLayout_ = function(newLayout) {
  var setCustomWallpaperLayoutImpl = (layout, onSuccess) => {
    chrome.wallpaperPrivate.setCustomWallpaperLayout(layout, () => {
      if (chrome.runtime.lastError != undefined &&
          chrome.runtime.lastError.message != str('canceledWallpaper')) {
        return;
      }
      WallpaperUtil.saveToLocalStorage(this.currentWallpaper_, layout);
      this.toggleLayoutButtonStates_(layout);
      WallpaperUtil.saveWallpaperInfo(
          this.currentWallpaper_, layout, Constants.WallpaperSourceEnum.Custom,
          '');
      if (onSuccess)
        onSuccess();
    });
  };

  if (!this.shouldPreviewWallpaper_()) {
    setCustomWallpaperLayoutImpl(newLayout, null /*onSuccess=*/);
    this.currentWallpaperLayout_ = newLayout;
    return;
  }

  // TODO(crbug.com/836396): Add |previewMode| option to
  // |setCustomWallpaperLayout| instead.
  var forcePreviewMode = () => {
    chrome.wallpaperPrivate.getCurrentWallpaperThumbnail(
        this.document_.body.offsetHeight, this.document_.body.offsetWidth,
        image => {
          chrome.wallpaperPrivate.setCustomWallpaper(
              image, Constants.WallpaperThumbnailDefaultLayout,
              false /*generateThumbnail=*/, this.currentWallpaper_,
              true /*previewMode=*/, () => {
                chrome.wallpaperPrivate.cancelPreviewWallpaper(() => {});
              });
        });
  };
  forcePreviewMode();

  var decorateLayoutButton = layout => {
    if (layout != 'CENTER' && layout != 'CENTER_CROPPED') {
      console.error('Wallpaper layout ' + layout + ' is not supported.');
      return;
    }
    var layoutButton = this.document_.querySelector(
        layout == 'CENTER' ? '#center-button' : '#center-cropped-button');
    var newLayoutButton = layoutButton.cloneNode(true);
    layoutButton.parentNode.replaceChild(newLayoutButton, layoutButton);
    newLayoutButton.addEventListener('click', () => {
      setCustomWallpaperLayoutImpl(layout, () => {
        this.document_.querySelector('#center-button')
            .classList.toggle('disabled', layout == 'CENTER');
        this.document_.querySelector('#center-cropped-button')
            .classList.toggle('disabled', layout == 'CENTER_CROPPED');
        this.onPreviewModeStarted_(
            {source: Constants.WallpaperSourceEnum.Custom},
            null /*optConfirmCallback=*/,
            setCustomWallpaperLayoutImpl.bind(
                null, this.currentWallpaperLayout_),
            /*optOnRefreshClicked=*/null);
      });
    });
  };

  decorateLayoutButton('CENTER');
  decorateLayoutButton('CENTER_CROPPED');
  this.document_
      .querySelector(
          newLayout == 'CENTER' ? '#center-button' : '#center-cropped-button')
      .click();
  this.setWallpaperAttribution({collectionName: str('customCategoryLabel')});
};

/**
 * Handles UI changes based on whether surprise me is enabled.
 * @param enabled Whether surprise me is enabled.
 * @private
 */
WallpaperManager.prototype.onSurpriseMeStateChanged_ = function(enabled) {
  WallpaperUtil.setSurpriseMeCheckboxValue(enabled);
  $('categories-list').disabled = enabled;
  $('wallpaper-grid').disabled = enabled;
  if (enabled)
    this.document_.body.removeAttribute('surprise-me-disabled');
  else
    this.document_.body.setAttribute('surprise-me-disabled', '');
};

/**
 * Handles user clicking on a different category.
 */
WallpaperManager.prototype.onCategoriesChange_ = function() {
  var categoriesList = this.categoriesList_;
  var selectedIndex = categoriesList.selectionModel.selectedIndex;
  if (selectedIndex == -1)
    return;
  // Cancel any ongoing wallpaper request if user clicks on another category.
  if (this.wallpaperRequest_) {
    this.wallpaperRequest_.abort();
    this.wallpaperRequest_ = null;
  }
  // Always start with the top when showing a new category.
  this.wallpaperGrid_.scrollTop = 0;
  var selectedListItem = categoriesList.getListItemByIndex(selectedIndex);
  if (selectedListItem.custom) {
    var wallpapersDataModel = new cr.ui.ArrayDataModel([]);
    if (loadTimeData.getBoolean('isOEMDefaultWallpaper')) {
      var defaultWallpaperInfo = {
        wallpaperId: null,
        baseURL: OEM_DEFAULT_WALLPAPER_URL,
        layout: Constants.WallpaperThumbnailDefaultLayout,
        source: Constants.WallpaperSourceEnum.OEM,
        ariaLabel: loadTimeData.getString('defaultWallpaperLabel'),
        availableOffline: true
      };
      wallpapersDataModel.push(defaultWallpaperInfo);
    }
    chrome.wallpaperPrivate.getLocalImagePaths(localImagePaths => {
      for (var imagePath of localImagePaths) {
        var wallpaperInfo = {
          // The absolute file path, used for retrieving the image data if user
          // chooses to set this wallpaper.
          filePath: imagePath,
          // Used as the file name when saving the wallpaper to local and sync
          // storage, which only happens after user chooses to set this
          // wallpaper. The name 'baseURL' is for consistency with the old
          // wallpaper picker.
          // TODO(crbug.com/812085): Rename it to fileName.
          baseURL: new Date().getTime().toString(),
          layout: Constants.WallpaperThumbnailDefaultLayout,
          source: Constants.WallpaperSourceEnum.Custom,
          availableOffline: true,
          collectionName: str('customCategoryLabel'),
          fileName: imagePath.split(/[/\\]/).pop(),
          // Use file name as aria-label.
          ariaLabel() {
            return this.fileName;
          },
          previewable: true,
        };
        wallpapersDataModel.push(wallpaperInfo);
      }
      // Show a "no images" message if there's no image.
      this.updateNoImagesVisibility_(wallpapersDataModel.length == 0);
      this.wallpaperGrid_.dataModel = wallpapersDataModel;

      var findAndUpdateActiveItem = currentWallpaperImageInfo => {
        // If the current wallpaper is not OEM or Custom,
        // the activeItem is already set to the correct imageInfo.
        if (currentWallpaperImageInfo &&
            currentWallpaperImageInfo.source !=
                Constants.WallpaperSourceEnum.Custom &&
            currentWallpaperImageInfo.source !=
                Constants.WallpaperSourceEnum.OEM) {
          return;
        }
        var desiredFileName = currentWallpaperImageInfo.fileName;
        // Since a new OEM and custom wallpaperDataModel is created each
        // time, wallpaperGrid_.activeItem references a different variable,
        // despite having the same value.
        for (var i = 0; i < wallpapersDataModel.length; ++i) {
          var item = wallpapersDataModel.item(i);
          // TODO(crbug/947543): Using the filename will not cover the case
          // when the image changes but not the fileName.
          if (item.fileName == desiredFileName)
            this.wallpaperGrid_.activeItem = item;
        }
      };

      if (this.wallpaperGrid_.activeItem) {
        findAndUpdateActiveItem(this.wallpaperGrid_.activeItem);
      } else {
        // Wallpaper app has just launched, determine if wallpaper image
        // is OEM or Custom.
        Constants.WallpaperLocalStorage.get(
            Constants.AccessLastUsedImageInfoKey, lastUsedImageInfo => {
              lastUsedImageInfo =
                  lastUsedImageInfo[Constants.AccessLastUsedImageInfoKey];
              // If the last used image is third party, the Wallpaper app is
              // used for the first time, or the Wallpaper app local storage is
              // cleared, lastUsedImageInfo will be falsy.
              if (!lastUsedImageInfo)
                return;
              findAndUpdateActiveItem(lastUsedImageInfo);
            });
      }



    });
  } else {
    this.document_.body.removeAttribute('custom');
    if (this.collectionsInfo_)
      this.showCollection_(selectedIndex);
  }
};

/**
 * Updates the visibility of the "no images" message.
 * @param {boolean} visible Whether the message should be visible.
 * @private
 */
WallpaperManager.prototype.updateNoImagesVisibility_ = function(visible) {
  if (visible == this.document_.body.classList.contains('no-images'))
    return;
  this.document_.body.classList.toggle('no-images', visible);
  this.placeWallpaperPicker_();
};

/**
 * Updates the visibility of the spinners. At most one spinner can be visible at
 * a time.
 * @param {boolean} visible Whether the spinner should be visible.
 * @private
 */
WallpaperManager.prototype.updateSpinnerVisibility_ = function(visible) {
  $('preview-spinner').hidden = !visible || !this.isDuringPreview_();
  $('current-wallpaper-spinner').hidden = !visible || this.isDuringPreview_();
};

/**
 * Returns if wallpaper should be previewed before being set.
 * @return {boolean} If wallpaper should be previewed.
 * @private
 */
WallpaperManager.prototype.shouldPreviewWallpaper_ = function() {
  return chrome.app.window.current().isFullscreen() ||
      chrome.app.window.current().isMaximized();
};

/**
 * Returns whether preview mode is currently on.
 * @return {boolean} Whether preview mode is currently on.
 * @private
 */
WallpaperManager.prototype.isDuringPreview_ = function() {
  return this.document_.body.classList.contains('preview-mode');
};

/**
 * Notifies the wallpaper manager that the scroll bar position changes.
 * @param {number} scrollTop The distance between the scroll bar and the top.
 */
WallpaperManager.prototype.onScrollPositionChanged = function(scrollTop) {
  // The current wallpaper info bar should scroll together with the image grid.
  $('current-wallpaper-info-bar').style.top = -scrollTop + 'px';
};

/**
 * Returns the currently selected layout.
 * @return {string} The selected layout.
 * @private
 */
WallpaperManager.prototype.getSelectedLayout_ = function() {
    return this.currentlySelectedLayout_ ?
        this.currentlySelectedLayout_ :
        Constants.WallpaperThumbnailDefaultLayout;
};

/**
 * Toggles the disabled states of the layout buttons on the top header bar based
 * on the currently selected layout.
 * @param {string} layout The currently selected layout.
 * @private
 */
WallpaperManager.prototype.toggleLayoutButtonStates_ = function(layout) {
  $('center').classList.toggle('disabled', layout == 'CENTER');
  $('center-cropped').classList.toggle('disabled', layout == 'CENTER_CROPPED');
};

/**
 * Fetches the info related to the daily refresh feature and updates the UI for
 * the items.
 * @private
 */
WallpaperManager.prototype.initializeDailyRefreshStates_ = function() {
  var initializeDailyRefreshStatesImpl = dailyRefreshInfo => {
    $('wallpaper-grid').classList.remove('daily-refresh-disabled');
    if (dailyRefreshInfo) {
      this.dailyRefreshInfo_ = dailyRefreshInfo;
    } else {
      // We reach here if it's the first time the new picker is in use, or the
      // unlikely case that the data was corrupted (it's safe to assume daily
      // refresh is disabled if this ever happens).
      this.dailyRefreshInfo_ = {
        enabled: false,
        collectionId: null,
        resumeToken: null
      };
    }

    this.updateDailyRefreshItemStates_(this.dailyRefreshInfo_);
    this.decorateCurrentWallpaperInfoBar_();
  };

  WallpaperUtil.getDailyRefreshInfo(
      initializeDailyRefreshStatesImpl.bind(null));
};

/**
 * Updates the UI of all the daily refresh items based on the info.
 * @param {Object} dailyRefreshInfo The daily refresh info.
 * @private
 */
WallpaperManager.prototype.updateDailyRefreshItemStates_ = function(
    dailyRefreshInfo) {
  if (!this.dailyRefreshItemMap_ || !dailyRefreshInfo)
    return;

  Object.entries(this.dailyRefreshItemMap_)
      .forEach(([collectionId, dailyRefreshItem]) => {
        var enabled = dailyRefreshInfo.enabled &&
            dailyRefreshInfo.collectionId === collectionId;
        dailyRefreshItem.classList.toggle('checked', enabled);
        dailyRefreshItem.querySelector('.daily-refresh-slider')
            .setAttribute('aria-checked', enabled);
      });
};

/**
 * Decorates the UI and registers event listener for the item.
 * @param {string} collectionId The collection id that this item is associated
 *     with.
 * @param {Object} dailyRefreshItem The daily refresh item.
 */
WallpaperManager.prototype.decorateDailyRefreshItem = function(
    collectionId, dailyRefreshItem) {
  if (!this.dailyRefreshItemMap_)
    this.dailyRefreshItemMap_ = {};

  this.dailyRefreshItemMap_[collectionId] = dailyRefreshItem;
  this.updateDailyRefreshItemStates_(this.dailyRefreshInfo_);
  dailyRefreshItem.addEventListener('click', () => {
    var isItemEnabled = dailyRefreshItem.classList.contains('checked');
    var isCollectionEnabled =
        collectionId === this.dailyRefreshInfo_.collectionId;
    if (isItemEnabled !== isCollectionEnabled) {
      console.error(
          'There is a mismatch between the enabled daily refresh collection ' +
          'and the item state. This should never happen.');
      return;
    }
    if (isItemEnabled) {
      this.disableDailyRefresh_();
    } else {
      // Enable daily refresh but do not overwrite |dailyRefreshInfo_| yet
      // (since it's still possible to revert). The resume token is left empty
      // for now.
      this.pendingDailyRefreshInfo_ = {
        enabled: true,
        collectionId: collectionId,
        resumeToken: null
      };
      this.setDailyRefreshWallpaper_();
    }
    var toggleRippleAnimation = enabled => {
      dailyRefreshItem.classList.toggle('ripple-animation', enabled);
    };
    toggleRippleAnimation(navigator.onLine);
    window.setTimeout(() => {
      toggleRippleAnimation(false);
    }, 360);
  });
  dailyRefreshItem.addEventListener('keypress', e => {
    if (e.keyCode == 13)
      dailyRefreshItem.click();
  });
  dailyRefreshItem.addEventListener('mousedown', e => {
    e.preventDefault();
  });
  dailyRefreshItem.setAttribute('aria-label', str('surpriseMeLabel'));
};

/**
 * Fetches the image for daily refresh based on |pendingDailyRefreshInfo_|.
 * Either sets it directly or enters preview mode.
 * @private
 */
WallpaperManager.prototype.setDailyRefreshWallpaper_ = function() {
  if (!this.pendingDailyRefreshInfo_)
    return;
  // There should be immediate UI update even though the info hasn't been saved.
  this.updateDailyRefreshItemStates_(this.pendingDailyRefreshInfo_);
  this.updateSpinnerVisibility_(true);

  var retryCount = 0;
  var getDailyRefreshImage = () => {
    chrome.wallpaperPrivate.getSurpriseMeImage(
        this.pendingDailyRefreshInfo_.collectionId,
        this.pendingDailyRefreshInfo_.resumeToken,
        (imageInfo, nextResumeToken) => {
          var failureCallback = () => {
            this.pendingDailyRefreshInfo_ = null;
            // Restore the original states.
            this.updateDailyRefreshItemStates_(this.dailyRefreshInfo_);
            this.updateSpinnerVisibility_(false);
          };
          if (chrome.runtime.lastError) {
            console.error(
                'Error fetching daily refresh wallpaper for collection id: ' +
                this.pendingDailyRefreshInfo_.collectionId);
            failureCallback();
            return;
          }
          // If the randomly selected wallpaper happens to be the current
          // wallpaper, try again.
          if (this.currentWallpaper_.includes(imageInfo['imageUrl']) &&
              retryCount < 5) {
            ++retryCount;
            getDailyRefreshImage();
            return;
          }

          this.pendingDailyRefreshInfo_.resumeToken = nextResumeToken;
          // Find the name of the collection based on its id for display
          // purpose.
          var collectionName;
          for (var i = 0; i < this.collectionsInfo_.length; ++i) {
            if (this.collectionsInfo_[i]['collectionId'] ===
                this.pendingDailyRefreshInfo_.collectionId) {
              collectionName = this.collectionsInfo_[i]['collectionName'];
            }
          }
          var dailyRefreshImageInfo = {
            highResolutionURL:
                imageInfo['imageUrl'] + str('highResolutionSuffix'),
            layout: Constants.WallpaperThumbnailDefaultLayout,
            source: Constants.WallpaperSourceEnum.Daily,
            displayText: imageInfo['displayText'],
            authorWebsite: imageInfo['actionUrl'],
            collectionName: collectionName
          };

          var previewMode = this.shouldPreviewWallpaper_();
          var successCallback = () => {
            this.updateSpinnerVisibility_(false);

            var onWallpaperConfirmed = () => {
              this.dailyRefreshInfo_ = this.pendingDailyRefreshInfo_;
              this.pendingDailyRefreshInfo_ = null;
              this.onWallpaperChanged_(
                  dailyRefreshImageInfo,
                  dailyRefreshImageInfo.highResolutionURL);
              var date = new Date().toDateString();
              WallpaperUtil.saveToLocalStorage(
                  Constants.AccessLastSurpriseWallpaperChangedDate, date,
                  () => {
                    WallpaperUtil.enabledSyncThemesCallback(syncEnabled => {
                      var saveInfo = WallpaperUtil.saveDailyRefreshInfo.bind(
                          WallpaperUtil, this.dailyRefreshInfo_);
                      if (syncEnabled) {
                        WallpaperUtil.saveToSyncStorage(
                            Constants.AccessLastSurpriseWallpaperChangedDate,
                            date, saveInfo);
                      } else {
                        saveInfo();
                      }
                    });
                  });
            };

            if (previewMode) {
              this.setWallpaperAttribution(dailyRefreshImageInfo);
              this.onPreviewModeStarted_(
                  dailyRefreshImageInfo, onWallpaperConfirmed, failureCallback,
                  this.setDailyRefreshWallpaper_.bind(this));
            } else {
              onWallpaperConfirmed();
            }
          };

          this.setSelectedOnlineWallpaper_(
              dailyRefreshImageInfo, successCallback, failureCallback,
              previewMode);
        });
  };

  getDailyRefreshImage();
};

/**
 * Helper function to register event listener for the button.
 * @param {Object} button The button object.
 * @param {function} eventListener The function to be called when the button is
 *     clicked or the Enter key is pressed.
 * @private
 */
WallpaperManager.prototype.addEventToButton_ = function(button, eventListener) {
  // Replace the button with a clone to clear all previous event listeners.
  var newButton = button.cloneNode(true);
  button.parentNode.replaceChild(newButton, button);
  button = newButton;
  button.addEventListener('click', eventListener);
  button.addEventListener('keypress', e => {
    if (e.keyCode == 13)
      button.click();
  });
  button.setAttribute('role', 'button');
  // The button should receive tab focus.
  button.tabIndex = 0;
  // Prevent showing the focused style of the button (e.g. an outline) when
  // it's clicked.
  button.addEventListener('mousedown', e => {
    e.preventDefault();
  });
};

/**
 * Helper function to disable daily refresh on the new wallpaper picker.
 * Discards the current values of collection id and resume token. No-op if it's
 * already disabled.
 * @private
 */
WallpaperManager.prototype.disableDailyRefresh_ = function() {
  if (!this.dailyRefreshInfo_ || !this.dailyRefreshInfo_.enabled)
    return;
  this.dailyRefreshInfo_ = {
    enabled: false,
    collectionId: null,
    resumeToken: null
  };
  WallpaperUtil.saveDailyRefreshInfo(this.dailyRefreshInfo_);
  this.updateDailyRefreshItemStates_(this.dailyRefreshInfo_);
  this.decorateCurrentWallpaperInfoBar_();
};
})();
