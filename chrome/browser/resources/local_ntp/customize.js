/**
 * @license
 * Copyright 2018 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 * SPDX-License-Identifier: BSD-3-Clause
 */

'use strict';

// TODO(crbug.com/937570): After the RP launches this should be renamed to
// customizationMenu along with the file, and large parts can be
// refactored/removed.
const customize = {};

/**
 * The browser embeddedSearch.newTabPage object.
 * @type {Object}
 */
let ntpApiHandle;

/**
 * The different types of events that are logged from the NTP. This enum is
 * used to transfer information from the NTP JavaScript to the renderer and is
 * not used as a UMA enum histogram's logged value.
 * Note: Keep in sync with common/ntp_logging_events.h
 * @enum {number}
 * @const
 */
customize.LOG_TYPE = {
  // The 'Chrome backgrounds' menu item was clicked.
  NTP_CUSTOMIZE_CHROME_BACKGROUNDS_CLICKED: 40,
  // The 'Upload an image' menu item was clicked.
  NTP_CUSTOMIZE_LOCAL_IMAGE_CLICKED: 41,
  // The 'Restore default background' menu item was clicked.
  // NOTE: NTP_CUSTOMIZE_RESTORE_BACKGROUND_CLICKED (42) is logged on the
  // backend.
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
  // NOTE: NTP_CUSTOMIZE_CHROME_BACKGROUND_DONE (50) is logged on the backend.
  // The richer picker was opened.
  NTP_CUSTOMIZATION_MENU_OPENED: 64,
  // 'Cancel' was clicked in the richer picker.
  NTP_CUSTOMIZATION_MENU_CANCEL: 65,
  // 'Done' was clicked in the richer picker.
  NTP_CUSTOMIZATION_MENU_DONE: 66,
  // 'Upload from device' selected in the richer picker.
  NTP_BACKGROUND_UPLOAD_FROM_DEVICE: 67,
  // A collection tile was selected in the richer picker.
  NTP_BACKGROUND_OPEN_COLLECTION: 68,
  // A image tile was selected in the richer picker.
  NTP_BACKGROUND_SELECT_IMAGE: 69,
  // The back arrow was clicked in the richer picker.
  NTP_BACKGROUND_BACK_CLICK: 72,
  // The 'No background' tile was selected in the richer picker.
  NTP_BACKGROUND_DEFAULT_SELECTED: 73,
  // The custom links option in the shortcuts submenu was clicked.
  NTP_CUSTOMIZE_SHORTCUT_CUSTOM_LINKS_CLICKED: 78,
  // The Most Visited option in the shortcuts submenu was clicked.
  NTP_CUSTOMIZE_SHORTCUT_MOST_VISITED_CLICKED: 79,
  // The visibility toggle in the shortcuts submenu was clicked.
  NTP_CUSTOMIZE_SHORTCUT_VISIBILITY_TOGGLE_CLICKED: 80,
  // The 'refresh daily' toggle was licked in the richer picker.
  NTP_BACKGROUND_REFRESH_TOGGLE_CLICKED: 81,
};

/**
 * Enum for key codes.
 * @enum {number}
 * @const
 */
