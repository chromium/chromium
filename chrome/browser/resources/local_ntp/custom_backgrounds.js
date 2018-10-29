// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


'use strict';

let customBackgrounds = {};

/**
 * The browser embeddedSearch.newTabPage object.
 * @type {Object}
 */
var ntpApiHandle;

/**
 * The different types of events that are logged from the NTP. This enum is
 * used to transfer information from the NTP JavaScript to the renderer and is
 * not used as a UMA enum histogram's logged value.
 * Note: Keep in sync with common/ntp_logging_events.h
 * @enum {number}
 * @const
 */
var BACKGROUND_CUSTOMIZATION_LOG_TYPE = {
  // The 'Chrome backgrounds' menu item was clicked.
  NTP_CUSTOMIZE_CHROME_BACKGROUNDS_CLICKED: 40,
  // The 'Upload an image' menu item was clicked.
  NTP_CUSTOMIZE_LOCAL_IMAGE_CLICKED: 41,
  // The 'Restore default background' menu item was clicked.
  NTP_CUSTOMIZE_RESTORE_BACKGROUND_CLICKED: 42,
  // The attribution link on a customized background image was clicked.
  NTP_CUSTOMIZE_ATTRIBUTION_CLICKED: 43,
  // The 'Restore default shortcuts' menu item was clicked.
  NTP_CUSTOMIZE_RESTORE_SHORTCUTS_CLICKED: 46,
  // A collection was selected in the 'Chrome backgrounds' dialog.
  NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_COLLECTION: 47,
  // An image was selected in the 'Chrome backgrounds' dialog.
  NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_IMAGE: 48,
  // 'Cancel' was clicked in the 'Chrome backgrounds' dialog.
  NTP_CUSTOMIZE_CHROME_BACKGROUND_CANCEL: 49,
  // NOTE: NTP_CUSTOMIZE_CHROME_BACKGROUND_DONE (50) is logged on the backend
  // when the selected image is saved.
  // 'Cancel' was clicked in the 'Upload an image' dialog.
  NTP_CUSTOMIZE_LOCAL_IMAGE_CANCEL: 51,
  // 'Done' was clicked in the 'Upload an image' dialog.
  NTP_CUSTOMIZE_LOCAL_IMAGE_DONE: 52,
};

/**
 * Enum for key codes.
 * @enum {int}
 * @const
 */
customBackgrounds.KEYCODES = {
  BACKSPACE: 8,
  DOWN: 40,
  ENTER: 13,
  ESC: 27,
  LEFT: 37,
  RIGHT: 39,
  SPACE: 32,
  TAB: 9,
  UP: 38,
};

/**
 * Enum for HTML element ids.
 * @enum {string}
 * @const
 */
customBackgrounds.IDS = {
  ATTRIBUTIONS: 'custom-bg-attr',
  BACK: 'bg-sel-back',
  BACK_CIRCLE: 'bg-sel-back-circle',
  CANCEL: 'bg-sel-footer-cancel',
  CUSTOM_LINKS_RESTORE_DEFAULT: 'custom-links-restore-default',
  CUSTOM_LINKS_RESTORE_DEFAULT_TEXT: 'custom-links-restore-default-text',
  DEFAULT_WALLPAPERS: 'edit-bg-default-wallpapers',
  DEFAULT_WALLPAPERS_TEXT: 'edit-bg-default-wallpapers-text',
  DONE: 'bg-sel-footer-done',
  EDIT_BG: 'edit-bg',
  EDIT_BG_DIALOG: 'edit-bg-dialog',
  EDIT_BG_DIVIDER: 'edit-bg-divider',
  EDIT_BG_GEAR: 'edit-bg-gear',
  EDIT_BG_MENU: 'edit-bg-menu',
  MSG_BOX: 'message-box',
  MSG_BOX_MSG: 'message-box-message',
  MSG_BOX_LINK: 'message-box-link',
  MSG_BOX_CONTAINER: 'message-box-container',
  LINK_ICON: 'link-icon',
  MENU: 'bg-sel-menu',
  OPTIONS_TITLE: 'edit-bg-title',
  RESTORE_DEFAULT: 'edit-bg-restore-default',
  RESTORE_DEFAULT_TEXT: 'edit-bg-restore-default-text',
  REFRESH_TEXT: 'bg-sel-refresh-text',
  REFRESH_TOGGLE: 'bg-daily-refresh',
  UPLOAD_IMAGE: 'edit-bg-upload-image',
  UPLOAD_IMAGE_TEXT: 'edit-bg-upload-image-text',
  TILES: 'bg-sel-tiles',
  TITLE: 'bg-sel-title',
};

/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
customBackgrounds.CLASSES = {
  ATTR_1: 'attr1',
  ATTR_2: 'attr2',
  ATTR_LINK: 'attr-link',
  COLLECTION_DIALOG: 'is-col-sel',
  COLLECTION_SELECTED: 'bg-selected',  // Highlight selected tile
  COLLECTION_TILE: 'bg-sel-tile',  // Preview tile for background customization
  COLLECTION_TILE_BG: 'bg-sel-tile-bg',
  COLLECTION_TITLE: 'bg-sel-tile-title',  // Title of a background image
  DONE_AVAILABLE: 'done-available',
  FLOAT_UP: 'float-up',
  HAS_LINK: 'has-link',
  HIDE_MSG_BOX: 'message-box-hide',
  IMAGE_DIALOG: 'is-img-sel',
  OPTION: 'bg-option',
  OPTION_DISABLED: 'bg-option-disabled',  // The menu option is disabled.
  PLUS_ICON: 'plus-icon',
  MOUSE_NAV: 'using-mouse-nav',
  SELECTED_BORDER: 'selected-border',
  SELECTED_CHECK: 'selected-check',
  SELECTED_CIRCLE: 'selected-circle',
  SINGLE_ATTR: 'single-attr'
};

/**
 * Enum for background sources.
 * @enum {int}
 * @const
 */
customBackgrounds.SOURCES = {
  NONE: -1,
  CHROME_BACKGROUNDS: 0,
  GOOGLE_PHOTOS: 1,
  IMAGE_UPLOAD: 2,
};

/**
 * Enum for background option menu entries, in the order they appear in the UI.
 * @enum {int}
 * @const
 */
customBackgrounds.MENU_ENTRIES = {
  CHROME_BACKGROUNDS: 0,
  UPLOAD_IMAGE: 1,
  CUSTOM_LINKS_RESTORE_DEFAULT: 2,
  RESTORE_DEFAULT: 3,
};

customBackgrounds.CUSTOM_BACKGROUND_OVERLAY =
    'linear-gradient(rgba(0, 0, 0, 0), rgba(0, 0, 0, 0.3))';

