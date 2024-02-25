// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-extra-containers' is the settings extras containers subpage for
 * Crostini.
 */
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/ash/common/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './crostini_extra_containers_create_dialog.js';
import '../settings_shared.css.js';

import {CrActionMenuElement} from 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLazyRenderElement} from 'chrome://resources/ash/common/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {hexColorToSkColor} from 'chrome://resources/js/color_utils.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsState} from '../common/types.js';
import {ContainerInfo, GuestId, ShareableDevices, VM_DEVICE_MICROPHONE} from '../guest_os/guest_os_browser_proxy.js';
import {equalContainerId} from '../guest_os/guest_os_container_select.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, DEFAULT_CROSTINI_CONTAINER, DEFAULT_CROSTINI_VM} from './crostini_browser_proxy.js';
import {getTemplate} from './crostini_extra_containers.html.js';

type HtmlElementWithData<T extends HTMLElement = HTMLElement> = T&{
  'dataContainerId': GuestId,
};

export interface ExtraContainersElement {
  $: {
    containerMenu: CrLazyRenderElement<CrActionMenuElement>,
  };
}

interface SharedVmDevices {
  id: GuestId;
  vmDevices: ShareableDevices;
}

interface CrostiniContainerInfo extends ContainerInfo {
  detailsExpanded: boolean;
}

const ExtraContainersElementBase = WebUiListenerMixin(PolymerElement);

