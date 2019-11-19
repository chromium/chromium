/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

// Single iframe for NTP tiles.

/**
 * Controls rendering the Most Visited iframe.
 * @return {Object} A limited interface for testing the iframe.
 */
function MostVisited() {
'use strict';

/**
 * Enum for key codes.
 * @enum {number}
 * @const
 */
const KEYCODES = {
  BACKSPACE: 8,
  DELETE: 46,
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
 * Enum for ids.
 * @enum {string}
 * @const
 */
const IDS = {
  MOST_VISITED: 'most-visited',  // Container for all tilesets.
  MV_TILES: 'mv-tiles',          // Most Visited tiles container.
};

/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
const CLASSES = {
  FAILED_FAVICON: 'failed-favicon',  // Applied when the favicon fails to load.
  GRID_TILE: 'grid-tile',
  GRID_TILE_CONTAINER: 'grid-tile-container',
  REORDER: 'reorder',  // Applied to the tile being moved while reordering.
  // Applied while we are reordering. Disables hover styling.
  REORDERING: 'reordering',
  MAC_CHROMEOS: 'mac-chromeos',  // Reduces font weight for MacOS and ChromeOS.
  // Material Design classes.
  MD_FALLBACK_LETTER: 'md-fallback-letter',
  MD_ICON: 'md-icon',
  MD_ADD_ICON: 'md-add-icon',
  MD_MENU: 'md-menu',
  MD_EDIT_MENU: 'md-edit-menu',
  MD_TILE: 'md-tile',
  MD_TILE_INNER: 'md-tile-inner',
  MD_TITLE: 'md-title',
};

/**
 * The different types of events that are logged from the NTP.  This enum is
 * used to transfer information from the NTP JavaScript to the renderer and is
 * not used as a UMA enum histogram's logged value.
 * Note: Keep in sync with common/ntp_logging_events.h
 * @enum {number}
 * @const
 */
const LOG_TYPE = {
  // All NTP tiles have finished loading (successfully or failing).
  NTP_ALL_TILES_LOADED: 11,
  // The 'Add shortcut' link was clicked.
  NTP_CUSTOMIZE_ADD_SHORTCUT_CLICKED: 44,
  // The 'Edit shortcut' link was clicked.
  NTP_CUSTOMIZE_EDIT_SHORTCUT_CLICKED: 45,
};

/**
 * The different (visual) types that an NTP tile can have.
 * Note: Keep in sync with components/ntp_tiles/tile_visual_type.h
 * @enum {number}
 * @const
 */
const TileVisualType = {
  NONE: 0,
  ICON_REAL: 1,
  ICON_COLOR: 2,
  ICON_DEFAULT: 3,
};

/**
 * Timeout delay for the window.onresize event throttle. Set to 15 frame per
 * second.
 * @const {number}
 */
const RESIZE_TIMEOUT_DELAY = 66;

/**
 * Maximum number of tiles if custom links is enabled.
 * @const {number}
 */
const MD_MAX_NUM_CUSTOM_LINK_TILES = 10;

/**
 * Maximum number of tiles if Most Visited is enabled.
 * @const {number}
 */
const MD_MAX_NUM_MOST_VISITED_TILES = 8;

/**
 * Maximum number of tiles per row for Material Design.
 * @const {number}
 */
const MD_MAX_TILES_PER_ROW = 5;

/**
 * Height of a tile for Material Design. Keep in sync with
 * most_visited_single.css.
 * @const {number}
 */
const MD_TILE_HEIGHT = 128;

/**
 * Width of a tile for Material Design. Keep in sync with
 * most_visited_single.css.
 * @const {number}
 */
const MD_TILE_WIDTH = 112;

/**
 * Number of tiles that will always be visible for Material Design. Calculated
 * by dividing minimum |--content-width| (see local_ntp.css) by |MD_TILE_WIDTH|
 * and multiplying by 2 rows.
 * @const {number}
 */
const MD_NUM_TILES_ALWAYS_VISIBLE = 6;

/**
 * The origin of this request, i.e. 'chrome-search://local-ntp' for the local
 * NTP.
 * @const {string}
 */
const DOMAIN_ORIGIN = '{{ORIGIN}}';

/**
 * Counter for DOM elements that we are waiting to finish loading. Starts out
 * at 1 because initially we're waiting for the "show" message from the parent.
 * @type {number}
 */
let loadedCounter = 1;

/**
 * DOM element containing the tiles we are going to present next.
 * Works as a double-buffer that is shown when we receive a "show" postMessage.
 * @type {Element}
 */
let tiles = null;

/**
 * List of parameters passed by query args.
 * @type {Object}
 */
let queryArgs = {};

/**
 * True if the custom links feature is enabled, i.e. when this is a Google NTP.
 * Set when the iframe is initialized.
 * @type {boolean}
 */
let customLinksFeatureEnabled = false;

/**
 * The current grid of tiles.
 * @type {?Grid}
 */
let currGrid = null;

/**
 * Additional API for Array. Moves the item at index |from| to index |to|.
 * @param {number} from Index of the item to move.
 * @param {number} to Index to move the item to.
 */
Array.prototype.move = function(from, to) {
  this.splice(to, 0, this.splice(from, 1)[0]);
};

/**
 * Class that handles layouts and animations for the tile grid. This includes
 * animations for adding, deleting, and reordering.
 */
class Grid {
  constructor() {
    /** @private {number} */
    this.tileHeight_ = 0;
    /** @private {number} */
    this.tileWidth_ = 0;
    /** @private {number} */
    this.tilesAlwaysVisible_ = 0;
    /**
     * The maximum number of tiles per row allowed by the grid parameters.
     * @private {number}
     */
    this.maxTilesPerRow_ = 0;
    /** @private {number} */
    this.maxTiles_ = 0;

    /** @private {number} */
    this.gridWidth_ = 0;
    /**
     * The maximum number of tiles per row allowed by the window width.
     * @private {number}
     */
    this.maxTilesPerRowWindow_ = 0;

    /** @private {?Element} */
    this.container_ = null;
    /** @private {?HTMLCollection} */
    this.tiles_ = null;

    /**
     * Array that stores the {x,y} positions of the tile layout.
     * @private {?Array<!Object>}
     */
    this.position_ = null;

    /**
     * Stores the current order of the tiles. Index corresponds to grid
     * position, while value is the index of the tile in |this.tiles_|.
     * @private {?Array<number>}
     */
    this.order_ = null;

    /** @private {number} The index of the tile we're reordering. */
    this.itemToReorder_ = -1;
    /** @private {number} The index to move the tile we're reordering to. */
    this.newIndexOfItemToReorder_ = -1;

    /** @private {boolean} True if the user is currently touching a tile. */
    this.touchStarted_ = false;
  }

  /**
   * Sets up the grid for the new tileset in |container|. The old tileset is
   * discarded.
   * @param {!Element} container The grid container element.
   * @param {Object=} params Customizable parameters for the grid. Used in
   *     testing.
   */
  init(container, params = {}) {
    this.container_ = container;

    this.tileHeight_ = params.tileHeight || MD_TILE_HEIGHT;
    this.tileWidth_ = params.tileWidth || MD_TILE_WIDTH;
    this.tilesAlwaysVisible_ =
        params.tilesAlwaysVisible || MD_NUM_TILES_ALWAYS_VISIBLE;
    this.maxTilesPerRow_ = params.maxTilesPerRow || MD_MAX_TILES_PER_ROW;
    this.maxTiles_ = params.maxTiles || getMaxNumTiles();

    this.maxTilesPerRowWindow_ = this.getMaxTilesPerRow_();

    this.tiles_ =
        this.container_.getElementsByClassName(CLASSES.GRID_TILE_CONTAINER);
    // Ignore any tiles past the maximum allowed.
    this.position_ = new Array(this.maxTiles_);
    this.order_ = new Array(this.maxTiles_);
    for (let i = 0; i < this.maxTiles_; i++) {
      this.position_[i] = {x: 0, y: 0};
      this.order_[i] = i;
    }

    if (isCustomLinksEnabled() || params.enableReorder) {
      // Set up reordering for all tiles except the add shortcut button.
      for (let i = 0; i < this.tiles_.length; i++) {
        if (this.tiles_[i].getAttribute('add') !== 'true') {
          this.setupReorder_(this.tiles_[i], i);
        }
      }
    }

    this.updateLayout();
  }

  /**
   * Returns a grid tile wrapper that contains |tile|.
   * @param {!Element} tile The tile element.
   * @param {number} rid The tile's restricted id.
   * @param {boolean} isAddButton True if this is the add shortcut button.
   * @return {!Element} A grid tile wrapper.
   */
  createGridTile(tile, rid, isAddButton) {
    const gridTileContainer = document.createElement('div');
    gridTileContainer.className = CLASSES.GRID_TILE_CONTAINER;
    gridTileContainer.setAttribute('rid', rid);
    gridTileContainer.setAttribute('add', isAddButton);
    const gridTile = document.createElement('div');
    gridTile.className = CLASSES.GRID_TILE;
    gridTile.appendChild(tile);
    gridTileContainer.appendChild(gridTile);
    return gridTileContainer;
  }

  /**
   * Updates the layout of the tiles. This is called for new tilesets and when
   * the window is resized or zoomed. Translates each tile's
   * |CLASSES.GRID_TILE_CONTAINER| to the correct position.
   */
  updateLayout() {
    const tilesPerRow = this.getTilesPerRow_();

    this.gridWidth_ = tilesPerRow * this.tileWidth_;
    this.container_.style.width = this.gridWidth_ + 'px';

    const maxVisibleTiles = tilesPerRow * 2;
    let x = 0;
    let y = 0;
    for (let i = 0; i < this.tiles_.length; i++) {
      const tile = this.tiles_[i];
      // Reset the offset for row 2.
      if (i === tilesPerRow) {
        x = this.getRow2Offset_(tilesPerRow);
        y = this.tileHeight_;
      }
      // Update the tile's position.
      this.translate_(tile, x, y);
      this.position_[i].x = x;
      this.position_[i].y = y;
      x += this.tileWidth_;  // Increment for the next tile.

      // Update visibility for tiles that may be hidden by the iframe border in
      // order to prevent keyboard navigation from reaching them. Ignores tiles
      // that will always be visible, since changing 'display' prevents
      // transitions from working.
      if (i >= this.tilesAlwaysVisible_) {
        const isVisible = i < maxVisibleTiles;
        tile.style.display = isVisible ? 'block' : 'none';
      }
    }
  }

  /**
   * Called when the window is resized/zoomed. Recalculates maximums for the new
   * window size and calls |updateLayout| if necessary.
   */
  onResize() {
    // Update the layout if the max number of tiles per row changes due to the
    // new window size.
    const maxPerRowWindow = this.getMaxTilesPerRow_();
    if (maxPerRowWindow !== this.maxTilesPerRowWindow_) {
      this.maxTilesPerRowWindow_ = maxPerRowWindow;
      this.updateLayout();
    }
  }

  /**
   * Returns the number of tiles per row. This may be balanced in order to make
   * even rows.
   * @return {number} The number of tiles per row.
   * @private
   */
  getTilesPerRow_() {
    const maxTilesPerRow =
        Math.min(this.maxTilesPerRow_, this.maxTilesPerRowWindow_);
    if (this.tiles_.length >= maxTilesPerRow * 2) {
      // We have enough for two full rows, so just return the max.
      return maxTilesPerRow;
    } else if (this.tiles_.length > maxTilesPerRow) {
      // We have have a little more than one full row, so we need to rebalance.
      return Math.ceil(this.tiles_.length / 2);
    } else {
      // We have (less than) a full row, so just return the tiles we have.
      return this.tiles_.length;
    }
  }

  /**
   * Returns the maximum number of tiles per row allowed by the window size.
   * @return {number} The maximum number of tiles per row.
   * @private
   */
  getMaxTilesPerRow_() {
    return Math.floor(window.innerWidth / this.tileWidth_);
  }

  /**
   * Returns row 2's x offset from row 1 in px. This will either be 0 or half a
   * tile length.
   * @param {number} tilesPerRow The number of tiles per row.
   * @return {number} The offset for row 2.
   * @private
   */
  getRow2Offset_(tilesPerRow) {
    // An odd number of tiles requires a half tile offset in the second row,
    // unless both rows are full (i.e. for smaller window widths).
    if (this.tiles_.length % 2 === 1 && this.tiles_.length / tilesPerRow < 2) {
      return Math.round(this.tileWidth_ / 2);
    }
    return 0;
  }

  /**
   * Returns true if the browser is in RTL.
   * @return {boolean}
   * @private
   */
  isRtl_() {
    return document.documentElement.dir === 'rtl';
  }

  /**
   * Translates the |element| by (x, y).
   * @param {?Element} element The element to apply the transform to.
   * @param {number} x The x value.
   * @param {number} y The y value.
   * @private
   */
  translate_(element, x, y) {
    if (!element) {
      throw new Error('Invalid element: cannot apply transform');
    }
    const rtlX = x * (this.isRtl_() ? -1 : 1);
    element.style.transform = 'translate(' + rtlX + 'px, ' + y + 'px)';
  }

  /**
   * Sets up event listeners necessary for tile reordering.
   * @param {!Element} tile Tile on which to set the event listeners.
   * @param {number} index The tile's index.
   * @private
   */
  setupReorder_(tile, index) {
    tile.setAttribute('index', index);

    // Set up mouse support.
    // Listen for the drag event on the tile instead of the tile container. The
    // tile container remains static during the reorder flow.
    tile.firstChild.draggable = true;
    // Prevent default drag events on the shortcut link.
    const tileItem = tile.firstChild.firstChild;
    tileItem.draggable = false;
    tile.firstChild.addEventListener('dragstart', (event) => {
      // Support link dragging (i.e. dragging the URL to the omnibox).
      event.dataTransfer.setData('text/uri-list', tileItem.href);
      // Remove the ghost image that appears when dragging.
      const emptyImg = new Image();
      event.dataTransfer.setDragImage(emptyImg, 0, 0);

      this.startReorder_(tile, event, /*mouseMode=*/ true);
    });
    // Show a 'move' cursor while dragging the tile within the grid bounds. This
    // is mostly intended for Windows, which will otherwise show a 'prohibited'
    // cursor.
    tile.addEventListener('dragover', (event) => {
      event.preventDefault();
      event.dataTransfer.dropEffect = 'move';
    });

    // Set up touch support.
    tile.firstChild.addEventListener('touchstart', (startEvent) => {
      // Ignore subsequent touchstart events, which can be triggered if a
      // different finger is placed on this tile.
      if (this.touchStarted_) {
        return;
      }
      this.touchStarted_ = true;

      // Start the reorder flow once the user moves their finger.
      const startReorder = (moveEvent) => {
        // Use the cursor position from 'touchstart' as the starting location.
        this.startReorder_(tile, startEvent, /*mouseMode=*/ false);
      };
      // Insert the held tile at the index we are hovering over.
      const moveOver = (moveEvent) => {
        // Touch events do not have a 'mouseover' equivalent, so we need to
        // manually check if we are hovering over a tile. If so, insert the held
        // tile there.
        // Note: The first item in |changedTouches| is the current position.
        const x = moveEvent.changedTouches[0].pageX;
        const y = moveEvent.changedTouches[0].pageY;
        this.reorderToIndexAtPoint_(x, y);
      };
      // Allow 'touchstart' events again when reordering stops/was never
      // started.
      const touchEnd = (endEvent) => {
        tile.firstChild.removeEventListener('touchmove', startReorder);
        tile.firstChild.removeEventListener('touchmove', moveOver);
        tile.firstChild.removeEventListener('touchend', touchEnd);
        tile.firstChild.removeEventListener('touchcancel', touchEnd);
        this.touchStarted_ = false;
      };

      tile.firstChild.addEventListener('touchmove', startReorder, {once: true});
      tile.firstChild.addEventListener('touchmove', moveOver);
      tile.firstChild.addEventListener('touchend', touchEnd, {once: true});
      tile.firstChild.addEventListener('touchcancel', touchEnd, {once: true});
    });
  }

  /**
   * Starts the reorder flow. Updates the visual style of the held tile to
   * indicate that it is being moved and sets up the relevant event listeners.
   * @param {!Element} tile Tile that is being moved.
   * @param {!Event} event The 'dragstart'/'touchmove' event. Used to obtain the
   *     current cursor position
   * @param {boolean} mouseMode True if the user is using a mouse.
   * @private
   */
  startReorder_(tile, event, mouseMode) {
    const index = Number(tile.getAttribute('index'));

    this.itemToReorder_ = index;
    this.newIndexOfItemToReorder_ = index;

    // Apply reorder styling.
    tile.classList.add(CLASSES.REORDER);
    // Disable other hover/active styling for all tiles.
    document.body.classList.add(CLASSES.REORDERING);

    // Set up event listeners for the reorder flow. Listen for drag events if
    // |mouseMode|, touch events otherwise.
    if (mouseMode) {
      const trackCursor =
          this.trackCursor_(tile, event.pageX, event.pageY, true);
      // The 'dragover' event must be tracked at the document level, since the
      // currently dragged tile will interfere with 'dragover' events on the
      // other tiles.
      const dragOver = (dragOverEvent) => {
        trackCursor(dragOverEvent);
        // Since the 'dragover' event is not tied to a specific tile, we need to
        // manually check if we are hovering over a tile. If so, insert the held
        // tile there.
        this.reorderToIndexAtPoint_(dragOverEvent.pageX, dragOverEvent.pageY);
      };
      document.addEventListener('dragover', dragOver);
      document.addEventListener('dragend', () => {
        document.removeEventListener('dragover', dragOver);
        this.stopReorder_(tile);
      }, {once: true});
    } else {
      // Track the cursor on subsequent 'touchmove' events (the first
      // 'touchmove' event that starts the reorder flow is ignored).
      const trackCursor = this.trackCursor_(
          tile, event.changedTouches[0].pageX, event.changedTouches[0].pageY,
          false);
      const touchEnd = (touchEndEvent) => {
        tile.firstChild.removeEventListener('touchmove', trackCursor);
        tile.firstChild.removeEventListener('touchend', touchEnd);
        tile.firstChild.removeEventListener('touchcancel', touchEnd);
        this.stopReorder_(tile);  // Stop the reorder flow.
      };
      tile.firstChild.addEventListener('touchmove', trackCursor);
      tile.firstChild.addEventListener('touchend', touchEnd, {once: true});
      tile.firstChild.addEventListener('touchcancel', touchEnd, {once: true});
    }
  }

  /**
   * Stops the reorder flow. Resets the held tile's visual style and tells the
   * EmbeddedSearchAPI that a tile has been moved.
   * @param {!Element} tile Tile that has been moved.
   * @private
   */
  stopReorder_(tile) {
    const index = Number(tile.getAttribute('index'));

    // Remove reorder styling.
    tile.classList.remove(CLASSES.REORDER);
    document.body.classList.remove(CLASSES.REORDERING);

    // Move the tile to its new position and notify EmbeddedSearchAPI that the
    // tile has been moved.
    this.applyReorder_(tile, this.newIndexOfItemToReorder_);
    chrome.embeddedSearch.newTabPage.reorderCustomLink(
        Number(this.tiles_[index].getAttribute('rid')),
        this.newIndexOfItemToReorder_);

    this.itemToReorder_ = -1;
    this.newIndexOfItemToReorder_ = -1;
  }

  /**
   * Attempts to insert the currently held tile at the index located at (x, y).
   * Does nothing if there is no tile at (x, y) or the reorder flow is not
   * ongoing.
   * @param {number} x The x coordinate.
   * @param {number} y The y coordinate.
   * @private
   */
  reorderToIndexAtPoint_(x, y) {
    const elements = document.elementsFromPoint(x, y);
    for (let i = 0; i < elements.length; i++) {
      if (elements[i].classList.contains(CLASSES.GRID_TILE_CONTAINER) &&
          elements[i].getAttribute('index') !== null) {
        this.reorderToIndex_(Number(elements[i].getAttribute('index')));
        return;
      }
    }
  }

  /**
   * Executed only when the reorder flow is ongoing. Inserts the currently held
   * tile at |index| and shifts tiles accordingly.
   * @param {number} index The index to insert the held tile at.
   * @private
   */
  reorderToIndex_(index) {
    if (this.newIndexOfItemToReorder_ === index ||
        !document.body.classList.contains(CLASSES.REORDERING)) {
      return;
    }

    // Moves the held tile from its current position to |index|.
    this.order_.move(this.newIndexOfItemToReorder_, index);
    this.newIndexOfItemToReorder_ = index;
    // Shift tiles according to the new order.
    for (let i = 0; i < this.tiles_.length; i++) {
      const tileIndex = this.order_[i];
      // Don't move the tile we're holding nor the add shortcut button.
      if (tileIndex === this.itemToReorder_ ||
          this.tiles_[i].getAttribute('add') === 'true') {
        continue;
      }
      this.applyReorder_(this.tiles_[tileIndex], i);
    }
  }

  /**
   * Translates the |tile|'s |CLASSES.GRID_TILE| from |index| to |newIndex|.
   * This is done to prevent interference with event listeners on the |tile|'s
   * |CLASSES.GRID_TILE_CONTAINER|, particularly 'mouseover'.
   * @param {!Element} tile Tile that is being shifted.
   * @param {number} newIndex New index for the tile.
   * @private
   */
  applyReorder_(tile, newIndex) {
    if (tile.getAttribute('index') === null) {
      throw new Error('Tile does not have an index.');
    }
    const index = Number(tile.getAttribute('index'));
    const x = this.position_[newIndex].x - this.position_[index].x;
    const y = this.position_[newIndex].y - this.position_[index].y;
    this.translate_(tile.children[0], x, y);
  }

  /**
   * Moves |tile| so that it tracks the cursor's position. This is done by
   * translating the |tile|'s |CLASSES.GRID_TILE|, which prevents interference
   * with event listeners on the |tile|'s |CLASSES.GRID_TILE_CONTAINER|.
   * @param {!Element} tile Tile that is being moved.
   * @param {number} origCursorX Original x cursor position.
   * @param {number} origCursorY Original y cursor position.
   * @param {boolean} mouseMode True if the user is using a mouse.
   * @private
   */
  trackCursor_(tile, origCursorX, origCursorY, mouseMode) {
    const index = Number(tile.getAttribute('index'));
    // RTL positions align with the right side of the grid. Therefore, the x
    // value must be recalculated to align with the left.
    const origPosX = this.isRtl_() ?
        (this.gridWidth_ - (this.position_[index].x + this.tileWidth_)) :
        this.position_[index].x;
    const origPosY = this.position_[index].y;

    // Get the max translation allowed by the grid boundaries. This will be the
    // x of the last tile in a row and the y of the tiles in the second row.
    const maxTranslateX = this.gridWidth_ - this.tileWidth_;
    const maxTranslateY = this.tileHeight_;

    const maxX = maxTranslateX - origPosX;
    const maxY = maxTranslateY - origPosY;
    const minX = 0 - origPosX;
    const minY = 0 - origPosY;

    return (event) => {
      const currX = mouseMode ? event.pageX : event.changedTouches[0].pageX;
      const currY = mouseMode ? event.pageY : event.changedTouches[0].pageY;
      // Do not exceed the iframe borders.
      const x = Math.max(Math.min(currX - origCursorX, maxX), minX);
      const y = Math.max(Math.min(currY - origCursorY, maxY), minY);
      tile.firstChild.style.transform = 'translate(' + x + 'px, ' + y + 'px)';
    };
  }
}

/**
 * Log an event on the NTP.
 * @param {number} eventType Event from LOG_TYPE.
 */
function logEvent(eventType) {
  chrome.embeddedSearch.newTabPage.logEvent(eventType);
}

/**
 * Log impression of an NTP tile.
 * @param {number} tileIndex Position of the tile, >= 0 and < getMaxNumTiles().
 * @param {number} tileTitleSource The source of the tile's title as received
 *     from getMostVisitedItemData.
 * @param {number} tileSource The tile's source as received from
 *     getMostVisitedItemData.
 * @param {number} tileType The tile's visual type from TileVisualType.
 * @param {Date} dataGenerationTime Timestamp representing when the tile was
 *     produced by a ranking algorithm.
 */
function logMostVisitedImpression(
    tileIndex, tileTitleSource, tileSource, tileType, dataGenerationTime) {
  chrome.embeddedSearch.newTabPage.logMostVisitedImpression(
      tileIndex, tileTitleSource, tileSource, tileType, dataGenerationTime);
}

/**
 * Log click on an NTP tile.
 * @param {number} tileIndex Position of the tile, >= 0 and < getMaxNumTiles().
 * @param {number} tileTitleSource The source of the tile's title as received
 *     from getMostVisitedItemData.
 * @param {number} tileSource The tile's source as received from
 *     getMostVisitedItemData.
 * @param {number} tileType The tile's visual type from TileVisualType.
 * @param {Date} dataGenerationTime Timestamp representing when the tile was
 *     produced by a ranking algorithm.
 */
function logMostVisitedNavigation(
    tileIndex, tileTitleSource, tileSource, tileType, dataGenerationTime) {
  chrome.embeddedSearch.newTabPage.logMostVisitedNavigation(
      tileIndex, tileTitleSource, tileSource, tileType, dataGenerationTime);
}

/**
 * Returns true if custom links are enabled.
 * @return {boolean}
 */
function isCustomLinksEnabled() {
  return customLinksFeatureEnabled &&
      !chrome.embeddedSearch.newTabPage.isUsingMostVisited;
}

/**
 * Returns the maximum number of tiles to show at any time. This can be changed
 * depending on what feature is enabled.
 * @return {number}
 */
function getMaxNumTiles() {
  return isCustomLinksEnabled() ? MD_MAX_NUM_CUSTOM_LINK_TILES :
                                  MD_MAX_NUM_MOST_VISITED_TILES;
}

/**
 * Down counts the DOM elements that we are waiting for the page to load.
 * When we get to 0, we send a message to the parent window.
 * This is usually used as an EventListener of onload/onerror.
 */
function countLoad() {
  loadedCounter -= 1;
  if (loadedCounter <= 0) {
    swapInNewTiles();
    logEvent(LOG_TYPE.NTP_ALL_TILES_LOADED);
    let tilesAreCustomLinks = isCustomLinksEnabled() &&
        chrome.embeddedSearch.newTabPage.isCustomLinks;
    // Tell the parent page whether to show the restore default shortcuts option
    // in the menu.
    window.parent.postMessage(
        {cmd: 'loaded', showRestoreDefault: tilesAreCustomLinks},
        DOMAIN_ORIGIN);
    tilesAreCustomLinks = false;
    // Reset to 1, so that any further 'show' message will cause us to swap in
    // fresh tiles.
    loadedCounter = 1;
  }
}

/**
 * Handles postMessages coming from the host page to the iframe.
 * Mostly, it dispatches every command to handleCommand.
 */
function handlePostMessage(event) {
  if (event.data instanceof Array) {
    for (let i = 0; i < event.data.length; ++i) {
      handleCommand(event.data[i]);
    }
  } else {
    handleCommand(event.data);
  }
}

/**
 * Handles a single command coming from the host page to the iframe.
 * We try to keep the logic here to a minimum and just dispatch to the relevant
 * functions.
 */
function handleCommand(data) {
  const cmd = data.cmd;

  if (cmd == 'tile') {
    addTile(data);
  } else if (cmd == 'show') {
    // TODO(crbug.com/946225): If this happens before we have finished loading
    // the previous tiles, we probably get into a bad state. If/when the iframe
    // is removed this might no longer be a concern.
    showTiles();
  } else if (cmd == 'updateTheme') {
    updateTheme(data);
  } else if (cmd === 'focusMenu') {
    focusTileMenu(data);
  } else {
    console.error('Unknown command: ' + JSON.stringify(data));
  }
}

/**
 * Handler for the 'show' message from the host page.
 */
function showTiles() {
  utils.setPlatformClass(document.body);
  countLoad();
}

/**
 * Handler for the 'updateTheme' message from the host page.
 * @param {!Object} info Data received in the message.
 */
function updateTheme(info) {
  document.body.style.setProperty('--tile-title-color', info.tileTitleColor);
  document.body.style.setProperty(
      '--icon-background-color', info.iconBackgroundColor);
  document.body.classList.toggle('dark-theme', info.isThemeDark);
  document.body.classList.toggle('use-title-container', info.useTitleContainer);
  document.body.classList.toggle('custom-background', info.customBackground);
  document.body.classList.toggle('use-white-add-icon', info.useWhiteAddIcon);

  // Reduce font weight on the default(white) background for Mac and CrOS.
  document.body.classList.toggle(
      CLASSES.MAC_CHROMEOS,
      !info.isThemeDark && !info.useTitleContainer &&
          (navigator.userAgent.indexOf('Mac') > -1 ||
           navigator.userAgent.indexOf('CrOS') > -1));
}

/**
 * Handler for 'focusMenu' message from the host page. Focuses the edited tile's
 * menu or the add shortcut tile after closing the custom link edit dialog
 * without saving.
 * @param {!Object} info Data received in the message.
 */
function focusTileMenu(info) {
  const tile = document.querySelector(`a.md-tile[data-rid="${info.rid}"]`);
  if (info.rid === -1 /* Add shortcut tile */) {
    tile.focus();
  } else {
    tile.parentNode.childNodes[1].focus();
  }
}

/**
 * Removes all old instances of |IDS.MV_TILES| that are pending for deletion.
 */
function removeAllOldTiles() {
  const parent = document.querySelector('#' + IDS.MOST_VISITED);
  const oldList = parent.querySelectorAll('.mv-tiles-old');
  for (let i = 0; i < oldList.length; ++i) {
    parent.removeChild(oldList[i]);
  }
}

/**
 * Called when all tiles have finished loading (successfully or not), and we are
 * ready to show the new tiles and drop the old ones.
 */
function swapInNewTiles() {
  // Store the tiles on the current closure.
  const cur = tiles;

  // Add an "add new custom link" button if we haven't reached the maximum
  // number of tiles.
  if (isCustomLinksEnabled() && cur.childNodes.length < getMaxNumTiles()) {
    const data = {
      'rid': -1,
      'title': queryArgs['addLink'],
      'url': '',
      'isAddButton': true,
      'dataGenerationTime': new Date(),
      'tileSource': -1,
      'tileTitleSource': -1
    };
    tiles.appendChild(renderTile(data));
  }

  const parent = document.querySelector('#' + IDS.MOST_VISITED);

  const old = parent.querySelector('#' + IDS.MV_TILES);
  if (old) {
    // Mark old tile DIV for removal after the transition animation is done.
    old.removeAttribute('id');
    old.classList.add('mv-tiles-old');
    old.style.opacity = 0.0;
    cur.addEventListener('transitionend', function(ev) {
      if (ev.target === cur) {
        removeAllOldTiles();
      }
    });
  }

  // Add new tileset.
  cur.id = IDS.MV_TILES;
  parent.appendChild(cur);

  // Initialize the new tileset before modifying opacity. This will prevent the
  // transform transition from applying after the tiles fade in.
  currGrid.init(cur);

  const flushOpacity = () => window.getComputedStyle(cur).opacity;

  // getComputedStyle causes the initial style (opacity 0) to be applied, so
  // that when we then set it to 1, that triggers the CSS transition.
  flushOpacity();
  cur.style.opacity = 1.0;

  // Make sure the tiles variable contain the next tileset we'll use if the host
  // page sends us an updated set of tiles.
  tiles = document.createElement('div');
}

/**
 * Explicitly hide tiles that are not visible in order to prevent keyboard
 * navigation.
 */
function updateTileVisibility() {
  const allTiles =
      document.querySelectorAll('#' + IDS.MV_TILES + ' .' + CLASSES.MD_TILE);
  if (allTiles.length === 0) {
    return;
  }

  // Get the current number of tiles per row. Hide any tile after the first two
  // rows.
  const tilesPerRow = Math.trunc(document.body.offsetWidth / MD_TILE_WIDTH);
  for (let i = MD_NUM_TILES_ALWAYS_VISIBLE; i < allTiles.length; i++) {
    allTiles[i].style.display = (i < tilesPerRow * 2) ? 'block' : 'none';
  }
}

/**
 * Handler for the 'show' message from the host page, called when it wants to
 * add a suggestion tile.
 * @param {!MostVisitedData} args Data for the tile to be rendered.
 */
function addTile(args) {
  if (!isFinite(args.rid)) {
    return;
  }

  // Grab the tile's data from the embeddedSearch API.
  const data =
      chrome.embeddedSearch.newTabPage.getMostVisitedItemData(args.rid);
  if (!data) {
    return;
  }

  if (!data.faviconUrl) {
    data.faviconUrl = 'chrome-search://favicon/size/16@' +
        window.devicePixelRatio + 'x/' + data.renderViewId + '/' + data.rid;
  }
  tiles.appendChild(renderTile(data));
}

/**
 * Called when the user decided to add a tile to the blacklist.
 * It sets off the animation for the blacklist and sends the blacklisted id
 * to the host page.
 * @param {Element} tile DOM node of the tile we want to remove.
 */
function blacklistTile(tile) {
  const rid = Number(tile.getAttribute('data-rid'));

  if (isCustomLinksEnabled()) {
    chrome.embeddedSearch.newTabPage.deleteMostVisitedItem(rid);
  } else {
    tile.classList.add('blacklisted');
    tile.addEventListener('transitionend', function(ev) {
      if (ev.propertyName != 'width') {
        return;
      }
      window.parent.postMessage(
          {cmd: 'tileBlacklisted', rid: Number(rid)}, DOMAIN_ORIGIN);
    });
  }
}

/**
 * Starts edit custom link flow. Tells host page to show the edit custom link
 * dialog and pre-populate it with data obtained using the link's id.
 * @param {?number} rid Restricted id of the tile we want to edit.
 */
function editCustomLink(rid) {
  window.parent.postMessage({cmd: 'startEditLink', rid: rid}, DOMAIN_ORIGIN);
}

/**
 * Renders a MostVisited tile (i.e. shortcut) to the DOM.
 * @param {!MostVisitedData} data Object containing rid, url, title, favicon,
 *     and optionally isAddButton. isAddButton is true if you want to construct
 *     an add custom link button, and can only be set if custom links is
 *     enabled.
 * @return {Element}
 */
function renderTile(data) {
  const mdTile = document.createElement('a');
  mdTile.className = CLASSES.MD_TILE;

  // The tile will be appended to |tiles|.
  const position = tiles.children.length;
  // This is set in the load/error event for the favicon image.
  let tileType = TileVisualType.NONE;

  mdTile.setAttribute('data-rid', data.rid);
  mdTile.setAttribute('data-pos', position);
  if (utils.isSchemeAllowed(data.url)) {
    mdTile.href = data.url;
  }
  mdTile.setAttribute('aria-label', data.title);
  mdTile.title = data.title;

  mdTile.addEventListener('click', function(ev) {
    if (data.isAddButton) {
      editCustomLink(null);
      logEvent(LOG_TYPE.NTP_CUSTOMIZE_ADD_SHORTCUT_CLICKED);
    } else {
      logMostVisitedNavigation(
          position, data.tileTitleSource, data.tileSource, tileType,
          data.dataGenerationTime);
    }
  });
  mdTile.addEventListener('keydown', function(event) {
    if ((event.keyCode === KEYCODES.DELETE ||
         event.keyCode === KEYCODES.BACKSPACE) &&
        !data.isAddButton) {
      event.preventDefault();
      event.stopPropagation();
      blacklistTile(mdTile);
    } else if (
        event.keyCode === KEYCODES.ENTER || event.keyCode === KEYCODES.SPACE) {
      event.preventDefault();
      this.click();
    } else if (event.keyCode === KEYCODES.LEFT) {
      const tiles = document.querySelectorAll(
          '#' + IDS.MV_TILES + ' .' + CLASSES.MD_TILE);
      tiles[Math.max(Number(this.getAttribute('data-pos')) - 1, 0)].focus();
    } else if (event.keyCode === KEYCODES.RIGHT) {
      const tiles = document.querySelectorAll(
          '#' + IDS.MV_TILES + ' .' + CLASSES.MD_TILE);
      tiles[Math.min(
                Number(this.getAttribute('data-pos')) + 1, tiles.length - 1)]
          .focus();
    }
  });
  utils.disableOutlineOnMouseClick(mdTile);

  const mdTileInner = document.createElement('div');
  mdTileInner.className = CLASSES.MD_TILE_INNER;

  if (data.isAddButton) {
    mdTile.tabIndex = 0;

    const mdIconAdd = document.createElement('div');
    mdIconAdd.classList.add(CLASSES.MD_ICON);
    mdIconAdd.classList.add(CLASSES.MD_ADD_ICON);

    mdTileInner.appendChild(mdIconAdd);
  } else {
    const mdIcon = document.createElement('img');
    mdIcon.classList.add(CLASSES.MD_ICON);
    // Set title and alt to empty so screen readers won't say the image name.
    mdIcon.title = '';
    mdIcon.alt = '';
    const url = new URL('chrome-search://ntpicon/');
    url.searchParams.set('size', '24@' + window.devicePixelRatio + 'x');
    url.searchParams.set('url', data.url);
    mdIcon.src = url.toString();
    loadedCounter += 1;
    mdIcon.addEventListener('load', function(ev) {
      // Store the type for a potential later navigation.
      tileType = TileVisualType.ICON_REAL;
      logMostVisitedImpression(
          position, data.tileTitleSource, data.tileSource, tileType,
          data.dataGenerationTime);
      // Note: It's important to call countLoad last, because that might emit
      // the NTP_ALL_TILES_LOADED event, which must happen after the impression
      // log.
      countLoad();
    });
    mdIcon.addEventListener('error', function(ev) {
      const fallbackBackground = document.createElement('div');
      fallbackBackground.className = CLASSES.MD_ICON;
      const fallbackLetter = document.createElement('div');
      fallbackLetter.className = CLASSES.MD_FALLBACK_LETTER;
      fallbackLetter.textContent = data.title.charAt(0).toUpperCase();
      fallbackBackground.classList.add(CLASSES.FAILED_FAVICON);

      fallbackBackground.appendChild(fallbackLetter);
      mdTileInner.replaceChild(fallbackBackground, mdIcon);

      // Store the type for a potential later navigation.
      tileType = TileVisualType.ICON_DEFAULT;
      logMostVisitedImpression(
          position, data.tileTitleSource, data.tileSource, tileType,
          data.dataGenerationTime);
      // Note: It's important to call countLoad last, because that might emit
      // the NTP_ALL_TILES_LOADED event, which must happen after the impression
      // log.
      countLoad();
    });

    mdTileInner.appendChild(mdIcon);
  }

  const mdTitle = document.createElement('div');
  mdTitle.className = CLASSES.MD_TITLE;
  mdTitle.style.direction = data.direction || 'ltr';
  const mdTitleTextwrap = document.createElement('span');
  mdTitleTextwrap.innerText = data.title;
  mdTitle.appendChild(mdTitleTextwrap);
  mdTileInner.appendChild(mdTitle);
  mdTile.appendChild(mdTileInner);

  if (!data.isAddButton) {
    const mdMenu = document.createElement('button');
    mdMenu.className = CLASSES.MD_MENU;
    if (isCustomLinksEnabled()) {
      mdMenu.classList.add(CLASSES.MD_EDIT_MENU);
      mdMenu.title = queryArgs['editLinkTooltip'] || '';
      mdMenu.setAttribute(
          'aria-label',
          (queryArgs['editLinkTooltip'] || '') + ' ' + data.title);
      mdMenu.addEventListener('click', function(ev) {
        editCustomLink(data.rid);
        ev.preventDefault();
        ev.stopPropagation();
        logEvent(LOG_TYPE.NTP_CUSTOMIZE_EDIT_SHORTCUT_CLICKED);
      });
    } else {
      mdMenu.title = queryArgs['removeTooltip'] || '';
      mdMenu.setAttribute(
          'aria-label', (queryArgs['removeTooltip'] || '') + ' ' + data.title);
      mdMenu.addEventListener('click', function(ev) {
        removeAllOldTiles();
        blacklistTile(mdTile);
        ev.preventDefault();
        ev.stopPropagation();
      });
    }
    // Don't allow the event to bubble out to the containing tile, as that would
    // trigger navigation to the tile URL.
    mdMenu.addEventListener('keydown', function(ev) {
      ev.stopPropagation();
    });
    utils.disableOutlineOnMouseClick(mdMenu);

    mdTile.appendChild(mdMenu);
  }

  return currGrid.createGridTile(mdTile, data.rid, !!data.isAddButton);
}

/**
 * Does some initialization and parses the query arguments passed to the iframe.
 */
function init() {
  // Create a new DOM element to hold the tiles. The tiles will be added
  // one-by-one via addTile, and the whole thing will be inserted into the page
  // in swapInNewTiles, after the parent has sent us the 'show' message, and all
  // favicons have loaded.
  tiles = document.createElement('div');

  // Parse query arguments.
  const query = window.location.search.substring(1).split('&');
  queryArgs = {};
  for (let i = 0; i < query.length; ++i) {
    const val = query[i].split('=');
    if (val[0] == '') {
      continue;
    }
    queryArgs[decodeURIComponent(val[0])] = decodeURIComponent(val[1]);
  }

  document.title = queryArgs['title'];

  // Enable RTL.
  if (queryArgs['rtl'] == '1') {
    document.documentElement.dir = 'rtl';
  }

  // Enable custom links.
  if (queryArgs['enableCustomLinks'] == '1') {
    customLinksFeatureEnabled = true;
  }

  currGrid = new Grid();
  // Set up layout updates on window resize. Throttled according to
  // |RESIZE_TIMEOUT_DELAY|.
  let resizeTimeout;
  window.onresize = () => {
    if (resizeTimeout) {
      window.clearTimeout(resizeTimeout);
    }
    resizeTimeout = window.setTimeout(() => {
      resizeTimeout = null;
      currGrid.onResize();
    }, RESIZE_TIMEOUT_DELAY);
  };

  window.addEventListener('message', handlePostMessage);
}

/**
 * Binds event listeners.
 */
function listen() {
  document.addEventListener('DOMContentLoaded', init);
}

return {
  Grid: Grid,  // Exposed for testing.
  init: init,  // Exposed for testing.
  listen: listen,
};
}

if (!window.mostVisitedUnitTest) {
  MostVisited().listen();
}
