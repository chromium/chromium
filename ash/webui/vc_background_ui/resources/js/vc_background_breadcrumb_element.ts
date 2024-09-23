// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * The breadcrumb that displays the current view stack and allows users to
 * navigate.
 */

import '/strings.m.js';
import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/cros_button_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_icons.css.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {AnchorAlignment, CrActionMenuElement} from 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {getSeaPenTemplates, SeaPenTemplate} from 'chrome://resources/ash/common/sea_pen/constants.js';
import {cleanUpSeaPenQueryStates} from 'chrome://resources/ash/common/sea_pen/sea_pen_controller.js';
import {SeaPenTemplateId} from 'chrome://resources/ash/common/sea_pen/sea_pen_generated.mojom-webui.js';
import {logSeaPenTemplateSelect} from 'chrome://resources/ash/common/sea_pen/sea_pen_metrics_logger.js';
import {SeaPenPaths, SeaPenRouterElement} from 'chrome://resources/ash/common/sea_pen/sea_pen_router_element.js';
import {getSeaPenStore} from 'chrome://resources/ash/common/sea_pen/sea_pen_store.js';
import {getTemplateIdFromString, isNonEmptyArray} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';
import {getTransitionEnabled, setTransitionsEnabled} from 'chrome://resources/ash/common/sea_pen/transition.js';
import {IronA11yKeysElement} from 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './vc_background_breadcrumb_element.html.js';

/** Event interface for dom-repeat. */
interface RepeaterEvent extends CustomEvent {
  model: {
    index: number,
  };
}

const VcBackgroundBreadcrumbElementBase = I18nMixin(PolymerElement);

export interface VcBackgroundBreadcrumbElement {
  $: {
    container: HTMLElement,
    keys: IronA11yKeysElement,
    selector: IronSelectorElement,
  };
}

