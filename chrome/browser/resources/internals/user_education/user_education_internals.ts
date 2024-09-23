// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import 'chrome://resources/cr_components/help_bubble/help_bubble.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import '//resources/cr_elements/icons_lit.html.js';
import './user_education_internals_card.js';
import './user_education_whats_new_internals_card.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import {CrContainerShadowMixinLit} from 'chrome://resources/cr_elements/cr_container_shadow_mixin_lit.js';
import type {CrMenuSelector} from 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './user_education_internals.css.js';
import {getHtml} from './user_education_internals.html.js';
import type {FeaturePromoDemoPageData, FeaturePromoDemoPageInfo, UserEducationInternalsPageHandlerInterface, WhatsNewEditionDemoPageInfo, WhatsNewModuleDemoPageInfo} from './user_education_internals.mojom-webui.js';
import {UserEducationInternalsPageHandler} from './user_education_internals.mojom-webui.js';

export interface UserEducationInternalsElement {
  $: {
    content: HTMLElement,
    errorMessageToast: CrToastElement,
    menu: CrMenuSelector,
  };
}

const UserEducationInternalsElementBase =
    CrContainerShadowMixinLit(HelpBubbleMixinLit(CrLitElement));

export class UserEducationInternalsElement extends
    UserEducationInternalsElementBase {
  static get is() {
    return 'user-education-internals';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Substring filter that (when set) shows only entries containing
       * `filter`.
       */
      filter: {type: String},
      /**
       * List of tutorials and feature_promos that can be started.
       * Each tutorial has a string identifier.
       */
      tutorials_: {type: Array},
      featurePromos_: {type: Array},
      featurePromoErrorMessage_: {type: String},
      narrow_: {type: Boolean},

      /**
       * Indicates if the information about session data is expanded or
       * collapsed.
       */
      sessionExpanded_: {
        type: Boolean,
        value: false,
      },
    };
  }

  filter: string = '';
  protected isWhatsNewV2_: boolean = loadTimeData.getBoolean('isWhatsNewV2');
  protected tutorials_: FeaturePromoDemoPageInfo[] = [];
  protected featurePromos_: FeaturePromoDemoPageInfo[] = [];
  protected newBadges_: FeaturePromoDemoPageInfo[] = [];
  protected whatsNewModules_: WhatsNewModuleDemoPageInfo[] = [];
  protected whatsNewEditions_: WhatsNewEditionDemoPageInfo[] = [];
  protected featurePromoErrorMessage_: string = '';
  protected narrow_: boolean = false;
  protected sessionExpanded_: boolean = false;
  protected sessionData_: FeaturePromoDemoPageData[] = [];

  private handler_: UserEducationInternalsPageHandlerInterface;

  constructor() {
    super();
    this.handler_ = UserEducationInternalsPageHandler.getRemote();
  }

  override updated(changedProperties: PropertyValues) {
    super.updated(changedProperties as PropertyValues<this>);

    // There is a self-referential demo IPH for showing a help bubble in a
    // WebUI (specifically, this WebUI). Because of that, the target anchor for
    // the help bubble needs to be registered.
    //
    // However, because we want to attach the help bubble to an element
    // dynamically created, we have to wait until after the element is
    // populated to register the anchor element.
    if (changedProperties.has('featurePromos_')) {
      if (this.shadowRoot!.querySelector('#IPH_WebUiHelpBubbleTest')) {
        this.registerHelpBubble(
            'kWebUIIPHDemoElementIdentifier',
            ['#IPH_WebUiHelpBubbleTest', '#launch']);
      }
    }
  }

  override firstUpdated() {
    ColorChangeUpdater.forDocument().start();

    this.handler_.getTutorials().then(({tutorialInfos}) => {
      this.tutorials_ = tutorialInfos;
    });

    this.handler_.getSessionData().then(({sessionData}) => {
      this.sessionData_ = sessionData;
    });

    this.handler_.getFeaturePromos().then(({featurePromos}) => {
      this.featurePromos_ = featurePromos;
    });

    this.handler_.getNewBadges().then(({newBadges}) => {
      this.newBadges_ = newBadges;
    });

    if (this.isWhatsNewV2_) {
      this.handler_.getWhatsNewModules().then(({whatsNewModules}) => {
        this.whatsNewModules_ = whatsNewModules;
      });
      this.handler_.getWhatsNewEditions().then(({whatsNewEditions}) => {
        this.whatsNewEditions_ = whatsNewEditions;
      });
    }
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.filter = e.detail.toLowerCase();
  }

  protected startTutorial_(e: CustomEvent) {
    const id = e.detail;
    this.featurePromoErrorMessage_ = '';

    this.handler_.startTutorial(id).then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      }
    });
  }

  protected showFeaturePromo_(e: CustomEvent) {
    const id = e.detail;
    this.featurePromoErrorMessage_ = '';

    this.handler_.showFeaturePromo(id).then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      }
    });
  }

  protected clearPromoData_(e: CustomEvent) {
    const id = e.detail;
    this.featurePromoErrorMessage_ = '';

    this.handler_.clearFeaturePromoData(id).then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      } else {
        this.handler_.getFeaturePromos().then(({featurePromos}) => {
          this.featurePromos_ = featurePromos;
        });
      }
    });
  }

  protected clearSessionData_() {
    this.handler_.clearSessionData().then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      } else {
        this.handler_.getSessionData().then(({sessionData}) => {
          this.sessionData_ = sessionData;
        });
      }
    });
  }

  protected clearNewBadgeData_(e: CustomEvent) {
    const id = e.detail;
    this.featurePromoErrorMessage_ = '';

    this.handler_.clearNewBadgeData(id).then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      } else {
        this.handler_.getNewBadges().then(({newBadges}) => {
          this.newBadges_ = newBadges;
        });
      }
    });
  }

  protected clearWhatsNewData_() {
    if (!this.isWhatsNewV2_) {
      return;
    }
    this.featurePromoErrorMessage_ = '';

    this.handler_.clearWhatsNewData().then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      } else {
        this.handler_.getWhatsNewModules().then(({whatsNewModules}) => {
          this.whatsNewModules_ = whatsNewModules;
        });
        this.handler_.getWhatsNewEditions().then(({whatsNewEditions}) => {
          this.whatsNewEditions_ = whatsNewEditions;
        });
      }
    });
  }

  protected promoFilter_(promo: FeaturePromoDemoPageInfo) {
    return this.filter === '' ||
        promo.displayTitle.toLowerCase().includes(this.filter) ||
        promo.displayDescription.toLowerCase().includes(this.filter) ||
        promo.instructions.find(
            (instruction: string) =>
                instruction.toLowerCase().includes(this.filter)) ||
        promo.supportedPlatforms.find(
            (platform: string) => platform.toLowerCase().includes(this.filter));
  }

  protected whatsNewFilter_(item: (WhatsNewModuleDemoPageInfo|
                                   WhatsNewEditionDemoPageInfo)) {
    return this.filter === '' ||
        item.displayTitle.toLowerCase().includes(this.filter);
  }

  /**
   * Prevent clicks on sidebar items from navigating.
   */
  protected onLinkClick_(event: Event) {
    event.preventDefault();
  }

  protected onSelectorActivate_(event: CustomEvent<{selected: string}>) {
    const url = event.detail.selected;
    this.$.menu.selected = url;
    const idx = url.lastIndexOf('#');
    const el = this.$.content.querySelector(url.substring(idx));
    el?.scrollIntoView(true);
  }

  protected onNarrowChanged_(e: CustomEvent<{value: boolean}>) {
    this.narrow_ = e.detail.value;
  }

  protected onSessionExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.sessionExpanded_ = e.detail.value;
  }

  protected launchWhatsNewStaging_() {
    this.handler_.launchWhatsNewStaging();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'user-education-internals': UserEducationInternalsElement;
  }
}

customElements.define(
    UserEducationInternalsElement.is, UserEducationInternalsElement);
