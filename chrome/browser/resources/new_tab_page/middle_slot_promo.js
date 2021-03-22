// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ImgElement} from './img.js';
import {NewTabPageProxy} from './new_tab_page_proxy.js';
import {PromoBrowserCommandProxy} from './promo_browser_command_proxy.js';
import {WindowProxy} from './window_proxy.js';

/**
 * If a promo exists with content and can be shown, an element containing
 * the rendered promo is returned with an id #container. Otherwise, null is
 * returned.
 * @return {!Promise<Element>}
 */
export async function renderPromo() {
  const browserHandler = NewTabPageProxy.getInstance().handler;
  const promoBrowserCommandHandler =
      PromoBrowserCommandProxy.getInstance().handler;
  const {promo} = await browserHandler.getPromo();
  if (!promo) {
    return null;
  }

  const commandIds = [];
  const createAnchor = target => {
    const commandIdMatch = /^command:(\d+)$/.exec(target.url);
    if (!commandIdMatch && !target.url.startsWith('https://')) {
      return null;
    }
    const el = /** @type {!HTMLAnchorElement} */ (document.createElement('a'));
    /** @type {?promoBrowserCommand.mojom.Command} */
    let commandId = null;
    if (!commandIdMatch) {
      el.href = target.url;
    } else {
      commandId =
          /** @type {promoBrowserCommand.mojom.Command} */ (+commandIdMatch[1]);
      // Make sure we don't send unsupported commands to the browser.
      if (!Object.values(promoBrowserCommand.mojom.Command)
               .includes(commandId)) {
        commandId = promoBrowserCommand.mojom.Command.kUnknownCommand;
      }
      commandIds.push(commandId);
    }
    const onClick = event => {
      if (commandId !== null) {
        promoBrowserCommandHandler.executeCommand(commandId, {
          middleButton: event.button === 1,
          altKey: event.altKey,
          ctrlKey: event.ctrlKey,
          metaKey: event.metaKey,
          shiftKey: event.shiftKey,
        });
      }
      browserHandler.onPromoLinkClicked();
    };
    // 'auxclick' handles the middle mouse button which does not trigger a
    // 'click' event.
    el.addEventListener('auxclick', onClick);
    el.addEventListener('click', onClick);
    return el;
  };

  let hasContent = false;
  const container = document.createElement('div');
  container.id = 'container';
  promo.middleSlotParts.forEach(({image, link, text}) => {
    let el;
    if (image) {
      el = new ImgElement();
      el.autoSrc = image.imageUrl.url;
      if (image.target) {
        const anchor = createAnchor(image.target);
        if (anchor) {
          anchor.appendChild(el);
          el = anchor;
        }
      }
      el.classList.add('image');
    } else if (link) {
      el = createAnchor(link.url);
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
      hasContent = true;
      container.appendChild(el);
    }
  });

  const canShow =
      (await Promise.all(commandIds.map(
           commandId =>
               promoBrowserCommandHandler.canShowPromoWithCommand(commandId))))
          .every(({canShow}) => canShow);
  if (hasContent && canShow) {
    browserHandler.onPromoRendered(
        WindowProxy.getInstance().now(), promo.logUrl || null);
    return container;
  }
  return null;
}

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

  /** @override */
  ready() {
    super.ready();
    this.hidden = true;
    renderPromo().then(container => {
      if (container) {
        this.shadowRoot.appendChild(container);
        this.hidden = false;
      }
      this.dispatchEvent(new Event(
          'ntp-middle-slot-promo-loaded', {bubbles: true, composed: true}));
    });
  }
}

customElements.define(MiddleSlotPromoElement.is, MiddleSlotPromoElement);