// These shound match the corresponding values in local_ntp.js, that control the
// mv-notice element.
customBackgrounds.delayedHideNotification = -1;
customBackgrounds.NOTIFICATION_TIMEOUT = 10000;

/* Tile that was selected by the user.
 * @type {HTMLElement}
 */
customBackgrounds.selectedTile = null;

/**
 * Number of rows in the custom background dialog to preload.
 * @type {number}
 * @const
 */
customBackgrounds.ROWS_TO_PRELOAD = 3;

/* Type of collection that is being browsed, needed in order
 * to return from the image dialog.
 * @type {int}
 */
customBackgrounds.dialogCollectionsSource = customBackgrounds.SOURCES.NONE;

/*
 * Called when the error notification should be shown.
 * @type {?Function}
 * @private
 */
customBackgrounds.showErrorNotification;

/*
 * Called when the custom link notification should be hidden.
 * @type {?Function}
 * @private
 */
customBackgrounds.hideCustomLinkNotification;

/**
 * Alias for document.getElementById.
 * @param {string} id The ID of the element to find.
 * @return {HTMLElement} The found element or null if not found.
 */
function $(id) {
  // eslint-disable-next-line no-restricted-properties
  return document.getElementById(id);
}

/**
 * Sets the visibility of the settings menu and individual options depending on
 * their respective features.
 */
customBackgrounds.setMenuVisibility = function() {
  // Hide the settings menu if:
  // - Custom links and custom backgrounds are not enabled.
  if (!configData.isCustomLinksEnabled &&
      !configData.isCustomBackgroundsEnabled) {
    $(customBackgrounds.IDS.EDIT_BG).hidden = true;
    return;
  }

  // Reset all hidden values.
  $(customBackgrounds.IDS.EDIT_BG).hidden = false;
  $(customBackgrounds.IDS.DEFAULT_WALLPAPERS).hidden = false;
  $(customBackgrounds.IDS.UPLOAD_IMAGE).hidden = false;
  $(customBackgrounds.IDS.RESTORE_DEFAULT).hidden = false;
  $(customBackgrounds.IDS.EDIT_BG_DIVIDER).hidden = false;
  $(customBackgrounds.IDS.CUSTOM_LINKS_RESTORE_DEFAULT).hidden = false;

  // Custom backgrounds is disabled or a theme is installed, hide all custom
  // background options.
  if (!configData.isCustomBackgroundsEnabled) {
    $(customBackgrounds.IDS.DEFAULT_WALLPAPERS).hidden = true;
    $(customBackgrounds.IDS.UPLOAD_IMAGE).hidden = true;
    $(customBackgrounds.IDS.RESTORE_DEFAULT).hidden = true;
    $(customBackgrounds.IDS.EDIT_BG_DIVIDER).hidden = true;
  }

  // Custom links is disabled, hide all custom link options.
  if (!configData.isCustomLinksEnabled)
    $(customBackgrounds.IDS.CUSTOM_LINKS_RESTORE_DEFAULT).hidden = true;
};

/**
 * Display custom background image attributions on the page.
 * @param {string} attributionLine1 First line of attribution.
 * @param {string} attributionLine2 Second line of attribution.
 * @param {string} attributionActionUrl Url to learn more about the image.
 */
customBackgrounds.setAttribution = function(
    attributionLine1, attributionLine2, attributionActionUrl) {
  var attributionBox = $(customBackgrounds.IDS.ATTRIBUTIONS);
  var attr1 = document.createElement('div');
  var attr2 = document.createElement('div');
  if (attributionLine1 !== '') {
    // Shouldn't be changed from textContent for security assurances.
    attr1.textContent = attributionLine1;
    attr1.classList.add(customBackgrounds.CLASSES.ATTR_1);
    $(customBackgrounds.IDS.ATTRIBUTIONS).appendChild(attr1);
  }
  if (attributionLine2 !== '') {
    // Shouldn't be changed from textContent for security assurances.
    attr2.textContent = attributionLine2;
    attr2.classList.add(customBackgrounds.CLASSES.ATTR_2);
    attributionBox.appendChild(attr2);
  }
  if (attributionActionUrl !== '') {
    var attr = (attributionLine2 !== '' ? attr2 : attr1);
    attr.classList.add(customBackgrounds.CLASSES.ATTR_LINK);

    var linkIcon = document.createElement('div');
    linkIcon.id = customBackgrounds.IDS.LINK_ICON;
    // Enlarge link-icon when there is only one line of attribution
    if (attributionLine2 === '')
      linkIcon.classList.add(customBackgrounds.CLASSES.SINGLE_ATTR);
    attr.insertBefore(linkIcon, attr.firstChild);

    attributionBox.classList.add(customBackgrounds.CLASSES.ATTR_LINK);
    attributionBox.onclick = function() {
      window.open(attributionActionUrl, '_blank');
      ntpApiHandle.logEvent(
          BACKGROUND_CUSTOMIZATION_LOG_TYPE.NTP_CUSTOMIZE_ATTRIBUTION_CLICKED);
    };
    attributionBox.style.cursor = 'pointer';
  }
};

customBackgrounds.clearAttribution = function() {
  var attributions = $(customBackgrounds.IDS.ATTRIBUTIONS);
  while (attributions.firstChild) {
    attributions.removeChild(attributions.firstChild);
  }
};

customBackgrounds.unselectTile = function() {
  $(customBackgrounds.IDS.DONE).disabled = true;
  customBackgrounds.selectedTile = null;
  $(customBackgrounds.IDS.DONE).tabIndex = -1;
};

/**
 * Remove all collection tiles from the container when the dialog
 * is closed.
 */
customBackgrounds.resetSelectionDialog = function() {
  $(customBackgrounds.IDS.TILES).scrollTop = 0;
  var tileContainer = $(customBackgrounds.IDS.TILES);
  while (tileContainer.firstChild) {
    tileContainer.removeChild(tileContainer.firstChild);
  }
  customBackgrounds.unselectTile();
};

/* Close the collection selection dialog and cleanup the state
 * @param {dialog} menu The dialog to be closed
 */
customBackgrounds.closeCollectionDialog = function(menu) {
  menu.close();
  customBackgrounds.dialogCollectionsSource = customBackgrounds.SOURCES.NONE;
  customBackgrounds.resetSelectionDialog();
};

/* Close and reset the dialog, and set the background.
 * @param {string} url The url of the selected background.
 */
customBackgrounds.setBackground = function(
    url, attributionLine1, attributionLine2, attributionActionUrl) {
  customBackgrounds.closeCollectionDialog($(customBackgrounds.IDS.MENU));
  window.chrome.embeddedSearch.newTabPage.setBackgroundURLWithAttributions(
      url, attributionLine1, attributionLine2, attributionActionUrl);
};

/**
 * Create a tile for a Chrome Backgrounds collection.
 */