export class ExtraContainersElement extends ExtraContainersElementBase {
  static get is() {
    return 'settings-crostini-extra-containers';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

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

      allSharedVmDevices_: {
        type: Array,
        value() {
          return [];
        },
      },

      allVms_: {
        type: Array,
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

  prefs: PrefsState;
  private allContainers_: CrostiniContainerInfo[];
  private allSharedVmDevices_: SharedVmDevices[];
  private allVms_: string[];
  private browserProxy_: CrostiniBrowserProxy;
  private exportImportInProgress_: boolean;
  private installerShowing_: boolean;
  private lastMenuContainerInfo_: CrostiniContainerInfo|null;
  private showCreateContainerDialog_: boolean;

  constructor() {
    super();
    /**
     * Tracks the last container that was selected for delete.
     */
    this.lastMenuContainerInfo_ = null;

    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();
    this.addWebUiListener(
        'crostini-container-info',
        (infos: ContainerInfo[]) => this.onContainerInfo_(infos));
    this.addWebUiListener(
        'crostini-shared-vmdevices',
        (sharedVmDevices: SharedVmDevices[]) =>
            this.onSharedVmDevices_(sharedVmDevices));
    this.browserProxy_.requestContainerInfo();
    this.browserProxy_.requestSharedVmDevices();
  }

  override connectedCallback(): void {
    super.connectedCallback();
    this.addWebUiListener(
        'crostini-export-import-operation-status-changed',
        (inProgress: boolean) => {
          this.exportImportInProgress_ = inProgress;
        });
    this.addWebUiListener(
        'crostini-installer-status-changed', (installerShowing: boolean) => {
          this.installerShowing_ = installerShowing;
        });

    this.browserProxy_.requestCrostiniExportImportOperationStatus();
    this.browserProxy_.requestCrostiniInstallerStatus();
  }

  private setMicrophoneToggle_(id: GuestId, checked: boolean): void {
    const crToggle: CrToggleElement|null =
        this.shadowRoot!.querySelector<CrToggleElement>(
            `#microphone-${id.vm_name}-${id.container_name}`);
    if (!crToggle) {
      // The toggles may not yet have been added to the DOM.
      return;
    }
    if (crToggle.checked !== checked) {
      crToggle.set('checked', checked);
    }
  }

  private onSharedVmDevices_(sharedVmDevices: SharedVmDevices[]): void {
    this.set('allSharedVmDevices_', sharedVmDevices);
    for (const sharing of sharedVmDevices) {
      this.setMicrophoneToggle_(
          sharing.id, sharing.vmDevices[VM_DEVICE_MICROPHONE]);
    }
  }

  private async updateSharedVmDevices_(id: GuestId): Promise<void> {
    let idx = this.allSharedVmDevices_.findIndex(
        sharing => equalContainerId(sharing.id, id));

    if (idx < 0) {
      idx = this.allSharedVmDevices_.push(
                {id: id, vmDevices: {[VM_DEVICE_MICROPHONE]: false}}) -
          1;
    }
    const result: boolean =
        await this.browserProxy_.isVmDeviceShared(id, VM_DEVICE_MICROPHONE);

    this.allSharedVmDevices_[idx].vmDevices[VM_DEVICE_MICROPHONE] = result;
    this.setMicrophoneToggle_(id, result);
  }

  private onContainerInfo_(containerInfos: ContainerInfo[]): void {
    const vmNames: Set<string> = new Set();
    const crostiniContainerInfos = containerInfos as CrostiniContainerInfo[];
    for (const info of crostiniContainerInfos) {
      vmNames.add(info.id.vm_name);
      const oldContainerInfo = this.allContainers_.find(
          container => equalContainerId(container.id, info.id));
      info.detailsExpanded = (oldContainerInfo !== undefined) ?
          oldContainerInfo.detailsExpanded :
          false;
    }
    this.set('allVms_', Array.from(vmNames.values()));
    this.set('allContainers_', crostiniContainerInfos);
  }

  private onCreateClick_(): void {
    this.showCreateContainerDialog_ = true;
  }

  private onCreateContainerDialogClose_(): void {
    this.showCreateContainerDialog_ = false;
  }

  private onContainerMenuClick_(event: Event): void {
    const target = event.currentTarget as HtmlElementWithData;
    const containerId = target['dataContainerId'];
    this.lastMenuContainerInfo_ =
        this.allContainers_.find(
            e => e.id.vm_name === containerId.vm_name &&
                e.id.container_name === containerId.container_name) ||
        null;
    this.getContainerMenu_().showAt(target);
  }

  private onDeleteContainerClick_(): void {
    if (this.lastMenuContainerInfo_) {
      this.browserProxy_.deleteContainer(this.lastMenuContainerInfo_.id);
    }
    this.closeContainerMenu_();
  }

  private onStopContainerClick_(): void {
    if (this.lastMenuContainerInfo_) {
      this.browserProxy_.stopContainer(this.lastMenuContainerInfo_.id);
    }
    this.closeContainerMenu_();
  }

  private onExportContainerClick_(): void {
    if (this.lastMenuContainerInfo_) {
      this.browserProxy_.exportCrostiniContainer(
          this.lastMenuContainerInfo_.id);
    }
    this.closeContainerMenu_();
  }

  private onImportContainerClick_(): void {
    if (this.lastMenuContainerInfo_) {
      this.browserProxy_.importCrostiniContainer(
          this.lastMenuContainerInfo_.id);
    }
    this.closeContainerMenu_();
  }

  private onContainerColorChange_(event: Event): void {
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

  private closeContainerMenu_(): void {
    const menu = this.getContainerMenu_();
    assert(menu.open && this.lastMenuContainerInfo_);
    menu.close();
    this.lastMenuContainerInfo_ = null;
  }

  private isEnabledButtons_(
      installerShowing: boolean, exportImportInProgress: boolean): boolean {
    return !(installerShowing || exportImportInProgress);
  }

  private byNameWithDefault_(name1: string, name2: string, defaultName: string):
      number {
    if (name1 === name2) {
      return 0;
    }
    // defaultName sorts first.
    if (name1 === defaultName) {
      return -1;
    }
    if (name2 === defaultName) {
      return 1;
    }
    return name1 < name2 ? -1 : 1;
  }

  private byVmName_(name1: string, name2: string): number {
    return this.byNameWithDefault_(name1, name2, DEFAULT_CROSTINI_VM);
  }

  private byGuestId_(id1: GuestId, id2: GuestId): number {
    const result = this.byVmName_(id1.vm_name, id2.vm_name);
    if (result !== 0) {
      return result;
    }
    return this.byNameWithDefault_(
        id1.container_name, id2.container_name, DEFAULT_CROSTINI_CONTAINER);
  }

  private infoHasVmName_(vmName: string): (info: ContainerInfo) => boolean {
    return info => vmName === info.id.vm_name;
  }

  private isMicrophoneShared_(id: GuestId): boolean {
    const deviceSharing: SharedVmDevices|undefined =
        this.allSharedVmDevices_.find(
            (sharing: SharedVmDevices) => equalContainerId(sharing.id, id));
    if (!deviceSharing) {
      return false;
    }
    return deviceSharing.vmDevices[VM_DEVICE_MICROPHONE];
  }

  private async onMicrophoneSharingChanged_(event: Event): Promise<void> {
    const target = event.currentTarget as HtmlElementWithData<HTMLInputElement>;
    const id = target['dataContainerId'];
    const shared = target.checked;

    await this.browserProxy_.setVmDeviceShared(
        id, VM_DEVICE_MICROPHONE, shared);
    await this.updateSharedVmDevices_(id);
  }

  private showIp_(info: ContainerInfo): boolean {
    return !!info.ipv4 && info.ipv4.length > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-crostini-extra-containers': ExtraContainersElement;
  }
}

customElements.define(ExtraContainersElement.is, ExtraContainersElement);
