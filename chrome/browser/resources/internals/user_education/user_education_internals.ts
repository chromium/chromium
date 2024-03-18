// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/help_bubble/help_bubble.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import 'chrome://resources/cr_elements/cr_nav_menu_item_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import './user_education_internals_card.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {HelpBubbleMixinInterface} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import type {CrContainerShadowMixinInterface} from 'chrome://resources/cr_elements/cr_container_shadow_mixin.js';
import {CrContainerShadowMixin} from 'chrome://resources/cr_elements/cr_container_shadow_mixin.js';
import type {CrMenuSelector} from 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import type {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import type {DomRepeat} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './user_education_internals.html.js';
import type {FeaturePromoDemoPageData, FeaturePromoDemoPageInfo, UserEducationInternalsPageHandlerInterface} from './user_education_internals.mojom-webui.js';
import {UserEducationInternalsPageHandler} from './user_education_internals.mojom-webui.js';

const UserEducationInternalsElementBase =
    CrContainerShadowMixin(HelpBubbleMixin(PolymerElement)) as {
      new (): PolymerElement & HelpBubbleMixinInterface &
          CrContainerShadowMixinInterface,
    };

interface UserEducationInternalsElement {
  $: {
    content: Element,
    errorMessageToast: CrToastElement,
    menu: CrMenuSelector,
    promos: DomRepeat,
    toolbar: CrToolbarElement,
    tutorials: DomRepeat,
  };
}

class UserEducationInternalsElement extends UserEducationInternalsElementBase {
  static get is() {
    return 'user-education-internals';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Substring filter that (when set) shows only entries containing
       * `filter`.
       */
      filter: String,
      /**
       * List of tutorials and feature_promos that can be started.
       * Each tutorial has a string identifier.
       */
      tutorials_: Array,
      featurePromos_: Array,
      featurePromoErrorMessage_: String,
      narrow_: Boolean,

      /**
       * Indicates if the list of promo data is expanded or collapsed.
       */
      sessionExpanded_: {
        type: Boolean,
        value: false,
      },
    };
  }

  filter: string = '';
  private tutorials_: FeaturePromoDemoPageInfo[];
  private featurePromos_: FeaturePromoDemoPageInfo[];
  private newBadges_: FeaturePromoDemoPageInfo[];
  private featurePromoErrorMessage_: string;
  private narrow_: boolean = false;
  private sessionData_: FeaturePromoDemoPageData[];

  private handler_: UserEducationInternalsPageHandlerInterface;

  constructor() {
    super();
    this.handler_ = UserEducationInternalsPageHandler.getRemote();
  }

  override ready() {
    super.ready();
    ColorChangeUpdater.forDocument().start();

    // There is a self-referential demo IPH for showing a help bubble in a
    // WebUI (specifically, this WebUI). Because of that, the target anchor for
    // the help bubble needs to be registered.
    //
    // However, because we want to attach the help bubble to an element created
    // by a dom-repeat, we have to wait until after the dom-repeat populates to
    // register the anchor element.
    this.$.promos.addEventListener(
        'rendered-item-count-changed', (_: Event) => {
          this.registerHelpBubble(
              'kWebUIIPHDemoElementIdentifier',
              ['#IPH_WebUiHelpBubbleTest', '#launch']);
        }, {
          once: true,
        });

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
  }

  private onSearchChanged_(e: CustomEvent) {
    this.filter = (e.detail as string).toLowerCase();
  }

  private startTutorial_(e: CustomEvent) {
    const id = e.detail;
    this.featurePromoErrorMessage_ = '';

    this.handler_.startTutorial(id).then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      }
    });
  }

  private showFeaturePromo_(e: CustomEvent) {
    const id = e.detail;
    this.featurePromoErrorMessage_ = '';

    this.handler_.showFeaturePromo(id).then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      }
    });
  }

  private clearPromoData_(e: CustomEvent) {
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

  private clearSessionData_() {
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

  private clearNewBadgeData_(e: CustomEvent) {
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

  private promoFilter_(promo: FeaturePromoDemoPageInfo, filter: string) {
    return filter === '' || promo.displayTitle.toLowerCase().includes(filter) ||
        promo.displayDescription.toLowerCase().includes(filter) ||
        promo.instructions.find(
            (instruction: string) =>
                instruction.toLowerCase().includes(filter)) ||
        promo.supportedPlatforms.find(
            (platform: string) => platform.toLowerCase().includes(filter));
  }

  /**
   * Prevent clicks on sidebar items from navigating.
   */
  private onLinkClick_(event: Event) {
    event.preventDefault();
  }

  private onSelectorActivate_(event: CustomEvent<{selected: string}>) {
    const url = event.detail.selected;
    this.$.menu.selected = url;
    const idx = url.lastIndexOf('#');
    const el = this.$.content.querySelector(url.substring(idx));
    el?.scrollIntoView(true);
  }
}

customElements.define(
    UserEducationInternalsElement.is, UserEducationInternalsElement);