customBackgrounds.createChromeBackgroundTile = function(data) {
  let tile = document.createElement('div');
  tile.style.backgroundImage = 'url(' + data.previewImageUrl + ')';
  tile.dataset.id = data.collectionId;
  tile.dataset.name = data.collectionName;
  fadeInImageTile(tile, data.previewImageUrl);
  return tile;
};

/**
 * Create a tile for a Google Photos album.
 */
customBackgrounds.createAlbumTile = function(data) {
  let tile = document.createElement('div');
  tile.style.backgroundImage = 'url(' + data.previewImageUrl + ')';
  tile.dataset.id = data.albumId;
  tile.dataset.name = data.albumName;
  tile.dataset.photoContainerId = data.photoContainerId;
  fadeInImageTile(tile, data.previewImageUrl);
  return tile;
};

customBackgrounds.createAlbumPlusTile = function() {
  var tile = document.createElement('div');
  var plusIcon = document.createElement('div');
  tile.classList.add(customBackgrounds.CLASSES.COLLECTION_TILE);
  plusIcon.classList.add(customBackgrounds.CLASSES.PLUS_ICON);
  tile.appendChild(plusIcon);
  tile.onclick = function() {
    window.open('https://photos.google.com/albums', '_blank');
    customBackgrounds.closeCollectionDialog($(customBackgrounds.IDS.MENU));
  };
  tile.id = 'coll_tile_0';
  return tile;
};

/**
 * Get the number of tiles in a row according to current window width.
 * @return {number} the number of tiles per row
 */
customBackgrounds.getTilesWide = function() {
  // Browser window can only fit two columns. Should match "#bg-sel-menu" width.
  if ($(customBackgrounds.IDS.MENU).offsetWidth < 517) {
    return 2;
  }
  // Browser window can only fit one column. Should match @media (max-width:
  // 356) "#bg-sel-menu" width.
  else if ($(customBackgrounds.IDS.MENU).offsetWidth < 356) {
    return 1;
  }

  return 3;
};

/* Get the next tile when the arrow keys are used to navigate the grid.
 * Returns null if the tile doesn't exist.
 * @param {int} deltaX Change in the x direction.
 * @param {int} deltaY Change in the y direction.
 * @param {string} current Number of the current tile.
 */
customBackgrounds.getNextTile = function(deltaX, deltaY, current) {
  let tilesWide = customBackgrounds.getTilesWide();

  var targetNum = parseInt(current) + deltaX + (deltaY * tilesWide);

  if ($(customBackgrounds.IDS.MENU)
          .classList.contains(customBackgrounds.CLASSES.IMAGE_DIALOG)) {
    return $('img_tile_' + targetNum);
  }
  if ($(customBackgrounds.IDS.MENU)
          .classList.contains(customBackgrounds.CLASSES.COLLECTION_DIALOG)) {
    return $('coll_tile_' + targetNum);
  }
};

/**
 * Show dialog for selecting either a Chrome background collection or Google
 * Photo album. Draw data from either coll or albums.
 * @param {int} collectionsSource The enum value of the source to fetch
 *              collection data from.
 */
customBackgrounds.showCollectionSelectionDialog = function(collectionsSource) {
  var tileContainer = $(customBackgrounds.IDS.TILES);
  var menu = $(customBackgrounds.IDS.MENU);
  var collData = null;
  var sourceIsChromeBackgrounds =
      (collectionsSource == customBackgrounds.SOURCES.CHROME_BACKGROUNDS);
  if (collectionsSource != customBackgrounds.SOURCES.CHROME_BACKGROUNDS &&
      collectionsSource != customBackgrounds.SOURCES.GOOGLE_PHOTOS) {
    console.log(
        'showCollectionSelectionDialog() called with invalid source=' +
        collectionsSource);
    return;
  }
  customBackgrounds.dialogCollectionsSource = collectionsSource;

  if (!menu.open)
    menu.showModal();

  // Create dialog header.
  if (sourceIsChromeBackgrounds) {
    $(customBackgrounds.IDS.TITLE).textContent =
        configData.translatedStrings.selectChromeWallpaper;
    collData = coll;
  } else {
    $(customBackgrounds.IDS.TITLE).textContent =
        configData.translatedStrings.selectGooglePhotoAlbum;
    collData = albums;
    if (albums.length == 0) {
      tileContainer.appendChild(customBackgrounds.createAlbumPlusTile());
    }
  }
  menu.classList.add(customBackgrounds.CLASSES.COLLECTION_DIALOG);
  menu.classList.remove(customBackgrounds.CLASSES.IMAGE_DIALOG);

  // Create dialog tiles.
  for (var i = 0; i < collData.length; ++i) {
    let tileBackground = document.createElement('div');
    tileBackground.classList.add(
        customBackgrounds.CLASSES.COLLECTION_TILE_BG);
    var tile = null;
    if (sourceIsChromeBackgrounds) {
      tile = customBackgrounds.createChromeBackgroundTile(collData[i]);
    } else {
      tile = customBackgrounds.createAlbumTile(collData[i]);
    }
    tile.classList.add(customBackgrounds.CLASSES.COLLECTION_TILE);
    tile.id = 'coll_tile_' + i;
    tile.dataset.tile_num = i;
    tile.tabIndex = -1;
    // Accessibility support for screen readers.
    tile.setAttribute('role', 'button');

    var title = document.createElement('div');
    title.classList.add(customBackgrounds.CLASSES.COLLECTION_TITLE);
    title.textContent = tile.dataset.name;

    var tileInteraction = function(event) {
      var tile = event.target;
      if (tile.classList.contains(customBackgrounds.CLASSES.COLLECTION_TITLE))
        tile = tile.parentNode;

      // Load images for selected collection.
      var imgElement = $('ntp-images-loader');
      if (imgElement) {
        imgElement.parentNode.removeChild(imgElement);
      }
      var imgScript = document.createElement('script');
      imgScript.id = 'ntp-images-loader';

      if (sourceIsChromeBackgrounds) {
        imgScript.src = 'chrome-search://local-ntp/ntp-background-images.js?' +
            'collection_type=background&collection_id=' + tile.dataset.id;
        ntpApiHandle.logEvent(
            BACKGROUND_CUSTOMIZATION_LOG_TYPE
                .NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_COLLECTION);
      } else {
        imgScript.src = 'chrome-search://local-ntp/ntp-background-images.js?' +
            'collection_type=album&album_id=' + tile.dataset.id +
            '&photo_container_id=' + tile.dataset.photoContainerId;
      }

      document.body.appendChild(imgScript);

      imgScript.onload = function() {
        // Verify that the individual image data was successfully loaded.
        var imageDataLoaded = false;
        if (sourceIsChromeBackgrounds) {
          imageDataLoaded =
              (coll_img.length > 0 &&
               coll_img[0].collectionId == tile.dataset.id);
        } else {
          imageDataLoaded =
              (photos.length > 0 && photos[0].albumId == tile.dataset.id &&
               photos[0].photoContainerId == tile.dataset.photoContainerId);
        }

        // Dependent upon the success of the load, populate the image selection
        // dialog or close the current dialog.
        if (imageDataLoaded) {
          customBackgrounds.resetSelectionDialog();
          customBackgrounds.showImageSelectionDialog(tile.dataset.name);
        } else {
          let errors =
              (collectionsSource ==
                       customBackgrounds.SOURCES.CHROME_BACKGROUNDS ?
                   coll_img_errors :
                   photos_errors);
          // If an auth error occurs leave the dialog open and redirect the
          // user to sign-in again. Then they can return to the same place in
          // the customization flow.
          if (!errors.auth_error) {
            customBackgrounds.closeCollectionDialog(menu);
          }
          customBackgrounds.handleError(errors);
        }
      };
    };

    tile.onclick = tileInteraction;
    tile.onkeydown = function(event) {

      if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
        event.preventDefault();
        event.stopPropagation();
        tileInteraction(event);
      }
      // Handle arrow key navigation.
      else if (
          event.keyCode === customBackgrounds.KEYCODES.LEFT ||
          event.keyCode === customBackgrounds.KEYCODES.UP ||
          event.keyCode === customBackgrounds.KEYCODES.RIGHT ||
          event.keyCode === customBackgrounds.KEYCODES.DOWN) {
        event.preventDefault();
        event.stopPropagation();

        let target = null;
        if (event.keyCode === customBackgrounds.KEYCODES.LEFT) {
          target = customBackgrounds.getNextTile(
              document.documentElement.classList.contains('rtl') ? 1 : -1, 0,
              this.dataset.tile_num);
        } else if (event.keyCode === customBackgrounds.KEYCODES.UP) {
          target = customBackgrounds.getNextTile(0, -1, this.dataset.tile_num);
        } else if (event.keyCode === customBackgrounds.KEYCODES.RIGHT) {
          target = customBackgrounds.getNextTile(
              document.documentElement.classList.contains('rtl') ? -1 : 1, 0,
              this.dataset.tile_num);
        } else if (event.keyCode === customBackgrounds.KEYCODES.DOWN) {
          target = customBackgrounds.getNextTile(0, 1, this.dataset.tile_num);
        }
        if (target)
          target.focus();
        else
          this.focus();
      }
    };

    tile.appendChild(title);
    tileBackground.appendChild(tile);
    tileContainer.appendChild(tileBackground);
  }

  $(customBackgrounds.IDS.TILES).focus();
};