customize.KEYCODES = {
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
 * Array for keycodes corresponding to arrow keys.
 * @type Array
 * @const
 */
customize.arrowKeys = [/*Left*/ 37, /*Up*/ 38, /*Right*/ 39, /*Down*/ 40];

/**
 * Enum for HTML element ids.
 * @enum {string}
 * @const
 */
customize.IDS = {
  ATTR1: 'attr1',
  ATTR2: 'attr2',
  ATTRIBUTIONS: 'custom-bg-attr',
  BACK_CIRCLE: 'bg-sel-back-circle',
  BACKGROUNDS_BUTTON: 'backgrounds-button',
  BACKGROUNDS_DEFAULT: 'backgrounds-default',
  BACKGROUNDS_DEFAULT_ICON: 'backgrounds-default-icon',
  BACKGROUNDS_DISABLED_MENU: 'backgrounds-disabled-menu',
  BACKGROUNDS_IMAGE_MENU: 'backgrounds-image-menu',
  BACKGROUNDS_MENU: 'backgrounds-menu',
  BACKGROUNDS_UPLOAD: 'backgrounds-upload',
  BACKGROUNDS_UPLOAD_ICON: 'backgrounds-upload-icon',
  CANCEL: 'bg-sel-footer-cancel',
  COLOR_PICKER: 'color-picker',
  COLOR_PICKER_TILE: 'color-picker-tile',
  COLOR_PICKER_CONTAINER: 'color-picker-container',
  COLOR_PICKER_ICON: 'color-picker-icon',
  COLORS_BUTTON: 'colors-button',
  COLORS_DEFAULT_ICON: 'colors-default-icon',
  COLORS_THEME: 'colors-theme',
  COLORS_THEME_NAME: 'colors-theme-name',
  COLORS_THEME_UNINSTALL: 'colors-theme-uninstall',
  COLORS_THEME_WEBSTORE_LINK: 'colors-theme-link',
  COLORS_MENU: 'colors-menu',
  CUSTOMIZATION_MENU: 'customization-menu',
  CUSTOM_BG: 'custom-bg',
  CUSTOM_BG_PREVIEW: 'custom-bg-preview',
  CUSTOM_LINKS_RESTORE_DEFAULT: 'custom-links-restore-default',
  CUSTOM_LINKS_RESTORE_DEFAULT_TEXT: 'custom-links-restore-default-text',
  DEFAULT_WALLPAPERS: 'edit-bg-default-wallpapers',
  DEFAULT_WALLPAPERS_TEXT: 'edit-bg-default-wallpapers-text',
  DONE: 'bg-sel-footer-done',
  EDIT_BG: 'edit-bg',
  EDIT_BG_DIALOG: 'edit-bg-dialog',
  EDIT_BG_DIVIDER: 'edit-bg-divider',
  EDIT_BG_MENU: 'edit-bg-menu',
  MENU_BACK_CIRCLE: 'menu-back-circle',
  MENU_CANCEL: 'menu-cancel',
  MENU_DONE: 'menu-done',
  MENU_TITLE: 'menu-title',
  LINK_ICON: 'link-icon',
  MENU: 'bg-sel-menu',
  OPTIONS_TITLE: 'edit-bg-title',
  REFRESH_DAILY_WRAPPER: 'refresh-daily-wrapper',
  REFRESH_TOGGLE: 'refresh-daily-toggle',
  RESTORE_DEFAULT: 'edit-bg-restore-default',
  RESTORE_DEFAULT_TEXT: 'edit-bg-restore-default-text',
  SHORTCUTS_BUTTON: 'shortcuts-button',
  SHORTCUTS_HIDE: 'sh-hide',
  SHORTCUTS_HIDE_TOGGLE: 'sh-hide-toggle',
  SHORTCUTS_MENU: 'shortcuts-menu',
  SHORTCUTS_OPTION_CUSTOM_LINKS: 'sh-option-cl',
  SHORTCUTS_OPTION_MOST_VISITED: 'sh-option-mv',
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
customize.CLASSES = {
  ATTR_SMALL: 'attr-small',
  ATTR_LINK: 'attr-link',
  COLLECTION_DIALOG: 'is-col-sel',
  COLLECTION_SELECTED: 'bg-selected',  // Highlight selected tile
  COLLECTION_TILE: 'bg-sel-tile',  // Preview tile for background customization
  COLLECTION_TILE_BG: 'bg-sel-tile-bg',
  COLLECTION_TITLE: 'bg-sel-tile-title',  // Title of a background image
  HIDDEN_SELECTED: 'hidden-selected',
  IMAGE_DIALOG: 'is-img-sel',
  ON_IMAGE_MENU: 'on-img-menu',
  OPTION: 'bg-option',
  OPTION_DISABLED: 'bg-option-disabled',  // The menu option is disabled.
  MENU_SHOWN: 'menu-shown',
  MOUSE_NAV: 'using-mouse-nav',
  SELECTED: 'selected',
  SELECTED_BORDER: 'selected-border',
  SELECTED_CHECK: 'selected-check',
  SELECTED_CIRCLE: 'selected-circle',
  SINGLE_ATTR: 'single-attr',
  VISIBLE: 'visible'
};

/**
 * Enum for background option menu entries, in the order they appear in the UI.
 * @enum {number}
 * @const
 */
customize.MENU_ENTRIES = {
  CHROME_BACKGROUNDS: 0,
  UPLOAD_IMAGE: 1,
  CUSTOM_LINKS_RESTORE_DEFAULT: 2,
  RESTORE_DEFAULT: 3,
};

/**
 * The semi-transparent, gradient overlay for the custom background. Intended
 * to improve readability of NTP elements/text.
 * @type {string}
 * @const
 */
customize.CUSTOM_BACKGROUND_OVERLAY =
    'linear-gradient(rgba(0, 0, 0, 0), rgba(0, 0, 0, 0.3))';

/**
 * Number of rows in the custom background dialog to preload.
 * @type {number}
 * @const
 */
customize.ROWS_TO_PRELOAD = 3;

// These should match the corresponding values in local_ntp.js, that control the
// mv-notice element.
customize.delayedHideNotification = -1;
customize.NOTIFICATION_TIMEOUT = 10000;

/**
 * Were the background tiles already created.
 * @type {boolean}
 */
customize.builtTiles = false;

/**
 * The default title for the richer picker.
 * @type {string}
 */
customize.richerPicker_defaultTitle = '';

/**
 * Called when the error notification should be shown.
 * @type {?Function}
 */
customize.showErrorNotification = null;

/**
 * Called when the custom link notification should be hidden.
 * @type {?Function}
 */
customize.hideCustomLinkNotification = null;

/**
 * Id of the currently open collection.
 * @type {string}
 */
customize.currentCollectionId = '';

/**
 * The currently active Background submenu. This can be the collections page or
 * a collection's image menu. Defaults to the collections page.
 * @type {Object}
 */
customize.richerPicker_openBackgroundSubmenu = {
  menuId: customize.IDS.BACKGROUNDS_MENU,
  title: '',
};

/**
 * The currently selected submenu (i.e. Background, Shortcuts, etc.) in the
 * richer picker.
 * @type {Object}
 */
customize.richerPicker_selectedSubmenu = {
  menuButton: null,  // The submenu's button element in the sidebar.
  menu: null,        // The submenu's menu element.
  // The submenu's title. Will usually be |customize.richerPicker_defaultTitle|
  // unless this is a background collection's image menu.
  title: '',
};

/**
 * The currently selected options in the customization menu.
 * @type {Object}
 */
customize.selectedOptions = {
  background: null,  // Contains the background image tile.
  // The data associated with a currently selected background.
  backgroundData: null,
  // Contains the selected shortcut type's DOM element, i.e. either custom links
  // or most visited.
  shortcutType: null,
  shortcutsAreHidden: false,
  color: null,  // Contains the selected color tile's DOM element.
};

/**
 * The preselected options in the richer picker.
 * @type {Object}
 */
customize.preselectedOptions = {
  // Contains the selected type's DOM element, i.e. either custom links or most
  // visited.
  shortcutType: null,
  shortcutsAreHidden: false,
  colorsMenuTile: null,       // Selected tile for Colors menu.
  backgroundsMenuTile: null,  // Selected tile for Backgrounds menu.
};

/**
 * Whether tiles for Colors menu already loaded.
 * @type {boolean}
 */
customize.colorsMenuLoaded = false;


/**
 * Default color for custom color picker in hex format.
 * @type {string}
 */
customize.defaultCustomColor = '#000000';

/**
 * Custom color picked in hex format.
 * @type {string}
 */
customize.customColorPicked = customize.defaultCustomColor;

/**
 * Sets the visibility of the settings menu and individual options depending on
 * their respective features.
 */
customize.setMenuVisibility = function() {
  // Reset all hidden values.
  $(customize.IDS.EDIT_BG).hidden = false;
  $(customize.IDS.DEFAULT_WALLPAPERS).hidden = false;
  $(customize.IDS.UPLOAD_IMAGE).hidden = false;
  $(customize.IDS.RESTORE_DEFAULT).hidden = false;
  $(customize.IDS.EDIT_BG_DIVIDER).hidden = false;
};

/**
 * Called when user changes the theme.
 */
customize.onThemeChange = function() {
  // Hide the settings menu or individual options if the related features are
  // disabled.
  customize.setMenuVisibility();

  // If theme changed after Colors menu was loaded, then reload theme info.
  if (customize.colorsMenuLoaded) {
    customize.colorsMenuOnThemeChange();
  }
};

/**
 * Display custom background image attributions on the page.
 * @param {string} attributionLine1 First line of attribution.
 * @param {string} attributionLine2 Second line of attribution.
 * @param {string} attributionActionUrl Url to learn more about the image.
 */
customize.setAttribution = function(
    attributionLine1, attributionLine2, attributionActionUrl) {
  const attributionBox = $(customize.IDS.ATTRIBUTIONS);
  const hasActionUrl = attributionActionUrl !== '';

  const attr1 = document.createElement(hasActionUrl ? 'a' : 'span');
  attr1.id = customize.IDS.ATTR1;
  const attr2 = document.createElement(hasActionUrl ? 'a' : 'span');
  attr2.id = customize.IDS.ATTR2;

  if (attributionLine1 !== '') {
    // Shouldn't be changed from textContent for security assurances.
    attr1.textContent = attributionLine1;
    if (hasActionUrl) {
      attr1.href = attributionActionUrl;
    }
    $(customize.IDS.ATTRIBUTIONS).appendChild(attr1);
  }

  if (attributionLine2 !== '') {
    // Shouldn't be changed from textContent for security assurances.
    attr2.textContent = attributionLine2;
    if (hasActionUrl) {
      attr2.href = attributionActionUrl;
    }
    attributionBox.appendChild(attr2);
  }

  if (hasActionUrl) {
    const attr = (attributionLine2 !== '' ? attr2 : attr1);
    attr.classList.add(customize.CLASSES.ATTR_LINK);

    const linkIcon = document.createElement('div');
    linkIcon.id = customize.IDS.LINK_ICON;
    // Enlarge link-icon when there is only one line of attribution
    if (attributionLine2 === '') {
      linkIcon.classList.add(customize.CLASSES.SINGLE_ATTR);
    }
    attr.insertBefore(linkIcon, attr.firstChild);

    attributionBox.onclick = e => {
      if (attr1.contains(e.target) || attr2.contains(e.target)) {
        ntpApiHandle.logEvent(
            customize.LOG_TYPE.NTP_CUSTOMIZE_ATTRIBUTION_CLICKED);
      }
    };
  }
  attributionBox.classList.toggle(customize.CLASSES.ATTR_LINK, hasActionUrl);
};

customize.clearAttribution = function() {
  $(customize.IDS.ATTRIBUTIONS).innerHTML = '';
};

customize.unselectTile = function() {
  if (configData.richerPicker) {
    return;
  }
  $(customize.IDS.DONE).disabled = true;
  customize.selectedOptions.background = null;
  $(customize.IDS.DONE).tabIndex = -1;
};

/**
 * Remove all collection tiles from the container when the dialog
 * is closed.
 */
customize.resetSelectionDialog = function() {
  $(customize.IDS.TILES).scrollTop = 0;
  const tileContainer = $(customize.IDS.TILES);
  while (tileContainer.firstChild) {
    tileContainer.removeChild(tileContainer.firstChild);
  }
  customize.unselectTile();
};

/**
 * Apply selected styling to |menuButton| and make corresponding |menu| visible.
 * If |title| is not specified, the default title will be used.
 * @param {?Element} menuButton The sidebar button element to apply styling to.
 * @param {?Element} menu The submenu element to apply styling to.
 * @param {string=} title The submenu's title.
 */
customize.richerPicker_showSubmenu = function(menuButton, menu, title = '') {
  if (!menuButton || !menu) {
    return;
  }

  customize.richerPicker_hideOpenSubmenu();

  if (!title) {  // Use the default title if not specified.
    title = customize.richerPicker_defaultTitle;
  }

  // Save this as the currently open submenu.
  customize.richerPicker_selectedSubmenu.menuButton = menuButton;
  customize.richerPicker_selectedSubmenu.menu = menu;
  customize.richerPicker_selectedSubmenu.title = title;

  menuButton.classList.toggle(customize.CLASSES.SELECTED, true);
  menu.classList.toggle(customize.CLASSES.MENU_SHOWN, true);
  $(customize.IDS.MENU_TITLE).textContent = title;
  menuButton.setAttribute('aria-selected', true);

  // Indicate if this is a Background collection's image menu, which will enable
  // the back button.
  $(customize.IDS.CUSTOMIZATION_MENU)
      .classList.toggle(
          customize.CLASSES.ON_IMAGE_MENU,
          menu.id === customize.IDS.BACKGROUNDS_IMAGE_MENU);
};

/**
 * Hides the currently open submenu if any.
 */
customize.richerPicker_hideOpenSubmenu = function() {
  if (!customize.richerPicker_selectedSubmenu.menuButton) {
    return;  // No submenu is open.
  }

  customize.richerPicker_selectedSubmenu.menuButton.classList.toggle(
      customize.CLASSES.SELECTED, false);
  customize.richerPicker_selectedSubmenu.menu.classList.toggle(
      customize.CLASSES.MENU_SHOWN, false);
  $(customize.IDS.MENU_TITLE).textContent = customize.richerPicker_defaultTitle;
  customize.richerPicker_selectedSubmenu.menuButton.setAttribute(
      'aria-selected', false);

  customize.richerPicker_selectedSubmenu.menuButton = null;
  customize.richerPicker_selectedSubmenu.menu = null;
  customize.richerPicker_selectedSubmenu.title =
      customize.richerPicker_defaultTitle;
};

/**
 * Remove image tiles and maybe swap back to main background menu.
 */
customize.richerPicker_resetImageMenu = function() {
  const backgroundMenu = $(customize.IDS.BACKGROUNDS_MENU);
  const imageMenu = $(customize.IDS.BACKGROUNDS_IMAGE_MENU);
  const menu = $(customize.IDS.CUSTOMIZATION_MENU);
  const menuTitle = $(customize.IDS.MENU_TITLE);

  imageMenu.innerHTML = '';
  menu.classList.toggle(customize.CLASSES.ON_IMAGE_MENU, false);
  customize.richerPicker_showSubmenu(
      $(customize.IDS.BACKGROUNDS_BUTTON), backgroundMenu);
  customize.richerPicker_openBackgroundSubmenu.menuId =
      customize.IDS.BACKGROUNDS_MENU;
  customize.richerPicker_openBackgroundSubmenu.title = '';
  backgroundMenu.scrollTop = 0;
};

/**
 * Close the collection selection dialog and cleanup the state
 * @param {?Element} menu The dialog to be closed
 */
customize.closeCollectionDialog = function(menu) {
  if (!menu) {
    return;
  }
  menu.close();
  customize.resetSelectionDialog();
};

/**
 * Close and reset the dialog if this is not the richer picker, and set the
 * background.
 * @param {string} url The url of the selected background.
 */
customize.setBackground = function(
    url, attributionLine1, attributionLine2, attributionActionUrl,
    collection_id) {
  if (!configData.richerPicker) {
    customize.closeCollectionDialog($(customize.IDS.MENU));
  }
  window.chrome.embeddedSearch.newTabPage.setBackgroundInfo(
      url, attributionLine1, attributionLine2, attributionActionUrl,
      collection_id);
};

/**
 * Apply selected shortcut options.
 */
customize.richerPicker_setShortcutOptions = function() {
  const shortcutTypeChanged = customize.preselectedOptions.shortcutType !==
      customize.selectedOptions.shortcutType;
  if (customize.preselectedOptions.shortcutsAreHidden !==
      customize.selectedOptions.shortcutsAreHidden) {
    // Only trigger a notification if |toggleMostVisitedOrCustomLinks| will not
    // be called immediately after. Successive |onmostvisitedchange| events can
    // interfere with each other.
    chrome.embeddedSearch.newTabPage.toggleShortcutsVisibility(
        !shortcutTypeChanged);
  }
  if (shortcutTypeChanged) {
    chrome.embeddedSearch.newTabPage.toggleMostVisitedOrCustomLinks();
  }
};

/**
 * Creates a tile for the customization menu with a title.
 * @param {string} id The id for the new element.
 * @param {string} imageUrl The background image url for the new element.
 * @param {string} name The name for the title of the new element.
 * @param {Object} dataset The dataset for the new element.
 * @param {?Function} onClickInteraction Function for onclick interaction.
 * @param {?Function} onKeyInteraction Function for onkeydown interaction.
 */
customize.createTileWithTitle = function(
    id, imageUrl, name, dataset, onClickInteraction, onKeyInteraction) {
  const tile = customize.createTileThumbnail(
      id, imageUrl, dataset, onClickInteraction, onKeyInteraction);
  tile.setAttribute('aria-label', name);
  tile.title = name;
  customize.fadeInImageTile(tile, imageUrl, null);

  const title = document.createElement('div');
  title.classList.add(customize.CLASSES.COLLECTION_TITLE);
  title.textContent = name;
  tile.appendChild(title);

  const tileBackground = document.createElement('div');
  tileBackground.classList.add(customize.CLASSES.COLLECTION_TILE_BG);
  tileBackground.appendChild(tile);
  return tileBackground;
};

/**
 * Creates a tile for the customization menu without a title.
 * @param {string} id The id for the new element.
 * @param {string} imageUrl The background image url for the new element.
 * @param {Object} dataset The dataset for the new element.
 * @param {?Function} onClickInteraction Function for onclick interaction.
 * @param {?Function} onKeyInteraction Function for onkeydown interaction.
 */
customize.createTileWithoutTitle = function(
    id, imageUrl, dataset, onClickInteraction, onKeyInteraction) {
  const tile = customize.createTileThumbnail(
      id, imageUrl, dataset, onClickInteraction, onKeyInteraction);
  customize.fadeInImageTile(tile, imageUrl, null);

  const tileBackground = document.createElement('div');
  tileBackground.classList.add(customize.CLASSES.COLLECTION_TILE_BG);
  tileBackground.appendChild(tile);
  return tileBackground;
};

/**
 * Create a tile thumbnail with image for customization menu.
 * @param {string} id The id for the new element.
 * @param {string} imageUrl The background image url for the new element.
 * @param {Object} dataset The dataset for the new element.
 * @param {?Function} onClickInteraction Function for onclick interaction.
 * @param {?Function} onKeyInteraction Function for onkeydown interaction.
 */
customize.createTileThumbnail = function(
    id, imageUrl, dataset, onClickInteraction, onKeyInteraction) {
  const tile = document.createElement('div');
  tile.id = id;
  tile.classList.add(customize.CLASSES.COLLECTION_TILE);
  tile.style.backgroundImage = 'url(' + imageUrl + ')';
  for (const key in dataset) {
    tile.dataset[key] = dataset[key];
  }
  tile.tabIndex = -1;

  // Accessibility support for screen readers.
  tile.setAttribute('role', 'button');
  tile.setAttribute('aria-pressed', false);

  tile.onclick = onClickInteraction;
  tile.onkeydown = onKeyInteraction;
  return tile;
};

/**
 * Get the number of tiles in a row according to current window width.
 * @return {number} the number of tiles per row
 */
customize.getTilesWide = function() {
  // Browser window can only fit two columns. Should match "#bg-sel-menu" width.
  if ($(customize.IDS.MENU).offsetWidth < 517) {
    return 2;
  } else if ($(customize.IDS.MENU).offsetWidth < 356) {
    // Browser window can only fit one column. Should match @media (max-width:
    // 356) "#bg-sel-menu" width.
    return 1;
  }

  return 3;
};

/**
 * @param {number} deltaX Change in the x direction.
 * @param {number} deltaY Change in the y direction.
 * @param {Element} current The current tile.
 */
customize.richerPicker_getNextTile = function(deltaX, deltaY, current) {
  const menu = $(customize.IDS.CUSTOMIZATION_MENU);
  const container = customize.richerPicker_selectedSubmenu.menu;

  const tiles = Array.from(
      container.getElementsByClassName(customize.CLASSES.COLLECTION_TILE_BG));
  let nextIndex = tiles.indexOf(current.parentElement);
  if (deltaX != 0) {
    nextIndex += deltaX;
  } else if (deltaY != 0) {
    const startingTop = current.parentElement.getBoundingClientRect().top;
    const startingLeft = current.parentElement.getBoundingClientRect().left;

    // Search until a tile in a different row and the same column is found.
    while (tiles[nextIndex] &&
           (tiles[nextIndex].getBoundingClientRect().top == startingTop ||
            tiles[nextIndex].getBoundingClientRect().left != startingLeft)) {
      nextIndex += deltaY;
    }
  }
  if (tiles[nextIndex]) {
    return tiles[nextIndex].children[0];
  }
  return null;
};

/**
 * Get the next tile when the arrow keys are used to navigate the grid.
 * Returns null if the tile doesn't exist.
 * @param {number} deltaX Change in the x direction.
 * @param {number} deltaY Change in the y direction.
 * @param {Element} currentElem The current tile.
 */
customize.getNextTile = function(deltaX, deltaY, currentElem) {
  if (configData.richerPicker) {
    return customize.richerPicker_getNextTile(deltaX, deltaY, currentElem);
  }
  const current = currentElem.dataset.tileIndex;
  let idPrefix = 'coll_tile_';
  if ($(customize.IDS.MENU)
          .classList.contains(customize.CLASSES.IMAGE_DIALOG)) {
    idPrefix = 'img_tile_';
  }

  if (deltaX != 0) {
    const target = parseInt(current, /*radix=*/ 10) + deltaX;
    return $(idPrefix + target);
  } else if (deltaY != 0) {
    let target = parseInt(current, /*radix=*/ 10);
    let nextTile = $(idPrefix + target);
    const startingTop = nextTile.getBoundingClientRect().top;
    const startingLeft = nextTile.getBoundingClientRect().left;

    // Search until a tile in a different row and the same column is found.
    while (nextTile &&
           (nextTile.getBoundingClientRect().top == startingTop ||
            nextTile.getBoundingClientRect().left != startingLeft)) {
      target += deltaY;
      nextTile = $(idPrefix + target);
    }
    return nextTile;
  }
};

/**
 * Event handler when a key is pressed on a tile in customization menu.
 * @param {Event} event The event attributes for the interaction.
 */
customize.tileOnKeyDownInteraction = function(event) {
  const tile = event.currentTarget;
  if (event.keyCode === customize.KEYCODES.ENTER ||
      event.keyCode === customize.KEYCODES.SPACE) {
    event.preventDefault();
    event.stopPropagation();
    if (tile.onClickOverride) {
      tile.onClickOverride(event);
      return;
    }
    tile.onclick(event);
  } else if (customize.arrowKeys.includes(event.keyCode)) {
    // Handle arrow key navigation.
    event.preventDefault();

    let target = null;
    if (event.keyCode === customize.KEYCODES.LEFT) {
      target = customize.getNextTile(
          window.chrome.embeddedSearch.searchBox.rtl ? 1 : -1, 0,
          /** @type HTMLElement */ (tile));
    } else if (event.keyCode === customize.KEYCODES.UP) {
      target = customize.getNextTile(0, -1, /** @type HTMLElement */ (tile));
    } else if (event.keyCode === customize.KEYCODES.RIGHT) {
      target = customize.getNextTile(
          window.chrome.embeddedSearch.searchBox.rtl ? -1 : 1, 0,
          /** @type HTMLElement */ (tile));
    } else if (event.keyCode === customize.KEYCODES.DOWN) {
      target = customize.getNextTile(0, 1, /** @type HTMLElement */ (tile));
    }
    if (target) {
      target.focus();
    } else {
      tile.focus();
    }
  }
};

/**
 * Show dialog for selecting a Chrome background.
 */
customize.showCollectionSelectionDialog = function() {
  const tileContainer = configData.richerPicker ?
      $(customize.IDS.BACKGROUNDS_MENU) :
      $(customize.IDS.TILES);
  if (configData.richerPicker && customize.builtTiles) {
    tileContainer.focus();
    return;
  }
  customize.builtTiles = true;
  const menu = configData.richerPicker ? $(customize.IDS.CUSTOMIZATION_MENU) :
                                         $(customize.IDS.MENU);
  if (!menu.open) {
    menu.showModal();
  }

  // Create dialog header.
  $(customize.IDS.TITLE).textContent =
      configData.translatedStrings.selectChromeWallpaper;
  if (!configData.richerPicker) {
    menu.classList.add(customize.CLASSES.COLLECTION_DIALOG);
    menu.classList.remove(customize.CLASSES.IMAGE_DIALOG);
  }

  const tileOnClickInteraction = function(event) {
    let tile = event.target;
    if (tile.classList.contains(customize.CLASSES.COLLECTION_TITLE)) {
      tile = tile.parentNode;
    }

    // Load images for selected collection.
    const imgElement = $('ntp-images-loader');
    if (imgElement) {
      imgElement.parentNode.removeChild(imgElement);
    }
    const imgScript = document.createElement('script');
    imgScript.id = 'ntp-images-loader';
    imgScript.src = 'chrome-search://local-ntp/ntp-background-images.js?' +
        'collection_id=' + tile.dataset.id;

    if (configData.richerPicker) {
      ntpApiHandle.logEvent(customize.LOG_TYPE.NTP_BACKGROUND_OPEN_COLLECTION);
    } else {
      ntpApiHandle.logEvent(
          customize.LOG_TYPE.NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_COLLECTION);
    }

    document.body.appendChild(imgScript);

    imgScript.onload = function() {
      // Verify that the individual image data was successfully loaded.
      const imageDataLoaded =
          (collImg.length > 0 && collImg[0].collectionId == tile.dataset.id);

      // Dependent upon the success of the load, populate the image selection
      // dialog or close the current dialog.
      if (imageDataLoaded) {
        $(customize.IDS.BACKGROUNDS_MENU)
            .classList.toggle(customize.CLASSES.MENU_SHOWN, false);
        $(customize.IDS.BACKGROUNDS_IMAGE_MENU)
            .classList.toggle(customize.CLASSES.MENU_SHOWN, true);

        // In the RP the upload or default tile may be selected.
        if (!configData.richerPicker) {
          customize.resetSelectionDialog();
        }
        customize.showImageSelectionDialog(
            tile.dataset.name, tile.dataset.tileIndex);
      } else {
        customize.handleError(collImgErrors);
      }
    };
  };

  // Create dialog tiles.
  for (let i = 0; i < coll.length; ++i) {
    const id = coll[i].collectionId;
    const name = coll[i].collectionName;
    const imageUrl = coll[i].previewImageUrl;
    const dataset = {'id': id, 'name': name, 'tileIndex': i};

    const tile = customize.createTileWithTitle(
        'coll_tile_' + i, imageUrl, name, dataset, tileOnClickInteraction,
        customize.tileOnKeyDownInteraction);
    tileContainer.appendChild(tile);
  }

  // Attach event listeners for upload and default tiles
  $(customize.IDS.BACKGROUNDS_UPLOAD_ICON).onkeydown =
      customize.tileOnKeyDownInteraction;
  $(customize.IDS.BACKGROUNDS_DEFAULT_ICON).onkeydown =
      customize.tileOnKeyDownInteraction;
  $(customize.IDS.BACKGROUNDS_UPLOAD_ICON).onClickOverride =
      $(customize.IDS.BACKGROUNDS_UPLOAD).onkeydown;
  $(customize.IDS.BACKGROUNDS_DEFAULT_ICON).onClickOverride =
      $(customize.IDS.BACKGROUNDS_DEFAULT).onkeydown;

  tileContainer.focus();
};

/**
 * Return true if any shortcut option is selected.
 * Note: Shortcut options are preselected according to current user settings.
 */
customize.richerPicker_isShortcutOptionSelected = function() {
  // Check if the currently selected options are not the preselection.
  const notPreselectedType = customize.preselectedOptions.shortcutType !==
      customize.selectedOptions.shortcutType;
  const notPreselectedHidden =
      customize.preselectedOptions.shortcutsAreHidden !==
      customize.selectedOptions.shortcutsAreHidden;
  return notPreselectedType || notPreselectedHidden;
};

/**
 * Apply styling to a selected option in the richer picker (i.e. the selected
 * background image, shortcut type, and color).
 * @param {?Element} option The option to apply styling to.
 */
customize.richerPicker_applySelectedState = function(option) {
  if (!option) {
    return;
  }

  option.parentElement.classList.toggle(customize.CLASSES.SELECTED, true);
  // Create and append a blue checkmark to the selected option.
  const selectedCircle = document.createElement('div');
  const selectedCheck = document.createElement('div');
  selectedCircle.classList.add(customize.CLASSES.SELECTED_CIRCLE);
  selectedCheck.classList.add(customize.CLASSES.SELECTED_CHECK);
  option.appendChild(selectedCircle);
  option.appendChild(selectedCheck);
  option.setAttribute('aria-pressed', true);
};

/**
 * Remove styling from a selected option in the richer picker (i.e. the selected
 * background image, shortcut type, and color).
 * @param {?Element} option The option to remove styling from.
 */
customize.richerPicker_removeSelectedState = function(option) {
  if (!option) {
    return;
  }

  option.parentElement.classList.toggle(customize.CLASSES.SELECTED, false);
  // Remove all blue checkmarks from the selected option (this includes the
  // checkmark and the encompassing circle).
  const select = option.querySelectorAll(
      '.' + customize.CLASSES.SELECTED_CHECK + ', .' +
      customize.CLASSES.SELECTED_CIRCLE);
  select.forEach((element) => {
    element.remove();
  });
  option.setAttribute('aria-pressed', false);
};

/**
 * Preview an image as a custom backgrounds.
 * @param {!Element} tile The tile that was selected.
 */
customize.richerPicker_previewImage = function(tile) {
  if (!configData.richerPicker) {
    return;
  }
  // Set preview images at 720p by replacing the params in the url.
  const background = $(customize.IDS.CUSTOM_BG);
  const preview = $(customize.IDS.CUSTOM_BG_PREVIEW);
  if (tile.id === customize.IDS.BACKGROUNDS_DEFAULT_ICON) {
    preview.dataset.hasImage = false;
    preview.style.backgroundImage = '';
    preview.style.backgroundColor = 'transparent';
  } else if (tile.id === customize.IDS.BACKGROUNDS_UPLOAD_ICON) {
    // No previews for uploaded images.
    return;
  } else {
    preview.dataset.hasImage = true;

    const re = /w\d+\-h\d+/;
    preview.style.backgroundImage =
        tile.style.backgroundImage.replace(re, 'w1280-h720');
    preview.dataset.attributionLine1 = tile.dataset.attributionLine1;
    preview.dataset.attributionLine2 = tile.dataset.attributionLine2;
    preview.dataset.attributionActionUrl = tile.dataset.attributionActionUrl;
  }
  background.style.opacity = 0;
  preview.style.opacity = 1;
  preview.dataset.hasPreview = true;

  ntpApiHandle.onthemechange();
};

/**
 * Remove a preview image of a custom backgrounds.
 */
customize.richerPicker_unpreviewImage = function() {
  if (!configData.richerPicker) {
    return;
  }
  const preview = $(customize.IDS.CUSTOM_BG_PREVIEW);
  if (!preview.dataset || !preview.dataset.hasPreview) {
    return;
  }

  preview.style.opacity = 0;
  preview.style.backgroundImage = '';
  preview.style.backgroundColor = 'transparent';
  preview.dataset.hasPreview = false;
  $(customize.IDS.CUSTOM_BG).style.opacity = 1;

  ntpApiHandle.onthemechange();
};

/**
 * Handles background selection. Apply styling to the selected background tile
 * in the richer picker, preview the background, and enable the done button.
 * @param {?Element} tile The selected background tile.
 */
customize.richerPicker_selectBackgroundTile = function(tile) {
  if (!tile) {
    return;
  }

  if (tile.parentElement.classList.contains(customize.CLASSES.SELECTED)) {
    // If the clicked tile is already selected do nothing.
    return;
  } else if (customize.selectedOptions.background) {
    // Deselect any currently selected tile.
    customize.richerPicker_removeSelectedState(
        customize.selectedOptions.background);
  }

  $(customize.IDS.REFRESH_TOGGLE).checked = false;

  // Remove any existing preview.
  customize.richerPicker_unpreviewImage();

  customize.selectedOptions.background = tile;
  customize.selectedOptions.backgroundData = {
    id: tile.id,
    url: tile.dataset.url || '',
    attr1: tile.dataset.attributionLine1 || '',
    attr2: tile.dataset.attributionLine2 || '',
    attrUrl: tile.dataset.attributionActionUrl || '',
    collectionId: '',
  };
  customize.richerPicker_applySelectedState(tile);

  // Don't apply a preview for a preselected image, as it's already the
  // page background.
  if (customize.preselectedOptions.backgroundsMenuTile !== tile) {
    customize.richerPicker_previewImage(tile);
  }
};

/**
 * Handles shortcut type selection. Apply styling to a selected shortcut option
 * and enable the done button.
 * @param {?Element} shortcutType The shortcut type option's element.
 */
customize.richerPicker_selectShortcutType = function(shortcutType) {
  if (!shortcutType ||
      customize.selectedOptions.shortcutType === shortcutType) {
    return;  // The option has already been selected.
  }

  // Clear the previous selection, if any.
  if (customize.selectedOptions.shortcutType) {
    customize.richerPicker_removeSelectedState(
        customize.selectedOptions.shortcutType);
  }
  customize.selectedOptions.shortcutType = shortcutType;
  customize.richerPicker_applySelectedState(shortcutType);
};

/**
 * Handles hide shortcuts toggle. Apply/remove styling for the toggle and
 * enable/disable the done button.
 * Note: If the toggle is enabled, the options for shortcut type will appear
 * "disabled".
 * @param {boolean} areHidden True if the shortcuts are hidden, i.e. the toggle
 *     is on.
 */
customize.richerPicker_toggleShortcutHide = function(areHidden) {
  // (De)select the shortcut hide option.
  $(customize.IDS.SHORTCUTS_HIDE)
      .classList.toggle(customize.CLASSES.SELECTED, areHidden);
  $(customize.IDS.SHORTCUTS_HIDE_TOGGLE).checked = areHidden;
  $(customize.IDS.SHORTCUTS_MENU)
      .classList.toggle(customize.CLASSES.HIDDEN_SELECTED, areHidden);

  customize.selectedOptions.shortcutsAreHidden = areHidden;
};

/**
 * Handles the "refresh daily" toggle.
 * @param {boolean} toggledOn True if the toggle has been enabled.
 */
customize.richerPicker_toggleRefreshDaily = function(toggledOn) {
  $(customize.IDS.REFRESH_TOGGLE).checked = toggledOn;
  if (!toggledOn) {
    customize.richerPicker_selectBackgroundTile(
        $(customize.IDS.BACKGROUNDS_DEFAULT_ICON));
    return;
  }

  if (customize.selectedOptions.background) {
    customize.richerPicker_removeSelectedState(
        customize.selectedOptions.background);
  }

  customize.selectedOptions.background = null;
  customize.selectedOptions.backgroundData = {
    id: '',
    url: '',
    attr1: '',
    attr2: '',
    attrUrl: '',
    collectionId: customize.currentCollectionId,
  };
};

/**
 * Apply border and checkmark when a tile is selected
 * @param {!Element} tile The tile to apply styling to.
 */
customize.applySelectedState = function(tile) {
  tile.classList.add(customize.CLASSES.COLLECTION_SELECTED);
  const selectedBorder = document.createElement('div');
  const selectedCircle = document.createElement('div');
  const selectedCheck = document.createElement('div');
  selectedBorder.classList.add(customize.CLASSES.SELECTED_BORDER);
  selectedCircle.classList.add(customize.CLASSES.SELECTED_CIRCLE);
  selectedCheck.classList.add(customize.CLASSES.SELECTED_CHECK);
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
 * @param {!Element} tile The tile to remove styling from.
 */
customize.removeSelectedState = function(tile) {
  tile.classList.remove(customize.CLASSES.COLLECTION_SELECTED);
  tile.removeChild(tile.firstChild);
  tile.setAttribute('aria-label', tile.dataset.oldLabel);
};

/**
 * Show dialog for selecting an image. Image data should previously have been
 * loaded into collImg via
 * chrome-search://local-ntp/ntp-background-images.js?collection_id=<collection_id>
 * @param {string} dialogTitle The title to be displayed at the top of the
 *     dialog.
 * @param {number} collIndex The index of the collection this image menu belongs
 * to.
 */
customize.showImageSelectionDialog = function(dialogTitle, collIndex) {
  const firstNTile = customize.ROWS_TO_PRELOAD * customize.getTilesWide();
  const tileContainer = configData.richerPicker ?
      $(customize.IDS.BACKGROUNDS_IMAGE_MENU) :
      $(customize.IDS.TILES);
  const menu = configData.richerPicker ? $(customize.IDS.CUSTOMIZATION_MENU) :
                                         $(customize.IDS.MENU);

  if (configData.richerPicker) {
    menu.classList.toggle(customize.CLASSES.ON_IMAGE_MENU, true);
    customize.richerPicker_showSubmenu(
        $(customize.IDS.BACKGROUNDS_BUTTON), tileContainer, dialogTitle);
    // Save the current image menu. Used to restore to when the Background
    // submenu is reopened.
    customize.richerPicker_openBackgroundSubmenu.menuId =
        customize.IDS.BACKGROUNDS_IMAGE_MENU;
    customize.richerPicker_openBackgroundSubmenu.title = dialogTitle;
  } else {
    $(customize.IDS.TITLE).textContent = dialogTitle;
    menu.classList.remove(customize.CLASSES.COLLECTION_DIALOG);
    menu.classList.add(customize.CLASSES.IMAGE_DIALOG);
  }

  const tileInteraction = function(tile) {
    if (customize.selectedOptions.background && !configData.richerPicker) {
      customize.removeSelectedState(customize.selectedOptions.background);
      if (customize.selectedOptions.background.id === tile.id) {
        customize.unselectTile();
        return;
      }
    }

    if (configData.richerPicker) {
      if (!customize.selectedOptions.backgroundData ||
          customize.selectedOptions.backgroundData.id !== tile.id) {
        ntpApiHandle.logEvent(customize.LOG_TYPE.NTP_BACKGROUND_SELECT_IMAGE);
      }
      customize.richerPicker_selectBackgroundTile(tile);
    } else {
      customize.applySelectedState(tile);
      customize.selectedOptions.background = tile;
    }

    $(customize.IDS.DONE).tabIndex = 0;

    // Turn toggle off when an image is selected.
    $(customize.IDS.DONE).disabled = false;
    ntpApiHandle.logEvent(
        customize.LOG_TYPE.NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_IMAGE);
  };

  const tileOnClickInteraction = function(event) {
    const clickCount = event.detail;
    // Control + option + space will fire the onclick event with 0 clickCount.
    if (clickCount <= 1) {
      tileInteraction(event.currentTarget);
    } else if (
        clickCount === 2 &&
        customize.selectedOptions.background === event.currentTarget) {
      customize.setBackground(
          event.currentTarget.dataset.url,
          event.currentTarget.dataset.attributionLine1,
          event.currentTarget.dataset.attributionLine2,
          event.currentTarget.dataset.attributionActionUrl,
          /*collection_id=*/ '');
    }
  };

  const preLoadTiles = [];
  const postLoadTiles = [];

  for (let i = 0; i < collImg.length; ++i) {
    const dataset = {};

    dataset.attributionLine1 =
        (collImg[i].attributions[0] !== undefined ? collImg[i].attributions[0] :
                                                    '');
    dataset.attributionLine2 =
        (collImg[i].attributions[1] !== undefined ? collImg[i].attributions[1] :
                                                    '');
    dataset.attributionActionUrl = collImg[i].attributionActionUrl;
    dataset.url = collImg[i].imageUrl;
    dataset.tileIndex = i;

    let tileId = 'img_tile_' + i;
    if (configData.richerPicker) {
      tileId = 'coll_' + collIndex + '_' + tileId;
    }
    const tile = customize.createTileThumbnail(
        tileId, collImg[i].thumbnailImageUrl, dataset, tileOnClickInteraction,
        customize.tileOnKeyDownInteraction);

    tile.setAttribute('aria-label', collImg[i].attributions[0]);
    tile.title = collImg[i].attributions[0];

    // Load the first |ROWS_TO_PRELOAD| rows of tiles.
    if (i < firstNTile) {
      preLoadTiles.push(tile);
    } else {
      postLoadTiles.push(tile);
    }

    const tileBackground = document.createElement('div');
    tileBackground.classList.add(customize.CLASSES.COLLECTION_TILE_BG);
    tileBackground.appendChild(tile);
    tileContainer.appendChild(tileBackground);
  }
  let tileGetsLoaded = 0;
  for (const tile of preLoadTiles) {
    customize.loadTile(tile, collImg, () => {
      // After the preloaded tiles finish loading, the rest of the tiles start
      // loading.
      if (++tileGetsLoaded === preLoadTiles.length) {
        postLoadTiles.forEach(
            (tile) => customize.loadTile(tile, collImg, null));
      }
    });
  }

  customize.currentCollectionId = collImg[0].collectionId;
  $(customize.IDS.REFRESH_TOGGLE).checked = false;
  // If an image tile was previously selected re-select it now.
  if (customize.selectedOptions.backgroundData) {
    const selected = $(customize.selectedOptions.backgroundData.id);
    if (selected) {
      customize.richerPicker_selectBackgroundTile(selected);
    } else if (
        customize.selectedOptions.backgroundData.collectionId ===
        customize.currentCollectionId) {
      $(customize.IDS.REFRESH_TOGGLE).checked = true;
    }
  } else {
    customize.richerPicker_preselectBackgroundOption();
  }
  $(customize.IDS.REFRESH_DAILY_WRAPPER).hidden = false;

  if (configData.richerPicker) {
    $(customize.IDS.BACKGROUNDS_IMAGE_MENU).focus();
  } else {
    $(customize.IDS.TILES).focus();
  }
};

/**
 * Add background image src to the tile and add animation for the tile once it
 * successfully loaded.
 * @param {!Object} tile the tile that needs to be loaded.
 * @param {!Object} imageData the source imageData.
 * @param {?Function} countLoad If not null, called after the tile finishes
 *     loading.
 */
customize.loadTile = function(tile, imageData, countLoad) {
  tile.style.backgroundImage =
      'url(' + imageData[tile.dataset.tileIndex].thumbnailImageUrl + ')';
  customize.fadeInImageTile(
      tile, imageData[tile.dataset.tileIndex].thumbnailImageUrl, countLoad);
};

/**
 * Fade in effect for both collection and image tile. Once the image
 * successfully loads, we can assume the background image with the same source
 * has also loaded. Then, we set opacity for the tile to start the animation.
 * @param {!Object} tile The tile to add the fade in animation to.
 * @param {string} imageUrl the image url for the tile
 * @param {?Function} countLoad If not null, called after the tile finishes
 *     loading.
 */
customize.fadeInImageTile = function(tile, imageUrl, countLoad) {
  const image = new Image();
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
 */
customize.loadChromeBackgrounds = function() {
  const collElement = $('ntp-collection-loader');
  if (collElement) {
    collElement.parentNode.removeChild(collElement);
  }
  const collScript = document.createElement('script');
  collScript.id = 'ntp-collection-loader';
  collScript.src = 'chrome-search://local-ntp/ntp-background-collections.js?' +
      'collection_type=background';
  collScript.onload = function() {
    if (configData.richerPicker) {
      customize.showCollectionSelectionDialog();
    }
  };
  document.body.appendChild(collScript);
};

/**
 * Close dialog when an image is selected via the file picker.
 */
customize.closeCustomizationDialog = function() {
  if (configData.richerPicker) {
    $(customize.IDS.CUSTOMIZATION_MENU).close();
  } else {
    $(customize.IDS.EDIT_BG_DIALOG).close();
  }
};

/**
 * Get the next visible option. There are times when various combinations of
 * options are hidden.
 * @param {number} current_index Index of the option the key press occurred on.
 * @param {number} deltaY Direction to search in, -1 for up, 1 for down.
 */
customize.getNextOption = function(current_index, deltaY) {
  // Create array corresponding to the menu. Important that this is in the same
  // order as the MENU_ENTRIES enum, so we can index into it.
  const entries = [];
  entries.push($(customize.IDS.DEFAULT_WALLPAPERS));
  entries.push($(customize.IDS.UPLOAD_IMAGE));
  entries.push($(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT));
  entries.push($(customize.IDS.RESTORE_DEFAULT));

  let idx = current_index;
  do {
    idx = idx + deltaY;
    if (idx === -1) {
      idx = 3;
    }
    if (idx === 4) {
      idx = 0;
    }
  } while (
      idx !== current_index &&
      (entries[idx].hidden ||
       entries[idx].classList.contains(customize.CLASSES.OPTION_DISABLED)));
  return entries[idx];
};

/**
 * Hide custom background options based on the network state
 * @param {boolean} online The current state of the network
 */
customize.networkStateChanged = function(online) {
  $(customize.IDS.DEFAULT_WALLPAPERS).hidden = !online;
};

/**
 * Open the customization menu and set it to the default submenu (Background).
 */
customize.richerPicker_openCustomizationMenu = function() {
  ntpApiHandle.logEvent(customize.LOG_TYPE.NTP_CUSTOMIZATION_MENU_OPENED);

  const ntpTheme = assert(ntpApiHandle.ntpTheme);
  if (!ntpTheme.customBackgroundDisabledByPolicy) {
    customize.richerPicker_showSubmenu(
        $(customize.IDS.BACKGROUNDS_BUTTON), $(customize.IDS.BACKGROUNDS_MENU));
    customize.loadChromeBackgrounds();
  } else {
    customize.richerPicker_showSubmenu(
        $(customize.IDS.BACKGROUNDS_BUTTON),
        $(customize.IDS.BACKGROUNDS_DISABLED_MENU));
    // Save the current Background submenu. Used to restore when the Background
    // submenu is reopened.
    customize.richerPicker_openBackgroundSubmenu.menuId =
        customize.IDS.BACKGROUNDS_DISABLED_MENU;
  }

  customize.richerPicker_preselectShortcutOptions();
  customize.richerPicker_preselectBackgroundOption();
  customize.loadColorsMenu();
  if (!$(customize.IDS.CUSTOMIZATION_MENU).open) {
    $(customize.IDS.CUSTOMIZATION_MENU).showModal();
  }
};

/**
 * Reset the selected options in the customization menu.
 */
customize.richerPicker_resetSelectedOptions = function() {
  // Reset background selection.
  customize.richerPicker_removeSelectedState(
      customize.selectedOptions.background);
  customize.selectedOptions.background = null;
  customize.selectedOptions.backgroundData = null;

  customize.resetColorsSelectedOptions();

  customize.richerPicker_preselectShortcutOptions();
};

/**
 * Preselect the shortcut type and visibility to reflect the current state on
 * the page.
 */
customize.richerPicker_preselectShortcutOptions = function() {
  const shortcutType = chrome.embeddedSearch.newTabPage.isUsingMostVisited ?
      $(customize.IDS.SHORTCUTS_OPTION_MOST_VISITED) :
      $(customize.IDS.SHORTCUTS_OPTION_CUSTOM_LINKS);
  const shortcutsAreHidden =
      !chrome.embeddedSearch.newTabPage.areShortcutsVisible;
  customize.preselectedOptions.shortcutType = shortcutType;
  customize.preselectedOptions.shortcutsAreHidden = shortcutsAreHidden;
  customize.richerPicker_selectShortcutType(shortcutType);
  customize.richerPicker_toggleShortcutHide(shortcutsAreHidden);
};

/**
 * Preselect the background tile that corresponds to the current page
 * background.
 */
customize.richerPicker_preselectBackgroundOption = function() {
  if (!configData.richerPicker) {
    return;
  }

  customize.preselectedOptions.backgroundsMenuTile = null;

  const ntpTheme = assert(ntpApiHandle.ntpTheme);
  if (!ntpTheme.customBackgroundConfigured) {
    // Default.
    customize.preselectedOptions.backgroundsMenuTile =
        $(customize.IDS.BACKGROUNDS_DEFAULT_ICON);
  } else if (ntpTheme.imageUrl.includes(
                 'chrome-search://local-ntp/background.jpg')) {
    // Local image.
    customize.preselectedOptions.backgroundsMenuTile =
        $(customize.IDS.BACKGROUNDS_UPLOAD_ICON);
  } else if (
      ntpTheme.collectionId !== '' &&
      customize.currentCollectionId == ntpTheme.collectionId) {
    // Daily refresh.
    $(customize.IDS.REFRESH_TOGGLE).checked = true;
  } else if (!customize.selectedOptions.backgroundData) {
    // Image tile. Only if another background hasn't already been selected.
    customize.preselectedOptions.backgroundsMenuTile =
        document.querySelector('[data-url="' + ntpTheme.imageUrl + '"]');
  }

  customize.richerPicker_selectBackgroundTile(
      customize.preselectedOptions.backgroundsMenuTile);
  customize.selectedOptions.backgroundData = null;
};

/**
 * Resets the customization menu.
 */
customize.richerPicker_resetCustomizationMenu = function() {
  customize.richerPicker_resetSelectedOptions();
  customize.richerPicker_resetImageMenu();
  customize.richerPicker_hideOpenSubmenu();
  customize.resetColorPicker();
};

/**
 * Close and reset the customization menu.
 */
customize.richerPicker_closeCustomizationMenu = function() {
  $(customize.IDS.BACKGROUNDS_MENU).scrollTop = 0;
  $(customize.IDS.CUSTOMIZATION_MENU).close();
  customize.richerPicker_resetCustomizationMenu();

  customize.richerPicker_unpreviewImage();
};

/**
 * Cancel customization, revert any changes, and close the richer picker.
 */
customize.richerPicker_cancelCustomization = function() {
  ntpApiHandle.logEvent(customize.LOG_TYPE.NTP_CUSTOMIZATION_MENU_CANCEL);

  if (customize.isColorOptionSelected()) {
    customize.cancelColor();
  }

  customize.richerPicker_closeCustomizationMenu();
};

/**
 * Apply the currently selected customization options and close the richer
 * picker.
 */
customize.richerPicker_applyCustomization = function() {
  if (customize.isBackgroundOptionSelected()) {
    customize.setBackground(
        customize.selectedOptions.backgroundData.url,
        customize.selectedOptions.backgroundData.attr1,
        customize.selectedOptions.backgroundData.attr2,
        customize.selectedOptions.backgroundData.attrUrl,
        customize.selectedOptions.backgroundData.collectionId);
  }
  if (customize.richerPicker_isShortcutOptionSelected()) {
    customize.richerPicker_setShortcutOptions();
  }
  if (customize.isColorOptionSelected()) {
    customize.confirmColor();
  }
  customize.richerPicker_closeCustomizationMenu();
};

/**
 * Initialize the settings menu, custom backgrounds dialogs, and custom
 * links menu items. Set the text and event handlers for the various
 * elements.
 * @param {!Function} showErrorNotification Called when the error notification
 *     should be displayed.
 * @param {!Function} hideCustomLinkNotification Called when the custom link
 *     notification should be hidden.
 */
customize.init = function(showErrorNotification, hideCustomLinkNotification) {
  ntpApiHandle = window.chrome.embeddedSearch.newTabPage;
  const editDialog = $(customize.IDS.EDIT_BG_DIALOG);

  $(customize.IDS.OPTIONS_TITLE).textContent =
      configData.translatedStrings.customizeThisPage;

  if (configData.richerPicker) {
    // Store the main menu title so it can be restored if needed.
    customize.richerPicker_defaultTitle =
        $(customize.IDS.MENU_TITLE).textContent;
  }

  $(customize.IDS.EDIT_BG)
      .setAttribute(
          'aria-label', configData.translatedStrings.customizeThisPage);

  $(customize.IDS.EDIT_BG)
      .setAttribute('title', configData.translatedStrings.customizeThisPage);

  // Selecting a local image for the background should close the picker.
  if (configData.richerPicker) {
    ntpApiHandle.onlocalbackgroundselected = () => {
      customize.selectedOptions.backgroundData = null;
      customize.richerPicker_applyCustomization();
    };
  }

  // Edit gear icon interaction events.
  const editBackgroundInteraction = function() {
    if (configData.richerPicker) {
      customize.richerPicker_openCustomizationMenu();
    } else {
      editDialog.showModal();
    }
  };
  $(customize.IDS.EDIT_BG).onclick = function(event) {
    $(customize.IDS.CUSTOMIZATION_MENU)
        .classList.add(customize.CLASSES.MOUSE_NAV);
    editDialog.classList.add(customize.CLASSES.MOUSE_NAV);
    editBackgroundInteraction();
  };

  $(customize.IDS.MENU_CANCEL).onclick = function(event) {
    customize.richerPicker_cancelCustomization();
  };


  // Find the first menu option that is not hidden or disabled.
  const findFirstMenuOption = () => {
    const editMenu = $(customize.IDS.EDIT_BG_MENU);
    for (let i = 1; i < editMenu.children.length; i++) {
      const option = editMenu.children[i];
      if (option.classList.contains(customize.CLASSES.OPTION) &&
          !option.hidden &&
          !option.classList.contains(customize.CLASSES.OPTION_DISABLED)) {
        option.focus();
        return;
      }
    }
  };

  $(customize.IDS.EDIT_BG).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      // no default behavior for ENTER
      event.preventDefault();
      editDialog.classList.remove(customize.CLASSES.MOUSE_NAV);
      editBackgroundInteraction();
      findFirstMenuOption();
    }
  };

  // Interactions to close the customization option dialog.
  const editDialogInteraction = function() {
    editDialog.close();
  };
  editDialog.onclick = function(event) {
    editDialog.classList.add(customize.CLASSES.MOUSE_NAV);
    if (event.target === editDialog) {
      editDialogInteraction();
    }
  };
  editDialog.onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ESC) {
      editDialogInteraction();
    } else if (
        editDialog.classList.contains(customize.CLASSES.MOUSE_NAV) &&
        (event.keyCode === customize.KEYCODES.TAB ||
         event.keyCode === customize.KEYCODES.UP ||
         event.keyCode === customize.KEYCODES.DOWN)) {
      // When using tab in mouse navigation mode, select the first option
      // available.
      event.preventDefault();
      findFirstMenuOption();
      editDialog.classList.remove(customize.CLASSES.MOUSE_NAV);
    } else if (event.keyCode === customize.KEYCODES.TAB) {
      // If keyboard navigation is attempted, remove mouse-only mode.
      editDialog.classList.remove(customize.CLASSES.MOUSE_NAV);
    } else if (customize.arrowKeys.includes(event.keyCode)) {
      event.preventDefault();
      editDialog.classList.remove(customize.CLASSES.MOUSE_NAV);
    }
  };

  customize.initCustomLinksItems(hideCustomLinkNotification);
  customize.initCustomBackgrounds(showErrorNotification);
};

