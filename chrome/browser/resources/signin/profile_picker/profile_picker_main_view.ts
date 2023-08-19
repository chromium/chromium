// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './icons.html.js';
import './profile_card.js';
import './profile_picker_shared.css.js';
import './strings.m.js';

import {listenOnce} from '//resources/js/util_ts.js';
import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DragDropReorderTileListDelegate, DraggableTileListInterface} from './drag_drop_reorder_tile_list_delegate.js';
import {ManageProfilesBrowserProxy, ManageProfilesBrowserProxyImpl, ProfileState} from './manage_profiles_browser_proxy.js';
import {navigateTo, NavigationMixin, Routes} from './navigation_mixin.js';
import {isAskOnStartupAllowed, isGuestModeEnabled, isProfileCreationAllowed} from './policy_helper.js';
import {getTemplate} from './profile_picker_main_view.html.js';

export interface ProfilePickerMainViewElement {
  $: {
    addProfile: HTMLElement,
    askOnStartup: CrCheckboxElement,
    'product-logo': HTMLElement,
    browseAsGuestButton: HTMLElement,
    profilesContainer: HTMLElement,
    wrapper: HTMLElement,
    profiles: DomRepeat,
  };
}

const ProfilePickerMainViewElementBase =
    WebUiListenerMixin(NavigationMixin(PolymerElement));

export class ProfilePickerMainViewElement extends
    ProfilePickerMainViewElementBase implements DraggableTileListInterface {
  static get is() {
    return 'profile-picker-main-view';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Profiles list supplied by ManageProfilesBrowserProxy.
       */
      profilesList_: {
        type: Array,
        value: () => [],
      },

      profilesListLoaded_: {
        type: Boolean,
        value: false,
      },

      hideAskOnStartup_: {
        type: Boolean,
        value: true,
        computed: 'computeHideAskOnStartup_(profilesList_.length)',
      },

      askOnStartup_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('askOnStartup');
        },
      },
    };
  }

  private profilesList_: ProfileState[];
  private profilesListLoaded_: boolean;
  private hideAskOnStartup_: boolean;
  private askOnStartup_: boolean;
  private manageProfilesBrowserProxy_: ManageProfilesBrowserProxy =
      ManageProfilesBrowserProxyImpl.getInstance();
  private resizeObserver_: ResizeObserver|null = null;
  private previousRoute_: Routes|null = null;

  private dragDelegate_: DragDropReorderTileListDelegate|null = null;
  private dragDuration_: number = 300;

  override ready() {
    super.ready();
    if (!isGuestModeEnabled()) {
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
    this.manageProfilesBrowserProxy_.initializeMainView();
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

  private onProductLogoClick_() {
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
   * Handler for when the profiles list are updated.
   */
  private handleProfilesListChanged_(profilesList: ProfileState[]) {
    this.profilesListLoaded_ = true;
    this.profilesList_ = profilesList;

    if (loadTimeData.getBoolean('profilesReorderingEnabled')) {
      if (this.dragDelegate_) {
        this.dragDelegate_.clearListeners();
      }

      this.dragDelegate_ = new DragDropReorderTileListDelegate(
          this, this.profilesList_.length, this.dragDuration_);

      listenOnce(this, 'dom-change', () => {
        afterNextRender(this, () => {
          this.dragDelegate_!.initializeListeners();
        });
      });
    }
  }

  /**
   * Called when the user modifies 'Ask on startup' preference.
   */
  private onAskOnStartupChangedByUser_() {
    if (this.hideAskOnStartup_) {
      return;
    }

    this.manageProfilesBrowserProxy_.askOnStartupChanged(this.askOnStartup_);
  }

  private onAddProfileClick_() {
    if (!isProfileCreationAllowed()) {
      return;
    }
    chrome.metricsPrivate.recordUserAction('ProfilePicker_AddClicked');
    navigateTo(Routes.NEW_PROFILE);
  }

  private onLaunchGuestProfileClick_() {
    if (!isGuestModeEnabled()) {
      return;
    }
    this.manageProfilesBrowserProxy_.launchGuestProfile();
  }

  private handleProfileRemoved_(profilePath: string) {
    for (let i = 0; i < this.profilesList_.length; i += 1) {
      if (this.profilesList_[i].profilePath === profilePath) {
        // TODO(crbug.com/1063856): Add animation.
        this.splice('profilesList_', i, 1);
        break;
      }
    }
  }

  private computeHideAskOnStartup_(): boolean {
    return !isAskOnStartupAllowed() || !this.profilesList_ ||
        this.profilesList_.length < 2;
  }

  private toggleDrag_(e: Event) {
    if (!this.dragDelegate_) {
      return;
    }

    const customEvent = e as CustomEvent;
    this.dragDelegate_.toggleDrag(customEvent.detail.toggle);
  }


  // @override
  getDraggableTile(index: number): HTMLElement {
    return this.shadowRoot!.querySelector<HTMLElement>('#profile-' + index)!;
  }

  // @override
  getDraggableTileIndex(tile: HTMLElement): number {
    return this.$.profiles.indexForElement(tile) as number;
  }

  setDraggingTransitionDurationForTesting(duration: number) {
    this.dragDuration_ = duration;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'profile-picker-main-view': ProfilePickerMainViewElement;
  }
}

customElements.define(
    ProfilePickerMainViewElement.is, ProfilePickerMainViewElement);