/**
 * Apply border and checkmark when a tile is selected
 * @param {div} tile The tile to apply styling to.
 */
customBackgrounds.applySelectedState = function(tile) {
  tile.classList.add(customBackgrounds.CLASSES.COLLECTION_SELECTED);
  var selectedBorder = document.createElement('div');
  var selectedCircle = document.createElement('div');
  var selectedCheck = document.createElement('div');
  selectedBorder.classList.add(customBackgrounds.CLASSES.SELECTED_BORDER);
  selectedCircle.classList.add(customBackgrounds.CLASSES.SELECTED_CIRCLE);
  selectedCheck.classList.add(customBackgrounds.CLASSES.SELECTED_CHECK);
  selectedBorder.appendChild(selectedCircle);
  selectedBorder.appendChild(selectedCheck);
  tile.appendChild(selectedBorder);
  tile.dataset.oldLabel = tile.getAttribute('aria-label');
  tile.setAttribute(
      'aria-label',
      tile.dataset.oldLabel + ' ' + configData.translatedStrings.selectedLabel);
};

/**
 * Remove border and checkmark when a tile is un-selected
 * @param {div} tile The tile to remove styling from.
 */
customBackgrounds.removeSelectedState = function(tile) {
  tile.classList.remove(customBackgrounds.CLASSES.COLLECTION_SELECTED);
  tile.removeChild(tile.firstChild);
  tile.setAttribute('aria-label', tile.dataset.oldLabel);
};

/**
 * Show dialog for selecting an image or toggling on daily refresh. Image
 * data should previous have been loaded into coll_img via
 * chrome-search://local-ntp/ntp-background-images.js?collection_id=<collection_id>
 * @param {string} dialogTitle The title to be displayed at the top of the
 *                 dialog.
 */
