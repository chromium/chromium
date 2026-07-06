// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assertNotReachedCase} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import type {PageActionState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {PageActionId, PageActionTrigger} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getCss} from './page_action_icon.css.js';
import {getHtml} from './page_action_icon.html.js';
import {getClickSourceType} from './toolbar_button.js';

export interface PageActionIconElement {
  $: {
    button: CrIconButtonElement,
  };
}

export class PageActionIconElement extends CrLitElement {
  static get is() {
    return 'page-action-icon';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      state: {type: Object},
    };
  }

  accessor state: PageActionState = {
    pageActionId: PageActionId.kActionAiMode,
    accessibleName: '',
    tooltipText: '',
  };

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  protected getIconClass_(): string {
    const actionId = this.state.pageActionId;

    switch (actionId) {
      case PageActionId.kActionShowPasswordsBubbleOrPage:
        return 'icon-password-manager';
      case PageActionId.kActionShowMemorySaverChip:
        return 'icon-performance-speedometer';
      case PageActionId.kActionShowTranslate:
        return 'icon-translate';
      case PageActionId.kActionBookmarkThisTab:
        return 'icon-bookmark';
      case PageActionId.kActionRecordReplay:
        return 'icon-screen-record';

      // Placeholders with TODOs for missing SVGs
      case PageActionId.kActionAiMode:
      case PageActionId.kActionIndigo:
      case PageActionId.kActionMultistepFilter:
      case PageActionId.kActionSidePanelShowLensOverlayResults:
      case PageActionId.kActionLensOverlayHomework:
      case PageActionId.kActionShowJsOptimizationsIcon:
      case PageActionId.kActionShowIntentPicker:
      case PageActionId.kActionZoomNormal:
      case PageActionId.kActionSidePanelShowReadAnything:
      case PageActionId.kActionOffersAndRewardsForPage:
      case PageActionId.kActionShowFileSystemAccess:
      case PageActionId.kActionInstallPwa:
      case PageActionId.kActionCommercePriceInsights:
      case PageActionId.kActionCommerceDiscounts:
      case PageActionId.kActionShowCollaborationRecentActivity:
      case PageActionId.kActionAutofillMandatoryReauth:
      case PageActionId.kActionFind:
      case PageActionId.kActionShowCookieControls:
      case PageActionId.kActionShowAddressesBubbleOrPage:
      case PageActionId.kActionVirtualCardEnroll:
      case PageActionId.kActionFilledCardInformation:
      case PageActionId.kActionShowPaymentsBubbleOrPage:
      case PageActionId.kActionSidePanelShowContextualTasks:
      case PageActionId.kActionFederation:
      case PageActionId.kActionGlicContextualCueing:
      case PageActionId.kActionAnchoredContextualCue:
      case PageActionId.kActionWebAuthnAmbientSignin:
      case PageActionId.kActionAutofillPayment:
      case PageActionId.kActionShowPaymentsChurnedUsersBubble:
      case PageActionId.kActionFakePageActionForDebug:
        // TODO: Track down the missing .svg files for these actions.
        return 'icon-placeholder';

      default:
        assertNotReachedCase(actionId);
    }
  }

  protected getAriaLabel_(): string {
    return this.state.accessibleName;
  }

  protected onClick_(e: Event) {
    this.browserProxy_.toolbarUIHandler.onPageActionClick(
        this.state.pageActionId, this.getPageActionTrigger_(e));
  }

  private getPageActionTrigger_(e: Event): PageActionTrigger {
    const sourceType = getClickSourceType(e);
    switch (sourceType) {
      case MenuSourceType.kTouch:
        return PageActionTrigger.kGesture;
      case MenuSourceType.kKeyboard:
        return PageActionTrigger.kKeyboard;
      case MenuSourceType.kMouse:
        return PageActionTrigger.kMouse;
      default:
        throw new Error('Unknown sourceType ' + sourceType);
    }
  }

  protected onPointerenter_() {
    this.fire('chip-pointerenter');
  }

  protected onPointerleave_() {
    this.fire('chip-pointerleave');
  }

  protected onPointercancel_() {
    this.fire('chip-pointercancel');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'page-action-icon': PageActionIconElement;
  }
}

customElements.define(PageActionIconElement.is, PageActionIconElement);
