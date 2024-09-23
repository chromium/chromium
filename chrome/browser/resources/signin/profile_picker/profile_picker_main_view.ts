// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import './icons.html.js';
import './profile_card.js';
import './strings.m.js';

import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {DraggableTileListInterface} from './drag_drop_reorder_tile_list_delegate.js';
import {DragDropReorderTileListDelegate} from './drag_drop_reorder_tile_list_delegate.js';
import type {ManageProfilesBrowserProxy, ProfileState} from './manage_profiles_browser_proxy.js';
import {ManageProfilesBrowserProxyImpl} from './manage_profiles_browser_proxy.js';
import {navigateTo, NavigationMixin, Routes} from './navigation_mixin.js';
import {isAskOnStartupAllowed, isProfileCreationAllowed} from './policy_helper.js';
import {getCss} from './profile_picker_main_view.css.js';
import {getHtml} from './profile_picker_main_view.html.js';

export interface ProfilePickerMainViewElement {
  $: {
    addProfile: HTMLElement,
    askOnStartup: CrCheckboxElement,
    'product-logo': HTMLElement,
    browseAsGuestButton: HTMLElement,
    profilesContainer: HTMLElement,
    wrapper: HTMLElement,
    forceSigninErrorDialog: CrDialogElement,
  };
}

const ProfilePickerMainViewElementBase =
    HelpBubbleMixinLit(WebUiListenerMixinLit(NavigationMixin(CrLitElement)));