customBackgrounds.showImageSelectionDialog = function(dialogTitle) {
  const firstNTile = customBackgrounds.ROWS_TO_PRELOAD
      * customBackgrounds.getTilesWide();
  var menu = $(customBackgrounds.IDS.MENU);
  var tileContainer = $(customBackgrounds.IDS.TILES);
  var sourceIsChromeBackgrounds =
      (customBackgrounds.dialogCollectionsSource ==
       customBackgrounds.SOURCES.CHROME_BACKGROUNDS);

  $(customBackgrounds.IDS.TITLE).textContent = dialogTitle;
  menu.classList.remove(customBackgrounds.CLASSES.COLLECTION_DIALOG);
  menu.classList.add(customBackgrounds.CLASSES.IMAGE_DIALOG);

  var imageData = null;
  if (sourceIsChromeBackgrounds) {
    imageData = coll_img;
  } else {
    imageData = photos;
  }

  let preLoadTiles = [];
  let postLoadTiles = [];

  for (var i = 0; i < imageData.length; ++i) {
    let tileBackground = document.createElement('div');
    tileBackground.classList.add(
        customBackgrounds.CLASSES.COLLECTION_TILE_BG);
    var tile = document.createElement('div');
    tile.classList.add(customBackgrounds.CLASSES.COLLECTION_TILE);
    // Accessibility support for screen readers.
    tile.setAttribute('role', 'button');

    // Set the background image, the name of the source variable differs
    // depending on if it's coming from Chrome Backgrounds or Google Photos.
    if (sourceIsChromeBackgrounds) {
      // TODO(crbug.com/854028): Remove this hardcoded check when wallpaper
      // previews are supported.
      if (imageData[i].collectionId === 'solidcolors') {
        tile.dataset.attributionLine1 = '';
        tile.dataset.attributionLine2 = '';
        tile.dataset.attributionActionUrl = '';
      } else {
        tile.dataset.attributionLine1 =
          (imageData[i].attributions[0] !== undefined ?
               imageData[i].attributions[0] :
               '');
        tile.dataset.attributionLine2 =
          (imageData[i].attributions[1] !== undefined ?
               imageData[i].attributions[1] :
               '');
        tile.dataset.attributionActionUrl = imageData[i].attributionActionUrl;
      }
      tile.setAttribute('aria-label', imageData[i].attributions[0]);
      tile.dataset.url = imageData[i].imageUrl;
    } else {
      tile.style.backgroundImage =
          'url(' + imageData[i].thumbnailPhotoUrl + ')';
      tile.dataset.url = imageData[i].photoUrl;
      tile.dataset.attributionLine1 = '';
      tile.dataset.attributionLine2 = '';
      tile.dataset.attributionActionUrl = '';
      tile.setAttribute('aria-label', configData.translatedStrings.photoLabel);
    }

    tile.id = 'img_tile_' + i;
    tile.dataset.tile_num = i;
    tile.tabIndex = -1;

    // Load the first |ROWS_TO_PRELOAD| rows of tiles.
    if (i < firstNTile)
      preLoadTiles.push(tile);
    else
      postLoadTiles.push(tile);

    let tileInteraction = function(tile) {
      if (customBackgrounds.selectedTile) {
        customBackgrounds.removeSelectedState(customBackgrounds.selectedTile);
        if (customBackgrounds.selectedTile.id === tile.id) {
          customBackgrounds.unselectTile();
          return ;
        }
      }
      customBackgrounds.selectedTile = tile;

      customBackgrounds.applySelectedState(tile);

      $(customBackgrounds.IDS.DONE).tabIndex = 0;

      // Turn toggle off when an image is selected.
      $(customBackgrounds.IDS.REFRESH_TOGGLE).children[0].checked = false;
      $(customBackgrounds.IDS.DONE).disabled = false;
      ntpApiHandle.logEvent(BACKGROUND_CUSTOMIZATION_LOG_TYPE
                                .NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_IMAGE);
    };

    tile.onclick = function(event) {
      let clickCount = event.detail;
      // Control + option + space will fire the onclick event with 0 clickCount.
      if (clickCount <= 1) {
        tileInteraction(this);
      } else if (clickCount === 2 && customBackgrounds.selectedTile === this) {
        customBackgrounds.setBackground(this.dataset.url,
            this.dataset.attributionLine1, this.dataset.attributionLine2,
            this.dataset.attributionActionUrl);
      }
    };
    tile.onkeydown = function(event) {

      if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
        event.preventDefault();
        event.stopPropagation();
        tileInteraction(this);
      }
      // Handle arrow key navigation.
      else if (
          event.keyCode === customBackgrounds.KEYCODES.LEFT ||
          event.keyCode === customBackgrounds.KEYCODES.UP ||
          event.keyCode === customBackgrounds.KEYCODES.RIGHT ||
          event.keyCode === customBackgrounds.KEYCODES.DOWN) {
        event.preventDefault();
        event.stopPropagation();

        let target = null;
        if (event.keyCode == customBackgrounds.KEYCODES.LEFT) {
          target = customBackgrounds.getNextTile(
              document.documentElement.classList.contains('rtl') ? 1 : -1, 0,
              this.dataset.tile_num);
        } else if (event.keyCode == customBackgrounds.KEYCODES.UP) {
          target = customBackgrounds.getNextTile(0, -1, this.dataset.tile_num);
        } else if (event.keyCode == customBackgrounds.KEYCODES.RIGHT) {
          target = customBackgrounds.getNextTile(
              document.documentElement.classList.contains('rtl') ? -1 : 1, 0,
              this.dataset.tile_num);
        } else if (event.keyCode == customBackgrounds.KEYCODES.DOWN) {
          target = customBackgrounds.getNextTile(0, 1, this.dataset.tile_num);
        }
        if (target)
          target.focus();
        else
          this.focus();
      }
    };

    tileBackground.appendChild(tile);
    tileContainer.appendChild(tileBackground);
  }
  let tileGetsLoaded = 0;
  for (let tile of preLoadTiles) {
    loadTile(tile, imageData, () => {
      // After the preloaded tiles finish loading, the rest of the tiles start
      // loading.
      if (++tileGetsLoaded === preLoadTiles.length) {
        postLoadTiles.forEach((tile) => loadTile(tile, imageData));
      }
    });
  }

  $(customBackgrounds.IDS.TILES).focus();
};

/**
 * Add background image src to the tile and add animation for the tile once it
 * successfully loaded.
 * @param {Object} tile the tile that needs to be loaded.
 * @param {object} imageData the source imageData.
 * @param {?Function} countLoad If not null, called after the tile finishes
 * loading.
 */
let loadTile = function(tile, imageData, countLoad) {
  if (imageData[tile.dataset.tile_num].collectionId === 'solidcolors') {
    tile.style.backgroundImage = [customBackgrounds.CUSTOM_BACKGROUND_OVERLAY,
      'url(' + imageData[tile.dataset.tile_num].thumbnailImageUrl + ')'].join(
        ',').trim();
  } else {
    tile.style.backgroundImage = 'url('
        + imageData[tile.dataset.tile_num].thumbnailImageUrl + ')' || 'url('
        + imageData[tile.dataset.tile_num].thumbnailPhotoUrl + ')';
  }
  fadeInImageTile(tile, imageData[tile.dataset.tile_num].thumbnailImageUrl
      || imageData[tile.dataset.tile_num].thumbnailPhotoUrl, countLoad);
};

/**
 * Fade in effect for both collection and image tile. Once the image
 * successfully loads, we can assume the background image with the same source
 * has also loaded. Then, we set opacity for the tile to start the animation.
 * @param {Object} tile The tile to add the fade in animation to.
 * @param {string} imageUrl the image url for the tile
 * @param {?Function} countLoad If not null, called after the tile finishes
 * loading.
 */
let fadeInImageTile = function(tile, imageUrl, countLoad) {
  let image = new Image();
  image.onload = () => {
    tile.style.opacity = '1';
    if (countLoad) {
      countLoad();
    }
  };
  image.src = imageUrl;
};

/**
 * Load the NTPBackgroundCollections script. It'll create a global
 * variable name "coll" which is a dict of background collections data.
 * @private
 */
customBackgrounds.loadChromeBackgrounds = function() {
  var collElement = $('ntp-collection-loader');
  if (collElement) {
    collElement.parentNode.removeChild(collElement);
  }
  var collScript = document.createElement('script');
  collScript.id = 'ntp-collection-loader';
  collScript.src = 'chrome-search://local-ntp/ntp-background-collections.js?' +
      'collection_type=background';
  document.body.appendChild(collScript);
};

/**
 * Load the NTPGooglePhotoAlbums script. It'll create a global
 * variable name "albums" which is a dict of album data.
 * @private
 */
customBackgrounds.loadGooglePhotosAlbums = function() {
  var albumElement = $('ntp-album-loader');
  if (albumElement) {
    albumElement.parentNode.removeChild(albumElement);
  }
  var albumScript = document.createElement('script');
  albumScript.id = 'ntp-album-loader';
  albumScript.src = 'chrome-search://local-ntp/ntp-background-collections.js?' +
      'collection_type=album';
  document.body.appendChild(albumScript);
};

