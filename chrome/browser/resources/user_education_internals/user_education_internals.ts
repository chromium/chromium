// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_components/help_bubble/help_bubble.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/icons.html.js';
import './user_education_internals_card.js';
import './user_education_whats_new_internals_card.js';

// <if expr="not is_chromeos">
import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
// </if>

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import type {CrMenuSelectorElement} from 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import type {CrPageSelectorElement} from 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {TrackedElementManager} from 'chrome://resources/js/tracked_element/tracked_element_manager.js';
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
    menu: CrMenuSelectorElement,
    selector: CrPageSelectorElement,
    // <if expr="not is_chromeos">
    whatsNewVersionOverride: CrInputElement,
    // </if>
  };
}

const UserEducationInternalsElementBase = HelpBubbleMixinLit(CrLitElement);

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
       * Index of the selected "tab" determining which data to show. -1 means
       * show all.
       */
      selectedTabIndex: {type: Number},
      /**
       * List of tutorials and feature_promos that can be started.
       * Each tutorial has a string identifier.
       */
      tutorials_: {type: Array},
      featurePromos_: {type: Array},
      featurePromoErrorMessage_: {type: String},
      narrow_: {type: Boolean},
      newBadges_: {type: Array},
      nonIphPromos_: {type: Array},

      /**
       * Indicates if the information about session data is expanded or
       * collapsed.
       */
      sessionExpanded_: {
        type: Boolean,
        value: false,
      },

      ntpPromoPreferencesExpanded_: {
        type: Boolean,
        value: false,
      },

      /**
       * The current version used to request the What's New page.
       */
      whatsNewVersionToRequest_: {
        type: Number,
      },
      whatsNewModules_: {type: Array},
      whatsNewEditions_: {type: Array},
      ntpPromos_: {type: Array},
      ntpPromoPreferences_: {type: Array},
      sessionData_: {type: Array},
      currentChromeVersion_: {type: Number},
      initialized_: {type: Boolean},
    };
  }

  accessor filter: string = '';
  accessor selectedTabIndex = -1;
  protected accessor tutorials_: FeaturePromoDemoPageInfo[] = [];
  protected accessor featurePromos_: FeaturePromoDemoPageInfo[] = [];
  protected accessor newBadges_: FeaturePromoDemoPageInfo[] = [];
  protected accessor nonIphPromos_: FeaturePromoDemoPageInfo[] = [];
  protected accessor whatsNewModules_: WhatsNewModuleDemoPageInfo[] = [];
  protected accessor whatsNewEditions_: WhatsNewEditionDemoPageInfo[] = [];
  protected accessor ntpPromos_: FeaturePromoDemoPageInfo[] = [];
  protected accessor ntpPromoPreferences_: FeaturePromoDemoPageData[] = [];
  protected accessor ntpPromoPreferencesExpanded_: boolean = false;
  protected accessor featurePromoErrorMessage_: string = '';
  protected accessor narrow_: boolean = false;
  protected accessor sessionExpanded_: boolean = false;
  protected accessor sessionData_: FeaturePromoDemoPageData[] = [];
  protected accessor whatsNewVersionToRequest_: number =
      loadTimeData.getInteger('whatsNewVersionToRequest');
  protected accessor currentChromeVersion_: number =
      loadTimeData.getInteger('currentChromeVersion');
  protected accessor initialized_ = false;

  private handler_: UserEducationInternalsPageHandlerInterface;

  constructor() {
    super();
    this.handler_ = UserEducationInternalsPageHandler.getRemote();
  }

  override firstUpdated() {
    ColorChangeUpdater.forDocument().start();

    this.checkInitializedAndReadAllData_();

    // These are used in tests of the TrackedElementManager and its handler.
    const manager = TrackedElementManager.getInstance();
    manager.startTracking(
        this.$.menu, 'UserEducationInternalsUI::kMenuElementId');
    for (const child of this.$.menu.children) {
      if (child instanceof HTMLElement && child.getAttribute('index')) {
        manager.startTracking(
            child, 'UserEducationInternalsUI::kMenuItemElementId',
            {secondaryId: 'index:' + child.getAttribute('index')});
      }
    }
  }

  // If data is read from the backend before the Feature Engagement system is
  // initialized, certain information will be missing (including all Non-IPH
  // Promos). This method polls the backend until initialization is complete and
  // then loads the data.
  private checkInitializedAndReadAllData_() {
    this.handler_.isFeatureEngagementInitialized().then(({isInitialized}) => {
      if (isInitialized) {
        this.initialized_ = true;
        this.readAllData_();
      } else {
        setTimeout(() => {
          this.checkInitializedAndReadAllData_();
        }, 1000);
      }
    });
  }

  private readAllData_() {
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

    this.handler_.getNonIphPromos().then(({nonIphPromos}) => {
      this.nonIphPromos_ = nonIphPromos;
    });

    this.handler_.getWhatsNewModules().then(({whatsNewModules}) => {
      this.whatsNewModules_ = whatsNewModules;
    });

    this.handler_.getWhatsNewEditions().then(({whatsNewEditions}) => {
      this.whatsNewEditions_ = whatsNewEditions;
    });

    this.handler_.getNtpPromos().then(({ntpPromos}) => {
      this.ntpPromos_ = ntpPromos;
    });

    this.handler_.getNtpPromoPreferences().then(({ntpPromoPreferences}) => {
      this.ntpPromoPreferences_ = ntpPromoPreferences;
    });
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
      if (this.shadowRoot.querySelector('#IPH_WebUiHelpBubbleTest')) {
        this.registerHelpBubble(
            'kWebUIIPHDemoElementIdentifier',
            ['#IPH_WebUiHelpBubbleTest', '#launch']);
      }
    }
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.filter = e.detail.toLowerCase();
  }

  protected onTutorialPromoLaunch_(e: CustomEvent<string>) {
    const id = e.detail;
    this.featurePromoErrorMessage_ = '';

    this.handler_.startTutorial(id).then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      }
    });
  }

  protected onFeaturePromoPromoLaunch_(e: CustomEvent<string>) {
    const id = e.detail;
    this.featurePromoErrorMessage_ = '';

    // Promos may go into a queue, so notify the user that the promo is waiting
    // to show. If an error occurs, the toast will be updated with the error
    // message.
    this.featurePromoErrorMessage_ = 'Waiting to show promo...';
    this.$.errorMessageToast.show();

    this.handler_.showFeaturePromo(id).then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      }
    });
  }

  protected onFeaturePromoClearPromoData_(e: CustomEvent<string>) {
    const id = e.detail;
    this.featurePromoErrorMessage_ = '';

    this.handler_.clearFeaturePromoData(id).then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      } else {
        this.handler_.getFeaturePromos().then(({featurePromos}) => {
          this.featurePromos_ = featurePromos;
          this.requestUpdate();
        });
      }
    });
  }

  protected onClearSessionDataClick_() {
    if (!confirm(
            'This will reset the browser to a "fresh profile" state,' +
            ' which may trigger multiple grace periods. Proceed?')) {
      return;
    }
    this.handler_.clearSessionData().then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      } else {
        this.handler_.getSessionData().then(({sessionData}) => {
          this.sessionData_ = sessionData;
          this.requestUpdate();
        });
      }
    });
  }

  protected onForceNewSessionClick_() {
    this.handler_.forceNewSession().then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      } else {
        this.handler_.getSessionData().then(({sessionData}) => {
          this.sessionData_ = sessionData;
          this.requestUpdate();
        });
      }
    });
  }

  protected onRemoveGracePeriodsClick_() {
    this.handler_.removeGracePeriods().then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      } else {
        this.handler_.getSessionData().then(({sessionData}) => {
          this.sessionData_ = sessionData;
          this.requestUpdate();
        });
      }
    });
  }

  protected onNewBadgeClearPromoData_(e: CustomEvent<string>) {
    const id = e.detail;
    this.featurePromoErrorMessage_ = '';

    this.handler_.clearNewBadgeData(id).then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      } else {
        this.handler_.getNewBadges().then(({newBadges}) => {
          this.newBadges_ = newBadges;
          this.requestUpdate();
        });
      }
    });
  }

  protected onNonIphClearPromoData_(e: CustomEvent<string>) {
    const id = e.detail;
    this.featurePromoErrorMessage_ = '';

    this.handler_.clearNonIphPromoData(id).then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      } else {
        this.handler_.getNonIphPromos().then(({nonIphPromos}) => {
          this.nonIphPromos_ = nonIphPromos;
          this.requestUpdate();
        });
      }
    });
  }

  protected onClearWhatsNewData_() {
    this.featurePromoErrorMessage_ = '';

    this.handler_.clearWhatsNewData().then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      } else {
        this.handler_.getWhatsNewModules().then(({whatsNewModules}) => {
          this.whatsNewModules_ = whatsNewModules;
          this.requestUpdate();
        });
        this.handler_.getWhatsNewEditions().then(({whatsNewEditions}) => {
          this.whatsNewEditions_ = whatsNewEditions;
          this.requestUpdate();
        });
      }
    });
  }

  protected onNtpPromoClearPromoData_(e: CustomEvent<string>) {
    const id = e.detail;
    this.featurePromoErrorMessage_ = '';
    this.handler_.clearNtpPromoData(id).then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      } else {
        this.handler_.getNtpPromos().then(({ntpPromos}) => {
          this.ntpPromos_ = ntpPromos;
          this.requestUpdate();
        });
      }
    });
  }

  protected onClearNtpPromoPreferencesClick_() {
    this.handler_.clearNtpPromoPreferences().then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
      if (errorMessage !== '') {
        this.$.errorMessageToast.show();
      } else {
        this.handler_.getNtpPromoPreferences().then(({ntpPromoPreferences}) => {
          this.ntpPromoPreferences_ = ntpPromoPreferences;
          this.requestUpdate();
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

  protected onSelectorIronActivate_(event: CustomEvent<{selected: string}>) {
    const index = Number(event.detail.selected);
    this.$.menu.selected = index;
    if (index >= 0) {
      this.$.selector.removeAttribute('show-all');
      this.selectedTabIndex = index;
    } else {
      this.$.selector.setAttribute('show-all', 'true');
    }
  }

  protected onNarrowChanged_(e: CustomEvent<{value: boolean}>) {
    this.narrow_ = e.detail.value;
  }

  protected onSessionExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.sessionExpanded_ = e.detail.value;
  }

  protected onNtpPromoPreferencesExpandedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.ntpPromoPreferencesExpanded_ = e.detail.value;
  }

  // <if expr="not is_chromeos">
  protected onLaunchWhatsNewStagingClick_() {
    this.handler_.launchWhatsNewStaging();
  }

  protected onWhatsNewVersionOverrideClick_() {
    if (!this.$.whatsNewVersionOverride.validate()) {
      return;
    }
    const versionOverride: number =
        Number.parseInt(this.$.whatsNewVersionOverride.value);
    if (Number.isNaN(versionOverride)) {
      return;
    }
    this.handler_.updateWhatsNewVersionOverride(versionOverride);
    this.whatsNewVersionToRequest_ = versionOverride;
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'user-education-internals': UserEducationInternalsElement;
  }
}

customElements.define(
    UserEducationInternalsElement.is, UserEducationInternalsElement);
