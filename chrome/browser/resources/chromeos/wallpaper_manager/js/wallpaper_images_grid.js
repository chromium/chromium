// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('wallpapers', function() {
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var Grid = cr.ui.Grid;
  /** @const */ var GridItem = cr.ui.GridItem;
  /** @const */ var GridSelectionController = cr.ui.GridSelectionController;
  /** @const */ var ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

  /**
   * The number of images that appear in the slideshow of the daily refresh
   * item.
   */
  var DAILY_REFRESH_IMAGES_NUM = 5;

  /**
   * The minimum number of images a collection should contain to enable daily
   * refresh.
   */
  var DAILY_REFRESH_MIN_IMAGES_COUNT = 13;

  /**
   * The following values should be kept in sync with the style sheet.
   */
  var GRID_SIZE_CSS = 160;

  /**
   * Creates a new wallpaper thumbnails grid item.
   * @param {{wallpaperId: number, baseURL: string, layout: string,
   *          source: string, availableOffline: boolean,
   *          opt_dynamicURL: string, opt_author: string,
   *          opt_authorWebsite: string}}
   *     wallpaperInfo Wallpaper data item in WallpaperThumbnailsGrid's data
   *     model.
   * @param {number} dataModelId A unique ID that this item associated to.
   * @param {object} thumbnail The thumbnail image Object associated with this
   *      grid item.
   * @param {function} callback The callback function when decoration finished.
   * @constructor
   * @extends {cr.ui.GridItem}
   */
  function WallpaperThumbnailsGridItem(
      wallpaperInfo, dataModelId, thumbnail, callback) {
    var el = new GridItem(wallpaperInfo);
    el.__proto__ = WallpaperThumbnailsGridItem.prototype;
    el.dataModelId_ = dataModelId;
    el.thumbnail_ = thumbnail;
    el.callback_ = callback;
    return el;
  }

  WallpaperThumbnailsGridItem.prototype = {
    __proto__: GridItem.prototype,

    /**
     * The unique ID this thumbnail grid associated to.
     * @type {number}
     */
    dataModelId_: null,

    /**
     * The thumbnail image associated with the current grid item.
     */
    thumbnail_: null,

    /**
     * Called when the WallpaperThumbnailsGridItem is decorated or failed to
     * decorate. If the decoration contains image, the callback function should
     * be called after image loaded.
     * @type {function}
     */
    callback_: null,

    /** @override */
    decorate: function() {
      GridItem.prototype.decorate.call(this);
      // Removes garbage created by GridItem.
      this.innerText = '';

      if (this.dataItem.isDailyRefreshItem) {
        this.callback_(this.dataModelId_);
        return;
      }

      if (this.thumbnail_) {
        this.appendChild(this.thumbnail_);
        this.callback_(this.dataModelId_, this.dataItem.wallpaperId);
        return;
      }

      var imageEl = document.createElement('img');
      // Do not show the image until |cropImageToFitGrid_| is done.
      imageEl.style.visibility = 'hidden';
      imageEl.setAttribute('aria-hidden', 'true');
      this.setAttribute('aria-label', this.dataItem.ariaLabel);
      this.tabIndex = 0;
      this.addEventListener('keypress', e => {
        if (e.keyCode == 13)
          this.parentNode.selectedItem = this.dataItem;
      });
      this.addEventListener('mousedown', e => {
        e.preventDefault();
      });

      imageEl.classList.add('thumbnail');
      cr.defineProperty(imageEl, 'offline', cr.PropertyKind.BOOL_ATTR);
      imageEl.offline = this.dataItem.availableOffline;
      this.appendChild(imageEl);

      switch (this.dataItem.source) {
        case Constants.WallpaperSourceEnum.Custom:
          this.decorateCustomWallpaper_(imageEl, this.dataItem);
          break;
        case Constants.WallpaperSourceEnum.OEM:
        case Constants.WallpaperSourceEnum.Online:
          this.decorateOnlineOrOEMWallpaper_(imageEl, this.dataItem);
          break;
        case Constants.WallpaperSourceEnum.Daily:
        case Constants.WallpaperSourceEnum.ThirdParty:
        default:
          // It's impossible to manually select a DAILY or THIRDPARTY type
          // wallpaper.
          console.error('Unsupported wallpaper source.');
          // Delay dispatching the completion callback until all items have
          // begun loading and are tracked.
          window.setTimeout(this.callback_.bind(this, this.dataModelId_), 0);
      }
    },

    /**
     * Initializes the grid item for custom wallpapers.
     * @param {Object} imageElement The image element.
     * @param {{filePath: string, baseURL: string, layout: string,
     *          source: string, availableOffline: boolean}
     *     dataItem The info related to the wallpaper image.
     * @private
     */
    decorateCustomWallpaper_(imageElement, dataItem) {
      if (dataItem.source != Constants.WallpaperSourceEnum.Custom) {
        console.error(
            '|decorateCustomWallpaper_| is called but the wallpaper source ' +
            'is not custom.');
        return;
      }
      // Read the image data from |filePath|.
      chrome.wallpaperPrivate.getLocalImageData(
          dataItem.filePath, imageData => {
            if (chrome.runtime.lastError || !imageData) {
              // TODO(crbug.com/810892): Decide the UI: either hide the grid or
              // show an error icon.
              console.error(
                  'Initialization of custom wallpaper grid failed for path ' +
                  dataItem.filePath);
              this.callback_(
                  this.dataModelId_, null /*opt_wallpaperId=*/, imageElement);
              return;
            }

            // |opt_wallpaperId| is used as the key to cache the image data, but
            // we do not want to cache local image data since it may change
            // frequently.
            WallpaperUtil.displayImage(
                imageElement, imageData,
                this.callback_.bind(
                    this, this.dataModelId_, null /*opt_wallpaperId=*/,
                    imageElement));
          });
    },

    /**
     * Initializes the grid item for online or OEM wallpapers.
     * @param {Object} imageElement The image element.
     * @param {{filePath: string, baseURL: string, layout: string,
     *          source: string, availableOffline: boolean}
     *     dataItem The info related to the wallpaper image.
     * @private
     */
    decorateOnlineOrOEMWallpaper_(imageElement, dataItem) {
      if (dataItem.source != Constants.WallpaperSourceEnum.Online &&
          dataItem.source != Constants.WallpaperSourceEnum.OEM) {
        console.error(
            '|decorateOnlineOrOEMWallpaper_| is called but the wallpaper ' +
            'source is not online or OEM.');
        return;
      }
      chrome.wallpaperPrivate
          .getThumbnail(
              dataItem.baseURL, dataItem.source, data => {
                if (data) {
                  WallpaperUtil.displayImage(
                      imageElement, data,
                      this.callback_.bind(
                          this, this.dataModelId_, dataItem.wallpaperId,
                          imageElement));
                } else if (
                    dataItem.source == Constants.WallpaperSourceEnum.Online) {
                  var xhr = new XMLHttpRequest();
                  xhr.open('GET', dataItem.baseURL, true);
                  xhr.responseType = 'arraybuffer';
                  xhr.send(null);
                  xhr.addEventListener('load', e => {
                    if (xhr.status === 200) {
                      chrome.wallpaperPrivate.saveThumbnail(
                          dataItem.baseURL, xhr.response);
                      WallpaperUtil.displayImage(
                          imageElement, xhr.response,
                          this.callback_.bind(
                              this, this.dataModelId_, dataItem.wallpaperId,
                              imageElement));
                    } else {
                      this.callback_(this.dataModelId_);
                    }
                  });
                }
              });
    },
  };

  /**
   * Creates a selection controller that wraps selection on grid ends
   * and translates Enter presses into 'activate' events.
   * @param {cr.ui.ListSelectionModel} selectionModel The selection model to
   *     interact with.
   * @param {cr.ui.Grid} grid The grid to interact with.
   * @constructor
   * @extends {cr.ui.GridSelectionController}
   */
  function WallpaperThumbnailsGridSelectionController(selectionModel, grid) {
    GridSelectionController.call(this, selectionModel, grid);
  }

  WallpaperThumbnailsGridSelectionController.prototype = {
    __proto__: GridSelectionController.prototype,

    /** @override */
    getIndexBefore: function(index) {
      var result =
          GridSelectionController.prototype.getIndexBefore.call(this, index);
      return result == -1 ? this.getLastIndex() : result;
    },

    /** @override */
    getIndexAfter: function(index) {
      var result =
          GridSelectionController.prototype.getIndexAfter.call(this, index);
      return result == -1 ? this.getFirstIndex() : result;
    },

    /** @override */
    handleKeyDown: function(e) {
      if (e.key == 'Enter')
        cr.dispatchSimpleEvent(this.grid_, 'activate');
      else
        GridSelectionController.prototype.handleKeyDown.call(this, e);
    },
  };

  /**
   * Creates a new user images grid element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.Grid}
   */
  var WallpaperThumbnailsGrid = cr.ui.define('grid');

  WallpaperThumbnailsGrid.prototype = {
    __proto__: Grid.prototype,

    /**
     * The checkbox element shown with the currently set wallpaper.
     * @type {Object}
     */
    checkmark_: undefined,

    get checkmark() {
      return this.checkmark_;
    },

    /**
     * The timer of the slideshow of the daily refresh item.
     * @private
     */
    dailyRefreshTimer_: undefined,

    /**
     * The cached list of images that can be used in the slideshow.
     * @private
     */
    dailyRefreshCacheList_: [],

    /**
     * The item in data model which should have a checkmark.
     * @type {{baseURL: string, dynamicURL: string, layout: string,
     *         author: string, authorWebsite: string,
     *         availableOffline: boolean}}
     *     wallpaperInfo The information of the wallpaper to be set active.
     */
    activeItem_: undefined,

    set activeItem(activeItem) {
      if (this.activeItem_ != activeItem) {
        this.activeItem_ = activeItem;
        this.updateActiveThumb_();
      }
    },

    get activeItem() {
      return this.activeItem_;
    },

    /**
     * Whether the daily refresh item is visible.
     * @type {boolean}
     */
    get isShowingDailyRefresh() {
      for (var i = 0; i < this.dataModel.length; ++i) {
        if (this.dataModel.item(i).isDailyRefreshItem)
          return true;
      }
      return false;
    },

    /**
     * The grid item corresponding to daily refresh.
     * @type {Object}
     */
    get dailyRefreshItem() {
      for (var i = 0; i < this.dataModel.length; ++i) {
        if (this.dataModel.item(i).isDailyRefreshItem)
          return this.getListItemByIndex(i);
      }
      return null;
    },

    /**
     * The list of images that are currently in the slideshow.
     * @type {Array<Object>}
     */
    get dailyRefreshImages() {
      return this.dailyRefreshItem ?
          this.dailyRefreshItem.querySelectorAll('.slide-show') :
          {};
    },

    /**
     * The id of the collection that the wallpapers belong to, assuming all the
     * wallpapers have the same id (enfored by the source of the data model).
     * @type {string}
     */
    get collectionId() {
      return this.dataModel.item(this.dataModel.length - 1).collectionId;
    },

    /**
     * A unique ID that assigned to each set dataModel operation. Note that this
     * id wont increase if the new dataModel is null or empty.
     */
    dataModelId_: 0,

    /**
     * The number of items that need to be generated after a new dataModel is
     * set.
     */
    pendingItems_: 0,

    /**
     * Maintains all grid items' thumbnail images for quickly switching between
     * different categories.
     */
    thumbnailList_: undefined,

    /** @override */
    set dataModel(dataModel) {
      if (this.dataModel_ == dataModel)
        return;

      if (dataModel && dataModel.length != 0) {
        this.dataModelId_++;
        // Clears old pending items. The new pending items will be counted when
        // item is constructed in function itemConstructor below.
        this.pendingItems_ = 0;

        // Add a daily refresh item as the first element of the grid if an
        // online collection contains sufficient number of images.
        if (dataModel.length >= DAILY_REFRESH_MIN_IMAGES_COUNT &&
            dataModel.item(0).source == Constants.WallpaperSourceEnum.Online) {
          dataModel.splice(
              0, 0, {isDailyRefreshItem: true, availableOffline: false});
        }
      }

      // eslint-disable-next-line no-restricted-properties
      var parentSetter = cr.ui.Grid.prototype.__lookupSetter__('dataModel');
      parentSetter.call(this, dataModel);
    },

    get dataModel() {
      return this.dataModel_;
    },

    /** @override */
    createSelectionController: function(sm) {
      return new WallpaperThumbnailsGridSelectionController(sm, this);
    },

    /**
     * Crops the image to diplay it in a square grid of size |GRID_SIZE_CSS|, by
     * adjusting its relative position with the outer image grid.
     * @param {object} image The wallpaper image.
     * @private
     */
    cropImageToFitGrid_: function(image) {
      var newHeight;
      var newWidth;
      if (image.offsetWidth == 0 || image.offsetHeight == 0) {
        newHeight = GRID_SIZE_CSS;
        newWidth = GRID_SIZE_CSS;
      } else {
        var ROUND_CORNER_RADIUS_PX = 4;
        var getClipPathString = (top, left, round) => {
          return 'inset(' + top + 'px ' + left + 'px round ' + round + 'px)';
        };
        var aspectRatio = image.offsetWidth / image.offsetHeight;
        if (aspectRatio > 1) {
          newHeight = GRID_SIZE_CSS;
          newWidth = GRID_SIZE_CSS * aspectRatio;
          // The center portion is visible, and the overflow area on the left
          // and right will be hidden.
          var leftDistance = (newWidth - GRID_SIZE_CSS) / 2;
          image.style.left = -leftDistance + 'px';
          image.style.clipPath =
              getClipPathString(0, leftDistance, ROUND_CORNER_RADIUS_PX);
        } else {
          newWidth = GRID_SIZE_CSS;
          newHeight = GRID_SIZE_CSS / aspectRatio;
          // The center portion is visible, and the overflow area on the top and
          // buttom will be hidden.
          var topDistance = (newHeight - GRID_SIZE_CSS) / 2;
          image.style.top = -topDistance + 'px';
          image.style.clipPath =
              getClipPathString(topDistance, 0, ROUND_CORNER_RADIUS_PX);
        }
      }

      image.style.height = newHeight + 'px';
      image.style.width = newWidth + 'px';
      image.style.visibility = 'visible';
    },

    /**
     * Check if new thumbnail grid finished loading. This reduces the count of
     * remaining items to be loaded and when 0, shows the thumbnail grid. Note
     * it does not reduce the count on a previous |dataModelId|.
     * @param {number} dataModelId A unique ID that a thumbnail item is
     *     associated to.
     * @param {number} opt_wallpaperId The unique wallpaper ID that associated
     *     with this thumbnail gird item.
     * @param {object} opt_thumbnail The thumbnail image that associated with
     *     the opt_wallpaperId.
     */
    pendingItemComplete: function(dataModelId, opt_wallpaperId, opt_thumbnail) {
      if (dataModelId != this.dataModelId_)
        return;
      --this.pendingItems_;
      if (opt_wallpaperId && opt_thumbnail)
        this.thumbnailList_[opt_wallpaperId] = opt_thumbnail;

      if (opt_thumbnail)
        this.cropImageToFitGrid_(opt_thumbnail);

      if (this.isShowingDailyRefresh) {
        var dailyRefreshItemReady = this.dailyRefreshItem &&
            this.dailyRefreshImages.length >= DAILY_REFRESH_IMAGES_NUM;
        if (!dailyRefreshItemReady) {
          // The daily refresh item may not be created at this point (e.g. when
          // user starts with the bottom of the list and scrolls upward). This
          // list is used to store the wallpaper ids that will be added to the
          // daily refresh item later.
          if (!this.pendingImageIds_)
            this.pendingImageIds_ = [];

          if (opt_wallpaperId && opt_thumbnail) {
            this.pendingImageIds_.push(opt_wallpaperId);
            // Cache the image thumbnail so that it can be retrieved when the
            // daily refresh item is created, or the next time the user clicks
            // on this collection.
            var slideShowImage = opt_thumbnail.cloneNode(true /*deep=*/);
            this.dailyRefreshCacheList_[opt_wallpaperId] = slideShowImage;
          } else if (
              opt_wallpaperId &&
              opt_wallpaperId in this.dailyRefreshCacheList_) {
            // The thumbnail is expected to exist in the cache when the user
            // clicks on a collection more than once.
            this.pendingImageIds_.push(opt_wallpaperId);
          }

          if (this.dailyRefreshItem) {
            for (var wallpaperId of this.pendingImageIds_) {
              this.addImageToDailyRefreshItem_(
                  this.dailyRefreshCacheList_[wallpaperId]);
            }
            this.pendingImageIds_ = [];
          }
        }
      }

      if (this.pendingItems_ == 0) {
        this.style.visibility = 'visible';
        // Start the slideshow.
        if (this.dailyRefreshItem) {
          window.clearTimeout(this.dailyRefreshTimer_);
          this.showNextImage_(0);
        }
        if (this.classList.contains('image-picker-offline'))
          this.highlightOfflineWallpapers();
      }
    },

    /** @override */
    decorate: function() {
      Grid.prototype.decorate.call(this);
      // checkmark_ needs to be initialized before set data model. Otherwise, we
      // may try to access checkmark before initialization in
      // updateActiveThumb_().
      this.checkmark_ = document.createElement('div');
      this.checkmark_.classList.add('check');
      this.checkmark_.setAttribute(
          'aria-label', loadTimeData.getString('setSuccessfullyMessage'));
      this.checkmark_.tabIndex = 0;
      this.dataModel = new ArrayDataModel([]);
      this.thumbnailList_ = new ArrayDataModel([]);
      var self = this;
      this.itemConstructor = function(value) {
        var dataModelId = self.dataModelId_;
        self.pendingItems_++;
        return WallpaperThumbnailsGridItem(
            value, dataModelId,
            (value.wallpaperId == null) ?
                null :
                self.thumbnailList_[value.wallpaperId],
            self.pendingItemComplete.bind(self));
      };
      this.selectionModel = new ListSingleSelectionModel();
      this.inProgramSelection_ = false;
    },

    /**
     * Should only be queried from the 'change' event listener, true if the
     * change event was triggered by a programmatical selection change.
     * @type {boolean}
     */
    get inProgramSelection() {
      return this.inProgramSelection_;
    },

    /**
     * Set index to the image selected.
     * @type {number} index The index of selected image.
     */
    set selectedItemIndex(index) {
      this.inProgramSelection_ = true;
      this.selectionModel.selectedIndex = index;
      this.inProgramSelection_ = false;
    },

    /**
     * The opacity that the slideshow images should have.
     * @type {number} opacity The desired opacity.
     */
    get slideShowImageOpacity() {
      return this.classList.contains('image-picker-offline') ? 0.5 : 1;
    },

    /**
     * The selected item.
     * @type {!Object} Wallpaper information inserted into the data model.
     */
    get selectedItem() {
      var index = this.selectionModel.selectedIndex;
      return index != -1 ? this.dataModel.item(index) : null;
    },
    set selectedItem(selectedItem) {
      var index = this.dataModel.indexOf(selectedItem);
      this.inProgramSelection_ = true;
      this.selectionModel.leadIndex = index;
      this.selectionModel.selectedIndex = index;
      this.inProgramSelection_ = false;
    },

    /**
     * Shows a checkmark on the active thumbnail and clears previous active one
     * if any. Note if wallpaper was not set successfully, checkmark should not
     * show on that thumbnail.
     */
    updateActiveThumb_: function() {
      var selectedGridItem = this.getListItem(this.activeItem_);

      // Clears previous checkmark.
      var previousSelectedGridItem = this.checkmark_.parentNode;
      if (previousSelectedGridItem) {
        previousSelectedGridItem.selected = false;
        previousSelectedGridItem.removeChild(this.checkmark_);
      }

      if (selectedGridItem) {
        selectedGridItem.selected = true;
        selectedGridItem.appendChild(this.checkmark_);
      }

      WallpaperUtil.saveToLocalStorage(
          Constants.AccessLastUsedImageInfoKey, this.activeItem_);
    },

    /**
     * Cache the thumbnail images so that they can be used in the slideshow of
     * the daily refresh item.
     * @param {Object} image The thumbnail image.
     * @private
     */
    cacheDailyRefreshThumbnailImages_: function(image) {
      // Decide heuristically if the image should be cached. There's no need to
      // cache everything if the list already contains the number of images
      // needed.
      if (this.dailyRefreshCacheList_.length == 0 ||
          Math.random() <
              DAILY_REFRESH_IMAGES_NUM / this.dailyRefreshCacheList_.length) {
        this.dailyRefreshCacheList_.push(image.cloneNode(true /*deep=*/));
      }
    },

    /**
     * Adds an image to the daily refresh slideshow.
     * @param {Object} slideShowImage The image to be used in the slideshow.
     * @private
     */
    addImageToDailyRefreshItem_: function(slideShowImage) {
      this.dailyRefreshItem.classList.add('daily-refresh-item');

      // Add the daily refresh label and toggle.
      if (!this.dailyRefreshItem.querySelector('.daily-refresh-banner')) {
        var dailyRefreshBanner = document.querySelector('.daily-refresh-banner')
                                     .cloneNode(true /*deep=*/);
        this.dailyRefreshItem.appendChild(dailyRefreshBanner);
        wallpaperManager.decorateDailyRefreshItem(
            this.collectionId, this.dailyRefreshItem);
      }

      slideShowImage.style.opacity =
          this.dailyRefreshImages.length == 0 ? this.slideShowImageOpacity : 0;
      slideShowImage.classList.add('slide-show');
      this.dailyRefreshItem.insertBefore(
          slideShowImage, this.dailyRefreshItem.firstChild);
    },

    /**
     * Shows the next image for the daily refresh item and hides the currently
     * visible one.
     * @param {number} index The index of the image to be shown.
     * @private
     */
    showNextImage_: function(index) {
      var images = this.dailyRefreshImages;
      if (images.length <= index)
        return;
      for (var i = 0; i < images.length; ++i) {
        images[i].style.opacity = i === index ? this.slideShowImageOpacity : 0;
      }
      var nextIndex = (index + 1) % images.length;
      this.dailyRefreshTimer_ =
          window.setTimeout(this.showNextImage_.bind(this, nextIndex), 3000);
    },

    /**
     * Highlights the wallpapers that are available offline by greying out all
     * the other wallpapers.
     */
    highlightOfflineWallpapers: function() {
      if (!this.classList.contains('image-picker-offline'))
        return;

      chrome.wallpaperPrivate.getOfflineWallpaperList(list => {
        var offlineWallpaperList = {};
        for (var url of list) {
          offlineWallpaperList[url] = true;
        }

        for (var i = 0; i < this.dataModel.length; ++i) {
          var url = this.dataModel.item(i).baseURL;
          if (!url)
            continue;
          var fileName = url.substring(url.lastIndexOf('/') + 1) +
              loadTimeData.getString('highResolutionSuffix');
          if (this.getListItemByIndex(i) &&
              offlineWallpaperList.hasOwnProperty(encodeURI(fileName))) {
            this.getListItemByIndex(i).querySelector('.thumbnail').offline =
                true;
          }
        }
      });
    },

    /**
     * Redraws the viewport.
     */
    redraw: function() {
      Grid.prototype.redraw.call(this);
      // The active thumbnail maybe deleted in the above redraw(). Sets it again
      // to make sure checkmark shows correctly.
      this.updateActiveThumb_();
      if (this.dataModel) {
        var scrollUp =
            this.cachedScrollTop_ && this.cachedScrollTop_ > this.scrollTop;
        for (var i = 0; i < this.dataModel.length; ++i) {
          const listItem = this.getListItemByIndex(i);
          if (listItem) {
            listItem.classList.toggle(
                'first-row',
                i < this.columns &&
                    (this.firstIndex_ == 0 || i != this.firstIndex_ ||
                     scrollUp));
            listItem.tabIndex = 0;
            listItem.setAttribute(
                'aria-label', this.dataModel.item(i).ariaLabel);
          }
        }
      }
      wallpaperManager.onScrollPositionChanged(this.scrollTop);
      this.cachedScrollTop_ = this.scrollTop;
    }
  };

  return {WallpaperThumbnailsGrid: WallpaperThumbnailsGrid};
});