/* Close dialog when an image is selected via the file picker. */
customBackgrounds.closeCustomizationDialog = function() {
  $(customBackgrounds.IDS.EDIT_BG_DIALOG).close();
};

/*
 * Get the next visible option. There are times when various combinations of
 * options are hidden.
 * @param {int} current_index Index of the option the key press occurred on.
 * @param {int} deltaY Direction to search in, -1 for up, 1 for down.
 */
customBackgrounds.getNextOption = function(current_index, deltaY) {
  // Create array corresponding to the menu. Important that this is in the same
  // order as the MENU_ENTRIES enum, so we can index into it.
  var entries = [];
  entries.push($(customBackgrounds.IDS.DEFAULT_WALLPAPERS));
  entries.push($(customBackgrounds.IDS.UPLOAD_IMAGE));
  entries.push($(customBackgrounds.IDS.CUSTOM_LINKS_RESTORE_DEFAULT));
  entries.push($(customBackgrounds.IDS.RESTORE_DEFAULT));

  var idx = current_index;
  do {
    idx = idx + deltaY;
    if (idx === -1)
      idx = 3;
    if (idx === 4)
      idx = 0;
  } while (idx !== current_index && (entries[idx].hidden ||
           entries[idx].classList.contains(
               customBackgrounds.CLASSES.OPTION_DISABLED)));
  return entries[idx];
};

/* Hide custom background options based on the network state
 * @param {bool} online The current state of the network
 */
customBackgrounds.networkStateChanged = function(online) {
  $(customBackgrounds.IDS.DEFAULT_WALLPAPERS).hidden = !online;
};

/**
 * Initialize the settings menu, custom backgrounds dialogs, and custom links
 * menu items. Set the text and event handlers for the various elements.
 * @param {!Function} showErrorNotification Called when the error notification
 *                    should be displayed.
 * @param {!Function} hideCustomLinkNotification Called when the custom link
 *                    notification should be hidden.
 */
customBackgrounds.init = function(
    showErrorNotification, hideCustomLinkNotification) {
  ntpApiHandle = window.chrome.embeddedSearch.newTabPage;
  let editDialog = $(customBackgrounds.IDS.EDIT_BG_DIALOG);
  let menu = $(customBackgrounds.IDS.MENU);

  $(customBackgrounds.IDS.OPTIONS_TITLE).textContent =
      configData.translatedStrings.customizeBackground;

  $(customBackgrounds.IDS.EDIT_BG_GEAR)
      .setAttribute(
          'aria-label', configData.translatedStrings.customizeThisPage);

  $(customBackgrounds.IDS.EDIT_BG_GEAR)
      .setAttribute('title', configData.translatedStrings.customizeBackground);

  // Edit gear icon interaction events.
  let editBackgroundInteraction = function() {
    editDialog.showModal();
  };
  $(customBackgrounds.IDS.EDIT_BG).onclick = function(event) {
    editDialog.classList.add(customBackgrounds.CLASSES.MOUSE_NAV);
    editBackgroundInteraction();
  };

  // Find the first menu option that is not hidden or disabled.
  let findFirstMenuOption = () => {
    let editMenu = $(customBackgrounds.IDS.EDIT_BG_MENU);
    for (let i = 1; i < editMenu.children.length; i++) {
      let option = editMenu.children[i];
      if (option.classList.contains(customBackgrounds.CLASSES.OPTION)
          && !option.hidden && !option.classList.contains(
              customBackgrounds.CLASSES.OPTION_DISABLED)) {
        option.focus();
        return;
      }
    }
  };

  $(customBackgrounds.IDS.EDIT_BG).onkeydown = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ENTER ||
        event.keyCode === customBackgrounds.KEYCODES.SPACE) {
      // no default behavior for ENTER
      event.preventDefault();
      editDialog.classList.remove(customBackgrounds.CLASSES.MOUSE_NAV);
      editBackgroundInteraction();
      findFirstMenuOption();
    }
  };

  // Handle focus state for the gear icon.
  $(customBackgrounds.IDS.EDIT_BG).onmousedown = function() {
    $(customBackgrounds.IDS.EDIT_BG)
        .classList.add(customBackgrounds.CLASSES.MOUSE_NAV);
  };

  // Interactions to close the customization option dialog.
  let editDialogInteraction = function() {
    editDialog.close();
  };
  editDialog.onclick = function(event) {
    editDialog.classList.add(customBackgrounds.CLASSES.MOUSE_NAV);
    if (event.target === editDialog)
      editDialogInteraction();
  };
  editDialog.onkeydown = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ESC) {
      editDialogInteraction();
    }
    // When using tab in mouse navigation mode, select the first option available.
    else if (editDialog.classList.contains(customBackgrounds.CLASSES.MOUSE_NAV)
        && (event.keyCode === customBackgrounds.KEYCODES.TAB || event.keyCode
            === customBackgrounds.KEYCODES.UP || event.keyCode
            === customBackgrounds.KEYCODES.DOWN)) {
      event.preventDefault();
      findFirstMenuOption();
      editDialog.classList.remove(customBackgrounds.CLASSES.MOUSE_NAV);
    }
    // If keyboard navigation is attempted, remove mouse-only mode.
    else if (event.keyCode === customBackgrounds.KEYCODES.TAB) {
      editDialog.classList.remove(customBackgrounds.CLASSES.MOUSE_NAV);
    }
    else if(
        event.keyCode === customBackgrounds.KEYCODES.LEFT ||
        event.keyCode === customBackgrounds.KEYCODES.UP ||
        event.keyCode === customBackgrounds.KEYCODES.RIGHT ||
        event.keyCode === customBackgrounds.KEYCODES.DOWN) {
      event.preventDefault();
      editDialog.classList.remove(customBackgrounds.CLASSES.MOUSE_NAV);
    }
  };

  if (configData.isCustomLinksEnabled)
    customBackgrounds.initCustomLinksItems(hideCustomLinkNotification);
  if (configData.isCustomBackgroundsEnabled)
    customBackgrounds.initCustomBackgrounds(showErrorNotification);
};

/**
 * Initialize custom link items in the settings menu dialog. Set the text
 * and event handlers for the various elements.
 * @param {!Function} hideCustomLinkNotification Called when the custom link
 *                    notification should be hidden.
 */
