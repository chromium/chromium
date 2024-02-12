// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'guest-os-shared-paths' is the settings shared paths subpage for guest OSes.
 */

import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';

import {getVMNameForGuestOsType, GuestOsBrowserProxy, GuestOsBrowserProxyImpl, GuestOsType} from './guest_os_browser_proxy.js';
import {getTemplate} from './guest_os_shared_paths.html.js';

interface PathObject {
  path: string;
  pathDisplayText: string;
}

export interface SettingsGuestOsSharedPathsElement {
  $: {
    guestOsInstructionsRemove: HTMLElement,
    guestOsList: HTMLElement,
    guestOsListEmpty: HTMLElement,
  };
}

const SettingsGuestOsSharedPathsElementBase = I18nMixin(PolymerElement);

/** @polymer */
export class SettingsGuestOsSharedPathsElement extends
    SettingsGuestOsSharedPathsElementBase {
  static get is() {
    return 'settings-guest-os-shared-paths';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The type of Guest OS to share with. Should be 'crostini', 'pluginVm' or
       * 'bruschetta'.
       */
      guestOsType: String,

      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * The shared path string suitable for display in the UI.
       */
      sharedPaths_: Array,

      /**
       * The shared path which failed to be removed in the most recent attempt
       * to remove a path. Null indicates that removal succeeded. When non-null,
       * the failure dialog is shown.
       */
      sharedPathWhichFailedRemoval_: {
        type: String,
        value: null,
      },
    };
  }

  static get observers() {
    return [
      'onGuestOsSharedPathsChanged_(prefs.guest_os.paths_shared_to_vms.value)',
    ];
  }

  prefs: {[key: string]: any};
  guestOsType: GuestOsType;
  private browserProxy_: GuestOsBrowserProxy;
  private sharedPaths_: PathObject[];
  private sharedPathWhichFailedRemoval_: string|null;

  constructor() {
    super();

    this.browserProxy_ = GuestOsBrowserProxyImpl.getInstance();
  }

  private onGuestOsSharedPathsChanged_(paths: {[key: string]: string[]}): void {
    const vmPaths: string[] = [];
    for (const path in paths) {
      const vms = paths[path];
      if (vms.includes(this.vmName_())) {
        vmPaths.push(path);
      }
    }
    this.browserProxy_.getGuestOsSharedPathsDisplayText(vmPaths).then(text => {
      this.sharedPaths_ =
          vmPaths.map((path, i) => ({path: path, pathDisplayText: text[i]}));
    });
  }

  private removeSharedPath_(path: string): void {
    this.sharedPathWhichFailedRemoval_ = null;
    this.browserProxy_.removeGuestOsSharedPath(this.vmName_(), path)
        .then(success => {
          if (!success) {
            this.sharedPathWhichFailedRemoval_ = path;
          }
        });
  }

  private onRemoveSharedPathClick_(event: DomRepeatEvent<PathObject>): void {
    this.removeSharedPath_(event.model.item.path);
  }

  private onRemoveFailedRetryClick_(): void {
    this.removeSharedPath_(castExists(this.sharedPathWhichFailedRemoval_));
  }

  private onRemoveFailedDismissClick_(): void {
    this.sharedPathWhichFailedRemoval_ = null;
  }

  /**
   * @return The name of the VM to share devices with.
   */
  private vmName_(): string {
    return getVMNameForGuestOsType(this.guestOsType);
  }

  /**
   * @return Description for the page.
   */
  private getDescriptionText_(): string {
    return this.i18n(this.guestOsType + 'SharedPathsInstructionsLocate') +
        '\n' + this.i18n(this.guestOsType + 'SharedPathsInstructionsAdd');
  }

  /**
   * @return Message to display when removing a shared path fails.
   */
  private getRemoveFailureMessage_(): string {
    return this.i18n(
        this.guestOsType + 'SharedPathsRemoveFailureDialogMessage');
  }

  private generatePathDisplayTextId_(index: number): string {
    return 'path-display-text-' + index;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-guest-os-shared-paths': SettingsGuestOsSharedPathsElement;
  }
}

customElements.define(
    SettingsGuestOsSharedPathsElement.is, SettingsGuestOsSharedPathsElement);
