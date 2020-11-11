// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {ImgElement} from './img.js';
import {PromoBrowserCommandProxy} from './promo_browser_command_proxy.js';

// Element that requests and renders the middle-slot promo. The element is
// hidden until the promo is rendered, If no promo exists or the promo is empty,
// the element remains hidden.
class MiddleSlotPromoElement extends PolymerElement {
  static get is() {
    return 'ntp-middle-slot-promo';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {boolean} */
      hidden: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
      },

      /** @private */
      modulesEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('modulesEnabled'),
        reflectToAttribute: true,
      },

      /**
       * The list of browser commands to run, if any. Used to decide whether the
       * promo can be shown.
       * @type {!Array<number>}
       * @private
       */
      /** @private */
      commandIds_: {
        type: Object,
        value: () => [],
      },
    };
  }

  constructor() {
    super();
    /** @private {newTabPage.mojom.PageHandlerRemote} */
    this.pageHandler_ = BrowserProxy.getInstance().handler;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.pageHandler_.getPromo().then(({promo}) => {
      if (promo) {
        promo.middleSlotParts.forEach(({image, link, text}) => {
          let el;
          if (image) {
            el = new ImgElement();
            el.autoSrc = image.imageUrl.url;
            if (image.target) {
              const anchor = this.createAnchor_(image.target);
              if (anchor) {
                anchor.appendChild(el);
                el = anchor;
              }
            }
            el.classList.add('image');
          } else if (link) {
            el = this.createAnchor_(link.url);
          } else if (text) {
            el = document.createElement('span');
          }
          const linkOrText = link || text;
          if (el && linkOrText) {
            el.innerText = linkOrText.text;
            if (linkOrText.color) {
              el.style.color = linkOrText.color;
            }
          }
          if (el) {
            this.$.container.appendChild(el);
          }
        });
        this.maybeShowPromo_().then(canShow => {
          if (canShow) {
            this.pageHandler_.onPromoRendered(
                BrowserProxy.getInstance().now(), promo.logUrl || null);
            this.hidden = false;
          }
          this.dispatchEvent(new Event(
              'ntp-middle-slot-promo-loaded', {bubbles: true, composed: true}));
        });
      } else {
        // Dispatch this event even if there is no promo as Modules wait for the
        // promo to be loaded before showing.
        this.dispatchEvent(new Event(
            'ntp-middle-slot-promo-loaded', {bubbles: true, composed: true}));
      }
    });
  }

  /**
   * @param {!url.mojom.Url} target
   * @return {HTMLAnchorElement}
   * @private
   */
  createAnchor_(target) {
    const commandIdMatch = /^command:(\d+)$/.exec(target.url);
    if (!commandIdMatch && !target.url.startsWith('https://')) {
      return null;
    }
    const el = /** @type {!HTMLAnchorElement} */ (document.createElement('a'));
    let commandId = null;
    if (!commandIdMatch) {
      el.href = target.url;
    } else {
      commandId = +commandIdMatch[1];
      // Make sure we don't send unsupported commands to the browser.
      if (!Object.values(promoBrowserCommand.mojom.Command)
               .includes(commandId)) {
        commandId = promoBrowserCommand.mojom.Command.kUnknownCommand;
      }
      this.commandIds_.push(commandId);
    }
    const onClick = event => {
      if (commandId !== null) {
        PromoBrowserCommandProxy.getInstance().handler.executeCommand(
            commandId, {
              middleButton: event.button === 1,
              altKey: event.altKey,
              ctrlKey: event.ctrlKey,
              metaKey: event.metaKey,
              shiftKey: event.shiftKey,
            });
      }
      this.pageHandler_.onPromoLinkClicked();
    };
    // 'auxclick' handles the middle mouse button which does not trigger a
    // 'click' event.
    el.addEventListener('auxclick', onClick);
    el.addEventListener('click', onClick);
    return el;
  }

  /**
   * @return {!Promise<boolean>} Whether or not the promo can be shown by
   * checking all command IDs seen in the promo.
   * @private
   */
  async maybeShowPromo_() {
    const {handler} = PromoBrowserCommandProxy.getInstance();
    const promises = this.commandIds_.map(
        commandId => handler.canShowPromoWithCommand(commandId));
    return (await Promise.all(promises)).every(({canShow}) => canShow);
  }
}

customElements.define(MiddleSlotPromoElement.is, MiddleSlotPromoElement);