customBackgrounds.initCustomLinksItems = function(hideCustomLinkNotification) {
  customBackgrounds.hideCustomLinkNotification = hideCustomLinkNotification;

  let editDialog = $(customBackgrounds.IDS.EDIT_BG_DIALOG);
  let menu = $(customBackgrounds.IDS.MENU);

  $(customBackgrounds.IDS.CUSTOM_LINKS_RESTORE_DEFAULT_TEXT).textContent =
      configData.translatedStrings.restoreDefaultLinks;

  // Interactions with the "Restore default shortcuts" option.
  let customLinksRestoreDefaultInteraction = function() {
    editDialog.close();
    customBackgrounds.hideCustomLinkNotification();
    window.chrome.embeddedSearch.newTabPage.resetCustomLinks();
    ntpApiHandle.logEvent(BACKGROUND_CUSTOMIZATION_LOG_TYPE
                              .NTP_CUSTOMIZE_RESTORE_SHORTCUTS_CLICKED);
  };
  $(customBackgrounds.IDS.CUSTOM_LINKS_RESTORE_DEFAULT).onclick = () => {
    if (!$(customBackgrounds.IDS.CUSTOM_LINKS_RESTORE_DEFAULT).classList.
        contains(customBackgrounds.CLASSES.OPTION_DISABLED)) {
      customLinksRestoreDefaultInteraction();
    }
  };
  $(customBackgrounds.IDS.CUSTOM_LINKS_RESTORE_DEFAULT).onkeydown = function(
      event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
      customLinksRestoreDefaultInteraction(event);
    }
    // Handle arrow key navigation.
    else if (event.keyCode === customBackgrounds.KEYCODES.UP) {
      event.preventDefault();
      customBackgrounds
          .getNextOption(
              customBackgrounds.MENU_ENTRIES.CUSTOM_LINKS_RESTORE_DEFAULT, -1)
          .focus();
    } else if (event.keyCode === customBackgrounds.KEYCODES.DOWN) {
      event.preventDefault();
      customBackgrounds
          .getNextOption(
              customBackgrounds.MENU_ENTRIES.CUSTOM_LINKS_RESTORE_DEFAULT, 1)
          .focus();
    }
  };
};

/**
 * Initialize the settings menu and custom backgrounds dialogs. Set the
 * text and event handlers for the various elements.
 * @param {!Function} showErrorNotification Called when the error notification
 *                    should be displayed.
 */