export class ProfilePickerMainViewElement extends
    ProfilePickerMainViewElementBase implements DraggableTileListInterface {
  static get is() {
    return 'profile-picker-main-view';
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
       * Profiles list supplied by ManageProfilesBrowserProxy.
       */
      profilesList_: {type: Array},
      profilesListLoaded_: {type: Boolean},
      hideAskOnStartup_: {type: Boolean},
      askOnStartup_: {type: Boolean},
      guestModeEnabled_: {type: Boolean},
      forceSigninErrorDialogTitle_: {type: String},
      forceSigninErrorDialogBody_: {type: String},
      forceSigninErrorProfilePath_: {type: String},
      shouldShownSigninButton_: {type: Boolean},
    };
  }

  protected profilesList_: ProfileState[] = [];
  protected profilesListLoaded_: boolean = false;
  protected hideAskOnStartup_: boolean = false;
  protected askOnStartup_: boolean = loadTimeData.getBoolean('askOnStartup');
  // Initial value when the page is rendered.
  // Potentially updated on profile addition/removal/sign-in.
  protected guestModeEnabled_: boolean =
      loadTimeData.getBoolean('isGuestModeEnabled');
  private manageProfilesBrowserProxy_: ManageProfilesBrowserProxy =
      ManageProfilesBrowserProxyImpl.getInstance();
  private resizeObserver_: ResizeObserver|null = null;
  private previousRoute_: Routes|null = null;

  private dragDelegate_: DragDropReorderTileListDelegate|null = null;
  private dragDuration_: number = 300;

  // TODO(crbug.com/40280498): Move the dialog into it's own element with the
  // below members. This dialog state should be independent of the Profile
  // Picker itself.
  protected forceSigninErrorDialogTitle_: string = '';
  protected forceSigninErrorDialogBody_: string = '';
  private forceSigninErrorProfilePath_: string = '';
  protected shouldShownSigninButton_: boolean = false;

  override firstUpdated() {
    if (!this.guestModeEnabled_) {
      this.$.browseAsGuestButton.style.display = 'none';
    }

    if (!isProfileCreationAllowed()) {
      this.$.addProfile.style.display = 'none';
    }

    this.addEventListener('view-enter-finish', this.onViewEnterFinish_);

    this.addEventListener('toggle-drag', this.toggleDrag_);
  }

  override connectedCallback() {
    super.connectedCallback();
    this.addResizeObserver_();
    this.addWebUiListener(
        'profiles-list-changed', this.handleProfilesListChanged_.bind(this));
    this.addWebUiListener(
        'profile-removed', this.handleProfileRemoved_.bind(this));
    this.addWebUiListener(
        'display-force-signin-error-dialog',
        (title: string, body: string, profilePath: string) =>
            this.showForceSigninErrorDialog(title, body, profilePath));
    this.addWebUiListener(
        'guest-mode-availability-updated',
        this.maybeUpdateGuestMode_.bind(this));
    this.manageProfilesBrowserProxy_.initializeMainView();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    this.hideAskOnStartup_ = this.computeHideAskOnStartup_();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    this.initializeDragDelegate_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.resizeObserver_!.disconnect();

    if (this.dragDelegate_) {
      this.dragDelegate_.clearListeners();
    }
  }

  override onRouteChange(route: Routes) {
    if (route === Routes.MAIN) {
      return;
    }
    this.previousRoute_ = route;
  }

  private onViewEnterFinish_() {
    if (this.previousRoute_ !== Routes.NEW_PROFILE) {
      return;
    }
    // Focus the 'Add' button if coming back from the Add Profile flow.
    this.$.addProfile.focus();
  }

  private addResizeObserver_() {
    const profilesContainer = this.$.profilesContainer;
    this.resizeObserver_ = new ResizeObserver(() => {
      this.shadowRoot!.querySelector('.footer')!.classList.toggle(
          'division-line',
          profilesContainer.scrollHeight > profilesContainer.clientHeight);
    });
    this.resizeObserver_.observe(profilesContainer);
  }

  protected onProductLogoClick_() {
    this.$['product-logo'].animate(
        {
          transform: ['none', 'rotate(-10turn)'],
        },
        {
          duration: 500,
          easing: 'cubic-bezier(1, 0, 0, 1)',
        });
  }

  /**
   * Initializes the drag delegate, making sure to clear a previously existing
   * one.
   */
  private initializeDragDelegate_() {
    if (loadTimeData.getBoolean('profilesReorderingEnabled')) {
      if (this.dragDelegate_) {
        this.dragDelegate_.clearListeners();
      }

      this.dragDelegate_ = new DragDropReorderTileListDelegate(
          this, this.profilesList_, this, this.dragDuration_);
    }
  }

  /**
   * Handler for when the profiles list are updated.
   */
  private handleProfilesListChanged_(profilesList: ProfileState[]) {
    this.profilesListLoaded_ = true;
    this.profilesList_ = profilesList;
  }

  /**
   * Called when the user modifies 'Ask on startup' preference.
   */
  protected onAskOnStartupChangedByUser_(e: CustomEvent<{value: boolean}>) {
    if (this.hideAskOnStartup_) {
      return;
    }

    this.askOnStartup_ = e.detail.value;
    this.manageProfilesBrowserProxy_.askOnStartupChanged(e.detail.value);
  }

  protected onAddProfileClick_() {
    if (!isProfileCreationAllowed()) {
      return;
    }
    chrome.metricsPrivate.recordUserAction('ProfilePicker_AddClicked');
    navigateTo(Routes.NEW_PROFILE);
  }

  protected onLaunchGuestProfileClick_() {
    if (!this.guestModeEnabled_) {
      return;
    }
    this.manageProfilesBrowserProxy_.launchGuestProfile();
  }

  private maybeUpdateGuestMode_(enableGuestMode: boolean) {
    if (enableGuestMode === this.guestModeEnabled_) {
      return;
    }
    this.guestModeEnabled_ = enableGuestMode;
    if (enableGuestMode) {
      this.$.browseAsGuestButton.style.display = '';
    } else {
      this.$.browseAsGuestButton.style.display = 'none';
    }
  }

  private handleProfileRemoved_(profilePath: string) {
    const index = this.profilesList_.findIndex(
        profile => profile.profilePath === profilePath);
    assert(index !== -1);
    // TODO(crbug.com/40123459): Add animation.
    this.profilesList_.splice(index, 1);
    this.requestUpdate();
  }

  private computeHideAskOnStartup_(): boolean {
    return !isAskOnStartupAllowed() || this.profilesList_.length < 2;
  }

  private toggleDrag_(e: Event) {
    if (!this.dragDelegate_) {
      return;
    }

    const customEvent = e as CustomEvent;
    this.dragDelegate_.toggleDrag(customEvent.detail.toggle);
  }

  // @override
  onDragEnd(initialIndex: number, finalIndex: number): void {
    this.manageProfilesBrowserProxy_.updateProfileOrder(
        initialIndex, finalIndex);
  }

  // @override
  getDraggableTile(index: number): HTMLElement {
    return this.shadowRoot!.querySelector<HTMLElement>(
        `profile-card[data-index="${index}"]`)!;
  }

  // @override
  getDraggableTileIndex(tile: HTMLElement): number {
    return Number(tile.dataset['index']);
  }

  setDraggingTransitionDurationForTesting(duration: number) {
    this.dragDuration_ = duration;
  }

  getProfileListForTesting(): ProfileState[] {
    return this.profilesList_;
  }

  showForceSigninErrorDialog(title: string, body: string, profilePath: string):
      void {
    this.forceSigninErrorDialogTitle_ = title;
    this.forceSigninErrorDialogBody_ = body;
    this.forceSigninErrorProfilePath_ = profilePath;
    this.shouldShownSigninButton_ = profilePath.length !== 0;
    this.$.forceSigninErrorDialog.showModal();
  }

  protected onForceSigninErrorDialogOkButtonClicked_(): void {
    this.$.forceSigninErrorDialog.close();
    this.clearErrorDialogInfo_();
  }

  protected onReauthClicked_(): void {
    this.$.forceSigninErrorDialog.close();
    this.manageProfilesBrowserProxy_.launchSelectedProfile(
        this.forceSigninErrorProfilePath_);
    this.clearErrorDialogInfo_();
  }

  private clearErrorDialogInfo_(): void {
    this.forceSigninErrorDialogTitle_ = '';
    this.forceSigninErrorDialogBody_ = '';
    this.forceSigninErrorProfilePath_ = '';
    this.shouldShownSigninButton_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'profile-picker-main-view': ProfilePickerMainViewElement;
  }
}

customElements.define(
    ProfilePickerMainViewElement.is, ProfilePickerMainViewElement);