/**
 * Initialize custom link items in the settings menu dialog. Set the text
 * and event handlers for the various elements.
 * @param {!Function} hideCustomLinkNotification Called when the custom link
 *     notification should be hidden.
 */
customize.initCustomLinksItems = function(hideCustomLinkNotification) {
  customize.hideCustomLinkNotification = hideCustomLinkNotification;

  const editDialog = $(customize.IDS.EDIT_BG_DIALOG);
  const menu = $(customize.IDS.MENU);

  $(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT_TEXT).textContent =
      configData.translatedStrings.restoreDefaultLinks;

  // Interactions with the "Restore default shortcuts" option.
  const customLinksRestoreDefaultInteraction = function() {
    editDialog.close();
    customize.hideCustomLinkNotification();
    window.chrome.embeddedSearch.newTabPage.resetCustomLinks();
    ntpApiHandle.logEvent(
        customize.LOG_TYPE.NTP_CUSTOMIZE_RESTORE_SHORTCUTS_CLICKED);
  };
  $(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT).onclick = () => {
    if (!$(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT)
             .classList.contains(customize.CLASSES.OPTION_DISABLED)) {
      customLinksRestoreDefaultInteraction();
    }
  };
  $(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      customLinksRestoreDefaultInteraction();
    } else if (event.keyCode === customize.KEYCODES.UP) {
      // Handle arrow key navigation.
      event.preventDefault();
      customize
          .getNextOption(
              customize.MENU_ENTRIES.CUSTOM_LINKS_RESTORE_DEFAULT, -1)
          .focus();
    } else if (event.keyCode === customize.KEYCODES.DOWN) {
      event.preventDefault();
      customize
          .getNextOption(customize.MENU_ENTRIES.CUSTOM_LINKS_RESTORE_DEFAULT, 1)
          .focus();
    }
  };
};

