// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-hub-module' is the settings page that presents the safety
 * state of Chrome.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../settings_shared.css.js';
import '../site_favicon.js';
import '../i18n_setup.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TooltipMixin} from '../tooltip_mixin.js';

import {getTemplate} from './safety_hub_module.html.js';

/**
 * Corresponds to the animation-duration CSS parameter defined
 * in review_notification_permissions.html. Set to be slightly higher, as we
 * want to ensure that the animation is finished before updating the model for
 * the right visual effect.
 */
const MODEL_UPDATE_DELAY_MS = 310;

export interface SiteInfo {
  origin: string;
  detail: string|TrustedHTML;
  icon?: string;
}

export interface SiteInfoWithTarget extends SiteInfo {
  target: EventTarget;
}

const SettingsSafetyHubModuleElementBase =
    TooltipMixin(I18nMixin(PolymerElement));

export class SettingsSafetyHubModuleElement extends
    SettingsSafetyHubModuleElementBase {
  static get is() {
    return 'settings-safety-hub-module';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // List of domains in the list. Each site has origin and detail field.
      sites: {
        type: Array,
        value: () => [],
        observer: 'onSitesChanged_',
      },

      // If set to true, users of this class MUST call animateShow() after
      // adding any items added to |sites|, otherwise these will not be
      // properly rendered. Users SHOULD also call animateHide() on any item
      // before removing it from |sites|, to apply the reverse animation.
      animated: {type: Boolean, value: false},

      // The string for the header label.
      header: String,

      // The string for the subheader label.
      subheader: String,

      // The icon for the module.
      headerIcon: {
        String,
        value: 'cr:error',
        observer: 'onHeaderIconChanged_',
      },

      // The color of the header-icon.
      headerIconColor: String,

      // The icon for the button of the list item.
      buttonIcon: String,

      // The string ID for the aria label for the button of the list item.
      buttonAriaLabelId: String,

      // The string for the tooltip for the button of the list item.
      buttonTooltipText: String,

      // Whether the more action button is visible.
      moreActionVisible: {
        type: Boolean,
        value: false,
      },

      // The string ID for the aria label for the more action button of the list
      // item.
      moreButtonAriaLabelId: String,
    };
  }

  sites: SiteInfo[];
  header: string;
  subheader: string|TrustedHTML;
  headerIcon: string;
  headerIconColor: string;
  buttonIcon: string;
  buttonAriaLabelId: string;
  buttonTooltipText: string;
  moreButtonAriaLabelId: string;
  moreActionVisible: boolean;

  private modelUpdateDelayMsForTesting_: number|null = null;

  setModelUpdateDelayMsForTesting(delayMs: number) {
    this.modelUpdateDelayMsForTesting_ = delayMs;
  }

  private setVisibility_(item: HTMLElement, visible: boolean) {
    // Removing the explicit visibility makes elements fall back on their
    // default CSS which is "display: hidden;".
    item.style.display = visible ? 'flex' : '';
  }

  private addItemLinkClickListeners(items: NodeListOf<HTMLElement>) {
    // Module items might contain links. If there is any link in the module,
    // this function adds a listener for the "Click" event on each link. 'Click'
    // events will be handled by derived module elements. For that, add
    // on-sh-module-item-link-click property to settings-safety-hub-module
    // element in the html file.
    for (const item of items) {
      const links = item.querySelectorAll('a');
      links.forEach((link) => {
        link.addEventListener('click', function() {
          this.dispatchEvent(new CustomEvent(
              'sh-module-item-link-click',
              {bubbles: true, composed: true, detail: item}));
        });
      });
    }
  }

  private onSitesChanged_() {
    const items =
        this.shadowRoot!.querySelectorAll<HTMLElement>('#siteList .list-item');

    // Polymer reuses the already rendered rows once |this.sites| changes,
    // some of which may have previously been made invisible at the end of the
    // hiding animation. Ensure that everything rendered is actually visible.
    for (const item of items) {
      this.setVisibility_(item, true);
    }

    // There's a delay between when |this.sites| is set and when the items
    // are actually rendered. If they're not in sync, wait until additional
    // items may be rendered.
    if (this.sites && this.sites.length !== items.length) {
      setTimeout(this.onSitesChanged_.bind(this), 0);
    }

    // Add an event listener to link elements of the module.
    this.addItemLinkClickListeners(items);
  }

  /**
   * Hides |origin| and when the animation finishes, calls |callback|. If
   * |origin| is null, all origins are hidden.
   *
   * The |callback| method MUST be provided and MUST remove the |origin| from
   * the underlying model.
   */
  animateHide(origin: string|null, callback: Function) {
    const items = this.shadowRoot!.querySelectorAll('#siteList .list-item');

    // There's a delay between when |this.sites| is set and when the items
    // are actually rendered. If they're not in sync, wait.
    if (items.length !== this.sites.length) {
      setTimeout(this.animateHide.bind(this, origin, callback), 0);
      return;
    }

    // Remove the item that corresponds to |origin|. If no origin is specified,
    // remove all items.
    let removedAll = (origin === null);
    for (let i = 0; i < this.sites!.length; ++i) {
      if (origin === null || origin === this.sites![i]!.origin) {
        items[i]!.classList.add('hiding');
        if (origin) {
          // If this is the last site being removed, the visuals should be
          // the same as if all sites were removed.
          removedAll ||= (this.sites!.length === 1);
          break;
        }
      }
    }

    // If all items are beign removed, also remove the line separator.
    if (removedAll) {
      this.shadowRoot!.querySelector('#line')!.classList.add('hiding');
    }

    // Call the callbacks once the animation is finished.
    const delayMs = this.modelUpdateDelayMsForTesting_ !== null ?
        this.modelUpdateDelayMsForTesting_ :
        MODEL_UPDATE_DELAY_MS;
    if (callback) {
      setTimeout(callback, delayMs);
    }
    setTimeout(this.finalizeAnimation_.bind(this), delayMs);
  }

  /**
   * Shows the given |origins| and calls |callback|.
   *
   * MUST be called once for each origin added, right after it is added.
   */
  animateShow(origins: string[], callback?: Function) {
    const items = this.shadowRoot!.querySelectorAll('#siteList .list-item');

    // Ensure the DOM was updated to reflect |this.sites|. If not, wait.
    if (items.length !== this.sites.length) {
      setTimeout(this.animateShow.bind(this, origins, callback), 0);
      return;
    }

    let wasEmpty = true;
    for (let i = 0; i < items.length; ++i) {
      if (origins.includes(this.sites![i]!.origin)) {
        items[i]!.classList.add('showing');
      } else {
        // If at least one item doesn't need showing, there was at least
        // one already rendered item.
        wasEmpty = false;
      }
    }

    // Ensure the line separator is animated to show when the first item
    // is being added.
    if (wasEmpty) {
      this.shadowRoot!.querySelector('#line')!.classList.add('showing');
    }

    // Call the callbacks once the animation is finished.
    const delayMs = this.modelUpdateDelayMsForTesting_ !== null ?
        this.modelUpdateDelayMsForTesting_ :
        MODEL_UPDATE_DELAY_MS;
    if (callback) {
      setTimeout(callback, delayMs);
    }
    setTimeout(this.finalizeAnimation_.bind(this), delayMs);
  }

  private finalizeAnimation_() {
    const items = this.shadowRoot!.querySelectorAll<HTMLElement>(
        '#siteList .list-item, #line');

    for (const item of items) {
      // Finish the ".showing" animation by making the element visible.
      if (item.classList.contains('showing')) {
        item.classList.remove('showing');
        this.setVisibility_(item, true);
      }
      // Finish the ".hiding" animation by making the element invisible.
      // This falls back to the default CSS of ".item" which is "display: none".
      if (item.classList.contains('hiding')) {
        item.classList.remove('hiding');
        this.setVisibility_(item, false);
      }
    }
  }

  private onItemButtonClick_(e: DomRepeatEvent<SiteInfo>) {
    const item = e.model.item;
    this.dispatchEvent(new CustomEvent(
        'sh-module-item-button-click',
        {bubbles: true, composed: true, detail: item}));
  }

  private onMoreActionClick_(e: DomRepeatEvent<SiteInfo>) {
    const item: SiteInfoWithTarget = {...e.model.item, target: e.target!};
    this.dispatchEvent(new CustomEvent('sh-module-more-action-button-click', {
      bubbles: true,
      composed: true,
      detail: item,
    }));
  }

  private onHeaderIconChanged_() {
    // The check icon is always green for all Safety Hub modules.
    if (this.headerIcon === 'cr:check') {
      this.headerIconColor = 'green';
      // The Safety Check icon color is managed in specific Safety Hub modules.
    } else if (this.headerIcon !== 'cr:security') {
      this.headerIconColor = '';
    }
  }

  private onShowTooltip_(e: DomRepeatEvent<SiteInfo>) {
    e.stopPropagation();
    const tooltip = this.shadowRoot!.querySelector('cr-tooltip');
    assert(tooltip);
    this.showTooltipAtTarget(tooltip, e.target! as Element);
  }

  private sanitizeInnerHtml_(rawString: string): TrustedHTML {
    return sanitizeInnerHtml(rawString);
  }

  private getButtonAriaLabelForOrigin_(origin: string): string {
    return this.i18n(this.buttonAriaLabelId, origin);
  }

  private getMoreButtonAriaLabelForOrigin_(origin: string): string {
    return this.i18n(this.moreButtonAriaLabelId, origin);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-hub-module': SettingsSafetyHubModuleElement;
  }
}

customElements.define(
    SettingsSafetyHubModuleElement.is, SettingsSafetyHubModuleElement);
