// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {CrAutoImgElement} from 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {Command} from 'chrome://resources/js/browser_command/browser_command.mojom-webui.js';
import {BrowserCommandProxy} from 'chrome://resources/js/browser_command/browser_command_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './middle_slot_promo.html.js';
import {NewTabPageProxy} from './new_tab_page_proxy.js';
import {WindowProxy} from './window_proxy.js';

/**
 * If a promo exists with content and can be shown, an element containing
 * the rendered promo is returned with an id #promoContainer. Otherwise, null is
 * returned.
 */
export async function renderPromo():
    Promise<{container: Element, id: string | undefined}|null> {
  const browserHandler = NewTabPageProxy.getInstance().handler;
  const promoBrowserCommandHandler = BrowserCommandProxy.getInstance().handler;
  const {promo} = await browserHandler.getPromo();
  if (!promo) {
    return null;
  }

  const commandIds: Command[] = [];

  function createAnchor(target: Url) {
    const commandIdMatch = /^command:(\d+)$/.exec(target.url);
    if (!commandIdMatch && !target.url.startsWith('https://')) {
      return null;
    }
    const el = document.createElement('a');
    let commandId: Command|null = null;
    if (!commandIdMatch) {
      el.href = target.url;
    } else {
      commandId = +commandIdMatch[1];
      // Make sure we don't send unsupported commands to the browser.
      if (!Object.values(Command).includes(commandId)) {
        commandId = Command.kUnknownCommand;
      }
      commandIds.push(commandId);
    }

    function onClick(event: MouseEvent) {
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
    }

    // 'auxclick' handles the middle mouse button which does not trigger a
    // 'click' event.
    el.addEventListener('auxclick', onClick);
    el.addEventListener('click', onClick);
    return el;
  }

  let hasContent = false;
  const promoContainer = document.createElement('div');
  promoContainer.id = 'promoContainer';
  promo.middleSlotParts.forEach(({image, link, text}) => {
    let el;
    if (image) {
      el = new CrAutoImgElement();
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
      promoContainer.appendChild(el);
    }
  });

  const canShow =
      (await Promise.all(commandIds.map(
           commandId =>
               promoBrowserCommandHandler.canExecuteCommand(commandId))))
          .every(({canExecute}) => canExecute);
  if (hasContent && canShow) {
    browserHandler.onPromoRendered(
        WindowProxy.getInstance().now(), promo.logUrl || null);
    return {container: promoContainer, id: promo.id};
  }
  return null;
}

// Element that requests and renders the middle-slot promo. The element is
// hidden until the promo is rendered, If no promo exists or the promo is empty,
// the element remains hidden.
export class MiddleSlotPromoElement extends PolymerElement {
  static get is() {
    return 'ntp-middle-slot-promo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      middleSlotPromoId_: {
        type: String,
        reflectToAttribute: true,
      },
    };
  }

  private middleSlotPromoId_: string;

  override ready() {
    super.ready();

    renderPromo().then(promo => {
      if (promo) {
        const promoId = promo.id;
        if (loadTimeData.getBoolean('middleSlotPromoDismissalEnabled') &&
            promoId) {
          this.middleSlotPromoId_ = promoId;
        }

        const promoAndDismissContainer =
            this.shadowRoot!.getElementById('promoAndDismissContainer');
        assert(promoAndDismissContainer);
        const promoContainer = promo.container;
        if (promoContainer) {
          promoAndDismissContainer.prepend(promoContainer);
          promoAndDismissContainer.hidden = false;
        }
      }
      this.dispatchEvent(new Event(
          'ntp-middle-slot-promo-loaded', {bubbles: true, composed: true}));
    });
  }

  private onDismissPromoButtonClick_() {
    const promoAndDismissContainer =
        this.shadowRoot!.getElementById('promoAndDismissContainer');
    assert(promoAndDismissContainer);
    promoAndDismissContainer.hidden = true;
    NewTabPageProxy.getInstance().handler.blocklistPromo(
        this.middleSlotPromoId_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-middle-slot-promo': MiddleSlotPromoElement;
  }
}

customElements.define(MiddleSlotPromoElement.is, MiddleSlotPromoElement);