/**
 * Initialize the settings menu and custom backgrounds dialogs. Set the
 * text and event handlers for the various elements.
 * @param {!Function} showErrorNotification Called when the error notification
 *     should be displayed.
 */
customize.initCustomBackgrounds = function(showErrorNotification) {
  customize.showErrorNotification = showErrorNotification;

  const editDialog = $(customize.IDS.EDIT_BG_DIALOG);
  const menu = $(customize.IDS.MENU);

  $(customize.IDS.DEFAULT_WALLPAPERS_TEXT).textContent =
      configData.translatedStrings.defaultWallpapers;
  $(customize.IDS.UPLOAD_IMAGE_TEXT).textContent =
      configData.translatedStrings.uploadImage;
  $(customize.IDS.RESTORE_DEFAULT_TEXT).textContent =
      configData.translatedStrings.restoreDefaultBackground;
  $(customize.IDS.DONE).textContent =
      configData.translatedStrings.selectionDone;
  $(customize.IDS.CANCEL).textContent =
      configData.translatedStrings.selectionCancel;

  window.addEventListener('online', function(event) {
    customize.networkStateChanged(true);
  });

  window.addEventListener('offline', function(event) {
    customize.networkStateChanged(false);
  });

  if (!window.navigator.onLine) {
    customize.networkStateChanged(false);
  }

  $(customize.IDS.BACK_CIRCLE)
      .setAttribute('aria-label', configData.translatedStrings.backLabel);
  $(customize.IDS.CANCEL)
      .setAttribute('aria-label', configData.translatedStrings.selectionCancel);
  $(customize.IDS.DONE)
      .setAttribute('aria-label', configData.translatedStrings.selectionDone);

  $(customize.IDS.DONE).disabled = true;

  // Interactions with the "Upload an image" option.
  const uploadImageInteraction = function() {
    window.chrome.embeddedSearch.newTabPage.selectLocalBackgroundImage();
    if (configData.richerPicker) {
      ntpApiHandle.logEvent(
          customize.LOG_TYPE.NTP_BACKGROUND_UPLOAD_FROM_DEVICE);
    } else {
      ntpApiHandle.logEvent(
          customize.LOG_TYPE.NTP_CUSTOMIZE_LOCAL_IMAGE_CLICKED);
    }
  };

  $(customize.IDS.UPLOAD_IMAGE).onclick = (event) => {
    if (!$(customize.IDS.UPLOAD_IMAGE)
             .classList.contains(customize.CLASSES.OPTION_DISABLED)) {
      uploadImageInteraction();
    }
  };
  $(customize.IDS.UPLOAD_IMAGE).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      uploadImageInteraction();
    }

    // Handle arrow key navigation.
    if (event.keyCode === customize.KEYCODES.UP) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.UPLOAD_IMAGE, -1).focus();
    }
    if (event.keyCode === customize.KEYCODES.DOWN) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.UPLOAD_IMAGE, 1).focus();
    }
  };

  // Interactions with the "Restore default background" option.
  const restoreDefaultInteraction = function() {
    editDialog.close();
    customize.clearAttribution();
    window.chrome.embeddedSearch.newTabPage.resetBackgroundInfo();
  };
  $(customize.IDS.RESTORE_DEFAULT).onclick = (event) => {
    if (!$(customize.IDS.RESTORE_DEFAULT)
             .classList.contains(customize.CLASSES.OPTION_DISABLED)) {
      restoreDefaultInteraction();
    }
  };
  $(customize.IDS.RESTORE_DEFAULT).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      restoreDefaultInteraction();
    }

    // Handle arrow key navigation.
    if (event.keyCode === customize.KEYCODES.UP) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.RESTORE_DEFAULT, -1)
          .focus();
    }
    if (event.keyCode === customize.KEYCODES.DOWN) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.RESTORE_DEFAULT, 1)
          .focus();
    }
  };

  // Interactions with the "Chrome backgrounds" option.
  const defaultWallpapersInteraction = function(event) {
    customize.loadChromeBackgrounds();
    $('ntp-collection-loader').onload = function() {
      editDialog.close();
      if (typeof coll != 'undefined' && coll.length > 0) {
        customize.showCollectionSelectionDialog();
      } else {
        customize.handleError(collErrors);
      }
    };
    ntpApiHandle.logEvent(
        customize.LOG_TYPE.NTP_CUSTOMIZE_CHROME_BACKGROUNDS_CLICKED);
  };
  $(customize.IDS.DEFAULT_WALLPAPERS).onclick = function(event) {
    $(customize.IDS.MENU).classList.add(customize.CLASSES.MOUSE_NAV);
    defaultWallpapersInteraction(event);
  };
  $(customize.IDS.DEFAULT_WALLPAPERS).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      $(customize.IDS.MENU).classList.remove(customize.CLASSES.MOUSE_NAV);
      defaultWallpapersInteraction(event);
    }

    // Handle arrow key navigation.
    if (event.keyCode === customize.KEYCODES.UP) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.CHROME_BACKGROUNDS, -1)
          .focus();
    }
    if (event.keyCode === customize.KEYCODES.DOWN) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.CHROME_BACKGROUNDS, 1)
          .focus();
    }
  };

  // Escape and Backspace handling for the background picker dialog.
  menu.onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.SPACE) {
      $(customize.IDS.TILES).scrollTop += $(customize.IDS.TILES).offsetHeight;
      event.stopPropagation();
      event.preventDefault();
    }
    if (event.keyCode === customize.KEYCODES.ESC ||
        event.keyCode === customize.KEYCODES.BACKSPACE) {
      event.preventDefault();
      event.stopPropagation();
      if (menu.classList.contains(customize.CLASSES.COLLECTION_DIALOG)) {
        menu.close();
        customize.resetSelectionDialog();
      } else {
        customize.resetSelectionDialog();
        customize.showCollectionSelectionDialog();
      }
    }

    // If keyboard navigation is attempted, remove mouse-only mode.
    if (event.keyCode === customize.KEYCODES.TAB ||
        event.keyCode === customize.KEYCODES.LEFT ||
        event.keyCode === customize.KEYCODES.UP ||
        event.keyCode === customize.KEYCODES.RIGHT ||
        event.keyCode === customize.KEYCODES.DOWN) {
      menu.classList.remove(customize.CLASSES.MOUSE_NAV);
    }
  };

  // Interactions with the back arrow on the image selection dialog.
  const backInteraction = function(event) {
    if (configData.richerPicker) {
      ntpApiHandle.logEvent(customize.LOG_TYPE.NTP_BACKGROUND_BACK_CLICK);
      customize.richerPicker_resetImageMenu();
    }
    customize.resetSelectionDialog();
    customize.showCollectionSelectionDialog();
  };
  $(customize.IDS.BACK_CIRCLE).onclick = backInteraction;
  $(customize.IDS.MENU_BACK_CIRCLE).onclick = backInteraction;
  $(customize.IDS.BACK_CIRCLE).onkeyup = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      backInteraction(event);
    }
  };
  $(customize.IDS.MENU_BACK_CIRCLE).onkeyup = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      backInteraction(event);
    }
  };
  // Pressing Spacebar on the back arrow shouldn't scroll the dialog.
  $(customize.IDS.BACK_CIRCLE).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.SPACE) {
      event.stopPropagation();
    }
  };

  // Interactions with the cancel button on the background picker dialog.
  $(customize.IDS.CANCEL).onclick = function(event) {
    customize.closeCollectionDialog(menu);
    ntpApiHandle.logEvent(
        customize.LOG_TYPE.NTP_CUSTOMIZE_CHROME_BACKGROUND_CANCEL);
  };
  $(customize.IDS.CANCEL).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      customize.closeCollectionDialog(menu);
      ntpApiHandle.logEvent(
          customize.LOG_TYPE.NTP_CUSTOMIZE_CHROME_BACKGROUND_CANCEL);
    }
  };

  // Interactions with the done button on the background picker dialog.
  const doneInteraction = function(event) {
    const done = configData.richerPicker ? $(customize.IDS.MENU_DONE) :
                                           $(customize.IDS.DONE);
    if (configData.richerPicker) {
      ntpApiHandle.logEvent(customize.LOG_TYPE.NTP_CUSTOMIZATION_MENU_DONE);
      customize.richerPicker_applyCustomization();
    } else if (customize.selectedOptions.background) {
      // Also closes the customization menu.
      customize.setBackground(
          customize.selectedOptions.background.dataset.url,
          customize.selectedOptions.background.dataset.attributionLine1,
          customize.selectedOptions.background.dataset.attributionLine2,
          customize.selectedOptions.background.dataset.attributionActionUrl,
          '');
    }
  };
  $(customize.IDS.DONE).onclick = doneInteraction;
  $(customize.IDS.MENU_DONE).onclick = doneInteraction;
  $(customize.IDS.DONE).onkeyup = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      doneInteraction(event);
    }
  };

  // On any arrow key event in the tiles area, focus the first tile.
  $(customize.IDS.TILES).onkeydown = function(event) {
    if (document.activeElement === $(customize.IDS.TILES) &&
        customize.arrowKeys.includes(event.keyCode)) {
      event.preventDefault();
      if ($(customize.IDS.MENU)
              .classList.contains(customize.CLASSES.COLLECTION_DIALOG)) {
        $('coll_tile_0').focus();
      } else {
        document.querySelector('[id$="img_tile_0"]').focus();
      }
    }
  };

  $(customize.IDS.BACKGROUNDS_MENU).onkeydown = function(event) {
    if (document.activeElement === $(customize.IDS.BACKGROUNDS_MENU) &&
        customize.arrowKeys.includes(event.keyCode)) {
      event.preventDefault();
      $(customize.IDS.BACKGROUNDS_UPLOAD_ICON).focus();
    }
  };

  $(customize.IDS.BACKGROUNDS_IMAGE_MENU).onkeydown = function(event) {
    if (document.activeElement === $(customize.IDS.BACKGROUNDS_IMAGE_MENU) &&
        customize.arrowKeys.includes(event.keyCode)) {
      event.preventDefault();
      document.querySelector('[id$="img_tile_0"]').focus();
    }
  };

  $(customize.IDS.BACKGROUNDS_UPLOAD).onclick = uploadImageInteraction;
  $(customize.IDS.BACKGROUNDS_UPLOAD).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      uploadImageInteraction();
    }
  };

  $(customize.IDS.BACKGROUNDS_DEFAULT).onclick = function(event) {
    const tile = $(customize.IDS.BACKGROUNDS_DEFAULT_ICON);
    tile.dataset.url = '';
    tile.dataset.attributionLine1 = '';
    tile.dataset.attributionLine2 = '';
    tile.dataset.attributionActionUrl = '';
    if (!$(customize.IDS.BACKGROUNDS_DEFAULT)
             .classList.contains(customize.CLASSES.SELECTED)) {
      ntpApiHandle.logEvent(customize.LOG_TYPE.NTP_BACKGROUND_DEFAULT_SELECTED);
      customize.richerPicker_selectBackgroundTile(tile);
    }
  };
  $(customize.IDS.BACKGROUNDS_DEFAULT).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      $(customize.IDS.BACKGROUNDS_DEFAULT).onclick(event);
    }
  };

  const richerPicker = $(customize.IDS.CUSTOMIZATION_MENU);
  richerPicker.onmousedown = function(event) {
    richerPicker.classList.add(customize.CLASSES.MOUSE_NAV);
  };
  richerPicker.onkeydown = function(event) {
    if (Object.values(customize.KEYCODES).includes(event.keyCode)) {
      richerPicker.classList.remove(customize.CLASSES.MOUSE_NAV);
    }

    if (event.keyCode === customize.KEYCODES.BACKSPACE &&
        customize.richerPicker_selectedSubmenu.menu.id ===
            customize.IDS.BACKGROUNDS_IMAGE_MENU) {
      backInteraction(event);
    } else if (
        event.keyCode === customize.KEYCODES.ESC ||
        event.keyCode === customize.KEYCODES.BACKSPACE) {
      customize.richerPicker_cancelCustomization();
    }
  };

  const richerPickerOpenBackgrounds = function() {
    // Open the previously open Background submenu, if applicable.
    customize.richerPicker_showSubmenu(
        $(customize.IDS.BACKGROUNDS_BUTTON),
        $(customize.richerPicker_openBackgroundSubmenu.menuId),
        customize.richerPicker_openBackgroundSubmenu.title);
  };

  $(customize.IDS.BACKGROUNDS_BUTTON).onclick = richerPickerOpenBackgrounds;
  $(customize.IDS.BACKGROUNDS_BUTTON).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      richerPickerOpenBackgrounds();
    }
  };

  const clOption = $(customize.IDS.SHORTCUTS_OPTION_CUSTOM_LINKS);
  const mvOption = $(customize.IDS.SHORTCUTS_OPTION_MOST_VISITED);
  const hideToggle = $(customize.IDS.SHORTCUTS_HIDE_TOGGLE);

  const rtl = window.chrome.embeddedSearch.searchBox.rtl;
  const forwardArrowKey =
      rtl ? customize.KEYCODES.LEFT : customize.KEYCODES.RIGHT;
  const backArrowKey = rtl ? customize.KEYCODES.RIGHT : customize.KEYCODES.LEFT;

  $(customize.IDS.SHORTCUTS_MENU).onkeydown = function(event) {
    if (customize.arrowKeys.includes(event.keyCode)) {
      clOption.focus();
    }
  };

  clOption.onclick = function() {
    if (customize.selectedOptions.shortcutType !== clOption) {
      ntpApiHandle.logEvent(
          customize.LOG_TYPE.NTP_CUSTOMIZE_SHORTCUT_CUSTOM_LINKS_CLICKED);
    }
    customize.richerPicker_selectShortcutType(clOption);
    // Selecting a shortcut type will turn off hidden shortcuts.
    customize.richerPicker_toggleShortcutHide(false);
  };
  clOption.onkeydown = function(event) {
    if (customize.arrowKeys.includes(event.keyCode)) {
      // Handle arrow key navigation.
      event.preventDefault();
      event.stopPropagation();
      richerPicker.classList.remove(customize.CLASSES.MOUSE_NAV);
      if (event.keyCode === forwardArrowKey) {
        mvOption.focus();
      } else if (event.keyCode === customize.KEYCODES.DOWN) {
        hideToggle.focus();
      }
    }
  };
  clOption.onkeyup = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      clOption.click();
    }
  };

  mvOption.onclick = function() {
    if (customize.selectedOptions.shortcutType !== mvOption) {
      ntpApiHandle.logEvent(
          customize.LOG_TYPE.NTP_CUSTOMIZE_SHORTCUT_MOST_VISITED_CLICKED);
    }
    customize.richerPicker_selectShortcutType(mvOption);
    // Selecting a shortcut type will turn off hidden shortcuts.
    customize.richerPicker_toggleShortcutHide(false);
  };
  mvOption.onkeydown = function(event) {
    if (customize.arrowKeys.includes(event.keyCode)) {
      // Handle arrow key navigation.
      event.preventDefault();
      event.stopPropagation();
      richerPicker.classList.remove(customize.CLASSES.MOUSE_NAV);
      if (event.keyCode === backArrowKey) {
        clOption.focus();
      } else if (
          event.keyCode === forwardArrowKey ||
          event.keyCode === customize.KEYCODES.DOWN) {
        hideToggle.focus();
      }
    }
  };
  mvOption.onkeyup = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      mvOption.click();
    }
  };

  hideToggle.onchange = function(event) {
    customize.richerPicker_toggleShortcutHide(hideToggle.checked);
    ntpApiHandle.logEvent(
        customize.LOG_TYPE.NTP_CUSTOMIZE_SHORTCUT_VISIBILITY_TOGGLE_CLICKED);
  };
  hideToggle.onkeydown = function(event) {
    if (customize.arrowKeys.includes(event.keyCode)) {
      // Handle arrow key navigation.
      event.preventDefault();
      event.stopPropagation();
      richerPicker.classList.remove(customize.CLASSES.MOUSE_NAV);
      if (event.keyCode === backArrowKey ||
          event.keyCode === customize.KEYCODES.UP) {
        mvOption.focus();
      }
    }
  };
  hideToggle.onkeyup = function(event) {
    // Handle enter since, unlike space, it does not trigger a click event.
    if (event.keyCode === customize.KEYCODES.ENTER) {
      hideToggle.click();
    }
  };
  hideToggle.onclick = function(event) {
    // Enter and space fire the 'onclick' event (which will remove special
    // keyboard navigation styling) unless propagation is stopped.
    event.stopPropagation();
  };

  const refreshToggle = $(customize.IDS.REFRESH_TOGGLE);
  refreshToggle.onchange = function(event) {
    ntpApiHandle.logEvent(
        customize.LOG_TYPE.NTP_BACKGROUND_REFRESH_TOGGLE_CLICKED);
    customize.richerPicker_toggleRefreshDaily(refreshToggle.checked);
  };
  refreshToggle.onkeyup = function(event) {
    // Handle enter since, unlike space, it does not trigger a click event.
    if (event.keyCode === customize.KEYCODES.ENTER) {
      refreshToggle.click();
    }
  };
  refreshToggle.onclick = function(event) {
    // Enter and space fire the 'onclick' event (which will remove special
    // keyboard navigation styling) unless propagation is stopped.
    event.stopPropagation();
  };


  const richerPickerOpenShortcuts = function() {
    customize.richerPicker_showSubmenu(
        $(customize.IDS.SHORTCUTS_BUTTON), $(customize.IDS.SHORTCUTS_MENU));
  };

  $(customize.IDS.SHORTCUTS_BUTTON).onclick = richerPickerOpenShortcuts;
  $(customize.IDS.SHORTCUTS_BUTTON).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      richerPickerOpenShortcuts();
    }
  };

  $(customize.IDS.COLORS_BUTTON).onclick = function() {
    customize.richerPicker_showSubmenu(
        $(customize.IDS.COLORS_BUTTON), $(customize.IDS.COLORS_MENU));
    ntpApiHandle.getColorsInfo();
  };
};

