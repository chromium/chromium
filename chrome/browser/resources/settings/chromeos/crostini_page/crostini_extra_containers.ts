// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-extra-containers' is the settings extras containers subpage for
 * Crostini.
 */
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './crostini_extra_containers_create_dialog.js';
import '../../settings_shared.css.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {hexColorToSkColor} from 'chrome://resources/js/color_utils.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ContainerInfo, GuestId} from '../guest_os/guest_os_browser_proxy.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, DEFAULT_CROSTINI_CONTAINER, DEFAULT_CROSTINI_VM} from './crostini_browser_proxy.js';
import {getTemplate} from './crostini_extra_containers.html.js';

type HtmlElementWithData<T extends HTMLElement = HTMLElement> = T&{
  'dataContainerId': GuestId,
};

interface ExtraContainersElement {
  $: {
    containerMenu: CrLazyRenderElement<CrActionMenuElement>,
  };
}

const ExtraContainersElementBase = WebUiListenerMixin(PolymerElement);

class ExtraContainersElement extends ExtraContainersElementBase {
  static get is() {
    return 'settings-crostini-extra-containers';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      showCreateContainerDialog_: {
        type: Boolean,
        value: false,
      },

      allContainers_: {
        type: Array,
        notify: true,
        value() {
          return [];
        },
      },

      lastMenuContainerInfo_: {
        type: Object,
      },

      /**
       * Whether the export import buttons should be enabled. Initially false
       * until status has been confirmed.
       */
      enableButtons_: {
        type: Boolean,
        computed:
            'isEnabledButtons_(installerShowing_, exportImportInProgress_)',
      },

      installerShowing_: {
        type: Boolean,
        value: false,
      },

      // TODO(b/231890242): Disable delete and stop buttons when a container is
      // being exported or imported.
      exportImportInProgress_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private allContainers_: ContainerInfo[];
  private browserProxy_: CrostiniBrowserProxy;
  private exportImportInProgress_: boolean;
  private installerShowing_: boolean;
  private lastMenuContainerInfo_: ContainerInfo|null;
  private showCreateContainerDialog_: boolean;

  constructor() {
    super();
    /**
     * Tracks the last container that was selected for delete.
     */
    this.lastMenuContainerInfo_ = null;

    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  override ready() {
    super.ready();
    this.addWebUIListener(
        'crostini-container-info',
        (infos: ContainerInfo[]) => this.onContainerInfo_(infos));
    this.browserProxy_.requestContainerInfo();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.addWebUIListener(
        'crostini-export-import-operation-status-changed',
        (inProgress: boolean) => {
          this.exportImportInProgress_ = inProgress;
        });
    this.addWebUIListener(
        'crostini-installer-status-changed', (installerShowing: boolean) => {
          this.installerShowing_ = installerShowing;
        });

    this.browserProxy_.requestCrostiniExportImportOperationStatus();
    this.browserProxy_.requestCrostiniInstallerStatus();
  }

  private onContainerInfo_(containerInfos: ContainerInfo[]) {
    this.set('allContainers_', containerInfos);
  }

  private onCreateClick_() {
    this.showCreateContainerDialog_ = true;
  }

  private onCreateContainerDialogClose_() {
    this.showCreateContainerDialog_ = false;
  }

  private onContainerMenuClick_(event: Event) {
    const target = event.currentTarget as HtmlElementWithData;
    const containerId = target['dataContainerId'];
    this.lastMenuContainerInfo_ =
        this.allContainers_.find(
            e => e.id.vm_name === containerId.vm_name &&
                e.id.container_name === containerId.container_name) ||
        null;
    this.getContainerMenu_().showAt(target);
  }

  private onDeleteContainerClick_() {
    if (this.lastMenuContainerInfo_) {
      this.browserProxy_.deleteContainer(this.lastMenuContainerInfo_.id);
    }
    this.closeContainerMenu_();
  }

  private onStopContainerClick_() {
    if (this.lastMenuContainerInfo_) {
      this.browserProxy_.stopContainer(this.lastMenuContainerInfo_.id);
    }
    this.closeContainerMenu_();
  }

  private onExportContainerClick_() {
    if (this.lastMenuContainerInfo_) {
      this.browserProxy_.exportCrostiniContainer(
          this.lastMenuContainerInfo_.id);
    }
    this.closeContainerMenu_();
  }

  private onImportContainerClick_() {
    if (this.lastMenuContainerInfo_) {
      this.browserProxy_.importCrostiniContainer(
          this.lastMenuContainerInfo_.id);
    }
    this.closeContainerMenu_();
  }

  private onContainerColorChange_(event: Event) {
    const target = event.currentTarget as HtmlElementWithData<HTMLInputElement>;
    const containerId = target['dataContainerId'];
    const hexColor = target.value;

    this.browserProxy_.setContainerBadgeColor(
        containerId, hexColorToSkColor(hexColor));
  }

  private shouldDisableDeleteContainer_(info: ContainerInfo): boolean {
    return info && info.id.vm_name === DEFAULT_CROSTINI_VM &&
        info.id.container_name === DEFAULT_CROSTINI_CONTAINER;
  }

  private shouldDisableStopContainer_(info: ContainerInfo): boolean {
    return !info || !info.ipv4;
  }

  private getContainerMenu_(): CrActionMenuElement {
    return this.$.containerMenu.get();
  }

  private closeContainerMenu_() {
    const menu = this.getContainerMenu_();
    assert(menu.open && this.lastMenuContainerInfo_);
    menu.close();
    this.lastMenuContainerInfo_ = null;
  }

  private isEnabledButtons_(
      installerShowing: boolean, exportImportInProgress: boolean): boolean {
    return !(installerShowing || exportImportInProgress);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-crostini-extra-containers': ExtraContainersElement;
  }
}

customElements.define(ExtraContainersElement.is, ExtraContainersElement);
