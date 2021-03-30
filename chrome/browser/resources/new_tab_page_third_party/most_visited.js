// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/text_direction.mojom-lite.js';
import 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';

/**
 * @enum {number}
 * @const
 */
const ScreenWidth = {
  NARROW: 0,
  MEDIUM: 1,
  WIDE: 2,
};

class MostVisitedElement extends PolymerElement {
  static get is() {
    return 'ntp3p-most-visited';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * When the tile icon background is dark, the add icon color is white for
       * contrast. This can be used to determine the color of the tile hover as
       * well.
       */
      useWhiteTileIcon_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /* If true wraps the tile titles in white pills. */
      useTitlePill_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /** @private */
      columnCount_: {
        type: Number,
        computed: `computeColumnCount_(tiles_, screenWidth_)`,
      },

      /** @private */
      rowCount_: {
        type: Number,
        computed: 'computeRowCount_(columnCount_, tiles_)',
      },

      /** @private */
      maxVisibleTiles_: {
        type: Number,
        computed: 'computeMaxVisibleTiles_(columnCount_, rowCount_)',
      },

      /** @private */
      showToastButtons_: Boolean,

      /** @private {!ScreenWidth} */
      screenWidth_: Number,

      /** @private {!Array<!newTabPageThirdParty.mojom.MostVisitedTile>} */
      tiles_: Array,

      /** @private {!newTabPageThirdParty.mojom.Theme} */
      theme_: Object,
    };
  }

  /** @private {!Array<!HTMLElement>} */
  get tileElements_() {
    return /** @type {!Array<!HTMLElement>} */ (
        Array.from(this.shadowRoot.querySelectorAll('.tile:not([hidden])')));
  }

  constructor() {
    performance.mark('most-visited-creation-start');
    super();
    const {callbackRouter, handler} = BrowserProxy.getInstance();
    /** @private {!newTabPageThirdParty.mojom.PageCallbackRouter} */
    this.callbackRouter_ = callbackRouter;
    /** @private {newTabPageThirdParty.mojom.PageHandlerRemote} */
    this.pageHandler_ = handler;
    /** @private {?number} */
    this.setMostVisitedTilesListenerId_ = null;
    /** @private {?number} */
    this.setThemeListenerId_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    /** @private {boolean} */
    this.isRtl_ = window.getComputedStyle(this)['direction'] === 'rtl';
    /** @private {!EventTracker} */
    this.eventTracker_ = new EventTracker();

    this.setThemeListenerId_ =
        this.callbackRouter_.setTheme.addListener(theme => {
          this.useWhiteTileIcon_ = theme.shortcutUseWhiteTileIcon;
          this.useTitlePill_ = theme.shortcutUseTitlePill;
          this.theme_ = theme;
        });
    this.setMostVisitedTilesListenerId_ =
        this.callbackRouter_.setMostVisitedTiles.addListener(tiles => {
          performance.measure('most-visited-mojo', 'most-visited-mojo-start');
          this.tiles_ = tiles.slice(0, 8);
        });
    performance.mark('most-visited-mojo-start');
    this.eventTracker_.add(document, 'visibilitychange', () => {
      // This updates the most visited tiles every time the NTP tab gets
      // activated.
      if (document.visibilityState === 'visible') {
        this.pageHandler_.updateTheme();
        this.pageHandler_.updateMostVisitedTiles();
      }
    });
    this.pageHandler_.updateTheme();
    this.pageHandler_.updateMostVisitedTiles();
    FocusOutlineManager.forDocument(document);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter_.removeListener(assert(this.setThemeListenerId_));
    this.callbackRouter_.removeListener(
        assert(this.setMostVisitedTilesListenerId_));
    this.mediaListenerWideWidth_.removeListener(
        assert(this.boundOnWidthChange_));
    this.mediaListenerMediumWidth_.removeListener(
        assert(this.boundOnWidthChange_));
    this.ownerDocument.removeEventListener(
        'keydown', this.boundOnDocumentKeyDown_);
    this.eventTracker_.removeAll();
  }

  /** @override */
  ready() {
    super.ready();

    /** @private {!Function} */
    this.boundOnWidthChange_ = this.updateScreenWidth_.bind(this);
    const {matchMedia} = BrowserProxy.getInstance();
    /** @private {!MediaQueryList} */
    this.mediaListenerWideWidth_ = matchMedia('(min-width: 672px)');
    this.mediaListenerWideWidth_.addListener(this.boundOnWidthChange_);
    /** @private {!MediaQueryList} */
    this.mediaListenerMediumWidth_ = matchMedia('(min-width: 560px)');
    this.mediaListenerMediumWidth_.addListener(this.boundOnWidthChange_);
    this.updateScreenWidth_();
    /** @private {!function(Event)} */
    this.boundOnDocumentKeyDown_ = e =>
        this.onDocumentKeyDown_(/** @type {!KeyboardEvent} */ (e));
    this.ownerDocument.addEventListener(
        'keydown', this.boundOnDocumentKeyDown_);

    performance.measure('most-visited-creation', 'most-visited-creation-start');
  }

  /**
   * @param {!skia.mojom.SkColor} skColor
   * @return {string}
   * @private
   */
  rgbaOrInherit_(skColor) {
    return skColor ? skColorToRgba(skColor) : 'inherit';
  }

  /**
   * @return {number}
   * @private
   */
  computeColumnCount_() {
    let maxColumns = 3;
    if (this.screenWidth_ === ScreenWidth.WIDE ||
        this.screenWidth_ === ScreenWidth.MEDIUM) {
      maxColumns = 4;
    }

    const shortcutCount = this.tiles_ ? this.tiles_.length : 0;
    const tileCount = Math.min(8, shortcutCount);
    const columnCount = tileCount <= maxColumns ?
        tileCount :
        Math.min(maxColumns, Math.ceil(tileCount / 2));
    return columnCount || 3;
  }

  /**
   * @return {number}
   * @private
   */
  computeRowCount_() {
    if (this.columnCount_ === 0) {
      return 0;
    }

    const shortcutCount = this.tiles_ ? this.tiles_.length : 0;
    return this.columnCount_ <= shortcutCount ? 2 : 1;
  }

  /**
   * @return {number}
   * @private
   */
  computeMaxVisibleTiles_() {
    return this.columnCount_ * this.rowCount_;
  }

  /**
   * @param {!url.mojom.Url} url
   * @return {string}
   * @private
   */
  getFaviconUrl_(url) {
    const faviconUrl = new URL('chrome://favicon2/');
    faviconUrl.searchParams.set('size', '24');
    faviconUrl.searchParams.set('scale_factor', '1x');
    faviconUrl.searchParams.set('show_fallback_monogram', '');
    faviconUrl.searchParams.set('page_url', url.url);
    return faviconUrl.href;
  }

  /**
   * @param {!newTabPageThirdParty.mojom.MostVisitedTile} tile
   * @return {string}
   * @private
   */
  getTileTitleDirectionClass_(tile) {
    return tile.titleDirection === mojoBase.mojom.TextDirection.RIGHT_TO_LEFT ?
        'title-rtl' :
        'title-ltr';
  }

  /**
   * @param {number} index
   * @return {boolean}
   * @private
   */
  isHidden_(index) {
    return index >= this.maxVisibleTiles_;
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onDocumentKeyDown_(e) {
    if (e.altKey || e.shiftKey) {
      return;
    }

    const modifier = isMac ? e.metaKey && !e.ctrlKey : e.ctrlKey && !e.metaKey;
    if (modifier && e.key === 'z') {
      e.preventDefault();
      this.onUndoClick_();
    }
  }

  /** @private */
  onRestoreDefaultsClick_() {
    if (!this.$.toast.open || !this.showToastButtons_) {
      return;
    }
    this.$.toast.hide();
    this.pageHandler_.restoreMostVisitedDefaults();
  }

  /**
   * @param {!Event} e
   * @private
   */
  onTileRemoveButtonClick_(e) {
    e.preventDefault();
    const {index} = this.$.tiles.modelForElement(e.target.parentElement);
    this.tileRemove_(index);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onTileClick_(e) {
    if (e.defaultPrevented) {
      // Ignore previousely handled events.
      return;
    }

    e.preventDefault();

    this.pageHandler_.onMostVisitedTileNavigation(
        this.$.tiles.itemForElement(e.target),
        this.$.tiles.indexForElement(e.target), e.button || 0, e.altKey,
        e.ctrlKey, e.metaKey, e.shiftKey);
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onTileKeyDown_(e) {
    if (e.altKey || e.shiftKey || e.metaKey || e.ctrlKey) {
      return;
    }

    if (e.key !== 'ArrowLeft' && e.key !== 'ArrowRight' &&
        e.key !== 'ArrowUp' && e.key !== 'ArrowDown' && e.key !== 'Delete') {
      return;
    }

    const {index} = this.$.tiles.modelForElement(e.target);
    if (e.key === 'Delete') {
      this.tileRemove_(index);
      return;
    }

    const advanceKey = this.isRtl_ ? 'ArrowLeft' : 'ArrowRight';
    const delta = (e.key === advanceKey || e.key === 'ArrowDown') ? 1 : -1;
    this.tileFocus_(Math.max(0, index + delta));
  }

  /** @private */
  onUndoClick_() {
    if (!this.$.toast.open || !this.showToastButtons_) {
      return;
    }
    this.$.toast.hide();
    this.pageHandler_.undoMostVisitedTileAction();
  }

  /**
   * @param {number} index
   * @private
   */
  tileFocus_(index) {
    if (index < 0) {
      return;
    }
    const tileElements = this.tileElements_;
    if (index < tileElements.length) {
      tileElements[index].focus();
    }
  }

  /**
   * @param {number} index
   * @return {!Promise}
   * @private
   */
  async tileRemove_(index) {
    const {url, isQueryTile} = this.tiles_[index];
    this.pageHandler_.deleteMostVisitedTile(url);
    // Do not show the toast buttons when a query tile is removed unless it is a
    // custom link. Removal is not reversible for non custom link query tiles.
    this.showToastButtons_ = !isQueryTile;
    this.$.toast.show();
    this.tileFocus_(index);
  }

  /** @private */
  updateScreenWidth_() {
    if (this.mediaListenerWideWidth_.matches) {
      this.screenWidth_ = ScreenWidth.WIDE;
    } else if (this.mediaListenerMediumWidth_.matches) {
      this.screenWidth_ = ScreenWidth.MEDIUM;
    } else {
      this.screenWidth_ = ScreenWidth.NARROW;
    }
  }

  /** @private */
  onTilesRendered_() {
    performance.measure('most-visited-rendered');
    this.pageHandler_.onMostVisitedTilesRendered(
        this.tiles_.slice(0, assert(this.maxVisibleTiles_)),
        BrowserProxy.getInstance().now());
  }
}

customElements.define(MostVisitedElement.is, MostVisitedElement);