customize.handleError = function(errors) {
  const unavailableString = configData.translatedStrings.backgroundsUnavailable;

  if (errors != 'undefined') {
    // Network errors.
    if (errors.net_error) {
      if (errors.net_error_no != 0) {
        const onClick = () => {
          window.open(
              'https://chrome://network-error/' + errors.net_error_no,
              '_blank');
        };
        customize.showErrorNotification(
            configData.translatedStrings.connectionError,
            configData.translatedStrings.moreInfo, onClick);
      } else {
        customize.showErrorNotification(
            configData.translatedStrings.connectionErrorNoPeriod);
      }
    } else if (errors.service_error) {  // Service errors.
      customize.showErrorNotification(unavailableString);
    }
    return;
  }

  // Generic error when we can't tell what went wrong.
  customize.showErrorNotification(unavailableString);
};

/**
 * Handles color selection. Apply styling to the selected color in the richer
 * picker and enable the done button.
 * @param {?Element} tile The selected color tile.
 */
customize.updateColorsMenuTileSelection = function(tile) {
  if (!tile) {
    return;
  }
  // Clear the previous selection, if any.
  if (customize.selectedOptions.color) {
    customize.richerPicker_removeSelectedState(customize.selectedOptions.color);
  }
  customize.selectedOptions.color = tile;
  customize.richerPicker_applySelectedState(tile);
};

