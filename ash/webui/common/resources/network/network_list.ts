// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a collapsable list of networks.
 */

import './network_list_item.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';

import {CrScrollableMixin} from '//resources/ash/common/cr_elements/cr_scrollable_mixin.js';
import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {ListPropertyUpdateMixin} from '//resources/ash/common/cr_elements/list_property_update_mixin.js';
import type {GlobalPolicy} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import type {IronListElement} from '//resources/polymer/v3_0/iron-list/iron-list.js';
import {microTask, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './network_list.html.js';
import type {NetworkList} from './network_list_types.js';
import type {OncMojo} from './onc_mojo.js';

/**
 * Polymer class definition for 'network-list'.
 */

const NetworkListElementBase =
    I18nMixin(CrScrollableMixin(ListPropertyUpdateMixin(PolymerElement)));

export class NetworkListElement extends NetworkListElementBase {
  static get is() {
    return 'network-list' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The list of network state properties for the items to display.
       */
      networks: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * The list of custom items to display after the list of networks.
       */
      customItems: {
        type: Array,
        value() {
          return [];
        },
      },

      /** True if action buttons should be shown for the items. */
      showButtons: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** Whether to show technology badges on mobile network icons. */
      showTechnologyBadge: {type: Boolean, value: true},

      /**
       * Reflects the iron-list selecteditem property.
       */
      selectedItem: {
        type: Object,
        observer: 'selectedItemChanged_',
      },

      /** Whether cellular activation is unavailable in the current context. */
      activationUnavailable: Boolean,

      /**
       * DeviceState associated with the type of |networks| listed, or undefined
       * if none was provided.
       */
      deviceState: Object,

      globalPolicy: Object,

      isBuiltInVpnManagementBlocked: {
        type: Boolean,
        value: false,
      },

      /**
       * Contains |networks| + |customItems|.
       */
      listItems_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * Used by FocusRowBehavior to track the last focused element on a row.
       */
      lastFocused_: Object,

      /**
       * Used by FocusRowBehavior to track if the list has been blurred.
       */
      listBlurred_: Boolean,

      /** Disables all the network items. */
      disabled: Boolean,
    };
  }

  static get observers() {
    return ['updateListItems_(networks, customItems)'];
  }


  activationUnavailable: boolean;
  customItems: NetworkList.NetworkListItemType[];
  deviceState: OncMojo.DeviceStateProperties|undefined;
  disabled: boolean;
  globalPolicy: GlobalPolicy|undefined;
  isBuiltInVpnManagementBlocked: boolean;
  networks: OncMojo.NetworkStateProperties[];
  selectedItem: NetworkList.NetworkListItemType;
  showButtons: boolean;
  showTechnologyBadge: boolean;

  private focusRequested_: boolean;
  private lastFocused_: Object;
  private listBlurred_: boolean;
  private listItems_: NetworkList.NetworkListItemType[];
  private resizeObserver_: ResizeObserver|null;

  constructor() {
    super();

    /**
     * used to observer size changes to this element
     */
    this.resizeObserver_ = null;

    this.focusRequested_ = false;
  }

  override connectedCallback() {
    super.connectedCallback();

    // This is a required work around to get the iron-list to display on first
    // view. Currently iron-list won't generate item elements on attach if the
    // element is not visible. Because there are some instances where this
    // component might not be visible when the items are bound, we listen for
    // resize events and manually call notifyResize on the iron-list
    this.resizeObserver_ = new ResizeObserver(_entries => {
      const networkList =
          this.shadowRoot!.querySelector<IronListElement>('#networkList');
      if (networkList) {
        networkList.notifyResize();
      }
    });
    this.resizeObserver_.observe(this);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
    }
  }

  override focus() {
    this.focusRequested_ = true;
    this.focusFirstItem_();
  }

  private updateListItems_() {
    // TODO(http://b/386245333): Remove testing existence of the property
    // showBeforeNetworksList.
    const beforeNetworks = this.customItems.filter(
        n =>
            ('showBeforeNetworksList' in n &&
             n.showBeforeNetworksList === true));
    const afterNetworks = this.customItems.filter(
        n =>
            !('showBeforeNetworksList' in n &&
              n.showBeforeNetworksList === true));
    const newList = beforeNetworks.concat(this.networks, afterNetworks);

    // TODO(http://b/386245333): Remove testing existence of the property guid.
    this.updateList(
        'listItems_', item => ('guid' in item) ? item.guid : '', newList);

    if (this.resizeObserver_) {
      this.updateScrollableContents();
    }
    if (this.focusRequested_) {
      microTask.run(() => {
        this.focusFirstItem_();
      });
    }
  }

  private focusFirstItem_() {
    // Select the first network-list-item if there is one.
    const item = this.shadowRoot!.querySelector('network-list-item');
    if (!item) {
      return;
    }
    item.focus();
    this.focusRequested_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkListElement.is]: NetworkListElement;
  }
}

customElements.define(NetworkListElement.is, NetworkListElement);
