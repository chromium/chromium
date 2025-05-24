// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import './icons.html.js';
import './profile_card.js';
import '/strings.m.js';

import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {DraggableTileListInterface} from './drag_drop_reorder_tile_list_delegate.js';
import {DragDropReorderTileListDelegate} from './drag_drop_reorder_tile_list_delegate.js';
import type {ManageProfilesBrowserProxy, ProfileState} from './manage_profiles_browser_proxy.js';
import {ManageProfilesBrowserProxyImpl} from './manage_profiles_browser_proxy.js';
import {navigateTo, NavigationMixin, Routes} from './navigation_mixin.js';
import {isAskOnStartupAllowed, isGlicVersion, isProfileCreationAllowed} from './profile_picker_flags.js';
import {getCss} from './profile_picker_main_view.css.js';
import {getHtml} from './profile_picker_main_view.html.js';

export interface ProfilePickerMainViewElement {
  $: {
    addProfile: HTMLElement,
    askOnStartup: CrCheckboxElement,
    'picker-logo': HTMLElement,
    browseAsGuestButton: HTMLElement,
    profilesContainer: HTMLElement,
    profilesWrapper: HTMLElement,
    forceSigninErrorDialog: CrDialogElement,
  };
}

const ProfilePickerMainViewElementBase = HelpBubbleMixinLit(
    WebUiListenerMixinLit(I18nMixinLit(NavigationMixin(CrLitElement))));

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
      profileCreationAllowed_: {type: Boolean},
      forceSigninErrorDialogTitle_: {type: String},
      forceSigninErrorDialogBody_: {type: String},
      forceSigninErrorProfilePath_: {type: String},
      shouldShownSigninButton_: {type: Boolean},

      // Exposed to CSS as 'is-glic_'.
      isGlic_: {type: Boolean, reflect: true},
    };
  }

  protected accessor profilesList_: ProfileState[] = [];
  protected accessor profilesListLoaded_: boolean = false;
  protected accessor hideAskOnStartup_: boolean = false;
  protected accessor askOnStartup_: boolean =
      loadTimeData.getBoolean('askOnStartup');
  // Initial value when the page is rendered.
  // Potentially updated on profile addition/removal/sign-in.
  protected accessor guestModeEnabled_: boolean =
      loadTimeData.getBoolean('isGuestModeEnabled');
  protected accessor profileCreationAllowed_: boolean =
      isProfileCreationAllowed();
  protected accessor isGlic_: boolean = isGlicVersion();
  private manageProfilesBrowserProxy_: ManageProfilesBrowserProxy =
      ManageProfilesBrowserProxyImpl.getInstance();
  private resizeObserver_: ResizeObserver|null = null;
  private previousRoute_: Routes|null = null;

  private dragDelegate_: DragDropReorderTileListDelegate|null = null;
  private dragDuration_: number = 300;

  // TODO(crbug.com/40280498): Move the dialog into it's own element with the
  // below members. This dialog state should be independent of the Profile
  // Picker itself.
  protected accessor forceSigninErrorDialogTitle_: string = '';
  protected accessor forceSigninErrorDialogBody_: string = '';
  private accessor forceSigninErrorProfilePath_: string = '';
  protected accessor shouldShownSigninButton_: boolean = false;

  private eventTracker_: EventTracker = new EventTracker();

  override firstUpdated() {
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
    if (!this.isGlic_) {
      this.addWebUiListener(
          'guest-mode-availability-updated',
          this.maybeUpdateGuestMode_.bind(this));
    }
    this.manageProfilesBrowserProxy_.initializeMainView();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    this.hideAskOnStartup_ = this.computeHideAskOnStartup_();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    this.initializeDragDelegate_();

    // Cast necessary to expose protected members.
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('profilesListLoaded_') ||
        changedPrivateProperties.has('profilesList_')) {
      // The strings containing the link may appear dynamically, so we need to
      // update their `click` events accordingly.
      this.updateLearnMoreLinkEvents_();
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
    }

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
    if (this.isGlic_) {
      // In the Glic version, the separator is not needed. If added it will
      // interfere with the special background in this mode. Also a footer text
      // is shown, which already acts as a separator.
      return;
    }

    const profilesContainer = this.$.profilesContainer;
    this.resizeObserver_ = new ResizeObserver(() => {
      this.shadowRoot.querySelector('.footer')!.classList.toggle(
          'division-line',
          profilesContainer.scrollHeight > profilesContainer.clientHeight);
    });
    this.resizeObserver_.observe(profilesContainer);
  }

  protected onProductLogoClick_() {
    // No animation for Glic logo version.
    if (this.isGlic_) {
      return;
    }

    this.$['picker-logo'].animate(
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

  // Redirects the call to the handler, to create/use a browser to show the
  // Help page.
  private onLearnMoreClicked_(): void {
    assert(this.isGlic_);
    this.manageProfilesBrowserProxy_.onLearnMoreClicked();
  }

  protected getTitle_(): TrustedHTML {
    const titleStringResouce =
        // <if expr="enable_glic">
        this.isProfileListLoadedAndEmptyAndGlic_() ? 'glicTitleNoProfile' :
        // </if>
                                                     'mainViewTitle';
    // Special styling through 'class' attribute in some version of the title.
    return this.i18nAdvanced(titleStringResouce, {attrs: ['class']});
  }

  protected getSubtitle_(): TrustedHTML {
    const subtitleStringResource =
        // <if expr="enable_glic">
        this.isProfileListLoadedAndEmptyAndGlic_() ?
        'mainViewSubtitleGlicNoProfile' :
        // </if>
        'mainViewSubtitle';
    // Special tagging through 'class' attribute in some version of the
    // subtitle.
    return this.i18nAdvanced(subtitleStringResource, {attrs: ['class']});
  }

  protected shouldHideProfilesWrapper_(): boolean {
    if (!this.profilesListLoaded_) {
      return true;
    }

    return this.isProfileListLoadedAndEmptyAndGlic_();
  }

  protected shouldHideFooterText_(): boolean {
    if (this.isProfileListLoadedAndEmptyAndGlic_()) {
      return true;
    }

    return !isGlicVersion();
  }

  private isProfileListLoadedAndEmptyAndGlic_(): boolean {
    return this.profilesListLoaded_ && this.profilesList_.length === 0 &&
        isGlicVersion();
  }

  private updateLearnMoreLinkEvents_(): void {
    // This class is set in the string as a placeholder - check
    // `IDS_PROFILE_PICKER_ADD_PROFILE_HELPER_GLIC` and
    // `IDS_PROFILE_PICKER_MAIN_VIEW_SUBTITLE_GLIC_NO_PROFILE`. The given link
    // cannot be directly opened from this page since it is controlled by the
    // System Profile that is not allowed to open a browser. Therefore we
    // redirect the call to the handler which will load the last used profile
    // and open a browser with it.
    const links = this.shadowRoot.querySelectorAll('.learn-more-link');
    for (const link of links) {
      // Remove any potential existing event to avoid duplication of execution.
      this.eventTracker_.remove(link, 'click');
      // Add the event listener dynamically since we do not have access to the
      // string content before the page is loaded.
      this.eventTracker_.add(
          link, 'click', this.onLearnMoreClicked_.bind(this));
    }
  }

  // @override
  onDragEnd(initialIndex: number, finalIndex: number): void {
    this.manageProfilesBrowserProxy_.updateProfileOrder(
        initialIndex, finalIndex);
  }

  // @override
  getDraggableTile(index: number): HTMLElement {
    return this.shadowRoot.querySelector<HTMLElement>(
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