/**
 * Called when a color tile is clicked. Applies the color, and the selected
 * style on the tile.
 * @param {Event} event The event attributes for the interaction.
 */
customize.colorTileInteraction = function(event) {
  customize.updateColorsMenuTileSelection(
      /** @type HTMLElement */ (event.currentTarget));
  const id = parseInt(event.currentTarget.dataset.id, 10);
  if (id) {
    ntpApiHandle.applyAutogeneratedTheme(
        id, event.currentTarget.dataset.color.split(','));
  }
};

/**
 * Called when the default theme tile is clicked. Applies the default theme, and
 * the selected style on the tile.
 * @param {Event} event The event attributes for the interaction.
 */
customize.defaultThemeTileInteraction = function(event) {
  customize.updateColorsMenuTileSelection(
      /** @type HTMLElement */ (event.currentTarget));
  ntpApiHandle.applyDefaultTheme();
};

/**
 * Called when a new color is picked with color picker. Applies the color, and
 * the selected style on the tile.
 * @param {Event} event The event attributes for the interaction.
 */
customize.colorPickerTileInteraction = function(event) {
  const hex = event.currentTarget.value;
  const r = parseInt(hex.substring(1, 3), 16);
  const g = parseInt(hex.substring(3, 5), 16);
  const b = parseInt(hex.substring(5, 7), 16);
  // If the picker is preselected and the user picks a new color, we need to
  // treat the picker as a new selection and not a preselection.
  if (customize.preselectedOptions.colorsMenuTile ===
      $(customize.IDS.COLOR_PICKER_TILE)) {
    customize.preselectedOptions.colorsMenuTile = null;
  }
  customize.updateColorsMenuTileSelection($(customize.IDS.COLOR_PICKER_TILE));
  ntpApiHandle.applyAutogeneratedTheme(0, [r, g, b, 255]);
};