export class VcBackgroundBreadcrumbElement extends
    VcBackgroundBreadcrumbElementBase {
  static get is() {
    return 'vc-background-breadcrumb';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** The current SeaPen template id to display. */
      seaPenTemplateId: String,

      /**
       * The current path of the page.
       */
      path: {
        type: String,
      },

      breadcrumbs_: {
        type: Array,
        computed:
            'computeBreadcrumbs_(path, seaPenTemplates_, seaPenTemplateId)',
        observer: 'onBreadcrumbsChanged_',
      },

      /** The list of SeaPen templates. */
      seaPenTemplates_: {
        type: Array,
        computed: 'computeSeaPenTemplates_()',
      },

      /** The breadcrumb being highlighted by keyboard navigation. */
      selectedBreadcrumb_: {
        type: Object,
        notify: true,
      },
    };
  }

  seaPenTemplateId: string;
  path: string;
  private breadcrumbs_: string[];
  private seaPenTemplates_: SeaPenTemplate[]|null;
  private selectedBreadcrumb_: HTMLElement;

  override ready() {
    super.ready();
    this.$.keys.target = this.$.selector;
  }

  private getInitialTabIndex_(index: number) {
    return index === 0 ? 0 : -1;
  }

  private onBreadcrumbsChanged_() {
    requestAnimationFrame(() => {
      // Note that only 1 breadcrumb is focusable at any given time. When
      // breadcrumbs change, the previously selected breadcrumb might not be in
      // DOM anymore. To allow keyboard users to focus the breadcrumbs again, we
      // add the first breadcrumb back to tab order.
      const allBreadcrumbs = this.$.selector.items as HTMLElement[];
      const hasFocusableBreadcrumb =
          allBreadcrumbs.some(el => el.getAttribute('tabindex') === '0');

      if (!hasFocusableBreadcrumb && allBreadcrumbs.length > 0) {
        allBreadcrumbs[0].setAttribute('tabindex', '0');
      }
    });
  }

  /** Handle keyboard navigation. */
  private onKeysPress_(
      e: CustomEvent<{key: string, keyboardEvent: KeyboardEvent}>) {
    const selector = this.$.selector;
    const prevBreadcrumb = this.selectedBreadcrumb_;
    switch (e.detail.key) {
      case 'left':
        selector.selectPrevious();
        break;
      case 'right':
        selector.selectNext();
        break;
      default:
        return;
    }
    // Remove focus state of previous breadcrumb.
    if (prevBreadcrumb) {
      prevBreadcrumb.setAttribute('tabindex', '-1');
    }
    // Add focus state for new breadcrumb.
    if (this.selectedBreadcrumb_) {
      this.selectedBreadcrumb_.setAttribute('tabindex', '0');
      this.selectedBreadcrumb_.focus();
    }
    e.detail.keyboardEvent.preventDefault();
  }

  /**
   * Returns the aria-current status of the breadcrumb. The last breadcrumb is
   * considered the "current" breadcrumb representing the active page.
   */
  private getBreadcrumbAriaCurrent_(index: number, breadcrumbs: string[]):
      'page'|'false' {
    if (index === (breadcrumbs.length - 1)) {
      return 'page';
    }
    return 'false';
  }

  private computeBreadcrumbs_(): string[] {
    const breadcrumbs = [];
    // Normalize the relative path for vc background matched with wallpaper as
    // 'chrome://vc-background/' has an extra single slash at the end.
    const relativePath = this.path === '/' ? '' : this.path;

    switch (relativePath) {
      case SeaPenPaths.TEMPLATES:
        breadcrumbs.push(this.i18n('vcBackgroundLabel'));
        break;
      case SeaPenPaths.RESULTS:
        breadcrumbs.push(this.i18n('vcBackgroundLabel'));
        if (this.seaPenTemplateId && isNonEmptyArray(this.seaPenTemplates_)) {
          const template = this.seaPenTemplates_.find(
              template => template.id.toString() === this.seaPenTemplateId);
          if (template) {
            breadcrumbs.push(template.title);
          }
        }
        break;
      case SeaPenPaths.FREEFORM:
        // TODO(b/345856242): update the final string.
        breadcrumbs.push('AI Prompting');
        break;
    }
    return breadcrumbs;
  }

  private computeSeaPenTemplates_(): SeaPenTemplate[] {
    return getSeaPenTemplates();
  }

  private onBreadcrumbClick_(e: RepeaterEvent) {
    const index = e.model.index;
    // stay in same page if the user clicks on the last breadcrumb,
    // else navigate to the corresponding page.
    if (index < this.breadcrumbs_.length - 1) {
      const pathElements = this.path.split('/');
      const newPath = pathElements.slice(0, index + 1).join('/');

      // Unfocus the breadcrumb to focus on the page
      // with new path.
      const breadcrumb = e.target as HTMLElement;
      breadcrumb.blur();
      this.goBackToRoute_(newPath as SeaPenPaths);
    }
  }

  private onClickMenuIcon_(e: Event) {
    const targetElement = e.currentTarget as HTMLElement;
    const rect = targetElement.getBoundingClientRect();
    // Anchors the menu at the top-left corner of the chip while also
    // accounting for the scrolling of the page.
    const config = {
      anchorAlignmentX: AnchorAlignment.AFTER_START,
      anchorAlignmentY: AnchorAlignment.AFTER_START,
      minX: 0,
      minY: 0,
      maxX: window.innerWidth,
      maxY: window.innerHeight,
      top: rect.top - document.scrollingElement!.scrollTop,
      left: rect.left - document.scrollingElement!.scrollLeft,
    };
    const menuElement =
        this.shadowRoot!.querySelector<CrActionMenuElement>('cr-action-menu');
    menuElement!.shadowRoot!.getElementById('dialog')!.style.position = 'fixed';
    menuElement!.showAt(targetElement, config);
  }

  private onClickMenuItem_(e: Event) {
    const targetElement = e.currentTarget as HTMLElement;
    const templateId = targetElement.dataset['id'];
    assert(!!templateId, 'templateId is required');
    // cleans up the Sea Pen states such as thumbnail response status code,
    // thumbnail loading status and Sea Pen query when
    // switching template; otherwise, states from the last query search will
    // remain in sea-pen-images element.
    cleanUpSeaPenQueryStates(getSeaPenStore());
    const transitionsEnabled = getTransitionEnabled();
    // disables the page transition when switching templates from the drop down.
    // Then resets it back to the original value after routing is done to not
    // interfere with other page transitions.
    setTransitionsEnabled(false);

    // log metrics for the selected template.
    if (templateId) {
      logSeaPenTemplateSelect(getTemplateIdFromString(templateId));
    }

    SeaPenRouterElement.instance()
        .goToRoute(SeaPenPaths.RESULTS, {seaPenTemplateId: templateId})
        ?.finally(() => {
          setTransitionsEnabled(transitionsEnabled);
        });
    this.closeOptionMenu_();
  }

  private closeOptionMenu_() {
    const menuElement = this.shadowRoot!.querySelector('cr-action-menu');
    menuElement!.close();
  }

  private shouldShowSeaPenDropdown_(path: string, breadcrumb: string): boolean {
    const template =
        this.seaPenTemplates_?.find(template => template.title === breadcrumb);

    return path === SeaPenPaths.RESULTS && !!template;
  }

  private getAriaChecked_(
      templateId: SeaPenTemplateId, seaPenTemplateId: string): 'true'|'false' {
    return templateId.toString() === seaPenTemplateId ? 'true' : 'false';
  }

  // Helper method to apply back transition style when navigating to path.
  private goBackToRoute_(path: SeaPenPaths) {
    document.documentElement.classList.add('back-transition');
    SeaPenRouterElement.instance().goToRoute(path)?.finally(() => {
      document.documentElement.classList.remove('back-transition');
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'vc-background-breadcrumb': VcBackgroundBreadcrumbElement;
  }
}

customElements.define(
    VcBackgroundBreadcrumbElement.is, VcBackgroundBreadcrumbElement);