customBackgrounds.initCustomBackgrounds = function(showErrorNotification) {
  customBackgrounds.showErrorNotification = showErrorNotification;

  var editDialog = $(customBackgrounds.IDS.EDIT_BG_DIALOG);
  var menu = $(customBackgrounds.IDS.MENU);

  $(customBackgrounds.IDS.DEFAULT_WALLPAPERS_TEXT).textContent =
      configData.translatedStrings.defaultWallpapers;
  $(customBackgrounds.IDS.UPLOAD_IMAGE_TEXT).textContent =
      configData.translatedStrings.uploadImage;
  $(customBackgrounds.IDS.RESTORE_DEFAULT_TEXT).textContent =
      configData.translatedStrings.restoreDefaultBackground;
  $(customBackgrounds.IDS.REFRESH_TEXT).textContent =
      configData.translatedStrings.dailyRefresh;
  $(customBackgrounds.IDS.DONE).textContent =
      configData.translatedStrings.selectionDone;
  $(customBackgrounds.IDS.CANCEL).textContent =
      configData.translatedStrings.selectionCancel;

  window.addEventListener('online', function(event) {
    customBackgrounds.networkStateChanged(true);
  });

  window.addEventListener('offline', function(event) {
    customBackgrounds.networkStateChanged(false);
  });

  if (!window.navigator.onLine) {
    customBackgrounds.networkStateChanged(false);
  }

  $(customBackgrounds.IDS.BACK_CIRCLE)
      .setAttribute('aria-label', configData.translatedStrings.backLabel);
  $(customBackgrounds.IDS.CANCEL)
      .setAttribute('aria-label', configData.translatedStrings.selectionCancel);
  $(customBackgrounds.IDS.DONE)
      .setAttribute('aria-label', configData.translatedStrings.selectionDone);

  $(customBackgrounds.IDS.DONE).disabled = true;

  // Interactions with the "Upload an image" option.
  var uploadImageInteraction = function(event) {
    window.chrome.embeddedSearch.newTabPage.selectLocalBackgroundImage();
    ntpApiHandle.logEvent(
        BACKGROUND_CUSTOMIZATION_LOG_TYPE.NTP_CUSTOMIZE_LOCAL_IMAGE_CLICKED);
  };

  $(customBackgrounds.IDS.UPLOAD_IMAGE).onclick = () => {
    if (!$(customBackgrounds.IDS.UPLOAD_IMAGE).classList.contains(
        customBackgrounds.CLASSES.OPTION_DISABLED)) {
      uploadImageInteraction();
    }
  } ;
  $(customBackgrounds.IDS.UPLOAD_IMAGE).onkeydown = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
      uploadImageInteraction(event);
    }

    // Handle arrow key navigation.
    if (event.keyCode === customBackgrounds.KEYCODES.UP) {
      event.preventDefault();
      customBackgrounds
          .getNextOption(customBackgrounds.MENU_ENTRIES.UPLOAD_IMAGE, -1)
          .focus();
    }
    if (event.keyCode === customBackgrounds.KEYCODES.DOWN) {
      event.preventDefault();
      customBackgrounds
          .getNextOption(customBackgrounds.MENU_ENTRIES.UPLOAD_IMAGE, 1)
          .focus();
    }
  };

  // Interactions with the "Restore default background" option.
  var restoreDefaultInteraction = function(event) {
    editDialog.close();
    customBackgrounds.clearAttribution();
    window.chrome.embeddedSearch.newTabPage.setBackgroundURL('');
    ntpApiHandle.logEvent(BACKGROUND_CUSTOMIZATION_LOG_TYPE
                              .NTP_CUSTOMIZE_RESTORE_BACKGROUND_CLICKED);
  };
  $(customBackgrounds.IDS.RESTORE_DEFAULT).onclick = () => {
    if (!$(customBackgrounds.IDS.RESTORE_DEFAULT).classList.contains(
        customBackgrounds.CLASSES.OPTION_DISABLED)) {
      restoreDefaultInteraction();
    }
  };
  $(customBackgrounds.IDS.RESTORE_DEFAULT).onkeydown = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
      restoreDefaultInteraction(event);
    }

    // Handle arrow key navigation.
    if (event.keyCode === customBackgrounds.KEYCODES.UP) {
      event.preventDefault();
      customBackgrounds
          .getNextOption(customBackgrounds.MENU_ENTRIES.RESTORE_DEFAULT, -1)
          .focus();
    }
    if (event.keyCode === customBackgrounds.KEYCODES.DOWN) {
      event.preventDefault();
      customBackgrounds
          .getNextOption(customBackgrounds.MENU_ENTRIES.RESTORE_DEFAULT, 1)
          .focus();
    }
  };

  // Interactions with the "Chrome backgrounds" option.
  var defaultWallpapersInteraction = function(event) {
    customBackgrounds.loadChromeBackgrounds();
    $('ntp-collection-loader').onload = function() {
      editDialog.close();
      if (typeof coll != 'undefined' && coll.length > 0) {
        customBackgrounds.showCollectionSelectionDialog(
            customBackgrounds.SOURCES.CHROME_BACKGROUNDS);
      } else {
        customBackgrounds.handleError(coll_errors);
      }
    };
    ntpApiHandle.logEvent(BACKGROUND_CUSTOMIZATION_LOG_TYPE
                              .NTP_CUSTOMIZE_CHROME_BACKGROUNDS_CLICKED);
  };
  $(customBackgrounds.IDS.DEFAULT_WALLPAPERS).onclick = function() {
    $(customBackgrounds.IDS.MENU)
        .classList.add(customBackgrounds.CLASSES.MOUSE_NAV);
    defaultWallpapersInteraction(event);
  };
  $(customBackgrounds.IDS.DEFAULT_WALLPAPERS).onkeydown = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
      $(customBackgrounds.IDS.MENU)
          .classList.remove(customBackgrounds.CLASSES.MOUSE_NAV);
      defaultWallpapersInteraction(event);
    }

    // Handle arrow key navigation.
    if (event.keyCode === customBackgrounds.KEYCODES.UP) {
      event.preventDefault();
      customBackgrounds
          .getNextOption(customBackgrounds.MENU_ENTRIES.CHROME_BACKGROUNDS, -1)
          .focus();
    }
    if (event.keyCode === customBackgrounds.KEYCODES.DOWN) {
      event.preventDefault();
      customBackgrounds
          .getNextOption(customBackgrounds.MENU_ENTRIES.CHROME_BACKGROUNDS, 1)
          .focus();
    }
  };

  // Escape and Backspace handling for the background picker dialog.
  menu.onkeydown = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ESC ||
        event.keyCode === customBackgrounds.KEYCODES.BACKSPACE) {
      event.preventDefault();
      event.stopPropagation();
      if (menu.classList.contains(
              customBackgrounds.CLASSES.COLLECTION_DIALOG)) {
        menu.close();
        customBackgrounds.resetSelectionDialog();
      } else {
        customBackgrounds.resetSelectionDialog();
        customBackgrounds.showCollectionSelectionDialog(
            customBackgrounds.dialogCollectionsSource);
      }
    }

    // If keyboard navigation is attempted, remove mouse-only mode.
    if (event.keyCode === customBackgrounds.KEYCODES.TAB ||
        event.keyCode === customBackgrounds.KEYCODES.LEFT ||
        event.keyCode === customBackgrounds.KEYCODES.UP ||
        event.keyCode === customBackgrounds.KEYCODES.RIGHT ||
        event.keyCode === customBackgrounds.KEYCODES.DOWN) {
      menu.classList.remove(customBackgrounds.CLASSES.MOUSE_NAV);
    }
  };

  // Interactions with the back arrow on the image selection dialog.
  var backInteraction = function(event) {
    customBackgrounds.resetSelectionDialog();
    customBackgrounds.showCollectionSelectionDialog(
        customBackgrounds.dialogCollectionsSource);
  };
  $(customBackgrounds.IDS.BACK_CIRCLE).onclick = backInteraction;
  $(customBackgrounds.IDS.BACK_CIRCLE).onkeyup = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ENTER ||
        event.keyCode === customBackgrounds.KEYCODES.SPACE) {
      backInteraction(event);
    }
  };

  // Interactions with the cancel button on the background picker dialog.
  $(customBackgrounds.IDS.CANCEL).onclick = function(event) {
    customBackgrounds.closeCollectionDialog(menu);
    ntpApiHandle.logEvent(BACKGROUND_CUSTOMIZATION_LOG_TYPE
                              .NTP_CUSTOMIZE_CHROME_BACKGROUND_CANCEL);
  };
  $(customBackgrounds.IDS.CANCEL).onkeyup = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
      customBackgrounds.closeCollectionDialog(menu);
      ntpApiHandle.logEvent(BACKGROUND_CUSTOMIZATION_LOG_TYPE
                                .NTP_CUSTOMIZE_CHROME_BACKGROUND_CANCEL);
    }
  };

  // Interactions with the done button on the background picker dialog.
  var doneInteraction = function(event) {
    if ($(customBackgrounds.IDS.DONE).disabled) {
      return;
    }
    customBackgrounds.setBackground(
        customBackgrounds.selectedTile.dataset.url,
        customBackgrounds.selectedTile.dataset.attributionLine1,
        customBackgrounds.selectedTile.dataset.attributionLine2,
        customBackgrounds.selectedTile.dataset.attributionActionUrl);
  };
  $(customBackgrounds.IDS.DONE).onclick = doneInteraction;
  $(customBackgrounds.IDS.DONE).onkeyup = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
      doneInteraction(event);
    }
  };

  // Interactions with the "Daily refresh" toggle.
  $(customBackgrounds.IDS.REFRESH_TOGGLE).onclick = function(event) {
    if (customBackgrounds.selectedTile) {
      customBackgrounds.removeSelectedState(customBackgrounds.selectedTile);
      customBackgrounds.selectedTile = null;
    }
    $(customBackgrounds.IDS.DONE).disabled = false;
  };

  // On any arrow key event in the tiles area, focus the first tile.
  $(customBackgrounds.IDS.TILES).onkeydown = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.LEFT ||
        event.keyCode === customBackgrounds.KEYCODES.UP ||
        event.keyCode === customBackgrounds.KEYCODES.RIGHT ||
        event.keyCode === customBackgrounds.KEYCODES.DOWN) {
      event.preventDefault();
      if ($(customBackgrounds.IDS.MENU)
              .classList.contains(
                  customBackgrounds.CLASSES.COLLECTION_DIALOG)) {
        $('coll_tile_0').focus();
      } else {
        $('img_tile_0').focus();
      }
    }
  };
};

customBackgrounds.handleError = function(errors) {
  var unavailableString = configData.translatedStrings.backgroundsUnavailable;

  if (errors != 'undefined') {
    // Network errors.
    if (errors.net_error) {
      if (errors.net_error_no != 0) {
        let onClick = () => {
          window.open(
              'https://chrome://network-error/' + errors.net_error_no,
              '_blank');
        };
        customBackgrounds.showErrorNotification(
            configData.translatedStrings.connectionError,
            configData.translatedStrings.moreInfo, onClick);
      } else {
        customBackgrounds.showErrorNotification(
            configData.translatedStrings.connectionErrorNoPeriod);
      }
    } else if (errors.auth_error) {  // Auth errors (Google Photos only).
      window.open('https://photos.google.com/login', '_blank');
    } else if (errors.service_error) {  // Service errors.
      customBackgrounds.showErrorNotification(unavailableString);
    }
    return;
  }

  // Generic error when we can't tell what went wrong.
  customBackgrounds.showErrorNotification(unavailableString);
};