/**
 * Loads Colors menu elements and initializes the selected state.
 */
customize.loadColorsMenu = function() {
  if (!customize.colorsMenuLoaded) {
    customize.loadColorMenuTiles();
    customize.colorsMenuLoaded = true;
  }

  customize.resetColorsSelectedOptions();
  customize.colorsMenuOnThemeChange();
};

/**
 * Loads Colors menu tiles, e.g. color picker, default tile and color tiles.
 */
customize.loadColorMenuTiles = function() {
  const colorsColl = ntpApiHandle.getColorsInfo();
  for (let i = 0; i < colorsColl.length; ++i) {
    const id = 'color_' + i;
    const imageUrl = colorsColl[i].icon;
    const dataset = {'color': colorsColl[i].color, 'id': colorsColl[i].id};

    const tile = customize.createTileWithoutTitle(
        id, imageUrl, dataset, customize.colorTileInteraction,
        customize.tileOnKeyDownInteraction);
    tile.firstElementChild.setAttribute('aria-label', colorsColl[i].label);
    tile.firstElementChild.setAttribute('title', colorsColl[i].label);

    $(customize.IDS.COLORS_MENU).appendChild(tile);
  }

  // Configure the default tile.
  $(customize.IDS.COLORS_DEFAULT_ICON).onclick =
      customize.defaultThemeTileInteraction;
  $(customize.IDS.COLORS_DEFAULT_ICON).onkeydown =
      customize.tileOnKeyDownInteraction;

  // On arrow keys focus the first element.
  $(customize.IDS.COLORS_MENU).onkeydown = function(event) {
    if (document.activeElement === $(customize.IDS.COLORS_MENU) &&
        customize.arrowKeys.includes(event.keyCode)) {
      $(customize.IDS.COLOR_PICKER_TILE).focus();
      event.preventDefault();
    }
  };

  // Configure custom color picker.
  $(customize.IDS.COLOR_PICKER_TILE).onclick = function(event) {
    $(customize.IDS.COLOR_PICKER).value = customize.customColorPicked;
    $(customize.IDS.COLOR_PICKER).click();
  };
  $(customize.IDS.COLOR_PICKER_TILE).onkeydown =
      customize.tileOnKeyDownInteraction;
  $(customize.IDS.COLOR_PICKER).onchange = customize.colorPickerTileInteraction;
};

