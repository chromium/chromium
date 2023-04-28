// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/help_bubble/help_bubble.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './user_education_internals_card.js';

import {startColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {HelpBubbleMixin, HelpBubbleMixinInterface} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {CrContainerShadowMixin, CrContainerShadowMixinInterface} from 'chrome://resources/cr_elements/cr_container_shadow_mixin.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './user_education_internals.html.js';
import {FeaturePromoDemoPageInfo, UserEducationInternalsPageHandler, UserEducationInternalsPageHandlerInterface} from './user_education_internals.mojom-webui.js';

const UserEducationInternalsElementBase =
    CrContainerShadowMixin(HelpBubbleMixin(PolymerElement)) as {
      new (): PolymerElement & HelpBubbleMixinInterface &
          CrContainerShadowMixinInterface,
    };

interface UserEducationInternalsElement {
  $: {
    errorMessageToast: CrToastElement,
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
    };
  }

  filter: string = '';
  private tutorials_: FeaturePromoDemoPageInfo[];
  private featurePromos_: FeaturePromoDemoPageInfo[];
  private featurePromoErrorMessage_: string;
  private narrow_: boolean = false;

  private handler_: UserEducationInternalsPageHandlerInterface;

  constructor() {
    super();
    this.handler_ = UserEducationInternalsPageHandler.getRemote();
  }

  override ready() {
    super.ready();
    startColorChangeUpdater();

    this.handler_.getTutorials().then(({tutorialInfos}) => {
      this.tutorials_ = tutorialInfos;
    });

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

    this.handler_.getFeaturePromos().then(({featurePromos}) => {
      this.featurePromos_ = featurePromos;
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

  private promoFilter_(promo: FeaturePromoDemoPageInfo, filter: string) {
    return filter === '' || promo.displayTitle.toLowerCase().includes(filter) ||
        promo.displayDescription.toLowerCase().includes(filter) ||
        promo.instructions.find(
            (instruction: string) =>
                instruction.toLowerCase().includes(filter)) ||
        promo.supportedPlatforms.find(
            (platform: string) => platform.toLowerCase().includes(filter));
  }
}

customElements.define(
    UserEducationInternalsElement.is, UserEducationInternalsElement);
