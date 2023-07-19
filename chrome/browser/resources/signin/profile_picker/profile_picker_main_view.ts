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
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ManageProfilesBrowserProxy, ManageProfilesBrowserProxyImpl, ProfileState} from './manage_profiles_browser_proxy.js';
import {navigateTo, NavigationMixin, Routes} from './navigation_mixin.js';
import {isAskOnStartupAllowed, isGuestModeEnabled, isProfileCreationAllowed} from './policy_helper.js';
import {getTemplate} from './profile_picker_main_view.html.js';

// Interface to interact with the real underlying tile list that expects to have
// the Drag and Drop functionality.
interface DraggableTileListInterface {
  // Given an index, returns the HTMLElement corresponding to the draggable
  // tile.
  getDraggableTile(index: number): HTMLElement;
}

// This delegate class allows any Polymer list container of tiles 'T' to add
// the drag and drop with reordering functionality.
//
// The events that will be redirected to this delegate are:
// - 'dragstart': triggered once when a tile is initially being dragged.
// - 'dragend': triggered once when a tile drag stops, after a drop.
// A full drag event cycle starts with 'dragstart' and ends with 'dragend'.
//
// To activate the drag and drop functionality, a call to
// `initializeTileListListeners()` will attach all necessary 'drag-' event
// listeners to the proper tiles. This method must be called once the HTML
// tiles, that are intended to be drag and dropped, are properly rendered.
class DragDropReorderTileListDelegate {
  // ---------------------------------------------------------------------------
  // public section:

  constructor(tileListInterface: DraggableTileListInterface) {
    this.tileListInterface_ = tileListInterface;
  }

  // Initialize tiles to be able to react to drag events.
  initializeTileListListeners(tileCount: number) {
    for (let i = 0; i < tileCount; ++i) {
      const tile = this.getDraggableTile_(i);
      tile.draggable = true;

      tile.addEventListener('dragstart', (event: DragEvent) => {
        this.onDragStart_(event);
      });

      // TODO: check if this event delay can be removed for MacOS.
      // It is making the drop have an awkward movement.
      tile.addEventListener('dragend', (event: DragEvent) => {
        this.onDragEnd_(event);
      });
    }
  }

  // ---------------------------------------------------------------------------
  // private section

  // Event 'dragstart' is applied on the tile that will be dragged. We store the
  // tile being dragged in temporary member variables that will be used
  // throughout a single drag event cycle. We need to store information in
  // member variables since future events will be triggered in different stack
  // calls.
  private onDragStart_(event: DragEvent) {
    // 'event.target' corresponds to the tile being dragged. Implicit cast to
    // an HTMLElement.
    this.markDraggingTile_(event.target as HTMLElement);
  }

  // Event 'dragend` is applied on the tile that was dragged and now dropped. We
  // restore all the temporary member variables to their original state. It is
  // the end of the drag event cycle.
  // TODO: Apply the reordering in this function later.
  private onDragEnd_(event: DragEvent) {
    // The 'event.target' of the 'dragend' event is expected to be the same as
    // the one that started the drag event cycle.
    assert(this.draggingTile_);
    assert(this.draggingTile_ === event.target as HTMLElement);
    this.resetDraggingTile_();
  }

  // Prepare 'this.draggingTile_' member variable as the dragging tile.
  // It will used throughout each drag event cycle and reset in the
  // `resetDraggingTile_()` method which restore the tile to it's initial state.
  private markDraggingTile_(element: HTMLElement) {
    this.draggingTile_ = element;
    this.draggingTile_.classList.add('dragging');

    // Apply specific style to hide the tile that is being dragged, making sure
    // only the image that sticks on the mouse pointer to be displayed while
    // dragging. A very low value different than 0 is needed, otherwise the
    // element would be considered invisible and would not react to drag events
    // anymore. A value of '0.001' is enough to simulate the 'invisible' effect.
    this.draggingTile_.style.opacity = '0.001';
  }

  // Restores `this.draggingTile_` to it's initial state.
  private resetDraggingTile_() {
    this.draggingTile_!.style.removeProperty('opacity');

    this.draggingTile_!.classList.remove('dragging');
    this.draggingTile_ = null;
  }

  private getDraggableTile_(index: number) {
    return this.tileListInterface_.getDraggableTile(index);
  }

  // ---------------------------------------------------------------------------
  // private members:

  private tileListInterface_: DraggableTileListInterface;

  private draggingTile_: HTMLElement|null = null;
}

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

  private dragDelegate_: DragDropReorderTileListDelegate;

  override ready() {
    super.ready();
    if (!isGuestModeEnabled()) {
      this.$.browseAsGuestButton.style.display = 'none';
    }

    if (!isProfileCreationAllowed()) {
      this.$.addProfile.style.display = 'none';
    }

    this.addEventListener('view-enter-finish', this.onViewEnterFinish_);
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
      this.dragDelegate_ = new DragDropReorderTileListDelegate(this);

      listenOnce(this, 'dom-change', () => {
        afterNextRender(this, () => {
          this.dragDelegate_.initializeTileListListeners(
              this.profilesList_.length);
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

  // @override
  getDraggableTile(index: number): HTMLElement {
    return this.shadowRoot!.querySelector<HTMLElement>('#profile-' + index)!;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'profile-picker-main-view': ProfilePickerMainViewElement;
  }
}

customElements.define(
    ProfilePickerMainViewElement.is, ProfilePickerMainViewElement);