/**
 * Update webstore theme info and preselect Colors menu tile according to
 * the theme update.
 */
customize.colorsMenuOnThemeChange = function() {
  // Update webstore theme information.
  const ntpTheme = assert(ntpApiHandle.ntpTheme);
  if (ntpTheme.themeId && ntpTheme.themeName) {
    $(customize.IDS.COLORS_THEME).classList.add(customize.CLASSES.VISIBLE);
    $(customize.IDS.COLORS_THEME_NAME).innerHTML = ntpTheme.themeName;
    $(customize.IDS.COLORS_THEME_WEBSTORE_LINK).href =
        'https://chrome.google.com/webstore/detail/' + ntpTheme.themeId;
    $(customize.IDS.COLORS_THEME_UNINSTALL).onclick =
        ntpApiHandle.useDefaultTheme;

    // Clear the previous selection, if any.
    if (customize.selectedOptions.color) {
      customize.richerPicker_removeSelectedState(
          customize.selectedOptions.color);
      customize.selectedOptions.color = null;
    }
    if (!customize.preselectedOptions.colorsMenuTile) {
      customize.preselectedOptions.colorsMenuTile =
          $(customize.IDS.COLORS_THEME);
    }
  } else {
    $(customize.IDS.COLORS_THEME).classList.remove(customize.CLASSES.VISIBLE);

    // Select the tile corresponding to the current theme/color.
    customize.colorsMenuPreselectTile();
  }
};

/**
 * Preselect Colors menu tile according to the theme info.
 */
customize.colorsMenuPreselectTile = function() {
  const ntpTheme = assert(ntpApiHandle.ntpTheme);
  let tile;
  if (ntpTheme.usingDefaultTheme) {
    tile = $(customize.IDS.COLORS_DEFAULT_ICON);
  } else if (ntpTheme.colorId && ntpTheme.colorId > 0) {
    // Color from predefined set is selected.
    const tiles = Array.from(
        $(customize.IDS.COLORS_MENU)
            .getElementsByClassName(customize.CLASSES.COLLECTION_TILE));
    for (let i = 0; i < tiles.length; i++) {
      if (tiles[i].dataset && tiles[i].dataset.id == ntpTheme.colorId) {
        tile = tiles[i];
        break;
      }
    }
  } else if (
      ntpTheme.colorDark && ntpTheme.colorLight && ntpTheme.colorPicked) {
    // Custom color is selected.
    tile = $(customize.IDS.COLOR_PICKER_TILE);

    // Update color picker tile colors.
    customize.customColorPicked = colorArrayToHex(ntpTheme.colorPicked);
    $(customize.IDS.COLORS_MENU)
        .style.setProperty(
            '--custom-color-border', colorArrayToHex(ntpTheme.colorDark));
    $(customize.IDS.COLORS_MENU)
        .style.setProperty(
            '--custom-color-dark', colorArrayToHex(ntpTheme.colorDark));
    $(customize.IDS.COLORS_MENU)
        .style.setProperty(
            '--custom-color-light', colorArrayToHex(ntpTheme.colorLight));
    $(customize.IDS.COLOR_PICKER_ICON)
        .classList.toggle('white', ntpTheme.isNtpBackgroundDark);
  }

  if (tile && tile !== customize.selectedOptions.color) {
    if (!customize.preselectedOptions.colorsMenuTile) {
      customize.preselectedOptions.colorsMenuTile = tile;
    }

    customize.updateColorsMenuTileSelection(
        /** @type HTMLElement */ (tile));
  }
};

/**
 * Indicates whether a color other then preselected one was selected on Colors
 * menu.
 */
customize.isColorOptionSelected = function() {
  return customize.preselectedOptions.colorsMenuTile !==
      customize.selectedOptions.color;
};

/**
 * Indicates whether a background tile other then the preselected one was
 * selected on the backgrounds menu.
 */
customize.isBackgroundOptionSelected = function() {
  return customize.selectedOptions.backgroundData &&
      (!customize.preselectedOptions.backgroundsMenuTile ||
       customize.selectedOptions.backgroundData.id !=
           customize.preselectedOptions.backgroundsMenuTile.id);
};

/**
 * Permanently applies the color changes. Called when the done button is
 * pressed.
 */
customize.confirmColor = function() {
  ntpApiHandle.confirmThemeChanges();
};

/**
 * Reverts the applied (but not confirmed) color changes. Called when the cancel
 * button is pressed.
 */
customize.cancelColor = function() {
  ntpApiHandle.revertThemeChanges();
};

/**
 * Reset color picker to its default state.
 */
customize.resetColorPicker = function() {
  customize.customColorPicked = customize.defaultCustomColor;
  $(customize.IDS.COLOR_PICKER).value = null;
  $(customize.IDS.COLORS_MENU).style.setProperty('--custom-color-border', '');
  $(customize.IDS.COLORS_MENU).style.setProperty('--custom-color-dark', '');
  $(customize.IDS.COLORS_MENU).style.setProperty('--custom-color-light', '');
  $(customize.IDS.COLOR_PICKER_ICON).classList.toggle('white', false);
};

/**
 * Reset color selection.
 */
customize.resetColorsSelectedOptions = function() {
  customize.richerPicker_removeSelectedState(customize.selectedOptions.color);
  customize.selectedOptions.color = null;
  customize.preselectedOptions.colorsMenuTile = null;
};

/**
 * Converts an RGBA component into hex format.
 * @param {number} c RGBA component.
 * @return {string} RGBA component in hex format.
 */
function rgbComponentToHex(c) {
  const hex = c.toString(16);
  return hex.length == 1 ? '0' + hex : hex;
}

/**
 * Converts an Array of color components into hex format "#000000".
 * @param {Array<number>} color Array of RGBA color components.
 * @return {string} color in hex format.
 */
function colorArrayToHex(color) {
  if (!color || color.length != 4) {
    return '';
  }

  return '#' + rgbComponentToHex(color[0]) + rgbComponentToHex(color[1]) +
      rgbComponentToHex(color[2]);
}
