// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-hub-module' is the settings page that presents the safety
 * state of Chrome.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import '../site_favicon.js';
import '../i18n_setup.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {TooltipMixinLit} from '../tooltip_mixin_lit.js';

import {getCss} from './safety_hub_module.css.js';
import {getHtml} from './safety_hub_module.html.js';

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

export interface SettingsSafetyHubModuleElement {
  $: {
    header: HTMLElement,
    headerTextWrapper: HTMLElement,
    headerWrapper: HTMLElement,
    subheader: HTMLElement,
  };
}

const SettingsSafetyHubModuleElementBase =
    TooltipMixinLit(I18nMixinLit(CrLitElement));

export class SettingsSafetyHubModuleElement extends
    SettingsSafetyHubModuleElementBase {
  static get is() {
    return 'settings-safety-hub-module';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // List of domains in the list. Each site has origin and detail field.
      sites: {type: Array},

      // If set to true, users of this class MUST call animateShow() after
      // adding any items added to |sites|, otherwise these will not be
      // properly rendered. Users SHOULD also call animateHide() on any item
      // before removing it from |sites|, to apply the reverse animation.
      animated: {
        type: Boolean,
        reflect: true,
      },

      // The string for the header label.
      header: {type: String},

      // The string for the subheader label.
      subheader: {type: String},

      // The icon for the module. Optional.
      headerIcon: {type: String},

      // The color of the header-icon. Optional.
      headerIconColor: {type: String},

      // The icon for the button of the list item.
      buttonIcon: {type: String},

      // The string ID for the aria label for the button of the list item.
      buttonAriaLabelId: {type: String},

      // The string for the tooltip for the button of the list item.
      buttonTooltipText: {type: String},

      // Whether the more action button is visible.
      moreActionVisible: {type: Boolean},

      // The string ID for the aria label for the more action button of the list
      // item.
      moreButtonAriaLabelId: {type: String},
    };
  }

  // Based on tests, some clients set this to null, even though it
  // is initialized as an empty array.
  accessor sites: SiteInfo[]|null = [];
  accessor animated: boolean = false;
  accessor header: string = '';
  accessor subheader: string = '';
  accessor headerIcon: string = '';
  accessor headerIconColor: string = '';
  accessor buttonIcon: string = '';
  accessor buttonAriaLabelId: string = '';
  accessor buttonTooltipText: string = '';
  accessor moreButtonAriaLabelId: string = '';
  accessor moreActionVisible: boolean = false;

  private modelUpdateDelayMsForTesting_: number|null = null;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('headerIcon')) {
      this.onHeaderIconChanged_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('sites')) {
      this.onSitesChanged_();
    }
  }

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
        if (link.target === '_blank') {
          link.setAttribute('aria-description', this.i18n('opensInNewTab'));
        }

        link.addEventListener('click', () => {
          this.fire('sh-module-item-link-click', item);
        });
      });
    }
  }

  private onSitesChanged_() {
    const items =
        this.shadowRoot.querySelectorAll<HTMLElement>('#siteList .list-item');

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
    const items = this.shadowRoot.querySelectorAll('#siteList .list-item');

    // There's a delay between when |this.sites| is set and when the items
    // are actually rendered. If they're not in sync, wait.
    if (!this.sites || items.length !== this.sites.length) {
      setTimeout(this.animateHide.bind(this, origin, callback), 0);
      return;
    }

    // Remove the item that corresponds to |origin|. If no origin is specified,
    // remove all items.
    let removedAll = (origin === null);
    for (let i = 0; i < this.sites.length; ++i) {
      if (origin === null || origin === this.sites[i].origin) {
        items[i].classList.add('hiding');
        if (origin) {
          // If this is the last site being removed, the visuals should be
          // the same as if all sites were removed.
          removedAll ||= (this.sites.length === 1);
          break;
        }
      }
    }

    // If all items are beign removed, also remove the line separator.
    if (removedAll) {
      const line = this.shadowRoot.querySelector('#line');
      if (line) {
        line.classList.add('hiding');
      }
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
    const items = this.shadowRoot.querySelectorAll('#siteList .list-item');

    // Ensure the DOM was updated to reflect |this.sites|. If not, wait.
    if (!this.sites || items.length !== this.sites.length) {
      setTimeout(this.animateShow.bind(this, origins, callback), 0);
      return;
    }

    let wasEmpty = true;
    for (let i = 0; i < items.length; ++i) {
      if (origins.includes(this.sites[i].origin)) {
        items[i].classList.add('showing');
      } else {
        // If at least one item doesn't need showing, there was at least
        // one already rendered item.
        wasEmpty = false;
      }
    }

    // Ensure the line separator is animated to show when the first item
    // is being added.
    if (wasEmpty) {
      const line = this.shadowRoot.querySelector('#line');
      if (line) {
        line.classList.add('showing');
      }
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

  /** Focus the main button for the given |origin|, if it exists. */
  focusOriginMainButton(origin: string) {
    for (const item of this.shadowRoot.querySelectorAll<HTMLElement>(
             '#siteList .list-item')) {
      const siteRepresentation = item.querySelector('.site-representation');
      if (siteRepresentation &&
          siteRepresentation.textContent.trim() === origin) {
        item.querySelector<HTMLElement>('#mainButton')!.focus();
        return;
      }
    }
  }

  private finalizeAnimation_() {
    const items = this.shadowRoot.querySelectorAll<HTMLElement>(
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

  protected onItemButtonClick_(e: Event) {
    assert(this.sites);
    const target = e.currentTarget as HTMLElement;
    const item = this.sites[Number(target.dataset['index'])];
    this.fire('sh-module-item-button-click', item);
  }

  protected onMoreActionClick_(e: Event) {
    assert(this.sites);
    const target = e.currentTarget as HTMLElement;
    const item: SiteInfoWithTarget = {
      ...this.sites[Number(target.dataset['index'])],
      target,
    };
    this.fire('sh-module-more-action-button-click', item);
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

  protected onMainButtonFocus_(e: Event) {
    this.showTooltip_(e);
  }

  protected onMainButtonMouseenter_(e: Event) {
    this.showTooltip_(e);
  }

  private showTooltip_(e: Event) {
    e.stopPropagation();
    const tooltip = this.shadowRoot.querySelector('cr-tooltip');
    assert(tooltip);
    this.showTooltipAtTarget(tooltip, e.target! as Element);
  }

  protected sanitizeInnerHtml_(rawString: string|TrustedHTML): TrustedHTML {
    return typeof rawString === 'string' ? sanitizeInnerHtml(rawString) :
                                           rawString;
  }

  protected getButtonAriaLabelForOrigin_(origin: string): string {
    return this.buttonAriaLabelId ? this.i18n(this.buttonAriaLabelId, origin) :
                                    '';
  }

  protected getMoreButtonAriaLabelForOrigin_(origin: string): string {
    return this.moreButtonAriaLabelId ?
        this.i18n(this.moreButtonAriaLabelId, origin) :
        '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-hub-module': SettingsSafetyHubModuleElement;
  }
}

customElements.define(
    SettingsSafetyHubModuleElement.is, SettingsSafetyHubModuleElement);
